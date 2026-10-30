// plugin/ui/ParamPanel.cpp
//
// Geometry, in one place:
//   a section is an open lilac bracket (left rule rising into a top rule) with
//   its title under the top rule; under the title, a pill row; under that, the
//   fader cells, packed edge to edge so their ladders join into one rail the way
//   the printed ladders do on the hardware.
//   Every fader cell in both rows has the same width, so the two rows read as
//   one panel; slack goes to the gaps between sections, never into wider faders
//   in one row only.

#include "plugin/ui/ParamPanel.h"

#include "plugin/cc/CcSpec.h"
#include "plugin/ui/Theme.h"

namespace nymphes::ui
{

using namespace theme;

namespace
{
    constexpr int kBracketPad   = 12;   // left rule to the first cell
    constexpr int kSectionTail  = 6;    // last cell to the section's right edge
    constexpr int kBankGap      = 12;   // between a section's banks
    constexpr int kSideGap      = 14;   // between the last bank and the side lists
    constexpr int kSideW        = 60;   // LFO TYPE / SYNC column
    constexpr int kTitleH       = 26;
    constexpr int kPillH        = 16;
    constexpr int kPillRowH     = 24;
    constexpr int kMinSectionGap= 16;
    constexpr int kRowGap       = 14;
    constexpr int kMenuW        = 214;
    constexpr int kMinCellW     = 34;
    constexpr int kMaxCellW     = 58;
    constexpr int kLegibleCellW = 42;   // below this the labels collide: scale instead
}

int ParamPanel::Section::cellCount() const
{
    int n = 0;
    for (const auto& b : banks)
        n += (int) b.cells.size();
    return n;
}

//==============================================================================
ParamPanel::ParamPanel (Processor& p) : proc (p)
{
    surface.onPaint = [this] (juce::Graphics& g) { paintSurface (g); };
    addAndMakeVisible (surface);

    osc = &addSection ("OSCILLATOR");
    addBank (*osc, {},      { { 70, "WAVE" }, { 9, "LVL" }, { 10, "SUB" }, { 11, "NOISE" }, { 13, "LFO" } });
    addBank (*osc, "SHIFT", { { 12, "PW" }, { 5, "GLD" }, { 15, "DTN" }, { 16, "CHORD" }, { 14, "EG" } });

    filter = &addSection ("FILTER");
    addBank (*filter, {},      { { 74, "CUT" }, { 71, "RES" }, { 3, "EG" } });
    addBank (*filter, "SHIFT", { { 81, "HPF" }, { 4, "TRACK" }, { 8, "LFO" } });

    reverb = &addSection ("REVERB");
    addBank (*reverb, {}, { { 75, "SIZE" }, { 76, "DEC" }, { 77, "FILT" }, { 78, "MIX" } });

    env = &addSection ("ENVELOPE");
    addBank (*env, "FILTER", { { 79, "A" }, { 80, "D" }, { 82, "S" }, { 83, "R" } });
    addBank (*env, "VCA",    { { 73, "A" }, { 84, "D" }, { 85, "S" }, { 72, "R" } });

    lfo1 = &addSection ("LFO 1");
    addBank (*lfo1, {},      { { 18, "RATE" }, { 19, "WAVE" } });
    addBank (*lfo1, "SHIFT", { { 20, "DELAY" }, { 21, "FADE" } });
    addCcList (*lfo1, 22, "TYPE", { "BPM", "LOW", "HIGH", "TRACK" });
    addCcList (*lfo1, 23, "SYNC", { "OFF", "ON" });

    lfo2 = &addSection ("LFO 2");
    addBank (*lfo2, {},      { { 24, "RATE" }, { 25, "WAVE" } });
    addBank (*lfo2, "SHIFT", { { 26, "DELAY" }, { 27, "FADE" } });
    addCcList (*lfo2, 28, "TYPE", { "BPM", "LOW", "HIGH", "TRACK" });
    addCcList (*lfo2, 29, "SYNC", { "OFF", "ON" });

    // MENU — the right-hand column, as on the instrument.
    menu = &addSection ("MENU");

    volume = cells.add (new FaderCell (proc, mod, 7, "VOLUME", FaderCell::Style::knob));
    wheel  = cells.add (new FaderCell (proc, mod, 1, "WHEEL"));
    surface.addAndMakeVisible (volume);
    surface.addAndMakeVisible (wheel);

    modList = lists.add (new OptionList (proc, "MOD EDIT", { "OFF", "LFO 2", "MOD W", "VELO", "AFTER" }));
    modList->getIndex = [this] { return mod.source + 1; };
    modList->setIndex = [this] (int i) { setModSource (i - 1); };
    modList->setTooltip ("Choose a modulation source and every fader sets its depth for that source "
                         "(CC 30 + the depth CCs). OFF returns the faders to the parameters.");
    surface.addAndMakeVisible (modList);

    modeList = lists.add (new OptionList (proc, "MODE", { "POLY", "UNI A", "UNI B", "TRI", "DUO", "MONO" }));
    modeList->bindToCc (17);
    modeList->setNumbered (true);
    surface.addAndMakeVisible (modeList);

    legato = lists.add (new OptionList (proc, "LEGATO", { "OFF", "ON" }, OptionList::Layout::horizontal));
    legato->bindToCc (68);
    surface.addAndMakeVisible (legato);

    sustain = lists.add (new OptionList (proc, "SUSTAIN", { "OFF", "ON" }, OptionList::Layout::horizontal));
    sustain->bindToCc (64);
    surface.addAndMakeVisible (sustain);

    // Capture hook, like NYMPHES_UI_VIEW: open in mod edit for tools/capture-ui.sh.
    // Display only — it does not send CC 30, so photographing the UI never changes
    // the synth's selected source.
    const int captureSource = juce::SystemStats::getEnvironmentVariable ("NYMPHES_UI_MODEDIT", "0").getIntValue();
    if (captureSource >= 1 && captureSource <= kModSourceCount)
    {
        mod.source = captureSource - 1;
        for (auto* c : cells)
            c->refresh();
    }
}

ParamPanel::~ParamPanel() = default;

ParamPanel::Section& ParamPanel::addSection (juce::String title)
{
    sections.push_back (std::make_unique<Section>());
    sections.back()->title = std::move (title);
    return *sections.back();
}

ParamPanel::Bank& ParamPanel::addBank (Section& s, juce::String pill,
                                       std::initializer_list<std::pair<int, const char*>> specs)
{
    s.banks.push_back ({});
    auto& bank = s.banks.back();
    bank.pill = std::move (pill);
    for (const auto& [cc, label] : specs)
    {
        auto* cell = cells.add (new FaderCell (proc, mod, cc, label));
        surface.addAndMakeVisible (cell);
        bank.cells.push_back (cell);
    }
    return bank;
}

OptionList& ParamPanel::addCcList (Section& s, int cc, juce::String title, juce::StringArray options,
                                   OptionList::Layout layout)
{
    auto* list = lists.add (new OptionList (proc, std::move (title), std::move (options), layout));
    list->bindToCc (cc);
    surface.addAndMakeVisible (list);
    s.side.push_back (list);
    return *list;
}

void ParamPanel::setModSource (int source)
{
    mod.source = juce::jlimit (-1, kModSourceCount - 1, source);
    // Selecting a source on the hardware selects it on the synth too; do the same,
    // so the first depth the user moves is already addressed correctly.
    if (mod.active())
        ccEngine (proc).setValue (ccui::kModSourceSelectorCc, mod.source);
    for (auto* c : cells)
        c->refresh();
    surface.repaint();
}

void ParamPanel::focusCc (int)
{
    // Single surface: nothing to reveal.
}

//==============================================================================
int ParamPanel::fixedWidth (const Section& s) const
{
    int w = kBracketPad + kSectionTail + kBankGap * juce::jmax (0, (int) s.banks.size() - 1);
    if (! s.side.empty())
        w += kSideGap + kSideW;
    return w;
}

void ParamPanel::layoutSection (Section& s, int cellW)
{
    const auto r = s.bounds;
    const int pillY = r.getY() + kTitleH;
    const int cellsY = pillY + kPillRowH;
    const int cellsH = r.getBottom() - cellsY;

    int x = r.getX() + kBracketPad;
    for (auto& bank : s.banks)
    {
        const int bankX = x;
        for (auto* c : bank.cells)
        {
            c->setBounds (x, cellsY, cellW, cellsH);
            x += cellW;
        }
        bank.pillBounds = { bankX + 3, pillY + (kPillRowH - kPillH) / 2 - 2, x - bankX - 6, kPillH };
        x += kBankGap;
    }

    if (! s.side.empty())
    {
        x += kSideGap - kBankGap;
        int y = cellsY + FaderCell::kValueH;
        for (auto* list : s.side)
        {
            const int h = list->preferredHeight();
            list->setBounds (x, y, kSideW, h);
            y += h + 14;
        }
    }
}

void ParamPanel::layoutRow (std::vector<Section*> row, juce::Rectangle<int> area, int cellW)
{
    int used = 0;
    for (auto* s : row)
        used += fixedWidth (*s) + s->cellCount() * cellW;

    const int gaps = (int) row.size() - 1;
    const int gap = gaps > 0 ? juce::jmax (kMinSectionGap, (area.getWidth() - used) / gaps) : 0;

    int x = area.getX();
    for (auto* s : row)
    {
        const int w = fixedWidth (*s) + s->cellCount() * cellW;
        s->bounds = { x, area.getY(), w, area.getHeight() };
        layoutSection (*s, cellW);
        x += w + gap;
    }
}

void ParamPanel::layoutMenu (juce::Rectangle<int> area)
{
    menu->bounds = area;
    auto r = area.withTrimmedLeft (kBracketPad).withTrimmedRight (kSectionTail)
                 .withTrimmedTop (kTitleH + 6);

    // VOLUME knob and the WHEEL fader share the top band.
    auto top = r.removeFromTop (juce::jmin (150, r.getHeight() / 3));
    wheel->setBounds (top.removeFromRight (44));
    volume->setBounds (top.reduced (6, 0));

    r.removeFromTop (18);

    // The hardware's MENU is a two-column table; so is this.
    auto table = r.removeFromTop (modeList->preferredHeight());
    const int colW = (table.getWidth() - 10) / 2;
    modList->setBounds (table.removeFromLeft (colW));
    table.removeFromLeft (10);
    modeList->setBounds (table);

    r.removeFromTop (18);
    legato->setBounds (r.removeFromTop (legato->preferredHeight()));
    r.removeFromTop (10);
    sustain->setBounds (r.removeFromTop (sustain->preferredHeight()));
    r.removeFromTop (14);
    hintBounds = r.removeFromTop (juce::jmin (r.getHeight(), 64));
}

int ParamPanel::naturalWidth (int cellW) const
{
    auto rowWidth = [&] (std::initializer_list<const Section*> row)
    {
        int w = kMinSectionGap * ((int) row.size() - 1);
        for (auto* s : row)
            w += fixedWidth (*s) + s->cellCount() * cellW;
        return w;
    };
    return juce::jmax (rowWidth ({ osc, filter, reverb }), rowWidth ({ env, lfo1, lfo2 }))
         + kMinSectionGap + 4 + kMenuW;
}

void ParamPanel::resized()
{
    const auto r = getLocalBounds();
    const int needed = naturalWidth (kLegibleCellW);

    if (r.getWidth() >= needed || r.getWidth() <= 0)
    {
        surface.setTransform ({});
        surface.setBounds (r);
    }
    else
    {
        const float scale = (float) r.getWidth() / (float) needed;
        surface.setBounds (0, 0, needed, juce::roundToInt ((float) r.getHeight() / scale));
        surface.setTransform (juce::AffineTransform::scale (scale));
    }
    layoutSurface (surface.getLocalBounds());
}

void ParamPanel::layoutSurface (juce::Rectangle<int> bounds)
{
    auto r = bounds.reduced (0, 2);

    auto menuArea = r.removeFromRight (kMenuW);
    r.removeFromRight (kMinSectionGap + 4);

    std::vector<Section*> rowA { osc, filter, reverb };
    std::vector<Section*> rowB { env, lfo1, lfo2 };

    // One cell width for both rows: the widest that lets the fuller row fit.
    auto widthFor = [&] (const std::vector<Section*>& row)
    {
        int fixed = kMinSectionGap * ((int) row.size() - 1);
        int n = 0;
        for (auto* s : row) { fixed += fixedWidth (*s); n += s->cellCount(); }
        return n > 0 ? (r.getWidth() - fixed) / n : kMaxCellW;
    };
    const int cellW = juce::jlimit (kMinCellW, kMaxCellW, juce::jmin (widthFor (rowA), widthFor (rowB)));

    const int rowH = (r.getHeight() - kRowGap) / 2;
    layoutRow (rowA, r.removeFromTop (rowH), cellW);
    r.removeFromTop (kRowGap);
    layoutRow (rowB, r, cellW);

    layoutMenu (menuArea);
}

//==============================================================================
void ParamPanel::paintSection (juce::Graphics& g, const Section& s) const
{
    const auto line = mod.active() ? kPinkAccent : kLilacLine;
    const auto r = s.bounds.toFloat();
    const float stroke = 2.0f;
    const float corner = 7.0f;
    const float x = r.getX() + stroke * 0.5f;
    const float y = r.getY() + stroke * 0.5f;

    g.setFont (fontGroup().withHeight (13.0f));
    const float titleW = trackedWidth (g.getCurrentFont(), s.title, 1.6f);

    // The bracket: left rule up from the bottom, round the corner, along the top a
    // little past the title — open on the right, as printed.
    const float topLen = juce::jmin (r.getWidth() - corner, titleW + 46.0f);
    juce::Path bracket;
    bracket.startNewSubPath (x, r.getBottom() - 4.0f);
    bracket.lineTo (x, y + corner);
    bracket.quadraticTo (x, y, x + corner, y);
    bracket.lineTo (x + topLen, y);
    g.setColour (line);
    g.strokePath (bracket, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    g.setColour (kCreamLabel);
    drawTrackedText (g, s.title, { r.getX() + kBracketPad, r.getY() + 6.0f, titleW + 4.0f, 16.0f },
                     juce::Justification::centredLeft, 1.6f);

    for (const auto& bank : s.banks)
    {
        if (bank.pill.isEmpty() || bank.pillBounds.isEmpty())
            continue;
        const auto pill = bank.pillBounds.toFloat();
        g.setColour (withAlpha (line, 0.9f));
        g.drawRoundedRectangle (pill.reduced (0.6f), pill.getHeight() * 0.5f, 1.3f);
        g.setFont (fontGroup().withHeight (9.5f));
        g.setColour (mod.active() ? kPinkText : kLilacBright);
        drawTrackedText (g, bank.pill, pill.translated (0.0f, 0.5f), juce::Justification::centred, 1.2f);
    }
}

void ParamPanel::paint (juce::Graphics& g)
{
    g.fillAll (kPanelBlue);
}

void ParamPanel::paintSurface (juce::Graphics& g)
{
    for (const auto& s : sections)
        paintSection (g, *s);

    // The mode is the one thing that changes what every fader means: say it.
    if (! hintBounds.isEmpty())
    {
        g.setFont (fontText().withHeight (11.5f));
        if (mod.active())
        {
            g.setColour (kPinkText);
            g.drawFittedText ("Faders set " + juce::String (kModSourceLong[mod.source])
                              + " depth. Choose OFF to return.",
                              hintBounds, juce::Justification::topLeft, 3);
        }
        else
        {
            g.setColour (withAlpha (kCreamMute, 0.75f));
            g.drawFittedText ("Right-click any control to MIDI-learn it.",
                              hintBounds, juce::Justification::topLeft, 3);
        }
    }
}

} // namespace nymphes::ui
