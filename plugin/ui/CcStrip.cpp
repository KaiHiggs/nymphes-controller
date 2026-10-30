// plugin/ui/CcStrip.cpp  —  L1

#include "plugin/ui/CcStrip.h"

namespace nymphes::ui
{

// The `updating` flag guards the feedback loop slider -> engine -> changeListener ->
// slider. Every path that writes the engine or the control must set it, or the strip
// will fight the user's drag.

CcStrip::CcStrip (Processor& processor, const ccui::CcSpec& s)
    : proc (processor), spec (s)
{
    nameLabel.setText (juce::String (spec.name.data(), (int) spec.name.size()),
                       juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcfd3dc));
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    valueLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a5));
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

    if (isDiscrete())
    {
        for (std::size_t i = 0; i < spec.labels.size(); ++i)
            combo.addItem (juce::String (spec.labels[i].data(),
                                         (int) spec.labels[i].size()),
                           (int) i + 1);   // ids are 1-based
        combo.onChange = [this] { comboChanged(); };
        combo.setTooltip ("CC " + juce::String ((int) spec.cc));
        addAndMakeVisible (combo);
    }
    else
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange ((double) spec.minValue, (double) spec.maxValue, 1.0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setDoubleClickReturnValue (true, (double) spec.minValue);
        slider.onValueChange = [this] { sliderChanged(); };
        slider.setTooltip ("CC " + juce::String ((int) spec.cc)
                           + "  (" + juce::String ((int) spec.minValue) + "-"
                           + juce::String ((int) spec.maxValue) + ")");
        addAndMakeVisible (slider);
    }

    learnButton.setTooltip ("MIDI learn: click, then move a control on your keyboard");
    learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2f38));
    learnButton.onClick = [this]
    {
        auto& engine = ccEngine (proc);
        if (engine.isLearning() && engine.getLearnTarget() == (int) spec.cc)
        {
            engine.cancelLearn();
            if (onLearnCancelled)
                onLearnCancelled ((int) spec.cc);
        }
        else
        {
            engine.beginLearn ((int) spec.cc);
            if (onLearnRequested)
                onLearnRequested ((int) spec.cc);
        }
        updateLearnVisual();
    };
    addAndMakeVisible (learnButton);

    ccEngine (proc).addChangeListener (this);
    startTimerHz (4);
    refresh();
    updateLearnVisual();
}

CcStrip::~CcStrip()
{
    ccEngine (proc).removeChangeListener (this);
    stopTimer();
}

void CcStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool learned = ccEngine (proc).getLearnedCc ((int) spec.cc) >= 0;
    const bool learning = ccEngine (proc).isLearning()
                          && ccEngine (proc).getLearnTarget() == (int) spec.cc;

    g.setColour (juce::Colour (0xff1b1e25));
    g.fillRoundedRectangle (r, 4.0f);

    if (learning)
        g.setColour (juce::Colour (0xffe0a23c).withAlpha (learnBlinkOn ? 1.0f : 0.35f));
    else if (learned)
        g.setColour (juce::Colour (0xff4a9d7a));
    else
        g.setColour (juce::Colour (0xff2b2f38));

    g.drawRoundedRectangle (r, 4.0f, (learning || learned) ? 1.6f : 1.0f);

    // A modulation-depth parameter is shared by all four mod sources; mark it so
    // the user can see the value depends on CC 30.
    if (spec.isModDepth)
    {
        g.setColour (juce::Colour (0xff5b6b8c));
        g.fillEllipse (r.getRight() - 9.0f, r.getY() + 4.0f, 4.0f, 4.0f);
    }
}

void CcStrip::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    auto top = r.removeFromTop (13);
    learnButton.setBounds (top.removeFromRight (16).withSizeKeepingCentre (15, 12));
    nameLabel.setBounds (top);

    auto bottom = r.removeFromBottom (11);
    valueLabel.setBounds (bottom);

    if (isDiscrete())
        combo.setBounds (r.reduced (1));
    else
        slider.setBounds (r);
}

void CcStrip::sliderChanged()
{
    if (updating)
        return;
    updating = true;
    ccEngine (proc).setValue ((int) spec.cc, (int) juce::roundToInt (slider.getValue()));
    updating = false;
    refresh();
}

void CcStrip::comboChanged()
{
    if (updating)
        return;
    updating = true;
    const int index = combo.getSelectedId() - 1;
    if (index >= 0)
        ccEngine (proc).setValue ((int) spec.cc, (int) spec.minValue + index);
    updating = false;
    refresh();
}

void CcStrip::refresh()
{
    if (updating)
        return;

    updating = true;
    auto& engine = ccEngine (proc);
    const int v = engine.getValue ((int) spec.cc);

    if (isDiscrete())
    {
        const int index = juce::jlimit (0, (int) spec.labels.size() - 1,
                                        v - (int) spec.minValue);
        combo.setSelectedId (index + 1, juce::dontSendNotification);
        valueLabel.setText (juce::String (spec.labels[(std::size_t) index].data(),
                                          (int) spec.labels[(std::size_t) index].size()),
                            juce::dontSendNotification);
    }
    else
    {
        slider.setValue ((double) v, juce::dontSendNotification);
        valueLabel.setText (juce::String (v), juce::dontSendNotification);
    }

    // Soft takeover: the control is not yet driving the parameter, say so
    // instead of showing a value the user is not actually setting.
    if (engine.isWaitingForTakeover ((int) spec.cc))
        valueLabel.setText ("waiting", juce::dontSendNotification);

    updating = false;
}

void CcStrip::updateLearnVisual()
{
    auto& engine = ccEngine (proc);
    const int learned = engine.getLearnedCc ((int) spec.cc);
    const bool learning = engine.isLearning() && engine.getLearnTarget() == (int) spec.cc;

    learnButton.setColour (juce::TextButton::buttonColourId,
                           learning ? juce::Colour (0xffe0a23c)
                                    : (learned >= 0 ? juce::Colour (0xff4a9d7a)
                                                    : juce::Colour (0xff2b2f38)));
    learnButton.setTooltip (learned >= 0
        ? "MIDI learn: driven by CC " + juce::String (learned) + " - click to re-learn"
        : juce::String ("MIDI learn: click, then move a control on your keyboard"));
    repaint();
}

void CcStrip::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
    updateLearnVisual();
}

void CcStrip::timerCallback()
{
    auto& engine = ccEngine (proc);
    const bool learning = engine.isLearning() && engine.getLearnTarget() == (int) spec.cc;
    if (! learning)
    {
        if (learnBlinkOn)
        {
            learnBlinkOn = false;
            repaint();
        }
        return;
    }
    learnBlinkOn = ! learnBlinkOn;
    repaint();
}

} // namespace nymphes::ui
