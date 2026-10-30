// plugin/ui/ParamPanel.h  —  the controls view, drawn as the instrument
//
// Not a grid of 82 equal cards. The Dreadbox panel is fourteen faders in four
// bracketed sections, each fader with a second function behind SHIFT or the MENU.
// On screen there is room to show both layers, so each section shows its main
// bank and its SHIFT bank side by side, under the hardware's own short labels:
//
//   OSCILLATOR  WAVE LVL SUB NOISE LFO | SHIFT  PW GLD DTN CHORD EG
//   FILTER      CUT RES EG             | SHIFT  HPF TRACK LFO
//   REVERB      SIZE DEC FILT MIX
//   ENVELOPE    FILTER  A D S R        | VCA    A D S R
//   LFO 1 / 2   RATE WAVE | SHIFT DELAY FADE    TYPE, SYNC
//   MENU        VOLUME, WHEEL, MOD EDIT, MODE, LEGATO, SUSTAIN
//
// MOD EDIT is the hardware's own modulation workflow: choose a source and every
// fader sets that source's depth (ModEdit.h). That replaces a page of 36 depth
// faders nobody could map back to the panel.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "plugin/ui/FaderCell.h"
#include "plugin/ui/ModEdit.h"
#include "plugin/ui/OptionList.h"
#include "plugin/ui/ProcessorAccess.h"

namespace nymphes::ui
{

class ParamPanel final : public juce::Component
{
public:
    explicit ParamPanel (Processor&);
    ~ParamPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Kept for the shell's API. Every CC is on this one surface now. */
    void focusCc (int nymphesCc);

private:
    struct Bank
    {
        juce::String pill;                 // empty = the section's main bank
        std::vector<FaderCell*> cells;
        juce::Rectangle<int> pillBounds;
    };

    struct Section
    {
        juce::String title;
        std::vector<Bank> banks;
        std::vector<OptionList*> side;     // a narrow column of lists after the banks
        juce::Rectangle<int> bounds;
        int cellCount() const;
    };

    Section& addSection (juce::String title);
    Bank& addBank (Section&, juce::String pill, std::initializer_list<std::pair<int, const char*>> cells);
    OptionList& addCcList (Section&, int cc, juce::String title, juce::StringArray options,
                           OptionList::Layout = OptionList::Layout::vertical);

    int  fixedWidth (const Section&) const;
    void layoutRow (std::vector<Section*> row, juce::Rectangle<int> area, int cellW);
    void layoutSection (Section&, int cellW);
    void layoutMenu (juce::Rectangle<int> area);
    void paintSection (juce::Graphics&, const Section&) const;
    void paintSurface (juce::Graphics&);
    int  naturalWidth (int cellW) const;
    void layoutSurface (juce::Rectangle<int>);
    void setModSource (int source);

    /** Every control lives on this one surface. Below the width the panel needs
        for legible faders, the surface is laid out at that width and scaled down
        as a whole, like a hardware-style plug-in, instead of crushing labels. */
    struct Surface final : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    Processor& proc;
    ModEditState mod;
    Surface surface;

    juce::OwnedArray<FaderCell> cells;
    juce::OwnedArray<OptionList> lists;
    std::vector<std::unique_ptr<Section>> sections;

    Section* osc = nullptr;
    Section* filter = nullptr;
    Section* reverb = nullptr;
    Section* env = nullptr;
    Section* lfo1 = nullptr;
    Section* lfo2 = nullptr;
    Section* menu = nullptr;

    FaderCell* volume = nullptr;
    FaderCell* wheel = nullptr;
    OptionList* modeList = nullptr;
    OptionList* modList = nullptr;
    OptionList* legato = nullptr;
    OptionList* sustain = nullptr;
    juce::Rectangle<int> hintBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamPanel)
};

} // namespace nymphes::ui
