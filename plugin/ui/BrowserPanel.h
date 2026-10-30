// plugin/ui/BrowserPanel.h  —  the presets view, shaped like PRESET CONTROL
//
// The Nymphes addresses a preset as page (USER / FACTORY) x bank (A..G) x
// program (1..7): 49 slots a page. The panel prints that as a 7x7 legend round
// the PRESET CONTROL selector, so the browser is that grid, not a 98-row table.
//
//   [ USER | FACTORY ]                    SELECTED
//      A   B   C   D   E   F   G          USER B3      (7-segment readout)
//   1  ■   ■   ■   ·   ■   ■   ■          summary line
//   2  ...                                [ AUDITION ]
//                                          [ COPY LOADED HERE ]
//                                          [ DUMP ] [ IMPORT ] [ EXPORT ]
//                                          status
//
// A tile shows whether the slot holds a preset the bank has actually seen. An
// empty tile is drawn empty and does nothing: the bank never invents a preset.
// Click selects, double-click (or Return) auditions, arrows move.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "plugin/ui/OptionList.h"
#include "plugin/ui/ProcessorAccess.h"

namespace nymphes::ui
{

class BrowserPanel final : public juce::Component,
                           private juce::ChangeListener
{
public:
    explicit BrowserPanel (Processor&);
    ~BrowserPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    int  indexFor (int bankCol, int programRow) const noexcept;   // into the 98-slot bank
    juce::Rectangle<int> tileBounds (int bankCol, int programRow) const;
    int  tileAt (juce::Point<int>) const;                          // bank index or -1
    void select (int index);
    void updateButtons();
    void setStatus (const juce::String&, bool isError = false);

    void paintGrid (juce::Graphics&) const;
    void paintTile (juce::Graphics&, int bankCol, int programRow) const;
    void paintDetail (juce::Graphics&) const;

    void rebuildPrints();

    void doRequestDump();
    void doAudition();
    void doCopyLoaded();
    void doImport();
    void doExport();

    Processor& proc;

    OptionList pageSwitch;
    int page = 0;                 // 0 = USER, 1 = FACTORY
    int selected = -1;            // bank index
    int hovered = -1;

    juce::TextButton auditionButton { "AUDITION" };
    juce::TextButton copyButton     { "COPY LOADED HERE" };
    juce::TextButton dumpButton     { "REQUEST DUMP" };
    juce::TextButton importButton   { "IMPORT .SYX" };
    juce::TextButton exportButton   { "EXPORT .SYX" };

    juce::Rectangle<int> gridArea, promptArea, detailArea, readoutArea, summaryArea, statusArea;
    juce::String status;
    bool statusIsError = false;

    // A tile's fingerprint: the main voice parameters as tiny faders, so presets
    // are recognisable by shape before they are auditioned.
    static constexpr int kPrintBars = 11;
    struct Print
    {
        bool valid = false;
        std::array<float, kPrintBars> v {};
        juce::String mode;
    };
    std::array<Print, PresetBank::kSize> prints {};

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

} // namespace nymphes::ui
