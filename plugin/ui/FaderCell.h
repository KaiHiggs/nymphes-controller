// plugin/ui/FaderCell.h  —  one fader column of the panel
//
// A black fader on a lilac ladder, the hardware's short label underneath, the
// value above. When the panel is in mod edit, a fader that has a depth twin sets
// that depth for the selected source instead (see ModEdit.h); its cap turns pink
// and a depth the UI has never seen reads "—".
//
// Right-click: MIDI learn, forget the learned CC. Double-click: back to 0.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "plugin/cc/CcSpec.h"
#include "plugin/ui/ModEdit.h"
#include "plugin/ui/ProcessorAccess.h"

namespace nymphes::ui
{

class FaderCell final : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::ChangeListener
{
public:
    enum class Style { fader, knob };

    FaderCell (Processor&, ModEditState&, int baseCc, juce::String shortLabel, Style = Style::fader);
    ~FaderCell() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Re-read the engine (and the mod state) into the control. */
    void refresh();

    int  getBaseCc() const noexcept  { return baseCc; }
    int  getDepthCc() const noexcept { return depthCc; }

    /** The cell's label row is this tall; the panel aligns label baselines with it. */
    static constexpr int kLabelH = 18;
    static constexpr int kValueH = 15;

private:
    struct CellSlider final : juce::Slider
    {
        std::function<void()> onContextMenu;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void userMoved();
    void showContextMenu();
    bool editingDepth() const noexcept { return mod.active() && depthCc >= 0; }
    int  activeCc() const noexcept     { return editingDepth() ? depthCc : baseCc; }
    juce::String valueText() const;
    void updateTooltip();

    Processor& proc;
    ModEditState& mod;
    const int baseCc;
    const int depthCc;
    const ccui::CcSpec* spec;
    const juce::String shortLabel;
    const Style style;

    CellSlider slider;
    bool updating = false;
    int  lastSeenDepth = -1;   // engine value of depthCc at the last refresh

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderCell)
};

} // namespace nymphes::ui
