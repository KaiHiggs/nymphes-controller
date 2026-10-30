// plugin/ui/Theme.h  —  L2  ·  the design system, as JUCE types
//
// Every colour, font and metric the UI uses comes from here or from Palette.h.
// Nothing in plugin/ui/ may contain a literal colour again: if you need a new
// one, it goes in Palette.h and a name is added here.
//
// This header introduces no behaviour and owns no state. It is safe to include
// from any UI translation unit, and it is the only file that knows both the raw
// values (Palette.h) and juce::Colour.
//
// Two spellings are provided for the colour tokens, on purpose:
//
//   kSurface          the canonical name (k + PascalCase), used by new code
//   panel             a short alias, kept because L1 and L3 wrote against it
//
// Both refer to the same juce::Colour object; use the k* form in new code.

#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/ui/Palette.h"

namespace nymphes::ui::theme
{

//==============================================================================
// Type scale
//==============================================================================

/** Multiplies every font size. Owned by the LookAndFeel; panels must not set it.
    A future HiDPI- or accessibility-driven text-size preference changes this one
    number instead of 60 call sites. */
inline float fontScale = 1.0f;

namespace detail
{
    inline juce::Font sans (float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              size * fontScale, styleFlags));
    }

    inline juce::Font mono (float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              size * fontScale, styleFlags));
    }
}

/** Section hero. Used for a view title only — never for a control name. */
inline juce::Font fontTitle() { return detail::sans (kFontTitle, juce::Font::bold); }

/** The name of a group card, drawn uppercase + tracked. */
inline juce::Font fontGroup() { return detail::sans (kFontGroup, juce::Font::bold); }

/** A control's name under its knob. Sentence case, not uppercase: uppercase at
    11px destroys word shape and is the main reason dense synth UIs are hard to
    scan. */
inline juce::Font fontLabel() { return detail::sans (kFontLabel); }

/** A numeric readout. Monospaced so a value does not jitter while dragging —
    that jitter is why the old UI's "0 -> 33 -> 48" readouts felt unstable. */
inline juce::Font fontValue() { return detail::mono (kFontValue, juce::Font::bold); }

/** Body text: list rows, status text, descriptions. */
inline juce::Font fontText() { return detail::sans (kFontText); }

/** Units, badges, captions, table headers. */
inline juce::Font fontMicro() { return detail::sans (kFontMicro); }

/** The 7-segment preset readout. Monospaced so the digits do not shift as the
    slot changes, which is the whole point of a segmented display. */
inline juce::Font fontDisplay() { return detail::mono (kFontDisplay, juce::Font::bold); }

/** Escape hatch for one-off sizes; L1 and L3 asked for this signature. Prefer
    the named helpers above, which are what the type scale actually is. */
inline juce::Font font (float size, bool bold = false)
{
    return detail::sans (size, bold ? juce::Font::bold : juce::Font::plain);
}

//==============================================================================
// Colours
//==============================================================================

namespace detail
{
    inline juce::Colour of (Argb argb) { return juce::Colour (argb); }
}

//--- the field (canonical hardware names) -------------------------------------
inline const juce::Colour kPanelBlue     = detail::of (kPanelBlueArgb);
inline const juce::Colour kPanelSurround = detail::of (kPanelSurroundArgb);
inline const juce::Colour kPlateBlue     = detail::of (kPlateBlueArgb);
inline const juce::Colour kPlateAlt      = detail::of (kPlateAltArgb);
inline const juce::Colour kRaisedBlue    = detail::of (kRaisedBlueArgb);
inline const juce::Colour kWellBlue      = detail::of (kWellBlueArgb);
inline const juce::Colour kFaderBlack    = detail::of (kFaderBlackArgb);
inline const juce::Colour kDisplayBg     = detail::of (kDisplayBgArgb);
inline const juce::Colour kDisplayInk    = detail::of (kDisplayInkArgb);

//--- line work ----------------------------------------------------------------
inline const juce::Colour kLilacLine   = detail::of (kLilacLineArgb);
inline const juce::Colour kLilacBright = detail::of (kLilacBrightArgb);
inline const juce::Colour kLilacKnob   = detail::of (kLilacKnobArgb);

//--- cream lettering ---------------------------------------------------------
inline const juce::Colour kCreamLabel = detail::of (kCreamLabelArgb);
inline const juce::Colour kCreamDim   = detail::of (kCreamDimArgb);
inline const juce::Colour kCreamMute  = detail::of (kCreamMuteArgb);
inline const juce::Colour kOnPink     = detail::of (kOnPinkArgb);

//--- identity -----------------------------------------------------------------
inline const juce::Colour kPinkAccent = detail::of (kPinkAccentArgb);
inline const juce::Colour kPinkText   = detail::of (kPinkTextArgb);
inline const juce::Colour kFocusIce   = detail::of (kFocusIceArgb);

//--- state --------------------------------------------------------------------
inline const juce::Colour kAmber = detail::of (kAmberArgb);
inline const juce::Colour kGreen = detail::of (kGreenArgb);
inline const juce::Colour kRed   = detail::of (kRedArgb);

//--- selection ----------------------------------------------------------------
inline const juce::Colour kSelected = detail::of (kSelectedArgb);

//--- first-cut aliases (same objects) -----------------------------------------
inline const juce::Colour& kBgDeep      = kPanelSurround;
inline const juce::Colour& kBg          = kPanelBlue;
inline const juce::Colour& kSurface     = kPlateBlue;
inline const juce::Colour& kSurfaceAlt  = kPlateAlt;
inline const juce::Colour& kRaised      = kRaisedBlue;
inline const juce::Colour& kRaisedHover = detail::of (kRaisedHoverArgb_);
inline const juce::Colour& kWell        = kWellBlue;
inline const juce::Colour& kOutline       = kLilacLine;
inline const juce::Colour& kOutlineStrong = kLilacBright;
inline const juce::Colour& kText     = kCreamLabel;
inline const juce::Colour& kTextDim  = kCreamDim;
inline const juce::Colour& kTextMute = kCreamMute;
inline const juce::Colour& kOnAccent = kOnPink;
inline const juce::Colour& kAccentPink    = kPinkText;
inline const juce::Colour& kAccentViolet  = kLilacKnob;
inline const juce::Colour& kAccentBlue    = kFocusIce;
inline const juce::Colour& kAccentBlueDim = detail::of (0xFF6E9BE8u);
inline const juce::Colour& kAccentAmber = kAmber;
inline const juce::Colour& kAccentGreen = kGreen;
inline const juce::Colour& kAccentRed   = kRed;
inline const juce::Colour& kArcTrack      = kLilacLine;
inline const juce::Colour& kArcTrackHover = kLilacBright;
inline const juce::Colour& kFocusRing     = kFocusIce;
inline const juce::Colour& kSelection     = kSelected;

//--- short role aliases (what L1's PresetStyle.h maps to) ---------------------
inline const juce::Colour& bg          = kPanelBlue;
inline const juce::Colour& panel       = kPlateBlue;
inline const juce::Colour& panelAlt    = kPlateAlt;
inline const juce::Colour& textPrimary = kCreamLabel;
inline const juce::Colour& textMuted   = kCreamDim;
inline const juce::Colour& accent      = kPinkText;      // PINK, not blue
inline const juce::Colour& accent2     = kLilacKnob;     // lilac
inline const juce::Colour& success     = kGreen;
inline const juce::Colour& warn        = kAmber;
inline const juce::Colour& danger      = kRed;
inline const juce::Colour& border      = kLilacLine;     // LILAC
inline const juce::Colour& well        = kWellBlue;
inline const juce::Colour& lit         = kSelected;

/** Default corner radius for a control. */
inline constexpr int radius = kRadiusM;

//==============================================================================
// Drawing helpers
//==============================================================================

inline juce::Colour withAlpha (juce::Colour c, float a) { return c.withAlpha (a); }
inline juce::Colour brighten (juce::Colour c, float amount) { return c.brighter (amount); }
inline juce::Colour darken (juce::Colour c, float amount) { return c.darker (amount); }

/** Blend two colours in sRGB; alpha of `a` is ignored. */
inline juce::Colour mix (juce::Colour a, juce::Colour b, float t)
{
    const float u = 1.0f - t;
    return juce::Colour::fromFloatRGBA (a.getFloatRed() * u + b.getFloatRed() * t,
                                        a.getFloatGreen() * u + b.getFloatGreen() * t,
                                        a.getFloatBlue() * u + b.getFloatBlue() * t,
                                        1.0f);
}

/** A group card: body, hairline, and a lit top edge so elevation reads on a
    near-black screen without any shadow. `emphasised` is the hover/active state. */
inline void drawCard (juce::Graphics& g, juce::Rectangle<float> r,
                      bool emphasised = false,
                      juce::Colour body = kSurface)
{
    const auto rr = r.reduced (0.5f);
    g.setColour (body);
    g.fillRoundedRectangle (rr, (float) kRadiusM);

    // lit top edge, inset so it does not bleed past the rounded corners
    g.setColour (juce::Colours::white.withAlpha (kCardTopHighlightAlpha));
    g.drawLine (rr.getX() + kRadiusM * 0.6f, rr.getY() + 1.0f,
                rr.getRight() - kRadiusM * 0.6f, rr.getY() + 1.0f, 1.0f);

    g.setColour (emphasised ? kOutlineStrong : kOutline);
    g.drawRoundedRectangle (rr, (float) kRadiusM, 1.0f);
}

/** A sunken area inside a card: knob wells, list wells, readout strips. */
inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, int corner = kRadiusS)
{
    g.setColour (kWell);
    g.fillRoundedRectangle (r, (float) corner);
}

/** Keyboard focus. Drawn around, never instead of, the control's own outline. */
inline void drawFocusRing (juce::Graphics& g, juce::Rectangle<float> r,
                          int corner = kRadiusS, bool on = true)
{
    if (! on)
        return;
    g.setColour (withAlpha (kFocusRing, 0.85f));
    g.drawRoundedRectangle (r.reduced (0.5f), (float) corner, 1.6f);
}

/** Width of `text` in `f`. Uses a GlyphArrangement rather than the deprecated
    Font::getStringWidthFloat, so a build of this tree adds no new deprecation
    warnings. */
inline float measureWidth (const juce::Font& f, const juce::String& text)
{
    if (text.isEmpty())
        return 0.0f;
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, false).getWidth();
}

/** Advance width of one character. Not measureWidth: that is the ink box, and a
    space has no ink, so tracked text lost its word gaps ("MODEDIT", "UNIA"). */
inline float advanceWidth (const juce::Font& f, const juce::String& ch)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, ch, 0.0f, 0.0f);
    return ga.getNumGlyphs() > 0 ? ga.getGlyph (0).getRight() : 0.0f;
}

/** Width of `text` when every glyph is followed by `tracking` pixels. */
inline float trackedWidth (const juce::Font& f, const juce::String& text, float tracking)
{
    if (text.isEmpty())
        return 0.0f;
    float w = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        w += advanceWidth (f, text.substring (i, i + 1));
    return w + tracking * (float) (text.length() - 1);
}

/** Draw text with letter spacing. Used for group headers and the nav bar: on a
    dark panel, tracked uppercase at a small size is what makes a label read as
    a label rather than as data. */
inline void drawTrackedText (juce::Graphics& g, const juce::String& text,
                             juce::Rectangle<float> area, juce::Justification j,
                             float tracking = 1.1f)
{
    const auto f = g.getCurrentFont();
    const float w = trackedWidth (f, text, tracking);
    float x = area.getX();
    if (j.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - w * 0.5f;
    else if (j.testFlags (juce::Justification::right))
        x = area.getRight() - w;

    float cursor = x;
    for (int i = 0; i < text.length(); ++i)
    {
        const auto ch = text.substring (i, i + 1);
        const float cw = advanceWidth (f, ch);
        g.drawText (ch, juce::Rectangle<float> (cursor, area.getY(), cw + tracking + 1.0f,
                                                area.getHeight()),
                    juce::Justification::centredLeft, false);
        cursor += cw + tracking;
    }
}

//==============================================================================
// Hardware line work
//==============================================================================

/** A section plate. This is the Nymphes' signature device: a rounded lilac
    OUTLINE with a pill-outlined title band that interrupts the top edge, and
    nothing else — no fill change, no shadow. The panel is a print, so the
    structure has to be drawn as a line.

    `emphasised` brightens the outline (hover / active plate). */
inline void drawPlate (juce::Graphics& g, juce::Rectangle<float> r,
                       const juce::String& title,
                       bool emphasised = false,
                       juce::Colour body = kPlateBlue)
{
    const auto inset = (float) kPlateInset;
    auto outline = r.reduced (inset, inset * 0.5f).withTrimmedTop (inset * 0.5f);
    const float corner = (float) kRadiusM;

    // the plate body: only a touch darker than the field, so the outline does
    // the work. A stronger fill difference would fight the line.
    g.setColour (body);
    g.fillRoundedRectangle (outline, corner);

    // the field is a print: give the plate's top edge a 1px lift only
    g.setColour (juce::Colours::white.withAlpha (kCardTopHighlightAlpha));
    g.drawLine (outline.getX() + corner, outline.getY() + 1.0f,
                outline.getRight() - corner, outline.getY() + 1.0f, 1.0f);

    g.setColour (emphasised ? kLilacBright : kLilacLine);
    g.drawRoundedRectangle (outline.reduced (0.5f), corner, 1.2f);

    if (title.isEmpty())
        return;

    //--- the title band -------------------------------------------------------
    const auto titleFont = fontGroup();
    const float textW = measureWidth (titleFont, title.toUpperCase());
    const float bandW = textW + 22.0f;
    const float bandH = (float) kPlateTitleH;
    auto band = juce::Rectangle<float> (bandW, bandH)
                    .withCentre ({ outline.getX() + 26.0f + bandW * 0.5f, outline.getY() });

    // knock the outline out behind the band so the band reads as part of the edge
    g.setColour (body);
    g.fillRect (band.expanded (1.0f, 0.0f));

    g.setColour (kLilacLine);
    g.drawRoundedRectangle (band, bandH * 0.5f, 1.0f);

    g.setColour (kPinkText);
    g.setFont (titleFont);
    drawTrackedText (g, title.toUpperCase(), band, juce::Justification::centred, 1.4f);
}

/** The lilac fader rail with its ladder of ticks — the detail that makes the
    hardware panel look like a measuring instrument rather than a row of sticks.
    Ticks at or below the value are lit. */
inline void drawFaderRail (juce::Graphics& g, juce::Rectangle<float> track,
                           float valueProportional, bool hovered)
{
    const float w = track.getWidth();
    const float radius = w * 0.5f;

    g.setColour (kWellBlue);
    g.fillRoundedRectangle (track, radius);

    // ticks: one every 8px, so they scale with the rail instead of being fixed
    const int ticks = juce::jlimit (5, 24, (int) (track.getHeight() / 11.0f));
    const float left = track.getX() + 2.0f;
    const float right = track.getRight() - 2.0f;

    for (int i = 0; i <= ticks; ++i)
    {
        const float t = 1.0f - (float) i / (float) ticks;          // 1 at the top
        const float y = track.getY() + t * track.getHeight();
        const bool lit = valueProportional >= t - 1.0e-4f;
        g.setColour (lit ? (hovered ? juce::Colours::white : kLilacBright)
                         : withAlpha (kLilacLine, hovered ? 0.62f : 0.40f));
        g.drawLine (left, y, right, y, 1.0f);
    }

    g.setColour (withAlpha (kLilacLine, 0.9f));
    g.drawRoundedRectangle (track.reduced (0.5f), radius, 1.0f);
}

//==============================================================================
// Seven-segment display
//==============================================================================

/** The seven bars of a digit, as fractions of `r`. */
enum class Segment { top, topRight, bottomRight, bottom, bottomLeft, topLeft, middle };

inline void drawSegmentBar (juce::Graphics& g, juce::Rectangle<float> r,
                            Segment seg, float thickness)
{
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    const float t = thickness;
    const float hw = w - 2.0f * t;      // horizontal bar length
    const float vh = (h - 3.0f * t) * 0.5f;   // vertical bar length

    juce::Rectangle<float> bar;
    switch (seg)
    {
        case Segment::top:         bar = { x + t, y, hw, t }; break;
        case Segment::middle:      bar = { x + t, y + (h - t) * 0.5f, hw, t }; break;
        case Segment::bottom:      bar = { x + t, y + h - t, hw, t }; break;
        case Segment::topLeft:     bar = { x, y + t, t, vh }; break;
        case Segment::topRight:    bar = { x + w - t, y + t, t, vh }; break;
        case Segment::bottomLeft:  bar = { x, y + t + vh + t, t, vh }; break;
        case Segment::bottomRight: bar = { x + w - t, y + t + vh + t, t, vh }; break;
    }
    g.fillRoundedRectangle (bar, t * 0.5f);
}

/** Draw one character as a seven-segment glyph. Supports 0-9 and the letters a
    bank letter can be (A-G) plus 'U' and 'F' for the page. Anything unsupported
    draws an underscore. */
inline void drawSevenSegChar (juce::Graphics& g, juce::Rectangle<float> r,
                              juce::juce_wchar c, juce::Colour ink, float thickness = 0.0f)
{
    const float t = thickness > 0.0f ? thickness : juce::jmax (2.0f, r.getHeight() * 0.11f);

    // bit order: top, topRight, bottomRight, bottom, bottomLeft, topLeft, middle
    using enum7 = unsigned;
    enum7 mask = 0;

    const auto upper = juce::CharacterFunctions::toUpperCase (c);
    switch (upper)
    {
        case '0': mask = 0b0111111; break;
        case '1': mask = 0b0000110; break;
        case '2': mask = 0b1011011; break;
        case '3': mask = 0b1001111; break;
        case '4': mask = 0b1100110; break;
        case '5': mask = 0b1101101; break;
        case '6': mask = 0b1111101; break;
        case '7': mask = 0b0000111; break;
        case '8': mask = 0b1111111; break;
        case '9': mask = 0b1101111; break;
        case 'A': mask = 0b1110111; break;
        case 'B': mask = 0b1111100; break;   // renders as a lowercase b, as on real displays
        case 'C': mask = 0b0111001; break;
        case 'D': mask = 0b1011110; break;   // renders as a lowercase d
        case 'E': mask = 0b1111001; break;
        case 'F': mask = 0b1110001; break;
        case 'G': mask = 0b0111101; break;
        case 'U': mask = 0b0111110; break;
        case 'H': mask = 0b1110110; break;
        case '-': mask = 0b1000000; break;
        default:  mask = 0b0000001; break;   // underscore
    }

    struct Bit { Segment seg; enum7 bit; };
    static const Bit bits[] {
        { Segment::top,         0b0000001 },
        { Segment::topRight,    0b0000010 },
        { Segment::bottomRight, 0b0000100 },
        { Segment::bottom,      0b0001000 },
        { Segment::bottomLeft,  0b0010000 },
        { Segment::topLeft,     0b0100000 },
        { Segment::middle,      0b1000000 }
    };

    for (const auto& b : bits)
    {
        // unlit segments stay faintly visible, the way a real LCD looks
        g.setColour (b.bit & mask ? ink : withAlpha (ink, 0.10f));
        drawSegmentBar (g, r, b.seg, t);
    }
}

/** A segmented text run inside a dark inset — the preset readout. */
inline void drawSegmentText (juce::Graphics& g, juce::Rectangle<float> r,
                             const juce::String& text, juce::Colour ink = kDisplayInk)
{
    const int n = text.length();
    if (n <= 0)
        return;

    const float gap = r.getHeight() * 0.22f;
    const float glyphW = juce::jmin (r.getHeight() * 0.62f,
                                     (r.getWidth() - gap * (float) (n - 1)) / (float) n);
    const float totalW = glyphW * (float) n + gap * (float) (n - 1);
    float x = r.getCentreX() - totalW * 0.5f;

    for (int i = 0; i < n; ++i)
    {
        drawSevenSegChar (g, { x, r.getY(), glyphW, r.getHeight() }, text[i], ink);
        x += glyphW + gap;
    }
}

/** A pill: used for badges (MOD, LEARN, USER/FACTORY) and the status rail. */
inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r,
                      juce::Colour fill, juce::Colour line, float lineThickness = 1.0f)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (! line.isTransparent())
    {
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (lineThickness * 0.5f),
                                r.getHeight() * 0.5f, lineThickness);
    }
}

/** A 1px horizontal rule inside a card, aligned to the card padding. */
inline void drawRule (juce::Graphics& g, float x1, float x2, float y,
                      float alpha = 0.7f)
{
    g.setColour (withAlpha (kOutline, alpha));
    g.drawLine (x1, y, x2, y, 1.0f);
}

} // namespace nymphes::ui::theme
