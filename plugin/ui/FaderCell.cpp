// plugin/ui/FaderCell.cpp

#include "plugin/ui/FaderCell.h"

#include "plugin/ui/Theme.h"

namespace nymphes::ui
{

using namespace theme;

namespace
{
    juce::String toString (std::string_view s) { return juce::String (s.data(), s.size()); }

    /** LFO waves are a 0..127 sweep across five named zones, not a 5-step enum. */
    juce::String zoneName (const ccui::CcSpec& spec, int v)
    {
        const int n = (int) spec.labels.size();
        if (n == 0)
            return {};
        const int span = spec.maxValue - spec.minValue + 1;
        const int index = juce::jlimit (0, n - 1, (v - spec.minValue) * n / juce::jmax (1, span));
        const auto label = toString (spec.labels[(std::size_t) index]);
        if (label == "triangle") return "TRI";
        if (label == "saw_down") return "SAW-";
        if (label == "saw_up")   return "SAW+";
        if (label == "square")   return "SQR";
        if (label == "random")   return "RND";
        return label.toUpperCase().substring (0, 4);
    }
}

//==============================================================================
void FaderCell::CellSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onContextMenu) onContextMenu();
        return;
    }
    juce::Slider::mouseDown (e);
}

void FaderCell::CellSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::Slider::mouseDrag (e);
}

void FaderCell::CellSlider::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::Slider::mouseUp (e);
}

//==============================================================================
FaderCell::FaderCell (Processor& p, ModEditState& m, int cc, juce::String label, Style s)
    : proc (p), mod (m), baseCc (cc), depthCc (depthCcFor (cc)),
      spec (ccui::findSpec (cc)), shortLabel (std::move (label)), style (s)
{
    jassert (spec != nullptr);

    slider.setSliderStyle (style == Style::knob ? juce::Slider::RotaryHorizontalVerticalDrag
                                                : juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 127.0, 1.0);
    if (spec != nullptr)
        slider.setRange ((double) spec->minValue, (double) spec->maxValue, 1.0);
    // Grabbing a fader must not make it jump to the pointer: the hardware never does.
    slider.setSliderSnapsToMousePosition (false);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setScrollWheelEnabled (true);
    slider.setPopupMenuEnabled (false);
    slider.onValueChange = [this] { userMoved(); };
    slider.onContextMenu = [this] { showContextMenu(); };
    slider.setTitle (spec != nullptr ? toString (spec->name) : shortLabel);
    addAndMakeVisible (slider);

    ccEngine (proc).addChangeListener (this);
    refresh();
}

FaderCell::~FaderCell()
{
    ccEngine (proc).removeChangeListener (this);
}

//==============================================================================
juce::String FaderCell::valueText() const
{
    auto& engine = ccEngine (proc);

    if (editingDepth())
    {
        const int d = mod.depth (mod.source, depthCc);
        return d < 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) : juce::String (d);
    }

    if (engine.isWaitingForTakeover (baseCc))
        return "PICK";

    const int v = engine.getValue (baseCc);
    if (spec != nullptr && ! spec->labels.empty())
        return zoneName (*spec, v);
    return juce::String (v);
}

void FaderCell::paint (juce::Graphics& g)
{
    auto& engine = ccEngine (proc);
    const bool depth = editingDepth();
    const bool dimmed = mod.active() && depthCc < 0 && style == Style::fader;
    const int learnedCc = engine.getLearnedCc (activeCc());
    const bool learning = engine.isLearning() && engine.getLearnTarget() == activeCc();

    auto r = getLocalBounds();
    auto valueRow = r.removeFromTop (kValueH);
    auto labelRow = r.removeFromBottom (kLabelH);

    // value: mono, quiet; pink while it is a depth so the mode is never ambiguous
    g.setFont (fontValue().withHeight (11.5f));
    g.setColour (depth ? kPinkText : withAlpha (kCreamMute, dimmed ? 0.35f : 0.85f));
    g.drawText (valueText(), valueRow, juce::Justification::centred, false);

    // label: the hardware's own abbreviation, bold, tracked
    g.setFont (fontGroup().withHeight (11.0f));
    g.setColour (dimmed ? withAlpha (kCreamLabel, 0.4f) : kCreamLabel);
    drawTrackedText (g, shortLabel, labelRow.toFloat().withTrimmedTop (3.0f),
                     juce::Justification::centredTop, 0.6f);

    // learn state: a dot under the label, only when there is something to say
    if (learning || learnedCc >= 0)
    {
        g.setColour (learning ? kAmber : kGreen);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f)
                           .withCentre ({ (float) labelRow.getCentreX(), (float) labelRow.getBottom() - 1.0f }));
    }
}

void FaderCell::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kValueH);
    r.removeFromBottom (kLabelH);
    if (style == Style::knob)
    {
        const int side = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (side, side));
    }
    else
    {
        slider.setBounds (r);
    }
}

//==============================================================================
void FaderCell::userMoved()
{
    if (updating)
        return;

    auto& engine = ccEngine (proc);
    const int v = juce::roundToInt (slider.getValue());

    if (editingDepth())
    {
        // The depth CC means whatever CC 30 says. Say it first, every time it is
        // not already true, so a depth never lands on the wrong source.
        if (engine.getValue (ccui::kModSourceSelectorCc) != mod.source)
            engine.setValue (ccui::kModSourceSelectorCc, mod.source);
        engine.setValue (depthCc, v);
        mod.remember (mod.source, depthCc, v);
        lastSeenDepth = v;
    }
    else
    {
        engine.setValue (baseCc, v);
    }
    repaint();
}

void FaderCell::refresh()
{
    if (updating)
        return;

    auto& engine = ccEngine (proc);
    updating = true;

    // A depth that arrived from the synth while this source was selected is a
    // depth we now know.
    if (depthCc >= 0)
    {
        const int seen = engine.getValue (depthCc);
        if (seen != lastSeenDepth)
        {
            const int src = engine.getValue (ccui::kModSourceSelectorCc);
            if (lastSeenDepth >= 0 && src >= 0 && src < kModSourceCount)
                mod.remember (src, depthCc, seen);
            lastSeenDepth = seen;
        }
    }

    const bool depth = editingDepth();
    int shown = engine.getValue (baseCc);
    bool unknown = false;
    if (depth)
    {
        shown = mod.depth (mod.source, depthCc);
        unknown = shown < 0;
        if (unknown)
            shown = 0;
    }

    slider.getProperties().set ("modEdit", depth);
    slider.getProperties().set ("unknown", unknown);
    slider.setEnabled (! (mod.active() && depthCc < 0 && style == Style::fader));
    slider.setValue ((double) shown, juce::dontSendNotification);
    updating = false;

    updateTooltip();
    slider.repaint();
    repaint();
}

void FaderCell::updateTooltip()
{
    const auto name = spec != nullptr ? toString (spec->name) : shortLabel;
    juce::String tip;
    if (editingDepth())
        tip << kModSourceLong[mod.source] << " depth  >  " << name
            << "   (CC " << depthCc << " with CC 30 = " << mod.source << ")";
    else
        tip << name << "   (CC " << baseCc << ")";
    tip << "\nRight-click to MIDI-learn. Double-click for 0.";
    setTooltip (tip);
    slider.setTooltip (tip);
}

void FaderCell::showContextMenu()
{
    auto& engine = ccEngine (proc);
    const int cc = activeCc();
    const int learned = engine.getLearnedCc (cc);
    const bool learning = engine.isLearning() && engine.getLearnTarget() == cc;

    juce::PopupMenu menu;
    menu.addSectionHeader (editingDepth() ? juce::String (kModSourceLong[mod.source]) + " depth: " + shortLabel
                                          : shortLabel);
    menu.addItem (1, learning ? "Cancel MIDI learn" : "MIDI learn");
    menu.addItem (2, learned >= 0 ? "Forget learned CC " + juce::String (learned) : "Forget learned CC",
                  learned >= 0);

    juce::Component::SafePointer<FaderCell> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                        [safe, cc] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            auto& eng = ccEngine (safe->proc);
                            if (result == 1)
                            {
                                if (eng.isLearning() && eng.getLearnTarget() == cc)
                                    eng.cancelLearn();
                                else
                                    eng.beginLearn (cc);
                            }
                            else if (result == 2)
                            {
                                eng.clearLearn (cc);
                            }
                            safe->repaint();
                        });
}

void FaderCell::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

} // namespace nymphes::ui
