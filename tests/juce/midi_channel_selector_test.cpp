#include "plugin/ui/MidiChannelSelector.h"

#include <cstdio>
#include <vector>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    nymphes::CCEngine engine;
    std::vector<juce::MidiMessage> sent;
    engine.setMidiSink ([&] (const juce::MidiMessage& m) { sent.push_back (m); });
    nymphes::ui::MidiChannelSelector selector (engine);
    selector.setBounds (0, 0, 196, 28);
    auto* box = dynamic_cast<juce::ComboBox*> (selector.findChildWithID ("midiChannel"));
    int failures = 0;
    auto check = [&] (bool ok, const char* message)
    {
        if (! ok) { ++failures; std::printf ("FAIL %s\n", message); }
    };
    check (box != nullptr, "the channel selector is accessible by ID");
    if (box == nullptr) return 1;
    check (box->getNumItems() == 17, "16 channels and explicit Omni/output-1 mode");
    check (box->getSelectedId() == 17, "legacy omni state is shown, not hidden");

    box->setSelectedId (5, juce::sendNotificationSync);
    check (engine.getMidiChannel() == 5, "changing the UI changes the engine channel");
    check (sent.empty(), "changing channel does not send CCs to the synth");
    engine.setValue (74, 80);
    check (! sent.empty() && sent.back().getChannel() == 5
           && sent.back().getControllerNumber() == 74 && sent.back().getControllerValue() == 80,
           "slider value reaches the sink as CC 74/80 on channel 5");

    auto saved = engine.getState();
    saved->setAttribute ("midiChannel", 12);
    engine.setState (*saved);
    engine.dispatchPendingMessages();
    check (box->getSelectedId() == 12, "host state restore updates an open selector");
    engine.setValue (74, 81);
    check (sent.back().getChannel() == 12, "restored channel is used for subsequent CCs");

    auto roundTrip = engine.getState();
    nymphes::CCEngine restored;
    restored.setState (*roundTrip);
    nymphes::ui::MidiChannelSelector reopened (restored);
    auto* reopenedBox = dynamic_cast<juce::ComboBox*> (reopened.findChildWithID ("midiChannel"));
    check (reopenedBox != nullptr && reopenedBox->getSelectedId() == 12,
           "saved channel persists across closing and reopening the editor");

    const auto count = sent.size();
    box->setSelectedId (17, juce::sendNotificationSync);
    check (sent.size() == count, "selecting Omni alone emits nothing");
    engine.setValue (74, 82);
    check (engine.getMidiChannel() == 0 && sent.back().getChannel() == 1,
           "Omni retains existing output-channel-1 semantics");
    engine.dispatchPendingMessages(); // drain the preceding value-change notice
    engine.setMidiChannel (7);
    engine.dispatchPendingMessages();
    check (box->getSelectedId() == 7, "external engine channel changes update the selector");

    if (failures == 0) std::puts ("midi_channel_selector_test: all checks passed");
    return failures == 0 ? 0 : 1;
}
