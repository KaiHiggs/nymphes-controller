// plugin/ui/OptionList.cpp

#include "plugin/ui/OptionList.h"

#include "plugin/cc/CcSpec.h"
#include "plugin/ui/Theme.h"

namespace nymphes::ui
{

using namespace theme;

namespace
{
    constexpr int kRowH = 17;
}

OptionList::OptionList (Processor& p, juce::String t, juce::StringArray o, Layout l)
    : proc (p), title (std::move (t)), options (std::move (o)), layout (l), accent (kPinkAccent)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    ccEngine (proc).addChangeListener (this);
}

OptionList::~OptionList()
{
    ccEngine (proc).removeChangeListener (this);
}

void OptionList::bindToCc (int cc)
{
    const auto* spec = ccui::findSpec (cc);
    const int lo = spec != nullptr ? spec->minValue : 0;
    getIndex = [this, cc, lo] { return ccEngine (proc).getValue (cc) - lo; };
    setIndex = [this, cc, lo] (int i) { ccEngine (proc).setValue (cc, lo + i); };
    setTooltip ((spec != nullptr ? juce::String (spec->name.data(), (int) spec->name.size()) : title)
                + "   (CC " + juce::String (cc) + ")");
}

int OptionList::preferredHeight() const
{
    const int body = layout == Layout::vertical ? options.size() * kRowH : kRowH;
    return (title.isNotEmpty() ? kTitleH : 0) + body;
}

juce::Rectangle<int> OptionList::rowBounds (int index) const
{
    auto r = getLocalBounds();
    if (title.isNotEmpty())
        r.removeFromTop (kTitleH);

    if (layout == Layout::vertical)
        return { r.getX(), r.getY() + index * kRowH, r.getWidth(), kRowH };

    const int n = juce::jmax (1, options.size());
    const int w = r.getWidth() / n;
    return { r.getX() + index * w, r.getY(), index == n - 1 ? r.getWidth() - w * (n - 1) : w, kRowH };
}

int OptionList::rowAt (juce::Point<int> p) const
{
    for (int i = 0; i < options.size(); ++i)
        if (rowBounds (i).contains (p))
            return i;
    return -1;
}

void OptionList::paint (juce::Graphics& g)
{
    if (title.isNotEmpty())
    {
        g.setColour (kCreamLabel);
        g.setFont (fontGroup().withHeight (10.5f));
        drawTrackedText (g, title, getLocalBounds().removeFromTop (kTitleH).toFloat(),
                         juce::Justification::centredLeft, 0.9f);
    }

    const int lit = getIndex ? getIndex() : -1;

    for (int i = 0; i < options.size(); ++i)
    {
        auto row = rowBounds (i).toFloat().reduced (layout == Layout::vertical ? 0.0f : 1.0f, 1.0f);
        const bool on = i == lit;
        const bool enabled = ! isRowEnabled || isRowEnabled (i);

        if (on)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (row, row.getHeight() * 0.5f);
        }
        else if (i == hovered && enabled)
        {
            g.setColour (withAlpha (kLilacLine, 0.22f));
            g.fillRoundedRectangle (row, row.getHeight() * 0.5f);
        }
        else if (layout == Layout::horizontal)
        {
            g.setColour (withAlpha (kLilacLine, 0.55f));
            g.drawRoundedRectangle (row.reduced (0.5f), row.getHeight() * 0.5f, 1.0f);
        }

        auto text = row.reduced (7.0f, 0.0f);
        g.setFont (fontGroup().withHeight (10.5f));

        if (numbered)
        {
            auto num = text.removeFromLeft (12.0f);
            g.setColour (on ? kOnPink : withAlpha (kLilacLine, enabled ? 0.95f : 0.4f));
            g.drawText (juce::String (i + 1), num, juce::Justification::centredLeft, false);
        }

        g.setColour (on ? kOnPink : (enabled ? kCreamDim : withAlpha (kCreamMute, 0.4f)));
        drawTrackedText (g, options[i], text,
                         layout == Layout::vertical ? juce::Justification::centredLeft
                                                    : juce::Justification::centred, 0.6f);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (withAlpha (kFocusIce, 0.85f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), (float) kRadiusS, 1.0f);
    }
}

void OptionList::mouseMove (const juce::MouseEvent& e)
{
    const int h = rowAt (e.getPosition());
    if (h != hovered) { hovered = h; repaint(); }
}

void OptionList::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void OptionList::mouseDown (const juce::MouseEvent& e)
{
    choose (rowAt (e.getPosition()));
}

void OptionList::choose (int index)
{
    if (index < 0 || index >= options.size() || ! setIndex)
        return;
    if (isRowEnabled && ! isRowEnabled (index))
        return;
    setIndex (index);
    repaint();
}

bool OptionList::keyPressed (const juce::KeyPress& key)
{
    const int lit = getIndex ? getIndex() : -1;
    const bool prev = key.isKeyCode (layout == Layout::vertical ? juce::KeyPress::upKey : juce::KeyPress::leftKey);
    const bool next = key.isKeyCode (layout == Layout::vertical ? juce::KeyPress::downKey : juce::KeyPress::rightKey);
    if (prev) { choose (juce::jmax (0, lit - 1)); return true; }
    if (next) { choose (juce::jmin (options.size() - 1, lit + 1)); return true; }
    return false;
}

} // namespace nymphes::ui
