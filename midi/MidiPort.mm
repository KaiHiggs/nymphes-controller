// CoreMIDI backend. Compiled only on Apple platforms (CMake gates this file).
//
// Uses CoreMIDI directly rather than a wrapper so the plugin opens the Nymphes
// port without depending on any MIDI routing inside Ableton Live: the plugin is
// the client, the OS delivers the bytes, the host is not involved.

#include "MidiPort.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace nymphes
{

namespace
{
std::string toStd (CFStringRef s)
{
    if (s == nullptr) return {};
    char buf[512] = {};
    if (CFStringGetCString (s, buf, sizeof buf, kCFStringEncodingUTF8)) return buf;
    return {};
}

std::string stringProperty (MIDIObjectRef obj, CFStringRef key)
{
    CFStringRef value = nullptr;
    if (MIDIObjectGetStringProperty (obj, key, &value) != noErr || value == nullptr) return {};
    std::string out = toStd (value);
    CFRelease (value);
    return out;
}

MidiDeviceInfo describe (MIDIEndpointRef ep, bool asSource)
{
    MidiDeviceInfo info;
    info.name = stringProperty (ep, kMIDIPropertyDisplayName);

    // The endpoint's owning entity/device carries the manufacturer/model.
    if (info.name.empty())
        info.name = stringProperty (ep, asSource ? kMIDIPropertyName : kMIDIPropertyName);

    MIDIEntityRef entity = 0;
    if (MIDIEndpointGetEntity (ep, &entity) == noErr && entity != 0)
    {
        MIDIDeviceRef device = 0;
        if (MIDIEntityGetDevice (entity, &device) == noErr && device != 0)
        {
            info.manufacturer = stringProperty (device, kMIDIPropertyManufacturer);
            info.model        = stringProperty (device, kMIDIPropertyModel);
        }
    }
    return info;
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

} // namespace

// ---------------------------------------------------------------------------

struct MidiPort::Impl
{
    MIDIClientRef      client  = 0;
    MIDIPortRef        input   = 0;
    MIDIPortRef        output  = 0;
    MIDIEndpointRef    source  = 0;   // device -> us
    MIDIEndpointRef    dest    = 0;   // us -> device
    MIDIEndpointRef    connectedSource = 0;

    std::string        openedName;
    std::string        wanted = "Nymphes";
    bool               watching = false;
    std::atomic<bool>  isOpenFlag { false };

    mutable std::mutex mutex;
    SysExCallback      sysexCb;
    ChannelCallback    channelCb;
    ConnectionCallback connectionCb;

    // SysEx reassembly state (guarded by `mutex` as well; only the input thread
    // mutates it, but a reconnect resets it).
    std::vector<uint8_t> sysexBuf;
    bool                 inSysex   = false;
    uint8_t              status    = 0;
    uint8_t              data[2]   = { 0, 0 };
    int                  dataCount = 0;

    ~Impl() { shutdown(); }

    void shutdown()
    {
        const bool wasOpen = isOpenFlag.exchange (false);
        if (input != 0 && connectedSource != 0)
            MIDIPortDisconnectSource (input, connectedSource);
        connectedSource = 0;
        source = dest = 0;
        if (input  != 0) { MIDIPortDispose (input);  input = 0; }
        if (output != 0) { MIDIPortDispose (output); output = 0; }
        if (client != 0) { MIDIClientDispose (client); client = 0; }
        {
            std::lock_guard<std::mutex> lock (mutex);
            sysexBuf.clear();
            inSysex = false;
            dataCount = 0;
        }
        if (wasOpen) { openedName.clear(); notify (false); }
    }

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

    // Called with the device's bytes, on the CoreMIDI thread.
    void feed (const uint8_t* d, size_t n)
    {
        std::vector<std::vector<uint8_t>> sysexFrames;
        std::vector<ChannelMessage>       channelMsgs;

        {
            std::lock_guard<std::mutex> lock (mutex);
            for (size_t i = 0; i < n; ++i)
            {
                const uint8_t b = d[i];

                // Real time (clock, start/stop/continue, active sensing) may be
                // interleaved *inside* a SysEx stream and must not be buffered.
                if (b >= 0xF8) continue;

                if (b == 0xF0)
                {
                    sysexBuf.clear();
                    sysexBuf.push_back (0xF0);
                    inSysex = true;
                    dataCount = 0;
                    continue;
                }

                if (inSysex)
                {
                    sysexBuf.push_back (b);
                    if (b == 0xF7)
                    {
                        inSysex = false;
                        sysexFrames.push_back (sysexBuf);
                        sysexBuf.clear();
                    }
                    continue;
                }

                if (b & 0x80)
                {
                    status    = b;
                    dataCount = 0;
                    if ((b & 0xF0) == 0xF0) status = 0;   // system common we do not handle
                    continue;
                }

                if (status == 0) continue;
                const int need = bytesNeeded (status);
                if (need == 0) { status = 0; continue; }
                data[dataCount++] = b;
                if (dataCount >= need)
                {
                    ChannelMessage m;
                    m.status = status;
                    m.data1  = data[0];
                    m.data2  = (need > 1) ? data[1] : 0;
                    channelMsgs.push_back (m);
                    dataCount = 0;
                }
            }
        }

        if (! sysexFrames.empty() || ! channelMsgs.empty())
        {
            SysExCallback   scb;
            ChannelCallback ccb;
            {
                std::lock_guard<std::mutex> lock (mutex);
                scb = sysexCb;
                ccb = channelCb;
            }
            for (auto& f : sysexFrames) if (scb) scb (f);
            for (auto& m : channelMsgs) if (ccb) ccb (m);
        }
    }

    void handlePacketList (const MIDIPacketList* list)
    {
        const MIDIPacket* pkt = &list->packet[0];
        for (UInt32 i = 0; i < list->numPackets; ++i)
        {
            feed (pkt->data, pkt->length);
            pkt = MIDIPacketNext (pkt);
        }
    }

    static void readProc (const MIDIPacketList* list, void* refCon, void* /*srcConnRefCon*/)
    {
        auto* self = static_cast<Impl*> (refCon);
        if (self != nullptr && list != nullptr) self->handlePacketList (list);
    }

    // Re-find the endpoint pair by name; returns true when both sides attached.
    bool attach (const std::string& want, std::string* error)
    {
        dest = 0; source = 0;

        auto matches = [&want] (const MidiDeviceInfo& info)
        {
            if (! want.empty())
            {
                std::string lowerName = info.name, lowerWant = want;
                std::transform (lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform (lowerWant.begin(), lowerWant.end(), lowerWant.begin(), ::tolower);
                return lowerName.find (lowerWant) != std::string::npos;
            }
            return looksLikeNymphes (info) || info.name.find ("Nymphes") != std::string::npos;
        };

        const ItemCount nDest = MIDIGetNumberOfDestinations();
        for (ItemCount i = 0; i < nDest; ++i)
        {
            MIDIEndpointRef ep = MIDIGetDestination (i);
            if (ep != 0 && matches (describe (ep, false))) { dest = ep; openedName = describe (ep, false).name; break; }
        }

        const ItemCount nSrc = MIDIGetNumberOfSources();
        for (ItemCount i = 0; i < nSrc; ++i)
        {
            MIDIEndpointRef ep = MIDIGetSource (i);
            if (ep != 0 && matches (describe (ep, true))) { source = ep; if (openedName.empty()) openedName = describe (ep, true).name; break; }
        }

        if (dest == 0 && source == 0)
        {
            if (error != nullptr)
                *error = want.empty() ? "no Nymphes MIDI endpoint found"
                                      : ("no MIDI endpoint matching \"" + want + "\"");
            return false;
        }
        return true;
    }

    bool start (const std::string& want, bool watch, std::string* error)
    {
        std::lock_guard<std::mutex> lifecycle (mutex);
        shutdownLocked();

        wanted = want;
        watching = watch;

        CFStringRef clientName = CFSTR ("NymphesController");
        OSStatus st = MIDIClientCreate (clientName, nullptr, nullptr, &client);
        if (st != noErr || client == 0)
        {
            if (error != nullptr) *error = "MIDIClientCreate failed (" + std::to_string (st) + ")";
            return false;
        }

        st = MIDIOutputPortCreate (client, CFSTR ("out"), &output);
        if (st != noErr)
        {
            if (error != nullptr) *error = "MIDIOutputPortCreate failed (" + std::to_string (st) + ")";
            MIDIClientDispose (client); client = 0;
            return false;
        }

        st = MIDIInputPortCreate (client, CFSTR ("in"), &Impl::readProc, this, &input);
        if (st != noErr)
        {
            if (error != nullptr) *error = "MIDIInputPortCreate failed (" + std::to_string (st) + ")";
            MIDIPortDispose (output); output = 0;
            MIDIClientDispose (client); client = 0;
            return false;
        }

        // NOTE: shutdownLocked() may call notify() which takes `mutex`; it only
        // does so when it was previously open, and this call path holds the lock.
        if (! attach (want, error))
        {
            MIDIPortDispose (input);  input  = 0;
            MIDIPortDispose (output); output = 0;
            MIDIClientDispose (client); client = 0;
            return false;
        }

        if (source != 0)
        {
            const OSStatus cs = MIDIPortConnectSource (input, source, nullptr);
            if (cs != noErr)
            {
                if (error != nullptr) *error = "MIDIPortConnectSource failed (" + std::to_string (cs) + ")";
            }
            else
            {
                connectedSource = source;
            }
        }

        isOpenFlag = true;
        return true;
    }

    // Variant that assumes `mutex` is already held, so callers do not deadlock.
    void shutdownLocked()
    {
        if (input != 0 && connectedSource != 0)
            MIDIPortDisconnectSource (input, connectedSource);
        connectedSource = 0;
        source = dest = 0;
        if (input  != 0) { MIDIPortDispose (input);  input = 0; }
        if (output != 0) { MIDIPortDispose (output); output = 0; }
        if (client != 0) { MIDIClientDispose (client); client = 0; }
        sysexBuf.clear();
        inSysex = false;
        dataCount = 0;
        if (isOpenFlag.exchange (false)) openedName.clear();
    }

    bool send (const uint8_t* data, size_t size, std::string* error)
    {
        if (! isOpenFlag.load() || dest == 0 || output == 0)
        {
            if (error != nullptr) *error = "MIDI port is not open";
            return false;
        }

        // One MIDIPacket per <=256-byte chunk; the packet list keeps them ordered.
        constexpr size_t kChunk = 256;
        const size_t chunks = (size + kChunk - 1) / kChunk;
        std::vector<uint8_t> storage (sizeof (MIDIPacketList) + chunks * sizeof (MIDIPacket) + 16);

        auto* list = reinterpret_cast<MIDIPacketList*> (storage.data());
        MIDIPacket* pkt = MIDIPacketListInit (list);
        size_t offset = 0;
        while (offset < size)
        {
            const size_t n = std::min (kChunk, size - offset);
            pkt = MIDIPacketListAdd (list, storage.size(), pkt, 0, n, data + offset);
            if (pkt == nullptr)
            {
                if (error != nullptr) *error = "MIDIPacketListAdd failed (list full)";
                return false;
            }
            offset += n;
        }

        const OSStatus st = MIDISend (output, dest, list);
        if (st != noErr)
        {
            if (error != nullptr) *error = "MIDISend failed (" + std::to_string (st) + ")";
            return false;
        }
        return true;
    }
};

// ---------------------------------------------------------------------------

MidiPort::MidiPort() : impl (new Impl()) {}
MidiPort::~MidiPort() = default;

const char* MidiPort::backendName() noexcept { return "CoreMIDI"; }

std::vector<MidiDeviceInfo> MidiPort::listDestinations()
{
    std::vector<MidiDeviceInfo> out;
    const ItemCount n = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < n; ++i)
        if (MIDIEndpointRef ep = MIDIGetDestination (i); ep != 0)
            out.push_back (describe (ep, false));
    return out;
}

std::vector<MidiDeviceInfo> MidiPort::listSources()
{
    std::vector<MidiDeviceInfo> out;
    const ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; ++i)
        if (MIDIEndpointRef ep = MIDIGetSource (i); ep != 0)
            out.push_back (describe (ep, true));
    return out;
}

bool MidiPort::looksLikeNymphes (const MidiDeviceInfo& info)
{
    auto has = [] (const std::string& hay, const char* needle)
    {
        std::string h = hay, w = needle;
        std::transform (h.begin(), h.end(), h.begin(), ::tolower);
        std::transform (w.begin(), w.end(), w.begin(), ::tolower);
        return h.find (w) != std::string::npos;
    };
    // Name/model only. Never VID:PID: 0483:5740 is ST's generic VCP pair and
    // matches unrelated devices.
    return has (info.name, "nymphes") || has (info.model, "nymphes");
}

bool MidiPort::open (const std::string& nameSubstring, std::string* error)
{
    return impl->start (nameSubstring, false, error);
}

bool MidiPort::openWatching (const std::string& nameSubstring, std::string* error)
{
    return impl->start (nameSubstring, true, error);
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
    return impl->send (frame.data(), frame.size(), error);
}

bool MidiPort::sendChannel (uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t bytes[3] = { status, data1, data2 };
    const int need = bytesNeeded (status);
    if (need == 0) return false;
    return impl->send (bytes, static_cast<size_t> (need + 1), nullptr);
}

bool MidiPort::sendCC (int channel, int cc, int value)
{
    if (channel < 0 || channel > 15 || cc < 0 || cc > 127) return false;
    const int v = std::clamp (value, 0, 127);
    return sendChannel (static_cast<uint8_t> (0xB0 | channel), static_cast<uint8_t> (cc), static_cast<uint8_t> (v));
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
