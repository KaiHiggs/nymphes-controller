// plugin/ui/Widgets.cpp  —  L2

#include "plugin/ui/Widgets.h"

#include <juce_audio_devices/juce_audio_devices.h>

namespace nymphes::ui
{

using namespace theme;

//==============================================================================
// PanelCard
//==============================================================================

PanelCard::PanelCard (juce::String cardTitle)
    : title (std::move (cardTitle))
{
    setInterceptsMouseClicks (false, true);
}

void PanelCard::setTitle (juce::String newTitle)
{
    title = std::move (newTitle);
    repaint();
}

void PanelCard::setCount (int newCount)
{
    count = newCount;
    repaint();
}

void PanelCard::setEmphasised (bool shouldEmphasise)
{
    if (emphasised == shouldEmphasise)
        return;
    emphasised = shouldEmphasise;
    repaint();
}

juce::Rectangle<int> PanelCard::contentArea() const
{
    auto r = getLocalBounds().reduced (kPlateInset + kGapM);
    r.removeFromTop (kCardHeaderH - kGapS);
    return r;
}

void PanelCard::paint (juce::Graphics& g)
{
    // A plate, not a card. The hardware draws sections as a lilac OUTLINE with a
    // pill title band interrupting the top edge; a filled card would be a generic
    // plugin panel, which is exactly what the operator rejected.
    drawPlate (g, getLocalBounds().toFloat(), title, emphasised, body);

    if (count >= 0)
    {
        auto r = getLocalBounds().toFloat().reduced ((float) kPlateInset, (float) kPlateInset);
        auto badge = juce::Rectangle<float> (30.0f, 15.0f)
                         .withRight (r.getRight() - 10.0f)
                         .withCentre ({ r.getRight() - 25.0f, r.getY() });
        drawPill (g, badge, withAlpha (kWellBlue, 0.9f), kLilacLine);
        g.setColour (kCreamMute);
        g.setFont (fontMicro());
        g.drawText (juce::String (count), badge, juce::Justification::centred, false);
    }
}

//==============================================================================
// SegmentedNav
//==============================================================================

SegmentedNav::SegmentedNav()
{
    setWantsKeyboardFocus (true);
    setRepaintsOnMouseActivity (true);
}

void SegmentedNav::addSegment (juce::String label, juce::String tooltip)
{
    segments.push_back ({ std::move (label), std::move (tooltip), {} });
    layoutSegments();
    repaint();
}

void SegmentedNav::layoutSegments()
{
    int x = 0;
    const int h = getHeight() > 0 ? getHeight() : kNavH;
    for (auto& s : segments)
    {
        const int w = juce::jmax (96, (int) std::ceil (trackedWidth (fontText(), s.label.toUpperCase(), 0.9f)) + 44);
        s.bounds = { x, 0, w, h };
        x += w;
    }
}

void SegmentedNav::resized() { layoutSegments(); }

void SegmentedNav::setActiveIndex (int index, juce::NotificationType notify)
{
    index = juce::jlimit (0, juce::jmax (0, (int) segments.size() - 1), index);
    if (index == active)
        return;
    active = index;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange (active);
}

void SegmentedNav::select (int index)
{
    if (index < 0 || index >= (int) segments.size())
        return;
    setActiveIndex (index);
}

int SegmentedNav::segmentAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) segments.size(); ++i)
        if (segments[(std::size_t) i].bounds.contains (p))
            return i;
    return -1;
}

void SegmentedNav::mouseMove (const juce::MouseEvent& e)
{
    const int i = segmentAt (e.getPosition());
    if (i != hovered)
    {
        hovered = i;
        setMouseCursor (i >= 0 ? juce::MouseCursor::PointingHandCursor
                               : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SegmentedNav::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

void SegmentedNav::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    select (segmentAt (e.getPosition()));
}

bool SegmentedNav::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
    {
        const int step = key == juce::KeyPress::leftKey ? -1 : 1;
        select (active + step);
        return true;
    }
    if (key == juce::KeyPress::homeKey) { select (0); return true; }
    if (key == juce::KeyPress::endKey)  { select ((int) segments.size() - 1); return true; }
    return false;
}

void SegmentedNav::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // the bar's own ground: the surround tone, so the nav reads as the window
    // frame and the blue field below reads as the panel
    g.setColour (kPanelSurround);
    g.fillRect (r);
    g.setColour (kLilacLine);
    g.fillRect (r.removeFromBottom (1.0f));

    for (int i = 0; i < (int) segments.size(); ++i)
    {
        const auto& seg = segments[(std::size_t) i];
        const auto b = seg.bounds.toFloat();
        const bool isActive = i == active;
        const bool isHovered = i == hovered;

        if (isActive)
        {
            g.setColour (kPinkAccent);
            g.fillRoundedRectangle (b.getX() + 12.0f, b.getBottom() - 3.0f,
                                    b.getWidth() - 24.0f, 2.0f, 1.0f);
        }
        else if (isHovered)
        {
            g.setColour (withAlpha (kRaisedBlue, 0.55f));
            g.fillRoundedRectangle (b.reduced (6.0f, 6.0f), (float) kRadiusS);
        }

        g.setColour (isActive ? kPinkText : (isHovered ? kCreamLabel : kCreamDim));
        g.setFont (fontText());
        drawTrackedText (g, seg.label.toUpperCase(), b,
                         juce::Justification::centred, 0.9f);
    }

    // focus ring around the active segment, inset so it never clips
    if (hasKeyboardFocus (false) && ! segments.empty())
    {
        const auto b = segments[(std::size_t) juce::jlimit (0, (int) segments.size() - 1, active)]
                           .bounds.toFloat();
        drawFocusRing (g, b.reduced (7.0f, 7.0f), kRadiusS, true);
    }
}

//==============================================================================
// StatusRail
//==============================================================================

StatusRail::StatusRail (Processor& processor)
    : proc (processor)
{
    const auto ghost = [this] (juce::TextButton& b)
    {
        b.getProperties().set ("style", "ghost");
        addAndMakeVisible (b);
    };

    portBox.setTextWhenNothingSelected ("no port");
    portBox.setTooltip ("MIDI destination the controller sends to. The Nymphes "
                        "appears by name; ports are matched by name, never by VID:PID.");
    portBox.onChange = [this]
    {
        const auto name = portBox.getText();
        if (name.isNotEmpty())
            proc.openMidiPort (name);
        deviceOpen = proc.getDeviceStatus().containsIgnoreCase ("open");
        repaint();
    };
    addAndMakeVisible (portBox);

    ghost (connectButton);
    connectButton.onClick = [this] { connectRequested(); };

    ghost (dumpButton);
    dumpButton.getProperties().set ("style", "chip");
    dumpButton.setTooltip ("Arm capture of preset SysEx from the synth. The Nymphes sends its "
                           "presets from its own MENU dump function; there is no verified "
                           "host-side request message, so this arms capture rather than "
                           "inventing one.");
    dumpButton.onClick = [this] { proc.requestFullDump(); };

    deviceChanged (proc).addChangeListener (this);

    refreshPorts();
    statusText = proc.getDeviceStatus();
}

StatusRail::~StatusRail()
{
    deviceChanged (proc).removeChangeListener (this);
}

void StatusRail::refreshPorts()
{
    const auto previous = portBox.getText();
    portBox.clear (juce::dontSendNotification);

    // The Nymphes appears as a MIDI *destination*; that is what we send to. The
    // port is matched by name inside MidiPort, never by VID:PID, because 0483:5740
    // is STMicroelectronics' generic pair and matches unrelated devices.
    int id = 1;
    for (const auto& dev : juce::MidiOutput::getAvailableDevices())
        portBox.addItem (dev.name, id++);

    if (previous.isNotEmpty())
        for (int i = 0; i < portBox.getNumItems(); ++i)
            if (portBox.getItemText (i) == previous)
            {
                portBox.setSelectedId (portBox.getItemId (i), juce::dontSendNotification);
                break;
            }

    if (portBox.getSelectedId() == 0 && portBox.getNumItems() > 0)
        for (int i = 1; i <= portBox.getNumItems(); ++i)
            if (portBox.getItemText (i - 1).containsIgnoreCase ("Nymphes"))
            {
                portBox.setSelectedId (i, juce::dontSendNotification);
                break;
            }

    repaint();
}

void StatusRail::connectRequested()
{
    refreshPorts();
    proc.openMidiPort (portBox.getText());
    statusText = proc.getDeviceStatus();
    deviceOpen = statusText.containsIgnoreCase ("open");
    repaint();
}

void StatusRail::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto s = proc.getDeviceStatus();
    if (s != statusText)
    {
        statusText = s;
        deviceOpen = s.containsIgnoreCase ("open");
        repaint();
    }
}

void StatusRail::resized()
{
    auto r = getLocalBounds().reduced (kGapL, 0);

    dumpButton.setBounds (r.removeFromRight (58).withSizeKeepingCentre (58, 20));
    r.removeFromRight (kGapS);
    connectButton.setBounds (r.removeFromRight (78).withSizeKeepingCentre (78, 20));
    r.removeFromRight (kGapS);
    portBox.setBounds (r.removeFromRight (196).withSizeKeepingCentre (196, 20));
}

void StatusRail::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (kPanelSurround);
    g.fillRect (r);
    g.setColour (kLilacLine);
    g.fillRect (r.removeFromTop (1.0f));

    // The dot is the whole point of this bar: it is the one place in the UI that
    // answers "is the synth listening" without reading a sentence.
    const auto dotColour = deviceOpen ? kGreen : kCreamMute;
    const juce::Point<float> dot (r.getX() + kGapL + 4.0f, r.getCentreY());
    if (deviceOpen)
    {
        g.setColour (withAlpha (dotColour, 0.22f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (dot));
    }
    g.setColour (dotColour);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (dot));

    auto text = r.withTrimmedLeft (kGapL + 16.0f);
    text.removeFromRight (58 + 78 + 196 + kGapS * 2 + kGapL);

    g.setColour (deviceOpen ? kCreamDim : kCreamMute);
    g.setFont (fontMicro());
    g.drawFittedText (statusText.isNotEmpty() ? statusText : juce::String ("device not connected"),
                      text.toNearestInt(), juce::Justification::centredLeft, 1);
}

} // namespace nymphes::ui
