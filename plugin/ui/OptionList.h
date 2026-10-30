// plugin/ui/OptionList.h  —  the MENU's numbered lists, and every small switch
//
// The hardware shows its enumerations as printed lists (POLY, UNI A, UNI B ...)
// beside a selector. On screen the list is the selector: click a row. The lit row
// is a pink bar with navy text — the same pink the panel uses for "selected".
//
// The list does not know whether it is bound to a CC or to the mod-edit state:
// the owner gives it getIndex/setIndex.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "plugin/ui/ProcessorAccess.h"

namespace nymphes::ui
{

class OptionList final : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::ChangeListener
{
public:
    enum class Layout { vertical, horizontal };

    OptionList (Processor&, juce::String title, juce::StringArray options, Layout = Layout::vertical);
    ~OptionList() override;

    /** Bind to a discrete CC: index = value - min. */
    void bindToCc (int nymphesCc);

    std::function<int()>     getIndex;          // -1 = none lit
    std::function<void (int)> setIndex;
    std::function<bool (int)> isRowEnabled;     // optional

    void setNumbered (bool shouldNumber) { numbered = shouldNumber; repaint(); }
    void setAccent (juce::Colour c)      { accent = c; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    static constexpr int kTitleH = 16;
    int preferredHeight() const;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    juce::Rectangle<int> rowBounds (int index) const;
    int rowAt (juce::Point<int>) const;
    void choose (int index);

    Processor& proc;
    const juce::String title;
    const juce::StringArray options;
    const Layout layout;
    bool numbered = false;
    int hovered = -1;
    juce::Colour accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionList)
};

} // namespace nymphes::ui
