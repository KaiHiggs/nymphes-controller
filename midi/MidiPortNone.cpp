// Fallback transport for platforms with no backend: the deliverable targets
// macOS (CoreMIDI) and Windows (WinMM), but the CLI and the tests must still
// build anywhere a plain C++
// toolchain exists, so an unavailable backend reports itself honestly instead of
// failing to link.

#include "MidiPort.h"

#if ! defined (__APPLE__) && ! defined (_WIN32)

namespace nymphes
{

struct MidiPort::Impl
{
    bool openFlag = false;
    std::string name;
    SysExCallback      sysexCb;
    ChannelCallback    channelCb;
    ConnectionCallback connectionCb;
};

MidiPort::MidiPort() : impl (new Impl()) {}
MidiPort::~MidiPort() = default;

const char* MidiPort::backendName() noexcept { return "none (no MIDI backend for this platform)"; }

std::vector<MidiDeviceInfo> MidiPort::listDestinations() { return {}; }
std::vector<MidiDeviceInfo> MidiPort::listSources()      { return {}; }

bool MidiPort::looksLikeNymphes (const MidiDeviceInfo& info)
{
    return info.name.find ("Nymphes") != std::string::npos
        || info.model.find ("Nymphes") != std::string::npos;
}

bool MidiPort::open (const std::string&, std::string* error)
{
    if (error != nullptr) *error = "no MIDI backend on this platform";
    return false;
}

bool MidiPort::openWatching (const std::string&, std::string* error)
{
    if (error != nullptr) *error = "no MIDI backend on this platform";
    return false;
}

void MidiPort::close() { impl->openFlag = false; }
bool MidiPort::isOpen() const { return impl->openFlag; }
std::string MidiPort::openedName() const { return impl->name; }

bool MidiPort::sendSysEx (const std::vector<uint8_t>&, std::string* error)
{
    if (error != nullptr) *error = "no MIDI backend on this platform";
    return false;
}

bool MidiPort::sendChannel (uint8_t, uint8_t, uint8_t) { return false; }
bool MidiPort::sendCC (int, int, int) { return false; }

void MidiPort::setSysExCallback (SysExCallback cb)      { impl->sysexCb = std::move (cb); }
void MidiPort::setChannelCallback (ChannelCallback cb)  { impl->channelCb = std::move (cb); }
void MidiPort::setConnectionCallback (ConnectionCallback cb) { impl->connectionCb = std::move (cb); }

} // namespace nymphes

#endif // ! __APPLE__
