// plugin/ui/CcStrip.h  —  L1
//
// One parameter: a rotary control, its name, its live value and its MIDI-learn
// state. Continuous parameters get a 0..127 rotary; discrete ones (play mode,
// LFO type, key sync, sustain, legato, mod source) get a combo box showing the
// real names, because a knob labelled 0..5 tells the user nothing.
//
// The label list comes from nymphes::CCParam::valueNames ("a|b|c"), split by
// plugin/cc/CcSpec.cpp. Nothing here holds CC data of its own.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/cc/CcSpec.h"
#include "plugin/ui/ProcessorAccess.h"

namespace nymphes::ui
{

class CcStrip final : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    CcStrip (Processor& processor, const ccui::CcSpec& spec);
    ~CcStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pull the engine's current value into the controls. */
    void refresh();

    const ccui::CcSpec& getSpec() const noexcept { return spec; }

    /** Right-clicking a control arms MIDI learn for it. */
    std::function<void (int nymphesCc)> onLearnRequested;
    std::function<void (int nymphesCc)> onLearnCancelled;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void sliderChanged();
    void comboChanged();
    void updateLearnVisual();
    bool isDiscrete() const noexcept { return ! spec.labels.empty(); }

    Processor& proc;
    const ccui::CcSpec& spec;

    juce::Label      nameLabel, valueLabel;
    juce::Slider     slider;      // used when ! isDiscrete()
    juce::ComboBox   combo;       // used when isDiscrete()
    juce::TextButton learnButton { "L" };

    bool updating = false;        // guards the feedback loop slider -> engine -> slider
    bool learnBlinkOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CcStrip)
};

} // namespace nymphes::ui
