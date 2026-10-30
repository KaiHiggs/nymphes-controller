// plugin/ui/NymphesLookAndFeel.h  —  L2
//
// The controller's entire control language. Nothing in the UI is drawn by a
// stock JUCE LookAndFeel any more: rotary knobs, faders, combo boxes, buttons,
// pills, scrollbars, tooltips, popup menus, table headers and text editors are
// all drawn here.
//
// One process-wide instance. It is stateless (it holds no reference to any
// component), so two editor instances — two Live windows, or the standalone plus
// a plugin instance — can share it. The instance is intentionally never freed:
// a LookAndFeel outlives every Component that points at it, and destroying one
// during static teardown is a well-known JUCE crash. One object, process
// lifetime.
//
// Usage, in PluginEditor's constructor, BEFORE any child is created:
//
//     juce::LookAndFeel::setDefaultLookAndFeel (&NymphesLookAndFeel::instance());
//
// Children capture the default at construction, so the order matters.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/ui/Theme.h"

namespace nymphes::ui
{

class NymphesLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NymphesLookAndFeel();
    ~NymphesLookAndFeel() override = default;

    /** The one instance. Never freed; see the note above. */
    static NymphesLookAndFeel& instance();

    //--- buttons --------------------------------------------------------------
    // A TextButton carries its intent in a component property:
    //     button.getProperties().set ("style", "primary" | "ghost" | "chip" | "danger");
    // Anything else is drawn as the default raised button. This keeps the whole
    // button family inside one class instead of five TextButton subclasses.
    enum class ButtonStyle { raised, primary, ghost, chip, danger };

    static ButtonStyle styleOf (const juce::Button&);

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    void drawTickBox (juce::Graphics&, juce::Component&,
                      float x, float y, float w, float h, bool ticked, bool isEnabled,
                      bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;
    void changeToggleButtonWidthToFitText (juce::ToggleButton&) override;

    //--- sliders --------------------------------------------------------------
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    //--- combo boxes ----------------------------------------------------------
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    //--- labels, text ---------------------------------------------------------
    juce::Font getLabelFont (juce::Label&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height,
                                   juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height,
                                juce::TextEditor&) override;

    //--- tabs (the shell uses its own nav, kept correct for any stock tab) -----
    void drawTabButton (juce::TabBarButton&, juce::Graphics&,
                        bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;
    int  getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;

    //--- tables ---------------------------------------------------------------
    void drawTableHeaderBackground (juce::Graphics&, juce::TableHeaderComponent&) override;
    void drawTableHeaderColumn (juce::Graphics&, juce::TableHeaderComponent&,
                                const juce::String& columnName, int columnId,
                                int width, int height, bool isMouseOver,
                                bool isMouseDown, int columnFlags) override;

    //--- popup menus ----------------------------------------------------------
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;
    juce::Font getPopupMenuFont() override;

    //--- scrollbars -----------------------------------------------------------
    int  getDefaultScrollbarWidth() override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

    //--- tooltips -------------------------------------------------------------
    void drawTooltip (juce::Graphics&, const juce::String& text,
                      int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;

    //--- callouts -------------------------------------------------------------
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&,
                                   const juce::Path&, juce::Image&) override;

    //--- groups ---------------------------------------------------------------
    void drawGroupComponentOutline (juce::Graphics&, int width, int height,
                                    const juce::String& text,
                                    const juce::Justification&,
                                    juce::GroupComponent&) override;

private:
    void applyColourScheme();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NymphesLookAndFeel)
};

} // namespace nymphes::ui
