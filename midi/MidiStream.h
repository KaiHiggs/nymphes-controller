// Pieces of the MIDI transport that every backend must implement IDENTICALLY.
//
// MidiPort.mm (CoreMIDI), MidiPortWin.cpp (WinMM) and MidiPortNone.cpp each used to
// carry their own copy of the same two rules, and the copies had drifted:
//
//  * endpoint matching was case-insensitive in two backends and case-sensitive in
//    the third, so a synthetic MidiDeviceInfo named "nymphes" answered differently
//    depending on the platform;
//  * the F0..F7 reassembler had no bound and no lost-terminator recovery, so one
//    dropped 0xF7 on USB left the port permanently deaf and growing memory.
//
// The rules live here once, JUCE-free and platform-free, so they compile on every
// target and can be tested without a device (tests/midi_module_test.cpp). The
// backends own their OS calls; they do not own these rules.
//
// The intent is fixed by MidiPort.h's design points: reassemble on F0..F7, drop
// real-time bytes (>= 0xF8) inside a SysEx stream, never assume packet alignment.

#pragma once

#include "MidiPort.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nymphes
{
namespace midi_detail
{

/** Case-insensitive substring test, byte-wise and locale-free.
    Windows decorates endpoint names ("2- Nymphes", "MIDIIN2 (Nymphes)") and macOS
    endpoints can carry UTF-8 suffixes, so both sides are lowered through
    `unsigned char` (passing a negative `char` to ::tolower is undefined). */
inline bool containsIgnoreCase (const std::string& hay, const std::string& needle) noexcept
{
    auto lower = [] (std::string s)
    {
        std::transform (s.begin(), s.end(), s.begin(),
                        [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
        return s;
    };
    return lower (hay).find (lower (needle)) != std::string::npos;
}

/** Bytes after the status byte for a channel-voice message; 0 for anything that is
    not channel voice (system common/real-time). */
inline int bytesNeeded (uint8_t status) noexcept
{
    switch (status & 0xF0)
    {
        case 0xC0: case 0xD0: return 1;
        case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 2;
        default:   return 0;   // 0xF0: system messages are not ours
    }
}

/** A MIDI byte-stream parser: SysEx frames on F0..F7 plus channel messages.
    Bytes may arrive in any fragmentation; the parser keeps the state a real device
    needs (running status, a partial SysEx frame) between calls.

    Deliberate behaviour, all of it tested:
      * bytes >= 0xF8 (clock, start/stop, active sensing) are dropped, including
        inside a SysEx stream - the device interleaves them;
      * a new 0xF0 restarts a frame (a truncated frame is discarded);
      * a status byte (0x80..0xF7) arriving *inside* a frame means the terminating
        0xF7 was lost: the partial frame is abandoned and the byte is interpreted as
        the start of the next message, instead of appending forever and swallowing
        every later channel message;
      * a frame that grows past kMaxSysExBytes is abandoned (memory is bounded);
      * running status is honoured (0x90 3C 40 3C 40 is two note-ons) and cancelled
        by System Exclusive, as MIDI 1.0 requires: a data byte after a frame does not
        complete a channel message from before it;
      * reset() forgets the partial frame AND the running status, so a close/reopen
        cannot turn the first byte of the next session into the tail of the last. */
class StreamParser
{
public:
    static constexpr std::size_t kMaxSysExBytes = 64 * 1024;

    void push (const uint8_t* d, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
            pushByte (d[i]);
    }

    /** Complete frames seen since the last take, in arrival order. */
    std::vector<std::vector<uint8_t>> takeSysEx()
    {
        auto out = std::move (frames_);
        frames_.clear();
        return out;
    }

    /** Complete channel messages seen since the last take, in arrival order. */
    std::vector<ChannelMessage> takeChannel()
    {
        auto out = std::move (msgs_);
        msgs_.clear();
        return out;
    }

    /** Forget partial state: used by every teardown path (close, reconnect). */
    void reset() noexcept
    {
        buf_.clear();
        frames_.clear();
        msgs_.clear();
        inSysEx_  = false;
        status_   = 0;
        dataCount_ = 0;
        data_[0] = data_[1] = 0;
    }

    bool inSysEx() const noexcept { return inSysEx_; }
    std::size_t pendingSysExBytes() const noexcept { return buf_.size(); }

private:
    void pushByte (uint8_t b)
    {
        if (b >= 0xF8) return;                      // real time: dropped even in SysEx

        if (b == 0xF0)
        {
            buf_.assign (1, 0xF0);                  // a truncated frame is discarded
            inSysEx_  = true;
            dataCount_ = 0;
            status_    = 0;   // MIDI 1.0: SysEx cancels running status
            return;
        }

        if (inSysEx_)
        {
            if (b == 0xF7)
            {
                inSysEx_ = false;
                buf_.push_back (b);
                frames_.push_back (std::move (buf_));
                buf_.clear();
                status_    = 0;   // ...and so does EOX
                dataCount_ = 0;
                return;
            }
            if ((b & 0x80) != 0)
            {
                // The terminator was lost. Do not keep appending: abandon the frame
                // and read this byte as the start of a new message.
                inSysEx_ = false;
                buf_.clear();
                status_    = ((b & 0xF0) == 0xF0) ? uint8_t { 0 } : b;
                dataCount_ = 0;
                return;
            }
            if (buf_.size() >= kMaxSysExBytes)
            {
                inSysEx_ = false;
                buf_.clear();
                return;
            }
            buf_.push_back (b);
            return;
        }

        if ((b & 0x80) != 0)
        {
            status_    = ((b & 0xF0) == 0xF0) ? uint8_t { 0 } : b;
            dataCount_ = 0;
            return;
        }

        if (status_ == 0) return;
        const int need = bytesNeeded (status_);
        if (need == 0) { status_ = 0; return; }

        if (dataCount_ < 2) data_[dataCount_] = b;
        ++dataCount_;
        if (dataCount_ >= need)
        {
            ChannelMessage m;
            m.status = status_;
            m.data1  = data_[0];
            m.data2  = (need > 1) ? data_[1] : 0;
            msgs_.push_back (m);
            dataCount_ = 0;
        }
    }

    std::vector<uint8_t>             buf_;
    std::vector<std::vector<uint8_t>> frames_;
    std::vector<ChannelMessage>      msgs_;
    bool    inSysEx_   = false;
    uint8_t status_    = 0;
    uint8_t data_[2]   = { 0, 0 };
    int     dataCount_ = 0;
};

} // namespace midi_detail
} // namespace nymphes
