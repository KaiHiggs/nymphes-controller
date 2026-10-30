// plugin/ui/PluginEditor.cpp  —  L2 (shell) / L1 (the panels it hosts)

#include "plugin/ui/PluginEditor.h"

#include <juce_audio_devices/juce_audio_devices.h>

namespace nymphes::ui
{

using namespace theme;

//==============================================================================
PluginEditor::PluginEditor (Processor& plugIn)
    : juce::AudioProcessorEditor (&plugIn), proc (plugIn), rail (plugIn)
{
    setOpaque (true);

    // FIRST, before any child exists: children capture the default LookAndFeel at
    // construction, so setting it later would leave the first widgets stock.
    juce::LookAndFeel::setDefaultLookAndFeel (&laf);

    // L4's hover/focus hooks, if that file is in this tree.
#if NYMPHES_HAVE_INTERACTION_HELPERS
    installHoverFocusRepaint (*this);
#endif

    nav.addSegment ("Presets",  "Browse and load the 98 preset slots");
    nav.addSegment ("Controls", "All 82 MIDI CC parameters");
    // Which view opens first. Presets by default, because the first thing a
    // player does with an unloaded bank is look for presets. NYMPHES_UI_VIEW is a
    // documented capture/debug hook so tools/capture-ui.sh can photograph the
    // controls view without a synthetic mouse click; it changes nothing else.
    const auto startView = juce::SystemStats::getEnvironmentVariable ("NYMPHES_UI_VIEW", "presets");
    nav.setActiveIndex (startView.containsIgnoreCase ("control") ? 1 : 0,
                        juce::dontSendNotification);
    nav.onChange = [this] (int i) { showView (i == 0 ? View::presets : View::controls); };
    addAndMakeVisible (nav);

    browser = std::make_unique<BrowserPanel> (proc);
    params  = std::make_unique<ParamPanel> (proc);

    addChildComponent (*browser);
    addChildComponent (*params);
    const bool startOnControls = nav.getActiveIndex() == 1;
    view = startOnControls ? View::controls : View::presets;
    browser->setVisible (! startOnControls);
    params->setVisible (startOnControls);

    addAndMakeVisible (rail);

#if NYMPHES_HAVE_PRESET_BAR
    presetBar = std::make_unique<PresetBar> (proc);
    addAndMakeVisible (*presetBar);
#endif

    // Tooltips: 650ms feels like a DAW, not like a website. Created last so it is
    // destroyed first among the editor's members.
    tooltips = std::make_unique<juce::TooltipWindow> (this, 650);

    presetChanged (proc).addChangeListener (this);
    presetLabel = currentPresetLabel();
    presetSegments = currentPresetSegments();

    setResizable (true, true);
    setResizeLimits (kShellMinW, kShellMinH, kShellMaxW, kShellMaxH);
    setSize (kShellDefaultW, kShellDefaultH);
}

PluginEditor::~PluginEditor()
{
    presetChanged (proc).removeChangeListener (this);

    // Children are still alive here, so the default LookAndFeel must stay set
    // until they are gone. Nulling it would leave them pointing at a live object
    // that is no longer the default, which is harmless — but clearing it in the
    // destructor body while children are destroyed afterwards is not. The
    // LookAndFeel is process-lifetime, so there is nothing to release.
}

//==============================================================================
void PluginEditor::showView (View v)
{
    view = v;

    if (browser != nullptr) browser->setVisible (v == View::presets);
    if (params  != nullptr) params->setVisible (v == View::controls);

    if ((int) v != nav.getActiveIndex())
        nav.setActiveIndex ((int) v, juce::dontSendNotification);

    resized();
    repaint();
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();

    g.fillAll (kBg);

    auto header = r.removeFromTop (kHeaderH);
    paintHeader (g, header);
}

juce::Rectangle<int> PluginEditor::presetClusterBounds() const
{
    const int w = 248;
    const int h = 44;
    return { getWidth() - kGapL - w, (kHeaderH - h) / 2, w, h };
}

juce::Rectangle<int> PluginEditor::presetBarBounds() const
{
    const auto cluster = presetClusterBounds();
    const int left = kGapL + 34 + kGapM + 210;      // past the mark and the wordmark
    const int right = cluster.getX() - kGapM;
    if (right - left < 160)
        return {};
    return { left, (kHeaderH - 40) / 2, right - left, 40 };
}

void PluginEditor::paintHeader (juce::Graphics& g, juce::Rectangle<int> header) const
{
    // The header is the window band, not the panel: it takes the surround tone so
    // the blue field below reads as the chassis, the way the hardware's bezel does.
    g.setColour (kPanelSurround);
    g.fillRect (header);

    auto inner = header.reduced (kGapL, 0);

    //--- the lockup -----------------------------------------------------------
    // The panel's own: a small tracked line over a wide pink wordmark whose lower
    // half is cut by horizontal stripes, the way NYMPHES is printed on the chassis.
    g.setColour (kPinkText);
    g.setFont (fontMicro().withHeight (10.0f));
    drawTrackedText (g, "6-VOICE ANALOG CONTROLLER",
                     { (float) inner.getX() + 2.0f, (float) header.getY() + 9.0f, 300.0f, 12.0f },
                     juce::Justification::centredLeft, 2.4f);

    g.setFont (theme::font (31.0f, true));
    const float markTracking = 7.0f;
    const auto word = juce::String ("NYMPHES");
    const float wordW = trackedWidth (g.getCurrentFont(), word, markTracking);
    const juce::Rectangle<float> wordArea ((float) inner.getX(), (float) header.getY() + 22.0f,
                                           wordW + 4.0f, 34.0f);
    g.setColour (kPinkAccent);
    drawTrackedText (g, word, wordArea, juce::Justification::centredLeft, markTracking);

    // the stripes: header-coloured rules across the lower part of the glyphs,
    // thickening toward the baseline
    g.setColour (kPanelSurround);
    float stripeY = wordArea.getCentreY() - 1.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float h = 1.0f + (float) i * 0.35f;
        g.fillRect (wordArea.getX(), stripeY, wordArea.getWidth(), h);
        stripeY += 3.6f;
    }

    //--- the preset cluster ---------------------------------------------------
    // Always painted. It is the instrument's own PRESET CONTROL, so a peer's
    // PresetBar (when present) sits to its left rather than replacing it — the
    // seven-segment readout is part of the identity, not decoration.
    paintPresetCluster (g, presetClusterBounds());

    g.setColour (kLilacLine);
    g.fillRect (header.getX(), header.getBottom() - 1, header.getWidth(), 1);
}

void PluginEditor::paintPresetCluster (juce::Graphics& g, juce::Rectangle<int> r) const
{
    // The hardware's PRESET CONTROL: a small outlined box holding the A-G / 1-7
    // grid and a seven-segment readout. Not a text field with a label — a cluster,
    // so the current location is legible without reading a sentence.
    auto box = r.toFloat();
    g.setColour (withAlpha (kWellBlue, 0.55f));
    g.fillRoundedRectangle (box, (float) kRadiusS);
    g.setColour (kLilacLine);
    g.drawRoundedRectangle (box.reduced (0.5f), (float) kRadiusS, 1.0f);

    // title band, pill-outlined, interrupting the top edge
    {
        const auto f = fontMicro();
        const auto title = juce::String ("PRESET CONTROL");
        const float w = measureWidth (f, title) + 16.0f;
        auto band = juce::Rectangle<float> (w, 14.0f)
                        .withCentre ({ box.getX() + 12.0f + w * 0.5f, box.getY() });
        g.setColour (withAlpha (kWellBlue, 0.95f));
        g.fillRect (band.expanded (1.0f, 0.0f));
        g.setColour (kLilacLine);
        g.drawRoundedRectangle (band, 7.0f, 1.0f);
        g.setColour (kPinkText);
        g.setFont (f);
        drawTrackedText (g, title, band, juce::Justification::centred, 1.3f);
    }

    //--- the seven-segment readout -------------------------------------------
    auto lcd = juce::Rectangle<float> (78.0f, 28.0f)
                   .withCentre ({ box.getX() + 12.0f + 39.0f, box.getCentreY() + 3.0f });
    g.setColour (kDisplayBg);
    g.fillRoundedRectangle (lcd, 3.0f);
    g.setColour (withAlpha (kLilacLine, 0.8f));
    g.drawRoundedRectangle (lcd.reduced (0.5f), 3.0f, 1.0f);

    const auto segs = presetSegments.isNotEmpty() ? presetSegments : juce::String ("---");
    drawSegmentText (g, lcd.reduced (8.0f, 5.0f), segs);

    //--- the A-G / 1-7 grid ---------------------------------------------------
    const auto& addr = proc.getCurrentPreset().address();
    const int curBank = juce::jlimit (1, 7, (int) addr.bank);
    const int curProg = juce::jlimit (1, 7, (int) addr.program);
    const bool hasLoaded = ! proc.getCurrentPreset().payload().empty();

    // withTrimmedLeft takes an AMOUNT, not an absolute coordinate: passing
    // lcd.getRight() directly trimmed away the whole rect once the cluster was
    // not at x=0, so the grid silently never drew. Subtract the box origin.
    auto grid = box.withTrimmedLeft (lcd.getRight() - box.getX() + 8.0f)
                   .withTrimmedRight (8.0f)
                   .withTrimmedTop (7.0f)
                   .withTrimmedBottom (7.0f);

    const float cellW = grid.getWidth() / 7.0f;
    const float rowH = grid.getHeight() * 0.5f;

    for (int i = 0; i < 7; ++i)
    {
        const bool lit = hasLoaded && (i + 1) == curBank;
        auto cellA = juce::Rectangle<float> (grid.getX() + (float) i * cellW, grid.getY(),
                                             cellW, rowH);
        g.setColour (lit ? kPinkAccent : withAlpha (kCreamMute, 0.85f));
        g.setFont (fontMicro());
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + i)), cellA,
                    juce::Justification::centred, false);
        if (lit)
        {
            g.fillRoundedRectangle (cellA.getCentreX() - 5.0f, cellA.getBottom() - 2.0f,
                                    10.0f, 1.5f, 0.75f);
        }

        const bool litN = hasLoaded && (i + 1) == curProg;
        auto cellB = cellA.translated (0.0f, rowH);
        g.setColour (litN ? kPinkAccent : withAlpha (kCreamMute, 0.85f));
        g.setFont (fontMicro());
        g.drawText (juce::String (i + 1), cellB, juce::Justification::centred, false);
        if (litN)
        {
            g.fillRoundedRectangle (cellB.getCentreX() - 5.0f, cellB.getBottom() - 2.0f,
                                    10.0f, 1.5f, 0.75f);
        }
    }
}

juce::String PluginEditor::currentPresetSegments() const
{
    // Nothing loaded: an empty preset's default address is not a slot, so show
    // dashes rather than a made-up one.
    if (proc.getCurrentPreset().payload().empty())
        return {};
    const auto& a = proc.getCurrentPreset().address();
    const int bank = juce::jlimit (1, 7, (int) a.bank);
    const int prog = juce::jlimit (1, 7, (int) a.program);
    juce::String s;
    s << (a.isUser() ? 'U' : 'F')
      << juce::String::charToString ((juce::juce_wchar) ('A' + bank - 1))
      << prog;
    return s;
}

juce::String PluginEditor::currentPresetLabel() const
{
    // Preset identity is the slot, not a name: the Nymphes payload has 22 numeric
    // protobuf fields and none of them is a string, so "USER A1" *is* the name.
    const auto& p = proc.getCurrentPreset();
    const auto s = nymphes::displayName (p);
    return juce::String::fromUTF8 (s.c_str(), (int) s.size());
}

void PluginEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto nextLabel = currentPresetLabel();
    const auto nextSegs = currentPresetSegments();
    if (nextLabel != presetLabel || nextSegs != presetSegments)
    {
        presetLabel = nextLabel;
        presetSegments = nextSegs;
        repaint (0, 0, getWidth(), kHeaderH);
    }
}

//==============================================================================
void PluginEditor::resized()
{
    auto r = getLocalBounds();

    r.removeFromTop (kHeaderH);

#if NYMPHES_HAVE_PRESET_BAR
    if (presetBar != nullptr)
    {
        const auto slot = presetBarBounds();
        presetBar->setVisible (! slot.isEmpty());
        if (! slot.isEmpty())
            presetBar->setBounds (slot);
    }
#endif

    nav.setBounds (r.removeFromTop (kNavH));
    rail.setBounds (r.removeFromBottom (kStatusRailH));

    // One content rect, shared by both views, so switching views never moves the
    // edges of the app. Side margin kGapL, top kGapM, bottom kGapS.
    auto content = r.reduced (kGapL, 0)
                    .withTrimmedTop (kGapM)
                    .withTrimmedBottom (kGapS);

    if (browser != nullptr) browser->setBounds (content);
    if (params  != nullptr) params->setBounds (content);
}

} // namespace nymphes::ui
