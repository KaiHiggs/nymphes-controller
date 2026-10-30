// plugin/ui/BrowserPanel.cpp

#include "plugin/ui/BrowserPanel.h"

#include "plugin/ui/Theme.h"

#include <cstring>

namespace nymphes::ui
{

using namespace theme;

namespace
{
    constexpr int kBanks = 7;
    constexpr int kPrograms = 7;
    constexpr int kPerPage = kBanks * kPrograms;
    constexpr int kDetailW = 330;
    constexpr int kLegend = 26;      // the A..G / 1..7 legend band
    constexpr int kTileGap = 8;

    juce::String bankLetter (int col) { return juce::String::charToString ((juce::juce_wchar) ('A' + col)); }
}

//==============================================================================
BrowserPanel::BrowserPanel (Processor& p)
    : proc (p),
      pageSwitch (p, {}, { "USER", "FACTORY" }, OptionList::Layout::horizontal)
{
    setWantsKeyboardFocus (true);

    pageSwitch.getIndex = [this] { return page; };
    pageSwitch.setIndex = [this] (int i)
    {
        page = juce::jlimit (0, 1, i);
        if (selected >= 0)
            select (page * kPerPage + selected % kPerPage);
        repaint();
    };
    pageSwitch.setTooltip ("USER slots can be written. FACTORY slots are read-only.");
    addAndMakeVisible (pageSwitch);

    auditionButton.getProperties().set ("style", "primary");
    // primary takes its pink from the style, not from the scheme's button colour
    auditionButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    copyButton.getProperties().set ("style", "raised");
    for (auto* b : { &dumpButton, &importButton, &exportButton })
        b->getProperties().set ("style", "ghost");

    auditionButton.setTooltip ("Send the selected preset to the synth's edit buffer (FF=00). "
                               "Nothing is written to its memory.");
    dumpButton.setTooltip ("Ask the Nymphes for all 98 presets.");
    importButton.setTooltip ("Load presets from a .syx file into this browser (not into the synth).");
    exportButton.setTooltip ("Save every preset this browser holds (both pages) as one .syx file.");

    auditionButton.onClick = [this] { doAudition(); };
    copyButton.onClick     = [this] { doCopyLoaded(); };
    dumpButton.onClick     = [this] { doRequestDump(); };
    importButton.onClick   = [this] { doImport(); };
    exportButton.onClick   = [this] { doExport(); };

    for (auto* b : { &auditionButton, &copyButton, &dumpButton, &importButton, &exportButton })
        addAndMakeVisible (b);

    presetChanged (proc).addChangeListener (this);
    deviceChanged (proc).addChangeListener (this);

    // Capture hook, like NYMPHES_UI_VIEW: fill the browser from a .syx so
    // tools/capture-ui.sh can photograph a populated bank. Reads a file into this
    // browser only; nothing is sent to the synth.
    const auto captureImport = juce::SystemStats::getEnvironmentVariable ("NYMPHES_UI_IMPORT", {});
    if (captureImport.isNotEmpty())
    {
        juce::String error;
        importSyxFile (proc, juce::File (captureImport), error);
    }

    rebuildPrints();

    const int loaded = loadedIndex (proc);
    if (loaded >= 0)
    {
        page = loaded / kPerPage;
        selected = loaded;
    }
    updateButtons();
}

void BrowserPanel::rebuildPrints()
{
    static constexpr const char* names[kPrintBars] =
        { "wave", "lvl", "sub", "noise", "cut", "reson", "cut_eg", "a1", "d1", "s1", "r1" };

    for (int i = 0; i < PresetBank::kSize; ++i)
    {
        auto& pr = prints[(std::size_t) i];
        pr = {};
        if (! slotPresent (proc, i))
            continue;

        for (const auto& f : describe (slotPreset (proc, i)))
        {
            if (f.group == Group::VoiceMode)
                pr.mode = juce::String (voiceModeName ((std::uint64_t) f.value)).toUpperCase();
            if (f.group != Group::Main || f.name == nullptr)
                continue;
            for (int b = 0; b < kPrintBars; ++b)
                if (std::strcmp (f.name, names[b]) == 0)
                    pr.v[(std::size_t) b] = juce::jlimit (0.0f, 1.0f, f.value);
        }
        pr.valid = true;
    }
}

BrowserPanel::~BrowserPanel()
{
    presetChanged (proc).removeChangeListener (this);
    deviceChanged (proc).removeChangeListener (this);
}

//==============================================================================
int BrowserPanel::indexFor (int col, int row) const noexcept
{
    // PresetBank's convention: bank-major within a page (A1..A7, B1..B7, ...).
    return page * kPerPage + col * kPrograms + row;
}

juce::Rectangle<int> BrowserPanel::tileBounds (int col, int row) const
{
    auto r = gridArea.withTrimmedLeft (kLegend).withTrimmedTop (kLegend);
    const int tw = (r.getWidth() - kTileGap * (kBanks - 1)) / kBanks;
    const int th = (r.getHeight() - kTileGap * (kPrograms - 1)) / kPrograms;
    return { r.getX() + col * (tw + kTileGap), r.getY() + row * (th + kTileGap), tw, th };
}

int BrowserPanel::tileAt (juce::Point<int> pt) const
{
    for (int col = 0; col < kBanks; ++col)
        for (int row = 0; row < kPrograms; ++row)
            if (tileBounds (col, row).contains (pt))
                return indexFor (col, row);
    return -1;
}

//==============================================================================
void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    detailArea = r.removeFromRight (kDetailW);
    r.removeFromRight (kGapXL);

    auto top = r.removeFromTop (30);
    pageSwitch.setBounds (top.removeFromLeft (220).withSizeKeepingCentre (220, pageSwitch.preferredHeight()));
    r.removeFromTop (kGapM);

    promptArea = top.withTrimmedLeft (kGapXL);
    gridArea = r;

    auto d = detailArea.reduced (kGapL, 0).withTrimmedTop (38);
    readoutArea = d.removeFromTop (58);
    d.removeFromTop (kGapS);
    summaryArea = d.removeFromTop (juce::jlimit (90, 170, d.getHeight() - 260));
    d.removeFromTop (kGapM);
    auditionButton.setBounds (d.removeFromTop (36));
    d.removeFromTop (kGapS);
    copyButton.setBounds (d.removeFromTop (32));
    d.removeFromTop (kGapXL);
    auto row = d.removeFromTop (28);
    const int bw = (row.getWidth() - 2 * kGapS) / 3;
    dumpButton.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (kGapS);
    importButton.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (kGapS);
    exportButton.setBounds (row);
    d.removeFromTop (kGapM);
    statusArea = d.removeFromTop (juce::jmin (d.getHeight(), 90));
}

//==============================================================================
void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (kPanelBlue);
    paintGrid (g);
    paintDetail (g);

    // One prompt for the state every first launch is in: nothing read yet.
    if (bank (proc).countPresent() == 0)
    {
        g.setFont (fontText().withHeight (12.5f));
        g.setColour (kPinkText);
        g.drawFittedText ("No presets read yet. REQUEST DUMP reads all 98 from the synth; "
                          "IMPORT .SYX loads a file.", promptArea, juce::Justification::centredLeft, 2);
    }
}

void BrowserPanel::paintGrid (juce::Graphics& g) const
{
    // legend: bank letters across the top, programs down the side, lilac like the
    // panel's printed legend round the selector
    g.setFont (fontGroup().withHeight (12.0f));
    for (int col = 0; col < kBanks; ++col)
    {
        const auto t = tileBounds (col, 0);
        g.setColour (kLilacBright);
        g.drawText (bankLetter (col), t.withY (gridArea.getY()).withHeight (kLegend - 6),
                    juce::Justification::centred, false);
    }
    for (int row = 0; row < kPrograms; ++row)
    {
        const auto t = tileBounds (0, row);
        g.setColour (kLilacBright);
        g.drawText (juce::String (row + 1), t.withX (gridArea.getX()).withWidth (kLegend - 8),
                    juce::Justification::centred, false);
    }

    for (int col = 0; col < kBanks; ++col)
        for (int row = 0; row < kPrograms; ++row)
            paintTile (g, col, row);

    if (hasKeyboardFocus (false) && selected >= 0)
    {
        const int local = selected % kPerPage;
        if (selected / kPerPage == page)
        {
            g.setColour (kFocusIce);
            g.drawRoundedRectangle (tileBounds (local / kPrograms, local % kPrograms).toFloat().expanded (2.5f),
                                    (float) kRadiusM + 2.0f, 1.5f);
        }
    }
}

void BrowserPanel::paintTile (juce::Graphics& g, int col, int row) const
{
    const int index = indexFor (col, row);
    const auto r = tileBounds (col, row).toFloat();
    const bool present = slotPresent (proc, index);
    const bool isSelected = index == selected;
    const bool isLoaded = index == loadedIndex (proc);
    const bool isHovered = index == hovered && present;
    const float radius = (float) kRadiusM;

    if (isSelected && present)
    {
        g.setColour (kPinkAccent);
        g.fillRoundedRectangle (r, radius);
    }
    else if (isSelected)
    {
        // selected but empty: the place is chosen, but there is nothing in it
        g.setColour (kPinkAccent);
        g.drawRoundedRectangle (r.reduced (1.0f), radius, 2.0f);
    }
    else if (present)
    {
        g.setColour (isHovered ? kRaisedBlue : kPlateBlue);
        g.fillRoundedRectangle (r, radius);
        g.setColour (withAlpha (kLilacLine, isHovered ? 1.0f : 0.75f));
        g.drawRoundedRectangle (r.reduced (0.6f), radius, 1.2f);
    }
    else
    {
        // empty: a dashed outline and nothing else — it is a place, not a preset
        juce::Path outline;
        outline.addRoundedRectangle (r.reduced (1.0f), radius);
        juce::Path dashed;
        const float dashes[] = { 3.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (withAlpha (kLilacLine, 0.38f));
        g.fillPath (dashed);
    }

    const auto ink = isSelected ? (present ? kOnPink : kPinkText)
                                : (present ? kCreamLabel : withAlpha (kCreamMute, 0.35f));
    g.setColour (ink);
    g.setFont (fontValue().withHeight (juce::jlimit (12.0f, 20.0f, r.getHeight() * 0.28f)));
    g.drawText (bankLetter (col) + juce::String (row + 1), r.reduced (8.0f, 6.0f),
                juce::Justification::topLeft, false);

    if (isLoaded)
    {
        // the slot the synth is playing: a small solid dot, top right
        g.setColour (isSelected && present ? kOnPink : kPinkAccent);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f)
                           .withPosition (r.getRight() - 14.0f, r.getY() + 9.0f));
    }

    const auto& pr = prints[(std::size_t) index];
    if (pr.valid && r.getHeight() > 56.0f)
    {
        // three groups, as on the panel: OSC 4 | FILTER 3 | ENV 4
        auto area = r.reduced (9.0f, 8.0f).withTrimmedTop (r.getHeight() * 0.36f).withTrimmedBottom (13.0f);
        const float gap = juce::jmax (4.0f, area.getWidth() * 0.06f);
        const float barW = juce::jmax (2.0f, (area.getWidth() - gap * 2.0f) / (float) kPrintBars - 2.0f);
        float x = area.getX();
        for (int b = 0; b < kPrintBars; ++b)
        {
            if (b == 4 || b == 7)
                x += gap;
            auto rail = juce::Rectangle<float> (x, area.getY(), barW, area.getHeight());
            g.setColour (isSelected ? withAlpha (kOnPink, 0.18f) : withAlpha (kFaderBlack, 0.55f));
            g.fillRoundedRectangle (rail, 1.0f);
            const float h = juce::jmax (1.5f, rail.getHeight() * pr.v[(std::size_t) b]);
            g.setColour (isSelected ? kOnPink : withAlpha (kLilacBright, 0.9f));
            g.fillRoundedRectangle (rail.withTrimmedTop (rail.getHeight() - h), 1.0f);
            x += barW + 2.0f;
        }

        g.setFont (fontMicro());
        g.setColour (isSelected ? withAlpha (kOnPink, 0.8f) : withAlpha (kCreamMute, 0.8f));
        g.drawText (pr.mode, r.reduced (9.0f, 5.0f), juce::Justification::bottomRight, false);
    }

    if (! present && r.getHeight() > 40.0f)
    {
        g.setFont (fontMicro());
        g.setColour (withAlpha (kCreamMute, 0.35f));
        g.drawText ("EMPTY", r.reduced (8.0f, 6.0f), juce::Justification::bottomLeft, false);
    }
}

void BrowserPanel::paintDetail (juce::Graphics& g) const
{
    // bracket, as on the controls page
    const auto r = detailArea.toFloat();
    juce::Path bracket;
    bracket.startNewSubPath (r.getX() + 1.0f, r.getBottom() - 4.0f);
    bracket.lineTo (r.getX() + 1.0f, r.getY() + 8.0f);
    bracket.quadraticTo (r.getX() + 1.0f, r.getY() + 1.0f, r.getX() + 8.0f, r.getY() + 1.0f);
    bracket.lineTo (r.getX() + 150.0f, r.getY() + 1.0f);
    g.setColour (kLilacLine);
    g.strokePath (bracket, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour (kCreamLabel);
    g.setFont (fontGroup().withHeight (13.0f));
    drawTrackedText (g, "SELECTED", { r.getX() + kGapL, r.getY() + 7.0f, 200.0f, 16.0f },
                     juce::Justification::centredLeft, 1.6f);

    // the readout: the same seven-segment display as PRESET CONTROL
    const auto lcd = readoutArea.toFloat();
    g.setColour (kDisplayBg);
    g.fillRoundedRectangle (lcd, (float) kRadiusS);
    g.setColour (withAlpha (kLilacLine, 0.7f));
    g.drawRoundedRectangle (lcd.reduced (0.5f), (float) kRadiusS, 1.0f);

    juce::String segs = "---";
    if (selected >= 0)
    {
        const auto s = slotOf (proc, selected);
        segs = juce::String (slotIsFactory (proc, selected) ? "F" : "U")
             + juce::String::charToString (s.bankLetter()) + juce::String ((int) s.program);
    }
    drawSegmentText (g, lcd.reduced (14.0f, 9.0f), segs);

    auto summary = summaryArea;
    if (selected < 0)
    {
        g.setFont (fontText());
        g.setColour (kCreamMute);
        g.drawFittedText ("Choose a slot.", summary, juce::Justification::topLeft, 3);
    }
    else
    {
        const auto& pr = prints[(std::size_t) selected];
        auto titleRow = summary.removeFromTop (18);
        g.setFont (fontGroup().withHeight (12.0f));
        g.setColour (kCreamLabel);
        g.drawText (slotName (proc, selected), titleRow, juce::Justification::centredLeft, false);
        if (pr.mode.isNotEmpty())
        {
            g.setColour (kPinkText);
            g.drawText (pr.mode, titleRow, juce::Justification::centredRight, false);
        }
        summary.removeFromTop (kGapS);

        if (! pr.valid)
        {
            g.setFont (fontText().withHeight (12.0f));
            g.setColour (kCreamMute);
            g.drawFittedText ("Empty: this browser has not seen a preset for this slot. "
                              "Request a dump from the synth or import a .syx.",
                              summary, juce::Justification::topLeft, 3);
        }
        else
        {
            // the fingerprint, large and labelled: a miniature of the panel
            static constexpr const char* labels[kPrintBars] =
                { "WAVE", "LVL", "SUB", "NOISE", "CUT", "RES", "EG", "A", "D", "S", "R" };
            static constexpr const char* groups[3] = { "OSC", "FILTER", "ENV" };
            static constexpr int groupStart[4] = { 0, 4, 7, kPrintBars };

            auto area = summary.toFloat();
            auto groupRow = area.removeFromTop (14.0f);
            auto labelRow = area.removeFromBottom (14.0f);
            const float gap = 12.0f;
            const float cellW = (area.getWidth() - gap * 2.0f) / (float) kPrintBars;

            float x = area.getX();
            for (int grp = 0; grp < 3; ++grp)
            {
                const float gx = x;
                for (int b = groupStart[grp]; b < groupStart[grp + 1]; ++b)
                {
                    auto cell = juce::Rectangle<float> (x, area.getY() + 2.0f, cellW, area.getHeight() - 4.0f);
                    auto rail = cell.withSizeKeepingCentre (juce::jmin (10.0f, cellW * 0.5f), cell.getHeight());
                    g.setColour (kFaderBlack);
                    g.fillRoundedRectangle (rail, 2.0f);
                    const float h = juce::jmax (2.0f, rail.getHeight() * pr.v[(std::size_t) b]);
                    g.setColour (kLilacBright);
                    g.fillRoundedRectangle (rail.withTrimmedTop (rail.getHeight() - h), 2.0f);

                    g.setFont (fontMicro());
                    g.setColour (kCreamDim);
                    g.drawText (labels[b], labelRow.withX (x).withWidth (cellW), juce::Justification::centred, false);
                    x += cellW;
                }
                g.setFont (fontMicro());
                g.setColour (kLilacBright);
                g.drawText (groups[grp], groupRow.withX (gx).withWidth (x - gx), juce::Justification::centredLeft, false);
                x += gap;
            }
        }
    }

    if (status.isNotEmpty())
    {
        g.setFont (fontText().withHeight (12.0f));
        g.setColour (statusIsError ? kRed : kCreamDim);
        g.drawFittedText (status, statusArea, juce::Justification::topLeft, 5);
    }
}

//==============================================================================
void BrowserPanel::select (int index)
{
    selected = juce::isPositiveAndBelow (index, numSlots (proc)) ? index : -1;
    updateButtons();
    repaint();
}

void BrowserPanel::updateButtons()
{
    const bool has = selected >= 0;
    const bool present = has && slotPresent (proc, selected);
    const bool factory = has && slotIsFactory (proc, selected);
    const int loaded = loadedIndex (proc);

    auditionButton.setEnabled (present);

    // The only place the write-enable is decided. FACTORY never enables.
    const bool canCopy = has && ! factory && loaded >= 0 && loaded != selected;
    copyButton.setEnabled (canCopy);
    if (factory)
        copyButton.setTooltip ("FACTORY slots cannot be written. Choose a USER slot.");
    else if (loaded < 0)
        copyButton.setTooltip ("Audition a preset first: the loaded preset is what gets copied here.");
    else
        copyButton.setTooltip ("Write " + slotName (proc, loaded) + " into this USER slot (FF=01), "
                               "then read it back to verify.");

    exportButton.setEnabled (bank (proc).countPresent() > 0);
}

void BrowserPanel::setStatus (const juce::String& s, bool isError)
{
    status = s;
    statusIsError = isError;
    repaint (detailArea);
}

void BrowserPanel::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &presetChanged (proc))
        rebuildPrints();
    updateButtons();
    repaint();
}

//==============================================================================
void BrowserPanel::mouseMove (const juce::MouseEvent& e)
{
    const int h = tileAt (e.getPosition());
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 && slotPresent (proc, h) ? juce::MouseCursor::PointingHandCursor
                                                        : juce::MouseCursor::NormalCursor);
        repaint (gridArea);
    }
}

void BrowserPanel::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint (gridArea);
}

void BrowserPanel::mouseDown (const juce::MouseEvent& e)
{
    const int t = tileAt (e.getPosition());
    if (t >= 0)
    {
        grabKeyboardFocus();
        select (t);
    }
}

void BrowserPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int t = tileAt (e.getPosition());
    if (t >= 0 && slotPresent (proc, t))
    {
        select (t);
        doAudition();
    }
}

bool BrowserPanel::keyPressed (const juce::KeyPress& key)
{
    int local = selected >= 0 && selected / kPerPage == page ? selected % kPerPage : -1;
    int col = local >= 0 ? local / kPrograms : 0;
    int row = local >= 0 ? local % kPrograms : 0;

    if (key.isKeyCode (juce::KeyPress::returnKey)) { doAudition(); return true; }
    if (key.isKeyCode (juce::KeyPress::leftKey))  col = juce::jmax (0, col - 1);
    else if (key.isKeyCode (juce::KeyPress::rightKey)) col = juce::jmin (kBanks - 1, col + 1);
    else if (key.isKeyCode (juce::KeyPress::upKey))    row = juce::jmax (0, row - 1);
    else if (key.isKeyCode (juce::KeyPress::downKey))  row = juce::jmin (kPrograms - 1, row + 1);
    else return false;

    select (indexFor (col, row));
    return true;
}

//==============================================================================
void BrowserPanel::doRequestDump()
{
    setStatus ("Requesting all 98 presets from the Nymphes...");
    requestFullDump (proc);
}

void BrowserPanel::doAudition()
{
    if (selected < 0)
        return;
    juce::String error;
    if (audition (proc, selected, error))
        setStatus ("Auditioning " + slotName (proc, selected) + ". Nothing was written to the synth's memory.");
    else
        setStatus (error, true);
    updateButtons();
    repaint();
}

void BrowserPanel::doCopyLoaded()
{
    const int loaded = loadedIndex (proc);
    if (selected < 0 || loaded < 0)
        return;

    const auto target = slotName (proc, selected);
    const auto source = slotName (proc, loaded);
    const bool overwriting = slotPresent (proc, selected);

    // A user slot is the player's work. Overwriting one is asked, every time.
    juce::Component::SafePointer<BrowserPanel> safe (this);
    const int index = selected;
    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Write " + target + "?",
        overwriting ? "This replaces the preset in " + target + " on the synth with " + source + ".\n"
                      "The old contents are kept until the read-back verifies the write."
                    : "This writes " + source + " into " + target + " on the synth.",
        "Write", "Cancel", this,
        juce::ModalCallbackFunction::create ([safe, index, target] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            juce::String error;
            if (saveLoadedTo (safe->proc, index, error))
                safe->setStatus ("Wrote " + target + ". Reading it back to verify...");
            else
                safe->setStatus ("Write refused: " + error, true);
            safe->updateButtons();
            safe->repaint();
        }));
}

void BrowserPanel::doImport()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Import a Nymphes .syx dump",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.syx");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File())
                return;
            juce::String error;
            if (importSyxFile (proc, f, error))
                setStatus ("Imported " + f.getFileName() + " into the browser.");
            else
                setStatus ("Import failed: " + error, true);
            updateButtons();
            repaint();
        });
}

void BrowserPanel::doExport()
{
    // The processor exports the whole bank (both pages), so the name says so.
    const auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("nymphes_bank.syx");

    chooser = std::make_unique<juce::FileChooser> ("Export a Nymphes .syx dump", suggested, "*.syx");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File())
                return;
            if (! f.hasFileExtension ("syx"))
                f = f.withFileExtension ("syx");
            juce::String error;
            if (exportSyxFile (proc, f, error))
                setStatus ("Exported to " + f.getFullPathName() + ".");
            else
                setStatus ("Export failed: " + error, true);
        });
}

} // namespace nymphes::ui
