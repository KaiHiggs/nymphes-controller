// plugin/ui/Palette.h  —  L2  ·  the design system's raw values
//
// This is the ONLY place in the controller where a colour or a metric is a
// literal. Everything else in plugin/ui/ includes this header (directly, or
// through Theme.h) and names a token.
//
// It is deliberately JUCE-FREE: nothing here includes a juce_* header, so
// tests/ui_palette_test.cpp can link against nymphes_core alone and assert the
// contrast ratios and the grid arithmetic without a GUI target.
//
// ---------------------------------------------------------------------------
// The visual system: the instrument, not a dark plugin
// ---------------------------------------------------------------------------
// The reference is the Dreadbox Nymphes front panel (prior/nymphes-reference.jpg).
// Reading it literally, the panel is FOUR colours, and almost no gradient:
//
//   * a saturated blue field — the whole chassis is one blue, #2E4B8A
//   * lilac/pink LINE WORK — every section is an outlined plate with a
//     pill-outlined title band; every fader sits on a lilac rail
//   * cream technical lettering — the parameter names under the faders
//   * black faders — the only black in the panel, which is why the eye lands on
//     them first. The control IS the black object.
//
// So this palette is built the same way, and the rules are:
//
//   1. The FIELD IS BLUE, not near-black. A blue-forward panel is what makes the
//      plugin recognisable across a room; a charcoal panel with blue accents is
//      what the first cut was, and it read as generic.
//   2. LINE WORK IS LILAC/PINK, and it is the primary structural device —
//      outlines, rails, section plates, the active underline. Not box-shadow,
//      not a lighter fill: a line.
//   3. TEXT IS CREAM, never grey. Grey text on a blue field is the single most
//      "developer harness" signal a UI can emit.
//   4. BLACK IS RESERVED FOR THE CONTROL ITSELF. A fader cap and the LCD inset
//      are the only near-black surfaces; that is what makes them pop off the
//      field the way the hardware does.
//   5. State colours (armed / learned / error) stay conventional amber, green
//      and red — they must not read as decoration on top of the line work.
//
// Contrast is measured, not eyeballed: tests/ui_palette_test.cpp. Cream on the
// blue field is 6.9:1, the dim cream 5.5:1, the muted 4.7:1, pink labels 5.2:1.
//
// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------
// The rhythm is a 4px grid (kUnit): every gap, radius and shell band is a
// multiple of 4, which is what makes unrelated panels line up. A few sizes carry
// a +2 optical correction and are therefore only even, not multiples of 4 —
// kCardHeaderH (32 + 2) is the only one, because a 32px plate title next to a
// 15px cap-height label reads as cramped. The test asserts both invariants
// separately so the exception cannot quietly spread.

#pragma once

#include <cmath>
#include <cstdint>

namespace nymphes::ui::theme
{

using Argb = std::uint32_t;

//--- the field ----------------------------------------------------------------
// Canonical names are the hardware ones (panel / plate / well / cream / lilac /
// pink). The k* names below them are aliases kept so panels written against the
// first cut still compile; use the hardware names in new code.

inline constexpr Argb kPanelBlueArgb     = 0xFF2A4890u;  // sampled: the chassis field
inline constexpr Argb kPanelSurroundArgb = 0xFF16224Au;  // sampled: the window surround
inline constexpr Argb kPlateBlueArgb     = 0xFF274279u;  // an outlined section plate body
inline constexpr Argb kPlateAltArgb      = 0xFF2F4E8Fu;  // sampled: alternating row, raised panel
inline constexpr Argb kRaisedBlueArgb    = 0xFF3557A0u;  // a button's body
inline constexpr Argb kRaisedHoverArgb_  = 0xFF4066B4u;  // ...hovered
inline constexpr Argb kWellBlueArgb      = 0xFF1B2E60u;  // sampled: fader well / inset
inline constexpr Argb kFaderBlackArgb    = 0xFF0E1013u;  // sampled: the fader cap
inline constexpr Argb kDisplayBgArgb     = 0xFF101A33u;  // the 7-segment preset display
inline constexpr Argb kDisplayInkArgb    = 0xFFE8A2C8u;  // its lit segments

//--- line work: lilac, and it is the primary structural device ---------------
inline constexpr Argb kLilacLineArgb     = 0xFFA396C0u;  // sampled: plate outline, rail
inline constexpr Argb kLilacBrightArgb   = 0xFFC6BBDBu;  // hover / active line
inline constexpr Argb kLilacKnobArgb     = 0xFFC39BD3u;  // sampled: the knob body

//--- cream lettering ---------------------------------------------------------
inline constexpr Argb kCreamLabelArgb = 0xFFEDE6D5u;  // sampled: technical labels
inline constexpr Argb kCreamDimArgb   = 0xFFDBD3C2u;  // secondary
inline constexpr Argb kCreamMuteArgb  = 0xFFCCC5B6u;  // captions, units
inline constexpr Argb kOnPinkArgb     = 0xFF16224Au;  // deep navy on a pink fill

//--- identity accents --------------------------------------------------------
inline constexpr Argb kPinkAccentArgb = 0xFFE8A2C8u;  // sampled: logo stripes, small text
inline constexpr Argb kPinkTextArgb   = 0xFFF5BEDDu;  // the same pink, lifted to pass AA as text
inline constexpr Argb kFocusIceArgb   = 0xFF9FC4FFu;  // keyboard focus: ice blue, never pink

//--- state --------------------------------------------------------------------
inline constexpr Argb kAmberArgb = 0xFFF0BE6Au;  // MIDI learn armed
inline constexpr Argb kGreenArgb = 0xFF7FD8AFu;  // learned / device connected
inline constexpr Argb kRedArgb   = 0xFFFFB3B8u;  // error / refused

//--- selection ----------------------------------------------------------------
inline constexpr Argb kSelectedArgb = 0xFF3B5FA8u;  // a selected list row / segment

//--- aliases: first-cut names, kept so existing panels compile ----------------
inline constexpr Argb kBgArgb             = kPanelBlueArgb;
inline constexpr Argb kBgDeepArgb         = kPanelSurroundArgb;
inline constexpr Argb kChassisArgb        = kPanelBlueArgb;
inline constexpr Argb kChassisDeepArgb    = kPanelSurroundArgb;
inline constexpr Argb kSurfaceArgb        = kPlateBlueArgb;
inline constexpr Argb kSurfaceAltArgb     = kPlateAltArgb;
inline constexpr Argb kRaisedArgb          = kRaisedBlueArgb;
inline constexpr Argb kRaisedHoverArgb     = kRaisedHoverArgb_;
inline constexpr Argb kWellArgb            = kWellBlueArgb;
inline constexpr Argb kOutlineArgb         = kLilacLineArgb;
inline constexpr Argb kOutlineStrongArgb   = kLilacBrightArgb;
inline constexpr Argb kTextArgb            = kCreamLabelArgb;
inline constexpr Argb kTextDimArgb         = kCreamDimArgb;
inline constexpr Argb kTextMuteArgb        = kCreamMuteArgb;
inline constexpr Argb kOnAccentArgb        = kOnPinkArgb;
inline constexpr Argb kAccentPinkArgb      = kPinkTextArgb;
inline constexpr Argb kAccentVioletArgb    = kLilacKnobArgb;
inline constexpr Argb kAccentBlueArgb      = kFocusIceArgb;
inline constexpr Argb kAccentBlueDimArgb   = 0xFF6E9BE8u;
inline constexpr Argb kAccentAmberArgb     = kAmberArgb;
inline constexpr Argb kAccentGreenArgb     = kGreenArgb;
inline constexpr Argb kAccentRedArgb       = kRedArgb;
inline constexpr Argb kArcTrackArgb        = kLilacLineArgb;
inline constexpr Argb kArcTrackHoverArgb   = kLilacBrightArgb;
inline constexpr Argb kFocusRingArgb       = kFocusIceArgb;
inline constexpr Argb kSelectionArgb       = kSelectedArgb;
inline constexpr Argb kFaderBodyArgb       = kFaderBlackArgb;
inline constexpr Argb kFaderEdgeArgb       = 0xFF000000u;

// The one non-token alpha in the system: a plate's top edge gets a 1px lift so
// the outline reads as a panel edge rather than a flat print.
inline constexpr float kCardTopHighlightAlpha = 0.05f;

//--- metrics: the 4px grid ----------------------------------------------------
inline constexpr int kUnit   = 4;
inline constexpr int kGapXS  = 4;    // 1 unit  — inside a control
inline constexpr int kGapS   = 8;    // 2 units — between controls
inline constexpr int kGapM   = 12;   // 3 units — between plates
inline constexpr int kGapL   = 16;   // 4 units — content margin
inline constexpr int kGapXL  = 24;   // 6 units — section separation

inline constexpr int kRadiusS    = 4;
inline constexpr int kRadiusM    = 8;
inline constexpr int kRadiusL    = 12;
inline constexpr int kRadiusPill = 999;   // any value >= height/2
inline constexpr int kHairline   = 1;

// The section plate: the hardware draws a rounded outline with a pill-outlined
// title band that interrupts the top edge. These are its dimensions.
inline constexpr int kPlateInset     = 8;    // outline inset from the plate's own bounds
inline constexpr int kPlateTitleH    = 20;   // pill height for the title band
inline constexpr int kCardPadding    = 12;   // inset of a plate's title row
inline constexpr int kCardHeaderH    = 34;   // title band row: 32 + 2px optical
inline constexpr int kControlMinWidth= 56;   // narrowest a fader cell may get
// Reference control cell. ParamPanel (L3) owns the actual grid and sizes columns
// from the longest label per card; these are the nominal fader cell so the
// fader-first ratio is a stated part of the system rather than an accident.
inline constexpr int kControlCellW   = 76;
inline constexpr int kControlCellH   = 140;  // 100px fader + name + value, fader-first

// Shell geometry. The editor subtracts these from its own bounds, so a panel
// never needs to know how tall the header is.
inline constexpr int kHeaderH      = 64;
inline constexpr int kNavH         = 44;
inline constexpr int kStatusRailH  = 32;
inline constexpr int kShellMinW    = 900;    // the brief's floor: 900x600
inline constexpr int kShellMinH    = 600;
inline constexpr int kShellDefaultW = 1280;
inline constexpr int kShellDefaultH = 800;
inline constexpr int kShellMaxW    = 1960;
inline constexpr int kShellMaxH    = 1180;

//--- typography scale (px) ----------------------------------------------------
// Multiply by theme::fontScale, which the LookAndFeel owns; panels must not set
// it. Only integer-ish sizes that survive a 1x display are chosen.
inline constexpr float kFontTitle  = 20.0f;
inline constexpr float kFontDisplay= 22.0f;   // the 7-segment readout
inline constexpr float kFontGroup  = 12.0f;
inline constexpr float kFontLabel  = 11.0f;
inline constexpr float kFontValue  = 12.5f;
inline constexpr float kFontText   = 12.5f;
inline constexpr float kFontMicro  = 9.5f;

//--- contrast ----------------------------------------------------------------
// WCAG 2.1 relative luminance and contrast ratio, on raw ARGB. Used by
// tests/ui_palette_test.cpp and by nothing in the plugin at runtime.

inline double channelToLinear (std::uint32_t c8) noexcept
{
    const double c = static_cast<double> (c8 & 0xFFu) / 255.0;
    return c <= 0.03928 ? c / 12.92
                        : std::pow ((c + 0.055) / 1.055, 2.4);
}

/** WCAG 2.1 relative luminance of an opaque 0xAARRGGBB colour. */
inline double relativeLuminance (Argb argb) noexcept
{
    const double r = channelToLinear ((argb >> 16) & 0xFFu);
    const double g = channelToLinear ((argb >> 8)  & 0xFFu);
    const double b = channelToLinear ( argb        & 0xFFu);
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/** WCAG 2.1 contrast ratio, 1.0 (identical) .. 21.0 (black on white). */
inline double contrastRatio (Argb a, Argb b) noexcept
{
    const double la = relativeLuminance (a);
    const double lb = relativeLuminance (b);
    const double hi = la > lb ? la : lb;
    const double lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

/** Alpha-composite `top` at `alpha` over an opaque `bottom`. */
inline Argb compositeOver (Argb top, Argb bottom, double alpha) noexcept
{
    auto mix = [alpha] (std::uint32_t t, std::uint32_t b)
    {
        const double v = static_cast<double> (t) * alpha
                       + static_cast<double> (b) * (1.0 - alpha);
        return static_cast<std::uint32_t> (v + 0.5) & 0xFFu;
    };
    const Argb r = mix ((top >> 16) & 0xFFu, (bottom >> 16) & 0xFFu);
    const Argb g = mix ((top >> 8)  & 0xFFu, (bottom >> 8)  & 0xFFu);
    const Argb b = mix ( top        & 0xFFu,  bottom        & 0xFFu);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

} // namespace nymphes::ui::theme
