// CoreMIDI transport for the Nymphes.
//
// Design points that are load-bearing (each one is a fact about this device, not
// a preference):
//
//  * The port is identified BY NAME ("Nymphes"), never by
//    VID:PID. The device answers to 0483:5740, which is STMicroelectronics'
//    generic virtual-COM-port pair — every ST VCP demo board in the world has
//    it — so matching on VID:PID would attach to an unrelated device.
//  * SysEx must be reassembled on F0..F7. The USB descriptors show bulk 64-byte
//    endpoints and no interrupt endpoint, so a 2109-byte preset frame arrives
//    chopped into dozens of USB-MIDI packets with no relation to the message
//    boundary. Nothing may assume packet alignment.
//  * Real-time bytes (>= 0xF8) may be interleaved inside a SysEx stream and must
//    be dropped rather than appended.
//  * Outgoing SysEx is chunked into <=256-byte MIDI packets. CoreMIDI preserves
//    order within one MIDISend packet list, so chunking is safe.
//
// No JUCE dependency: the headless CLI links this directly, and the plugin's
// AudioProcessor owns one instance.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nymphes
{

struct MidiDeviceInfo
{
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string serial;      // not always available
};

struct ChannelMessage
{
    uint8_t status = 0;      // full status byte, channel in the low nibble
    uint8_t data1  = 0;
    uint8_t data2  = 0;
    int channel() const noexcept { return status & 0x0F; }   // 0-based
    int type() const noexcept    { return status & 0xF0; }
};

class MidiPort
{
public:
    using SysExCallback      = std::function<void (const std::vector<uint8_t>& frame)>;
    using ChannelCallback    = std::function<void (const ChannelMessage&)>;
    using ConnectionCallback = std::function<void (bool connected, const std::string& name)>;

    MidiPort();
    ~MidiPort();

    MidiPort (const MidiPort&)            = delete;
    MidiPort& operator= (const MidiPort&) = delete;

    /** Endpoints we can send to, i.e. what the OS calls destinations.
        A connected Nymphes normally appears as a destination named "Nymphes". */
    static std::vector<MidiDeviceInfo> listDestinations();
    /** Endpoints we can receive from (the same device's output side). */
    static std::vector<MidiDeviceInfo> listSources();

    /** True when the endpoint looks like the synth: its name or model advertises
        "Nymphes", case-insensitively. Never a VID:PID test. Serial numbers are not
        consulted (they are often absent, and the field is informational).
        This is a heuristic: it says "probably a Nymphes", not "the right one". */
    static bool looksLikeNymphes (const MidiDeviceInfo&);

    /** Open the first endpoint whose name contains `nameSubstring`
        (case-insensitive; empty means "any Nymphes-looking endpoint").
        The first match wins, so with several identically named units the choice is
        whichever the OS lists first - check openedName() if that matters.

        Success means "we can send": true only when a destination was opened.
        Without a matching destination the call returns false and fills `error`, even
        if an input source named "Nymphes" exists, because every operation this class
        exists for (preset send, CC, dump request) is outgoing. A destination without
        a usable input is still a success - sending works - and no error is reported
        for that case; input problems are not errors of an outgoing port.

        `error` is written only when the call returns false. */
    bool open (const std::string& nameSubstring = "Nymphes", std::string* error = nullptr);

    /** Like open(), but asks the backend to keep watching for the device to come
        back after it disappears.

        Support is per backend and this is NOT a promise that it reconnects: the
        CoreMIDI backend installs no notification proc, and WinMM has none, so today
        this behaves exactly like open() on every platform and the connection callback
        fires on open/close only. The return value reports whether the initial open
        succeeded, never whether watching is armed. A caller that needs reconnection
        must poll listDestinations()/isOpen() and call open() again. */
    bool openWatching (const std::string& nameSubstring = "Nymphes", std::string* error = nullptr);

    void close();
    bool isOpen() const;
    std::string openedName() const;

    /** Send one complete SysEx frame: it must be bounded by F0..F7 (the shortest
        legal frame, F0 F7, is accepted). A malformed frame is rejected and `error` is
        filled; nothing is sent.

        Chunks internally. Sends are serialised as one unit, so a channel message from
        another thread can never land inside the frame: on WinMM both short and long
        messages take the same send lock, and on CoreMIDI one frame is one ordered
        MIDISend packet list. Safe to call from any thread, but a send can therefore
        block behind an in-flight SysEx transfer (WinMM waits up to its send timeout) -
        do not call it from a real-time/audio callback. */
    bool sendSysEx (const std::vector<uint8_t>& frame, std::string* error = nullptr);

    /** Send one channel-voice message. `status` must be a channel-voice status
        (0x80..0xEF, channel in the low nibble) - anything else returns false; the
        data bytes are sent as given, so they must already be 0..127.
        `data2` is ignored for the 2-byte messages 0xC0 (program change) and
        0xD0 (channel pressure). Omitting `data2` on a note-on sends velocity 0,
        i.e. a note-off - pass it explicitly.
        Returns false when no port is open. */
    bool sendChannel (uint8_t status, uint8_t data1, uint8_t data2 = 0);

    /** Convenience: CC on a 0-based channel. Returns false when no port is open,
        `channel` is outside 0..15 or `cc` outside 0..127; `value` is CLAMPED to
        0..127 (the two failure modes differ deliberately: a bad address is a caller
        bug, an out-of-range value is a UI overshoot worth sending). */
    bool sendCC (int channel, int cc, int value);

    void setSysExCallback (SysExCallback);
    void setChannelCallback (ChannelCallback);
    void setConnectionCallback (ConnectionCallback);

    /** Callbacks are invoked on the MIDI I/O thread (CoreMIDI's, or WinMM's).

        Lifetime rules a handler depends on:
          * the `std::vector<uint8_t>&` given to SysExCallback is only valid for the
            duration of the call - copy it to keep it;
          * callbacks are copied out under the port lock and invoked after it is
            released, so a handler may call sendCC()/sendSysEx();
          * passing an empty function unregisters. Do that before destroying an object
            the callback captures: `close()` stops new input from being delivered, but it
            does not wait for a callback that is already running (or one that has already
            been handed its copy of the data), so a handler must tolerate its owner going
            away - the plugin clears its callbacks in ~AudioProcessor before close();
          * a handler must not call open()/close() synchronously. */
    static const char* backendName() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace nymphes
