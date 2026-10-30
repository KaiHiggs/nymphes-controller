// WinMM transport for the Nymphes — the Windows twin of MidiPort.mm.
//
// Same contract as the CoreMIDI backend (see MidiPort.h):
//
//  * The port is found BY NAME, case-insensitively, never by VID:PID (0483:5740
//    is ST's generic VCP pair and matches unrelated hardware). Windows often
//    decorates the name ("Nymphes", "2- Nymphes", "MIDIIN2 (Nymphes)"), which a
//    substring match covers.
//  * SysEx is reassembled on F0..F7. WinMM delivers it in MIDIHDR buffers that
//    need not align with message boundaries, so a frame may span buffers.
//  * Real-time bytes (>= 0xF8) inside a SysEx stream are dropped.
//  * Callbacks run on the WinMM callback thread.
//
// WinMM differences worth knowing:
//  * A WinMM device is exclusive: if another application (a DAW with the
//    Nymphes enabled as a MIDI port) holds it, opening fails with "allocated".
//  * There is no hot-plug notification, so openWatching() is open().
//  * Short messages and SysEx arrive on separate callbacks (MIM_DATA vs
//    MIM_LONGDATA), so they are parsed separately: a CC can never be appended to
//    a half-received SysEx frame.
//
// No JUCE dependency: the headless CLI links this directly.

#include "MidiPort.h"

#if defined (_WIN32)

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>

namespace nymphes
{

namespace
{

std::string narrow (const wchar_t* w)
{
    if (w == nullptr || *w == 0)
        return {};
    const int n = WideCharToMultiByte (CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1)
        return {};
    std::string out ((size_t) n - 1, '\0');
    WideCharToMultiByte (CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

bool containsIgnoreCase (std::string hay, std::string needle)
{
    auto lower = [] (std::string& s)
    {
        std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return (char) ::tolower (c); });
    };
    lower (hay);
    lower (needle);
    return hay.find (needle) != std::string::npos;
}

// Bytes needed after the status byte for a channel message.
int bytesNeeded (uint8_t status)
{
    switch (status & 0xF0)
    {
        case 0xC0: case 0xD0: return 1;
        case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 2;
        default: return 0;
    }
}

std::string errorText (MMRESULT r, bool input)
{
    wchar_t buf[MAXERRORLENGTH] = {};
    if (input) midiInGetErrorTextW (r, buf, MAXERRORLENGTH);
    else       midiOutGetErrorTextW (r, buf, MAXERRORLENGTH);
    auto s = narrow (buf);
    return s.empty() ? ("MMRESULT " + std::to_string (r)) : s;
}

MidiDeviceInfo outInfo (UINT id)
{
    MIDIOUTCAPSW caps {};
    MidiDeviceInfo info;
    if (midiOutGetDevCapsW (id, &caps, sizeof (caps)) == MMSYSERR_NOERROR)
        info.name = narrow (caps.szPname);
    return info;
}

MidiDeviceInfo inInfo (UINT id)
{
    MIDIINCAPSW caps {};
    MidiDeviceInfo info;
    if (midiInGetDevCapsW (id, &caps, sizeof (caps)) == MMSYSERR_NOERROR)
        info.name = narrow (caps.szPname);
    return info;
}

constexpr int    kInBuffers   = 8;
constexpr DWORD  kInBufferLen = 4096;    // a preset frame is ~2.1 KB
constexpr DWORD  kSendTimeoutMs = 5000;

} // namespace

// ---------------------------------------------------------------------------

struct MidiPort::Impl
{
    HMIDIOUT out = nullptr;
    HMIDIIN  in  = nullptr;

    std::array<MIDIHDR, kInBuffers> inHeaders {};
    std::array<std::array<char, kInBufferLen>, kInBuffers> inData {};

    std::string        openedName;
    std::atomic<bool>  isOpenFlag { false };
    std::atomic<bool>  closing { false };

    mutable std::mutex mutex;       // callbacks, name, SysEx reassembly
    std::mutex         lifecycle;   // open/close
    std::mutex         sendMutex;   // one long message at a time

    SysExCallback      sysexCb;
    ChannelCallback    channelCb;
    ConnectionCallback connectionCb;

    std::vector<uint8_t> sysexBuf;
    bool                 inSysex = false;

    ~Impl() { shutdown(); }

    void notify (bool connected)
    {
        ConnectionCallback cb;
        std::string name;
        {
            std::lock_guard<std::mutex> lock (mutex);
            cb = connectionCb;
            name = openedName;
        }
        if (cb) cb (connected, name);
    }

    // --- input ---------------------------------------------------------------

    void feedShort (DWORD packed)
    {
        const auto status = (uint8_t) (packed & 0xFF);
        if (status >= 0xF0)
            return;                         // system / real-time: not ours
        const int need = bytesNeeded (status);
        if (need == 0)
            return;

        ChannelMessage m;
        m.status = status;
        m.data1  = (uint8_t) ((packed >> 8) & 0x7F);
        m.data2  = need > 1 ? (uint8_t) ((packed >> 16) & 0x7F) : 0;

        ChannelCallback cb;
        {
            std::lock_guard<std::mutex> lock (mutex);
            cb = channelCb;
        }
        if (cb) cb (m);
    }

    void feedSysex (const uint8_t* d, size_t n)
    {
        std::vector<std::vector<uint8_t>> frames;
        SysExCallback cb;
        {
            std::lock_guard<std::mutex> lock (mutex);
            for (size_t i = 0; i < n; ++i)
            {
                const uint8_t b = d[i];
                if (b >= 0xF8) continue;               // real time inside SysEx
                if (b == 0xF0)
                {
                    sysexBuf.assign (1, 0xF0);
                    inSysex = true;
                    continue;
                }
                if (! inSysex) continue;
                sysexBuf.push_back (b);
                if (b == 0xF7)
                {
                    inSysex = false;
                    frames.push_back (std::move (sysexBuf));
                    sysexBuf.clear();
                }
            }
            cb = sysexCb;
        }
        if (cb)
            for (auto& f : frames)
                cb (f);
    }

    static void CALLBACK inProc (HMIDIIN handle, UINT msg, DWORD_PTR instance,
                                 DWORD_PTR param1, DWORD_PTR)
    {
        auto* self = reinterpret_cast<Impl*> (instance);
        if (self == nullptr)
            return;

        if (msg == MIM_DATA)
        {
            self->feedShort ((DWORD) param1);
        }
        else if (msg == MIM_LONGDATA)
        {
            auto* hdr = reinterpret_cast<MIDIHDR*> (param1);
            if (hdr == nullptr)
                return;
            if (hdr->dwBytesRecorded > 0)
                self->feedSysex (reinterpret_cast<const uint8_t*> (hdr->lpData), hdr->dwBytesRecorded);
            // Hand the buffer back unless we are closing (midiInReset returns
            // every buffer through here; re-adding them would stall the close).
            if (! self->closing.load())
            {
                hdr->dwBytesRecorded = 0;
                midiInAddBuffer (handle, hdr, sizeof (MIDIHDR));
            }
        }
    }

    // --- lifecycle -----------------------------------------------------------

    bool start (const std::string& want, std::string* error)
    {
        std::lock_guard<std::mutex> guard (lifecycle);
        shutdownLocked();

        auto matches = [&want] (const MidiDeviceInfo& info)
        {
            return want.empty() ? MidiPort::looksLikeNymphes (info)
                                : containsIgnoreCase (info.name, want);
        };

        int outId = -1, inId = -1;
        std::string name;
        for (UINT i = 0, n = midiOutGetNumDevs(); i < n; ++i)
            if (const auto info = outInfo (i); matches (info)) { outId = (int) i; name = info.name; break; }
        for (UINT i = 0, n = midiInGetNumDevs(); i < n; ++i)
            if (const auto info = inInfo (i); matches (info)) { inId = (int) i; if (name.empty()) name = info.name; break; }

        if (outId < 0 && inId < 0)
        {
            if (error != nullptr)
                *error = want.empty() ? "no Nymphes MIDI endpoint found"
                                      : ("no MIDI endpoint matching \"" + want + "\"");
            return false;
        }

        closing = false;

        if (outId >= 0)
        {
            const MMRESULT r = midiOutOpen (&out, (UINT) outId, 0, 0, CALLBACK_NULL);
            if (r != MMSYSERR_NOERROR)
            {
                out = nullptr;
                if (error != nullptr)
                    *error = "cannot open MIDI output \"" + name + "\": " + errorText (r, false)
                           + (r == MMSYSERR_ALLOCATED ? " (another application is using it)" : "");
                return false;
            }
        }

        if (inId >= 0)
        {
            MMRESULT r = midiInOpen (&in, (UINT) inId, (DWORD_PTR) &Impl::inProc,
                                     (DWORD_PTR) this, CALLBACK_FUNCTION);
            if (r == MMSYSERR_NOERROR)
            {
                for (int i = 0; i < kInBuffers; ++i)
                {
                    auto& h = inHeaders[(size_t) i];
                    h = {};
                    h.lpData = inData[(size_t) i].data();
                    h.dwBufferLength = kInBufferLen;
                    midiInPrepareHeader (in, &h, sizeof (MIDIHDR));
                    midiInAddBuffer (in, &h, sizeof (MIDIHDR));
                }
                r = midiInStart (in);
            }
            if (r != MMSYSERR_NOERROR)
            {
                // Output alone still lets CCs and preset sends work; say so.
                if (error != nullptr)
                    *error = "MIDI input \"" + name + "\" unavailable: " + errorText (r, true);
                closeInput();
            }
        }

        {
            std::lock_guard<std::mutex> lock (mutex);
            openedName = name;
            sysexBuf.clear();
            inSysex = false;
        }
        isOpenFlag = true;     // like CoreMIDI: the callback reports closes, not opens
        return true;
    }

    void closeInput()
    {
        if (in == nullptr)
            return;
        closing = true;
        midiInStop (in);
        midiInReset (in);           // returns all buffers via MIM_LONGDATA
        for (auto& h : inHeaders)
            if (h.dwFlags & MHDR_PREPARED)
                midiInUnprepareHeader (in, &h, sizeof (MIDIHDR));
        midiInClose (in);
        in = nullptr;
    }

    // Requires `lifecycle`. Never holds `mutex` across WinMM calls: the input
    // callback takes `mutex`, and midiInReset waits for that callback. Returns
    // whether the port was open; the caller notifies after releasing `lifecycle`,
    // so a connection handler may reopen the port without deadlocking.
    bool shutdownLocked()
    {
        closeInput();
        if (out != nullptr)
        {
            std::lock_guard<std::mutex> s (sendMutex);
            midiOutReset (out);
            midiOutClose (out);
            out = nullptr;
        }
        const bool wasOpen = isOpenFlag.exchange (false);
        std::lock_guard<std::mutex> lock (mutex);
        sysexBuf.clear();
        inSysex = false;
        if (wasOpen)
            openedName.clear();
        return wasOpen;
    }

    void shutdown()
    {
        bool wasOpen = false;
        {
            std::lock_guard<std::mutex> guard (lifecycle);
            wasOpen = shutdownLocked();
        }
        if (wasOpen)
            notify (false);
    }

    // --- output --------------------------------------------------------------

    bool sendShort (uint8_t status, uint8_t d1, uint8_t d2)
    {
        if (! isOpenFlag.load() || out == nullptr)
            return false;
        const DWORD packed = (DWORD) status | ((DWORD) d1 << 8) | ((DWORD) d2 << 16);
        return midiOutShortMsg (out, packed) == MMSYSERR_NOERROR;
    }

    bool sendLong (const std::vector<uint8_t>& frame, std::string* error)
    {
        std::lock_guard<std::mutex> s (sendMutex);
        if (! isOpenFlag.load() || out == nullptr)
        {
            if (error != nullptr) *error = "MIDI port is not open";
            return false;
        }

        std::vector<char> data (frame.begin(), frame.end());
        MIDIHDR h {};
        h.lpData = data.data();
        h.dwBufferLength = (DWORD) data.size();
        h.dwBytesRecorded = (DWORD) data.size();

        MMRESULT r = midiOutPrepareHeader (out, &h, sizeof (MIDIHDR));
        if (r == MMSYSERR_NOERROR)
            r = midiOutLongMsg (out, &h, sizeof (MIDIHDR));
        if (r != MMSYSERR_NOERROR)
        {
            if (h.dwFlags & MHDR_PREPARED)
                midiOutUnprepareHeader (out, &h, sizeof (MIDIHDR));
            if (error != nullptr) *error = "midiOutLongMsg failed: " + errorText (r, false);
            return false;
        }

        // The header and its buffer must outlive the transfer: wait for DONE.
        const DWORD started = GetTickCount();
        while ((h.dwFlags & MHDR_DONE) == 0 && GetTickCount() - started < kSendTimeoutMs)
            Sleep (1);

        const bool done = (h.dwFlags & MHDR_DONE) != 0;
        if (! done)
            midiOutReset (out);     // abandons the transfer and marks the header done
        midiOutUnprepareHeader (out, &h, sizeof (MIDIHDR));

        if (! done && error != nullptr)
            *error = "SysEx send timed out";
        return done;
    }
};

// ---------------------------------------------------------------------------

MidiPort::MidiPort() : impl (new Impl()) {}
MidiPort::~MidiPort() = default;

const char* MidiPort::backendName() noexcept { return "WinMM"; }

std::vector<MidiDeviceInfo> MidiPort::listDestinations()
{
    std::vector<MidiDeviceInfo> v;
    for (UINT i = 0, n = midiOutGetNumDevs(); i < n; ++i)
        v.push_back (outInfo (i));
    return v;
}

std::vector<MidiDeviceInfo> MidiPort::listSources()
{
    std::vector<MidiDeviceInfo> v;
    for (UINT i = 0, n = midiInGetNumDevs(); i < n; ++i)
        v.push_back (inInfo (i));
    return v;
}

bool MidiPort::looksLikeNymphes (const MidiDeviceInfo& info)
{
    // Name/model only. Never VID:PID: 0483:5740 is ST's generic VCP pair.
    return containsIgnoreCase (info.name, "nymphes") || containsIgnoreCase (info.model, "nymphes");
}

bool MidiPort::open (const std::string& nameSubstring, std::string* error)
{
    return impl->start (nameSubstring, error);
}

bool MidiPort::openWatching (const std::string& nameSubstring, std::string* error)
{
    // WinMM has no hot-plug notification; the caller reconnects explicitly.
    return impl->start (nameSubstring, error);
}

void MidiPort::close() { impl->shutdown(); }
bool MidiPort::isOpen() const { return impl->isOpenFlag.load(); }

std::string MidiPort::openedName() const
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    return impl->openedName;
}

bool MidiPort::sendSysEx (const std::vector<uint8_t>& frame, std::string* error)
{
    if (frame.size() < 4 || frame.front() != 0xF0 || frame.back() != 0xF7)
    {
        if (error != nullptr) *error = "not a complete F0..F7 SysEx frame";
        return false;
    }
    return impl->sendLong (frame, error);
}

bool MidiPort::sendChannel (uint8_t status, uint8_t data1, uint8_t data2)
{
    if (bytesNeeded (status) == 0)
        return false;
    return impl->sendShort (status, data1, data2);
}

bool MidiPort::sendCC (int channel, int cc, int value)
{
    if (channel < 0 || channel > 15 || cc < 0 || cc > 127) return false;
    const int v = std::clamp (value, 0, 127);
    return sendChannel ((uint8_t) (0xB0 | channel), (uint8_t) cc, (uint8_t) v);
}

void MidiPort::setSysExCallback (SysExCallback cb)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->sysexCb = std::move (cb);
}

void MidiPort::setChannelCallback (ChannelCallback cb)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->channelCb = std::move (cb);
}

void MidiPort::setConnectionCallback (ConnectionCallback cb)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->connectionCb = std::move (cb);
}

} // namespace nymphes

#endif // _WIN32
