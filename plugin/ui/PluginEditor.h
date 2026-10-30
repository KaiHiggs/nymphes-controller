// plugin/ui/PluginEditor.h  —  L2 (shell) / L1 (the panels it hosts)
//
// The editor's hierarchy, top to bottom:
//
//   header      56px  the Nymphes lockup, then the current preset on the right
//   nav         42px  PRESETS / CONTROLS, a segmented bar rather than stock tabs
//   content           BrowserPanel or ParamPanel, side margin kGapL
//   status rail 30px  device state, then port / connect / dump
//
// The editor owns no MIDI port, no preset codec and no CC state: everything goes
// through ProcessorAccess.h, so what the UI shows is what getStateInformation()
// saves. That contract is unchanged from the previous editor — this file only
// changes how it looks and how it is arranged.
//
// Two optional seams are wired with __has_include so the shell compiles whether
// or not a peer's file has landed yet, in any merge order:
//
//   plugin/ui/PresetBar.h          L1's preset identity bar, hosted in the header
//   plugin/ui/InteractionHelpers.h L4's hover/focus repaint hooks
//
// On integration both are present and both are used; neither is required to build.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "plugin/ui/BrowserPanel.h"
#include "plugin/ui/NymphesLookAndFeel.h"
#include "plugin/ui/ParamPanel.h"
#include "plugin/ui/ProcessorAccess.h"
#include "plugin/ui/Theme.h"
#include "plugin/ui/Widgets.h"

#if defined (__has_include)
#  if __has_include ("plugin/ui/PresetBar.h")
#    include "plugin/ui/PresetBar.h"
#    define NYMPHES_HAVE_PRESET_BAR 1
#  endif
#  if __has_include ("plugin/ui/InteractionHelpers.h")
#    include "plugin/ui/InteractionHelpers.h"
#    define NYMPHES_HAVE_INTERACTION_HELPERS 1
#  endif
#endif

namespace nymphes::ui
{

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::ChangeListener
{
public:
    explicit PluginEditor (Processor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Which view the nav is showing. Public so a host or a test can drive it. */
    enum class View { presets = 0, controls = 1 };
    void showView (View);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void paintHeader (juce::Graphics&, juce::Rectangle<int>) const;
    void paintPresetCluster (juce::Graphics&, juce::Rectangle<int>) const;
    /** Where the 7-segment PRESET CONTROL cluster sits. Always present: it is the
        instrument's identity element, so a peer's PresetBar hosts BESIDE it, never
        instead of it. */
    juce::Rectangle<int> presetClusterBounds() const;
    /** The rect L1's PresetBar occupies when it exists: left of the cluster. */
    juce::Rectangle<int> presetBarBounds() const;
    juce::String currentPresetLabel() const;
    /** "UA1" / "FA5": the page letter, bank letter and program digit that the
        seven-segment readout shows. */
    juce::String currentPresetSegments() const;

    Processor& proc;

    NymphesLookAndFeel& laf = NymphesLookAndFeel::instance();

    SegmentedNav nav;
    StatusRail   rail;

    std::unique_ptr<BrowserPanel> browser;
    std::unique_ptr<ParamPanel>   params;

#if NYMPHES_HAVE_PRESET_BAR
    std::unique_ptr<PresetBar> presetBar;
#endif

    // Without a TooltipWindow component in the hierarchy, no tooltip is ever
    // shown no matter how many setTooltip() calls the panels make. One window,
    // parented to the editor, so it follows the plugin window and dies with it.
    std::unique_ptr<juce::TooltipWindow> tooltips;

    View view = View::presets;

    // Cached so paint() never allocates a std::string on the message thread.
    juce::String presetLabel;
    juce::String presetSegments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

} // namespace nymphes::ui
