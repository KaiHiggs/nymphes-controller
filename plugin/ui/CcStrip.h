// plugin/ui/CcStrip.h  —  L1
//
// One parameter: a rotary control, its name, its live value and its MIDI-learn
// state. Continuous parameters get a rotary spanning the table's own range; a
// parameter whose value names cover its whole range (play mode, LFO type, key
// sync, sustain, legato, mod source) gets a combo box instead, because a knob
// labelled 0..5 tells the user nothing.
//
// The label list comes from nymphes::CCParam::valueNames ("a|b|c"), split by
// plugin/cc/CcSpec.cpp. Nothing here holds CC data of its own.
//
// Lifetime: the Processor must outlive the strip (the editor owns both). The CcSpec
// is held BY VALUE — it is a cheap view over the static table (its string_views
// point at literals) — so a strip built from a temporary or from a vector element
// that later reallocates cannot dangle.

#pragma once

#include <functional>

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

    /** Pull the engine's current value into the controls. A call made while the
        strip is writing the engine is applied afterwards instead of being
        dropped. */
    void refresh();

    const ccui::CcSpec& getSpec() const noexcept { return spec; }

    /** The "L" button arms MIDI learn for this parameter (there is no right-click
        shortcut). onLearnCancelled fires only for THIS strip's parameter; when
        another strip takes over the learn target, the engine's change broadcast
        is what updates the visuals. */
    std::function<void (int nymphesCc)> onLearnRequested;
    std::function<void (int nymphesCc)> onLearnCancelled;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void sliderChanged();
    void comboChanged();
    /** refresh()'s body; call with the re-entrancy guard free. */
    void applyEngineToControls();
    void updateLearnVisual();
    /** True when the label list covers the range, i.e. a combo can hold the value. */
    bool isDiscrete() const noexcept { return spec.labelsSpanRange(); }

    Processor& proc;
    const ccui::CcSpec spec;

    juce::Label      nameLabel, valueLabel;
    juce::Slider     slider;      // used when ! isDiscrete()
    juce::ComboBox   combo;       // used when isDiscrete()
    juce::TextButton learnButton { "L" };

    bool updating = false;        // guards the feedback loop slider -> engine -> slider
    bool refreshPending = false;  // a refresh() arrived while updating
    bool learnBlinkOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CcStrip)
};

} // namespace nymphes::ui
