#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/cc/CCEngine.h"
#include "plugin/ui/Theme.h"

namespace nymphes::ui
{

// A routing control, not a synth parameter: changing it sends nothing. Omni is
// the legacy state (receive on all channels, transmit on 1), not an output channel.
class MidiChannelSelector final : public juce::Component,
                                  private juce::ChangeListener
{
public:
    explicit MidiChannelSelector (CCEngine& e) : engine (e)
    {
        box.setComponentID ("midiChannel");
        box.setTitle ("MIDI channel");
        box.setTooltip ("Match the Nymphes receive channel. Preset SysEx ignores this setting. "
                        "Omni receives all channels and sends CCs on channel 1.");
        for (int channel = 1; channel <= 16; ++channel)
            box.addItem (juce::String (channel), channel);
        box.addItem ("Omni (out 1)", 17);
        box.onChange = [this]
        {
            const int selected = box.getSelectedId();
            if (selected > 0)
                engine.setMidiChannel (selected == 17 ? 0 : selected);
            repaint();
        };
        addAndMakeVisible (box);
        engine.addChangeListener (this);
        refresh();
    }

    ~MidiChannelSelector() override { engine.removeChangeListener (this); }

    void resized() override { box.setBounds (getLocalBounds().withTrimmedLeft (65)); }

    void paint (juce::Graphics& g) override
    {
        // Non-default routing is visible even when the dropdown is closed.
        g.setColour (engine.getMidiChannel() > 1 ? theme::kPinkText : theme::kCreamLabel);
        g.setFont (theme::fontLabel());
        g.drawText ("MIDI CH", getLocalBounds().withWidth (61),
                    juce::Justification::centredLeft, false);
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh()
    {
        const int channel = engine.getMidiChannel();
        box.setSelectedId (channel == 0 ? 17 : channel, juce::dontSendNotification);
        repaint();
    }

    CCEngine& engine;
    juce::ComboBox box;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiChannelSelector)
};

} // namespace nymphes::ui
