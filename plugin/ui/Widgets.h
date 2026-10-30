// plugin/ui/Widgets.h  —  L2
//
// Three containers that give the editor its structure. None of them owns
// parameter state, MIDI state or preset state: they are chrome. A panel can use
// them or draw its own card with theme::drawCard — nothing here is mandatory,
// which is why L1 and L3 can keep their own internals.
//
//   PanelCard     a titled surface with a content area, so every group in the
//                 app has the same padding, header height and accent rule
//   SegmentedNav  the view switcher that replaces juce::TabbedComponent
//   StatusRail    the bottom bar: device state, port, connect, dump. It reports
//                 status; it is not a form.
//
// All three are keyboard reachable and draw a focus ring, because a hardware
// controller that cannot be driven from the keyboard is a mouse-only toy.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "plugin/ui/ProcessorAccess.h"
#include "plugin/ui/Theme.h"

namespace nymphes::ui
{

//==============================================================================
/** A card: rounded surface, lit top edge, a 34px header strip with a tracked
    uppercase title and an optional count badge. Children are positioned by the
    owner inside contentArea(). */
class PanelCard final : public juce::Component
{
public:
    explicit PanelCard (juce::String cardTitle = {});

    void setTitle (juce::String newTitle);
    void setCount (int newCount);                 // -1 hides the badge
    void setEmphasised (bool shouldEmphasise);    // hover / active state

    /** The rect inside the plate outline and below the title band. Never negative. */
    juce::Rectangle<int> contentArea() const;

    /** The plate body colour; the browser list and the controls grid use the same. */
    void setBodyColour (juce::Colour c) { body = c; repaint(); }

    void paint (juce::Graphics&) override;

private:
    juce::String title;
    juce::Colour body = theme::kPlateBlue;
    int count = -1;
    bool emphasised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelCard)
};

//==============================================================================
/** The view switcher. Two or more named segments, one active, driven by mouse or
    by the left/right arrow keys when focused. Reports the index through
    onChange; owns no view. */
class SegmentedNav final : public juce::Component
{
public:
    SegmentedNav();

    /** Segments are laid out left to right at their natural width. */
    void addSegment (juce::String label, juce::String tooltip = {});
    void setActiveIndex (int index, juce::NotificationType = juce::sendNotification);
    int  getActiveIndex() const noexcept { return active; }

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Segment { juce::String label, tooltip; juce::Rectangle<int> bounds; };
    std::vector<Segment> segments;
    int active = 0;
    int hovered = -1;

    void layoutSegments();
    int  segmentAt (juce::Point<int>) const;
    void select (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedNav)
};

//==============================================================================
/** The bottom bar. Reads left to right as status: a state dot, a sentence, then
    the controls that change the device. Deliberately NOT a label + combo + two
    buttons in a row, which is what made the previous UI read as a form. */
class StatusRail final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit StatusRail (Processor&);
    ~StatusRail() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Re-reads the port list. Called on construction and on Connect. */
    void refreshPorts();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void connectRequested();

    Processor& proc;

    juce::ComboBox   portBox;
    juce::TextButton connectButton { "Connect" };
    juce::TextButton dumpButton    { "Dump" };

    juce::String statusText;
    bool deviceOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusRail)
};

} // namespace nymphes::ui
