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

    /** True when the endpoint looks like the synth: its name advertises
        "Nymphes", and/or the known unit serial. Never a VID:PID test. */
    static bool looksLikeNymphes (const MidiDeviceInfo&);

    /** Open the first endpoint whose name contains `nameSubstring`
        (case-insensitive; empty means "any Nymphes-looking endpoint").
        Returns false and fills `error` if nothing matches. */
    bool open (const std::string& nameSubstring = "Nymphes", std::string* error = nullptr);

    /** Like open(), but also survives the device disappearing and coming back:
        the connection callback fires on every state change and a reconnect is
        attempted automatically. */
    bool openWatching (const std::string& nameSubstring = "Nymphes", std::string* error = nullptr);

    void close();
    bool isOpen() const;
    std::string openedName() const;

    /** Send one complete SysEx frame (must start with F0 and end with F7).
        Chunks internally; safe to call from any thread. */
    bool sendSysEx (const std::vector<uint8_t>& frame, std::string* error = nullptr);

    /** Send a channel message. Returns false when no port is open. */
    bool sendChannel (uint8_t status, uint8_t data1, uint8_t data2 = 0);

    /** Convenience: CC on a 0-based channel. */
    bool sendCC (int channel, int cc, int value);

    void setSysExCallback (SysExCallback);
    void setChannelCallback (ChannelCallback);
    void setConnectionCallback (ConnectionCallback);

    /** Callbacks are invoked on the CoreMIDI I/O thread. Nothing here blocks,
        but a handler must not call back into open()/close() synchronously. */
    static const char* backendName() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace nymphes
