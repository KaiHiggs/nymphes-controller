// plugin/ui/NymphesLookAndFeel.cpp  —  L2

#include "plugin/ui/NymphesLookAndFeel.h"

#include <cmath>

namespace nymphes::ui
{

using namespace theme;

namespace
{
    juce::String styleString (const juce::Button& b)
    {
        const auto v = b.getProperties()["style"];
        return v.isVoid() ? juce::String() : v.toString();
    }

    /** Optical thickness for an arc of a given radius: a 6px stroke looks right on
        a 60px knob and clumsy on a 24px one. */
    float arcThicknessFor (float radius) noexcept
    {
        return juce::jlimit (2.0f, 5.0f, radius * 0.145f);
    }

    void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, float thickness)
    {
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY());
        p.lineTo (r.getCentreX(), r.getBottom());
        p.lineTo (r.getRight(), r.getY());
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
}

//==============================================================================
NymphesLookAndFeel& NymphesLookAndFeel::instance()
{
    // Intentional process-lifetime leak: see the header. A LookAndFeel must
    // outlive every Component that has captured it, and Components can outlive
    // static destruction order.
    static NymphesLookAndFeel* const one = new NymphesLookAndFeel();
    return *one;
}

NymphesLookAndFeel::NymphesLookAndFeel()
{
    applyColourScheme();

    setDefaultSansSerifTypefaceName (juce::Font::getDefaultSansSerifFontName());

    // Per-component defaults. A panel may override any of these on the component
    // itself (e.g. the LFO 2 card sets its knobs' fill to kAccentPink); these are
    // the colours a control gets when nobody says otherwise.
    setColour (juce::Slider::rotarySliderFillColourId,    kPinkAccent);
    setColour (juce::Slider::rotarySliderOutlineColourId, kLilacLine);
    setColour (juce::Slider::trackColourId,               kPinkAccent);
    setColour (juce::Slider::backgroundColourId,          kWellBlue);
    setColour (juce::Slider::thumbColourId,               kFaderBlack);
    setColour (juce::Slider::textBoxTextColourId,         kText);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);

    setColour (juce::Label::textColourId,                 kText);
    setColour (juce::Label::backgroundColourId,           juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId,              juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId,          kRaisedBlue);
    setColour (juce::TextButton::buttonOnColourId,        kPinkAccent);
    setColour (juce::TextButton::textColourOffId,         kText);
    setColour (juce::TextButton::textColourOnId,          kOnPink);

    setColour (juce::ComboBox::backgroundColourId,        kRaisedBlue);
    setColour (juce::ComboBox::textColourId,              kText);
    setColour (juce::ComboBox::outlineColourId,           kLilacLine);
    setColour (juce::ComboBox::arrowColourId,             kTextDim);
    setColour (juce::ComboBox::buttonColourId,            juce::Colours::transparentBlack);
    setColour (juce::ComboBox::focusedOutlineColourId,    kFocusIce);

    setColour (juce::PopupMenu::backgroundColourId,            kPlateAlt);
    setColour (juce::PopupMenu::textColourId,                  kText);
    setColour (juce::PopupMenu::headerTextColourId,            kTextMute);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kSelected);
    setColour (juce::PopupMenu::highlightedTextColourId,       kText);

    setColour (juce::TextEditor::backgroundColourId,      kWellBlue);
    setColour (juce::TextEditor::textColourId,            kText);
    setColour (juce::TextEditor::outlineColourId,         kLilacLine);
    setColour (juce::TextEditor::focusedOutlineColourId,  kFocusRing);
    setColour (juce::TextEditor::highlightColourId,       kSelected);
    setColour (juce::TextEditor::highlightedTextColourId, kText);
    setColour (juce::CaretComponent::caretColourId,       kPinkAccent);

    setColour (juce::ListBox::backgroundColourId,         kSurface);
    setColour (juce::ListBox::textColourId,               kText);
    setColour (juce::ListBox::outlineColourId,            kOutline);
    setColour (juce::TableHeaderComponent::backgroundColourId, kWellBlue);
    setColour (juce::TableHeaderComponent::textColourId,       kTextMute);
    setColour (juce::TableHeaderComponent::outlineColourId,    kOutline);
    setColour (juce::TableHeaderComponent::highlightColourId,  kRaisedBlue);

    setColour (juce::ScrollBar::backgroundColourId,       juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId,            kLilacLine);
    setColour (juce::ScrollBar::trackColourId,            juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId,   kPanelSurround);
    setColour (juce::TooltipWindow::textColourId,         kText);
    setColour (juce::TooltipWindow::outlineColourId,      kLilacBright);

    setColour (juce::GroupComponent::outlineColourId,     kOutline);
    setColour (juce::GroupComponent::textColourId,        kTextDim);

    setColour (juce::ToggleButton::textColourId,          kText);
    setColour (juce::ToggleButton::tickColourId,          kPinkAccent);
    setColour (juce::ToggleButton::tickDisabledColourId,  kTextMute);

    setColour (juce::ResizableWindow::backgroundColourId, kPanelBlue);
    setColour (juce::DocumentWindow::textColourId,        kText);
    setColour (juce::TabbedComponent::backgroundColourId, kPanelBlue);
    setColour (juce::TabbedComponent::outlineColourId,    kOutline);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, kOutline);
    setColour (juce::TabbedButtonBar::tabTextColourId,    kTextDim);
    setColour (juce::TabbedButtonBar::frontTextColourId,  kText);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, kPinkAccent);
    setColour (juce::AlertWindow::backgroundColourId,     kPlateAlt);
    setColour (juce::AlertWindow::textColourId,           kText);
    setColour (juce::AlertWindow::outlineColourId,        kOutlineStrong);
}

void NymphesLookAndFeel::applyColourScheme()
{
    // The hardware ramp: blue field, lilac line, cream lettering, pink for a fill.
    juce::LookAndFeel_V4::ColourScheme scheme {
        juce::Colour (kPanelBlueArgb),      // windowBackground
        juce::Colour (kPlateBlueArgb),      // widgetBackground
        juce::Colour (kPlateAltArgb),       // menuBackground
        juce::Colour (kLilacLineArgb),      // outline
        juce::Colour (kCreamLabelArgb),     // defaultText
        juce::Colour (kPinkAccentArgb),     // defaultFill
        juce::Colour (kCreamLabelArgb),     // highlightedText
        juce::Colour (kSelectedArgb),       // highlightedFill
        juce::Colour (kCreamLabelArgb)      // menuText
    };
    setColourScheme (scheme);
}

//==============================================================================
NymphesLookAndFeel::ButtonStyle NymphesLookAndFeel::styleOf (const juce::Button& b)
{
    const auto s = styleString (b);
    if (s == "primary") return ButtonStyle::primary;
    if (s == "ghost")   return ButtonStyle::ghost;
    if (s == "chip")    return ButtonStyle::chip;
    if (s == "danger")  return ButtonStyle::danger;
    return ButtonStyle::raised;
}

void NymphesLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                               const juce::Colour& backgroundColour,
                                               bool highlighted, bool down)
{
    const auto style = styleOf (b);
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool enabled = b.isEnabled();
    const int corner = style == ButtonStyle::chip ? (int) r.getHeight() / 2 : kRadiusS;
    const float radius = (float) corner;

    juce::Colour fill;
    juce::Colour line;
    float lineThickness = 1.0f;

    switch (style)
    {
        case ButtonStyle::primary:
            fill = backgroundColour.isTransparent() ? kPinkAccent : backgroundColour;
            if (down)          fill = darken (fill, 0.22f);
            else if (highlighted) fill = brighten (fill, 0.16f);
            break;

        case ButtonStyle::ghost:
            fill = down ? kRaised : (highlighted ? withAlpha (kRaised, 0.65f)
                                                 : juce::Colours::transparentBlack);
            line = highlighted ? kOutlineStrong : kOutline;
            break;

        case ButtonStyle::chip:
            fill = b.getToggleState() ? withAlpha (kPinkAccent, 0.22f)
                                     : (down || highlighted ? withAlpha (kRaisedBlue, 0.85f)
                                                            : juce::Colours::transparentBlack);
            line = b.getToggleState() ? kPinkAccent : kLilacLine;
            lineThickness = b.getToggleState() ? 1.4f : 1.0f;
            break;

        case ButtonStyle::danger:
            fill = kRed;
            if (down)          fill = darken (fill, 0.25f);
            else if (highlighted) fill = brighten (fill, 0.14f);
            break;

        case ButtonStyle::raised:
        default:
            fill = backgroundColour.isTransparent() ? kRaised : backgroundColour;
            if (down)          fill = darken (fill, 0.20f);
            else if (highlighted) fill = style == ButtonStyle::raised ? kRaisedHover : brighten (fill, 0.12f);
            line = highlighted ? kLilacBright : kLilacLine;
            break;
    }

    if (! enabled)
        fill = withAlpha (mix (fill, kBg, 0.55f), 0.55f);

    if (! fill.isTransparent())
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, radius);

        // a lit top edge on the neutral family only; a filled accent does not need it
        if (style == ButtonStyle::raised)
        {
            g.setColour (juce::Colours::white.withAlpha (enabled ? 0.06f : 0.02f));
            g.drawLine (r.getX() + radius, r.getY() + 1.0f,
                        r.getRight() - radius, r.getY() + 1.0f, 1.0f);
        }
    }

    if (! line.isTransparent())
    {
        g.setColour (enabled ? line : withAlpha (line, 0.28f));
        g.drawRoundedRectangle (r, radius, lineThickness);
    }

    drawFocusRing (g, r, corner, b.hasKeyboardFocus (true));
}

void NymphesLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                         bool, bool)
{
    const auto style = styleOf (b);
    const bool enabled = b.isEnabled();

    juce::Colour c = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId
                                                     : juce::TextButton::textColourOffId);
    if (style == ButtonStyle::primary)
        c = kOnPink;
    else if (style == ButtonStyle::danger)
        c = kPanelSurround;
    else if (style == ButtonStyle::ghost || style == ButtonStyle::chip)
        c = enabled ? kPinkText : kCreamMute;

    if (! enabled)
        c = withAlpha (c, 0.32f);

    g.setColour (c);
    g.setFont (getTextButtonFont (b, b.getHeight()));

    if (style == ButtonStyle::chip)
    {
        // chips are tracked micro-caps: they read as badges, not as prose
        drawTrackedText (g, b.getButtonText().toUpperCase(),
                         b.getLocalBounds().toFloat(), juce::Justification::centred, 0.8f);
        return;
    }

    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0),
                      juce::Justification::centred, 2, 0.9f);
}

juce::Font NymphesLookAndFeel::getTextButtonFont (juce::TextButton& b, int buttonHeight)
{
    if (styleOf (b) == ButtonStyle::chip)
        return fontMicro();
    return buttonHeight < 24 ? fontMicro() : fontText();
}

//==============================================================================
void NymphesLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                           bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    // A pill switch, not a tick box: on/off is a hardware gesture here.
    const float h = juce::jmin (18.0f, r.getHeight() - 4.0f);
    const float w = h * 1.9f;
    auto track = juce::Rectangle<float> (r.getX() + 1.0f, r.getCentreY() - h * 0.5f, w, h);

    g.setColour (on ? withAlpha (kPinkAccent, 0.30f) : kWellBlue);
    g.fillRoundedRectangle (track, h * 0.5f);
    g.setColour (on ? kPinkAccent : (highlighted ? kLilacBright : kLilacLine));
    g.drawRoundedRectangle (track.reduced (0.5f), h * 0.5f, 1.0f);

    auto knob = juce::Rectangle<float> (h - 4.0f, h - 4.0f)
                    .withCentre ({ on ? track.getRight() - h * 0.5f : track.getX() + h * 0.5f,
                                   track.getCentreY() });
    g.setColour (on ? kPinkAccent : kCreamMute);
    g.fillEllipse (knob);
    if (! on)
    {
        g.setColour (withAlpha (juce::Colours::white, 0.06f));
        g.fillEllipse (knob.reduced (1.0f));
    }

    auto textArea = r.withTrimmedLeft (w + kGapS);
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (b.isEnabled() ? kText : kTextMute);
        g.setFont (fontText());
        g.drawFittedText (b.getButtonText(), textArea.toNearestInt(),
                          juce::Justification::centredLeft, 1);
    }

    drawFocusRing (g, r, kRadiusS, b.hasKeyboardFocus (true));
}

void NymphesLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& c,
                                      float x, float y, float w, float h, bool ticked,
                                      bool isEnabled, bool highlighted, bool)
{
    auto box = juce::Rectangle<float> (w, h).withCentre ({ x + w * 0.5f, y + h * 0.5f })
                                                 .withSizeKeepingCentre (h * 0.78f, h * 0.78f);
    g.setColour (ticked ? kPinkAccent : kWellBlue);
    g.fillRoundedRectangle (box, kRadiusS * 0.75f);
    g.setColour (isEnabled ? (highlighted ? kOutlineStrong : kOutline) : withAlpha (kOutline, 0.5f));
    g.drawRoundedRectangle (box.reduced (0.5f), kRadiusS * 0.75f, 1.0f);

    if (ticked)
    {
        juce::Path p;
        p.startNewSubPath (box.getX() + box.getWidth() * 0.26f, box.getCentreY());
        p.lineTo (box.getCentreX() - box.getWidth() * 0.04f, box.getBottom() - box.getHeight() * 0.28f);
        p.lineTo (box.getRight() - box.getWidth() * 0.24f, box.getY() + box.getHeight() * 0.30f);
        g.setColour (kOnPink);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    drawFocusRing (g, box.expanded (2.0f), kRadiusS, c.hasKeyboardFocus (true));
}

void NymphesLookAndFeel::changeToggleButtonWidthToFitText (juce::ToggleButton& b)
{
    const float h = juce::jmin (18.0f, (float) b.getHeight() - 4.0f);
    const float textW = measureWidth (fontText(), b.getButtonText());
    b.setSize ((int) std::ceil (h * 1.9f + kGapS + textW + 6.0f), b.getHeight());
}

//==============================================================================
void NymphesLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width,
                                           int height, float pos, float startAngle,
                                           float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto area = bounds.withSizeKeepingCentre (size, size);

    const float radius = size * 0.5f;
    const float thickness = arcThicknessFor (radius);
    const float arcRadius = radius - thickness * 0.5f - 1.0f;
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();

    const bool enabled = s.isEnabled();
    const bool active = s.isMouseOverOrDragging() && enabled;
    const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);
    juce::ignoreUnused (s.findColour (juce::Slider::rotarySliderOutlineColourId));

    const float angle = startAngle + pos * (endAngle - startAngle);

    // 2. the band: the hardware prints a thick lilac band round the VOLUME knob,
    //    open at the bottom. The value is that band lit pink up to the pointer.
    const float band = juce::jmax (thickness, radius * 0.2f);
    const float bandR = radius - band * 0.5f - 1.0f;
    juce::Path track;
    track.addCentredArc (cx, cy, bandR, bandR, 0.0f, startAngle, endAngle, true);
    g.setColour (enabled ? (active ? kLilacBright : kLilacKnob) : withAlpha (kLilacKnob, 0.4f));
    g.strokePath (track, juce::PathStrokeType (band, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::butt));
    if (pos > 0.0001f)
    {
        juce::Path value;
        value.addCentredArc (cx, cy, bandR, bandR, 0.0f, startAngle, angle, true);
        g.setColour (enabled ? kPinkAccent : withAlpha (kPinkAccent, 0.4f));
        g.strokePath (value, juce::PathStrokeType (band * 0.42f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));
    }
    juce::ignoreUnused (fill);

    // 3. the knob: black, knurled at the rim, a brushed metal cap with a black
    //    pointer — the only chrome on the panel, so it is the only place it goes.
    const float knobR = bandR - band * 0.5f - 3.0f;
    const juce::Point<float> c (cx, cy);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c).translated (0.0f, 2.5f));
    g.setColour (kFaderBlack);
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    constexpr int ridges = 36;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges + angle;
        const juce::Point<float> o (cx + knobR * std::sin (a), cy - knobR * std::cos (a));
        const juce::Point<float> in (cx + knobR * 0.84f * std::sin (a), cy - knobR * 0.84f * std::cos (a));
        g.drawLine ({ in, o }, 1.2f);
    }

    const float capR = knobR * 0.66f;
    auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    juce::ColourGradient metal (juce::Colour (0xFFF2F3F6), cx - capR * 0.5f, cy - capR * 0.7f,
                                juce::Colour (0xFF7D828C), cx + capR * 0.6f, cy + capR * 0.9f, true);
    g.setGradientFill (metal);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (cap, 1.0f);

    const juce::Point<float> tip (cx + capR * 0.92f * std::sin (angle), cy - capR * 0.92f * std::cos (angle));
    const juce::Point<float> tail (cx + capR * 0.25f * std::sin (angle), cy - capR * 0.25f * std::cos (angle));
    g.setColour (kFaderBlack);
    g.drawLine ({ tail, tip }, 2.4f);

    drawFocusRing (g, area.reduced (1.0f).withSizeKeepingCentre (arcRadius * 2.0f + thickness,
                                                                 arcRadius * 2.0f + thickness),
                   (int) (arcRadius + thickness * 0.5f), s.hasKeyboardFocus (true));
}

int NymphesLookAndFeel::getSliderThumbRadius (juce::Slider& s)
{
    return juce::jlimit (6, 12, (int) (juce::jmin (s.getWidth(), s.getHeight()) * 0.12f));
}

void NymphesLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width,
                                           int height, float sliderPos, float minPos,
                                           float maxPos, juce::Slider::SliderStyle style,
                                           juce::Slider& s)
{
    juce::ignoreUnused (minPos);
    auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
    const bool enabled = s.isEnabled();
    const bool active = s.isMouseOverOrDragging() && enabled;
    juce::ignoreUnused (s.findColour (juce::Slider::trackColourId));

    const bool vertical = style == juce::Slider::LinearVertical
                       || style == juce::Slider::LinearBarVertical;

    if (! vertical)
    {
        const float trackH = 8.0f;
        auto track = r.withSizeKeepingCentre (r.getWidth(), trackH);
        g.setColour (withAlpha (kWellBlue, 0.9f));
        g.fillRoundedRectangle (track, trackH * 0.5f);

        auto filled = track.withWidth (juce::jlimit (0.0f, track.getWidth(),
                                                     sliderPos - track.getX()));
        g.setColour (withAlpha (kPinkAccent, enabled ? 0.85f : 0.4f));
        g.fillRoundedRectangle (filled, trackH * 0.5f);
        g.setColour (withAlpha (kLilacLine, 0.9f));
        g.drawRoundedRectangle (track.reduced (0.5f), trackH * 0.5f, 1.0f);

        auto cap = juce::Rectangle<float> (12.0f, 20.0f)
                       .withCentre ({ sliderPos, r.getCentreY() });
        g.setColour (kFaderBlack);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (withAlpha (kCreamLabel, active ? 0.6f : 0.4f));
        g.drawRoundedRectangle (cap.reduced (1.4f), 2.5f, 1.0f);
        drawFocusRing (g, cap.expanded (2.0f), kRadiusS, s.hasKeyboardFocus (true));
        return;
    }

    // Vertical fader, drawn the way the panel prints it: a black slot, a lilac
    // ladder running edge to edge so neighbouring faders share one rail, and a
    // pale cap with a black index line. In mod edit the cap is pink: the fader now
    // means a depth, and that must be visible on the control itself.
    const bool modEdit = (bool) s.getProperties()["modEdit"];
    const bool unknown = (bool) s.getProperties()["unknown"];

    const float capH = 22.0f;
    const float top = r.getY() + capH * 0.5f;
    const float bottom = r.getBottom() - capH * 0.5f;

    // ladder: 11 rungs, the middle one longer — the hardware's centre detent mark
    constexpr int rungs = 11;
    for (int i = 0; i < rungs; ++i)
    {
        const float yy = juce::jmap ((float) i, 0.0f, (float) (rungs - 1), top, bottom);
        const bool mid = i == rungs / 2;
        g.setColour (withAlpha (kLilacLine, enabled ? (mid ? 0.95f : 0.62f) : 0.25f));
        const float inset = mid ? 0.0f : r.getWidth() * 0.12f;
        g.fillRect (juce::Rectangle<float> (r.getX() + inset, yy - 0.6f, r.getWidth() - inset * 2.0f, 1.2f));
    }

    // slot
    const float slotW = juce::jlimit (6.0f, 10.0f, r.getWidth() * 0.2f);
    auto slot = juce::Rectangle<float> (slotW, r.getHeight() - 2.0f).withCentre (r.getCentre());
    g.setColour (kFaderBlack);
    g.fillRoundedRectangle (slot, slotW * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (slot.reduced (0.5f), slotW * 0.5f, 1.0f);

    // cap
    const float capW = juce::jlimit (14.0f, 22.0f, r.getWidth() * 0.46f);
    // JUCE reports sliderPos across the whole track; the cap travels between the
    // first and last rung, so rescale rather than clamp (clamping leaves dead ends).
    const float prop = r.getHeight() > 0.0f ? juce::jlimit (0.0f, 1.0f, (r.getBottom() - sliderPos) / r.getHeight()) : 0.0f;
    const float cy = bottom - prop * (bottom - top);
    auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ r.getCentreX(), cy });

    juce::Colour face = modEdit ? kPinkAccent : juce::Colour (0xFFD9DAE0);
    if (! enabled)      face = face.withMultipliedSaturation (0.3f).withAlpha (0.35f);
    else if (active)    face = face.brighter (0.12f);

    if (unknown && enabled)
    {
        // a depth the UI has never seen: a hollow pink cap — present, not asserted
        g.setColour (kFaderBlack);
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (kPinkAccent);
        g.drawRoundedRectangle (cap.reduced (0.7f), 2.5f, 1.4f);
        g.setColour (withAlpha (kPinkAccent, 0.55f));
        g.fillRect (cap.getX() + 4.0f, cy - 0.75f, capW - 8.0f, 1.5f);
        drawFocusRing (g, cap.expanded (2.0f), kRadiusS, s.hasKeyboardFocus (true));
        return;
    }

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.5f);
    g.setGradientFill (juce::ColourGradient (face.brighter (0.18f), cap.getX(), cap.getY(),
                                             face.darker (0.28f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (cap.reduced (0.5f), 2.5f, 1.0f);
    g.setColour (kFaderBlack);
    g.fillRect (cap.getX() + 2.0f, cy - 1.0f, capW - 4.0f, 2.0f);

    drawFocusRing (g, cap.expanded (2.0f), kRadiusS, s.hasKeyboardFocus (true));
}

juce::Label* NymphesLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V2::createSliderTextBox (s);
    l->setFont (fontValue());
    l->setColour (juce::Label::textColourId, kText);
    l->setColour (juce::Label::backgroundColourId, kWell);
    l->setColour (juce::Label::outlineColourId, kOutline);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

//==============================================================================
void NymphesLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                       bool isButtonDown, int, int, int, int,
                                       juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    const bool enabled = box.isEnabled();
    const bool active = box.isMouseOver() && enabled;

    juce::Colour fill = box.findColour (juce::ComboBox::backgroundColourId);
    if (isButtonDown)   fill = darken (fill, 0.18f);
    else if (active)    fill = brighten (fill, 0.10f);

    g.setColour (enabled ? fill : withAlpha (mix (fill, kBg, 0.5f), 0.6f));
    g.fillRoundedRectangle (r, (float) kRadiusS);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (r.getX() + kRadiusS, r.getY() + 1.0f, r.getRight() - kRadiusS, r.getY() + 1.0f, 1.0f);

    const bool focused = box.hasKeyboardFocus (true);
    g.setColour (focused ? kFocusRing : (active ? kOutlineStrong : kOutline));
    g.drawRoundedRectangle (r, (float) kRadiusS, focused ? 1.4f : 1.0f);

    // chevron, vertically centred in a square on the right
    const float chev = 7.0f;
    auto chevArea = juce::Rectangle<float> (chev, chev * 0.6f)
                        .withCentre ({ r.getRight() - kGapM, r.getCentreY() });
    g.setColour (enabled ? kTextDim : kTextMute);
    drawChevron (g, chevArea, 1.5f);
}

juce::Font NymphesLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return box.getHeight() < 22 ? fontMicro() : fontText();
}

void NymphesLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (kGapS, 1, box.getWidth() - kGapS - 20, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId,
                     box.isEnabled() ? box.findColour (juce::ComboBox::textColourId) : kTextMute);
}

//==============================================================================
juce::Font NymphesLookAndFeel::getLabelFont (juce::Label& l)
{
    if (l.getProperties().contains ("font"))
    {
        const auto name = l.getProperties()["font"].toString();
        if (name == "micro") return fontMicro();
        if (name == "value") return fontValue();
        if (name == "group") return fontGroup();
        if (name == "title") return fontTitle();
    }
    return fontText();
}

void NymphesLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                   juce::TextEditor& e)
{
    auto r = juce::Rectangle<int> (0, 0, width, height).toFloat();
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (r, (float) kRadiusS);
}

void NymphesLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                juce::TextEditor& e)
{
    auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    const bool focused = e.hasKeyboardFocus (true);
    g.setColour (focused ? kFocusRing
                        : (e.isMouseOver() ? kOutlineStrong : kOutline));
    g.drawRoundedRectangle (r, (float) kRadiusS, focused ? 1.4f : 1.0f);
}

//==============================================================================
void NymphesLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g,
                                        bool isMouseOver, bool isMouseDown)
{
    auto r = b.getLocalBounds().toFloat();
    const bool front = b.isFrontTab();

    if (front)
    {
        g.setColour (kSurfaceAlt);
        g.fillRoundedRectangle (r.reduced (0.0f, 2.0f), (float) kRadiusS);
        g.setColour (kPinkAccent);
        g.fillRoundedRectangle (r.removeFromBottom (2.0f), 1.0f);
    }
    else if (isMouseOver || isMouseDown)
    {
        g.setColour (withAlpha (kRaised, 0.5f));
        g.fillRoundedRectangle (r.reduced (0.0f, 2.0f), (float) kRadiusS);
    }

    g.setColour (front ? kText : kTextDim);
    g.setFont (fontText());
    drawTrackedText (g, b.getButtonText().toUpperCase(),
                     b.getLocalBounds().toFloat(),
                     juce::Justification::centred, 0.9f);
}

void NymphesLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&,
                                                        juce::Graphics& g)
{
    g.fillAll (kBg);
    auto r = g.getClipBounds().toFloat().removeFromBottom (1.0f);
    g.setColour (kOutline);
    g.fillRect (r);
}

int NymphesLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int tabDepth)
{
    juce::ignoreUnused (tabDepth);
    return juce::jmax (72, (int) measureWidth (fontText(), b.getButtonText().toUpperCase()) + 32);
}

//==============================================================================
void NymphesLookAndFeel::drawTableHeaderBackground (juce::Graphics& g,
                                                    juce::TableHeaderComponent& h)
{
    g.fillAll (kSurfaceAlt);
    auto r = h.getLocalBounds().toFloat();
    g.setColour (kOutline);
    g.fillRect (r.removeFromBottom (1.0f));
}

void NymphesLookAndFeel::drawTableHeaderColumn (juce::Graphics& g,
                                                juce::TableHeaderComponent&,
                                                const juce::String& columnName, int,
                                                int width, int height, bool isMouseOver,
                                                bool isMouseDown, int)
{
    auto r = juce::Rectangle<int> (0, 0, width, height);

    if (isMouseDown || isMouseOver)
    {
        g.setColour (withAlpha (kRaised, isMouseDown ? 0.7f : 0.4f));
        g.fillRect (r.reduced (2, 4));
    }

    g.setColour (kTextMute);
    g.setFont (fontMicro());
    drawTrackedText (g, columnName.toUpperCase(), r.reduced (kGapS, 0).toFloat(),
                     juce::Justification::centredLeft, 0.9f);

    g.setColour (withAlpha (kOutline, 0.8f));
    g.fillRect (r.removeFromRight (1).reduced (0, 6));
}

//==============================================================================
void NymphesLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (kSurfaceAlt);
    g.fillRoundedRectangle (r, (float) kRadiusS);
    g.setColour (kOutlineStrong);
    g.drawRoundedRectangle (r, (float) kRadiusS, 1.0f);
}

void NymphesLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool hasSubMenu,
                                            const juce::String& text,
                                            const juce::String& shortcutKeyText,
                                            const juce::Drawable* icon,
                                            const juce::Colour* textColour)
{
    auto r = area.toFloat().reduced (0.0f, 1.0f);

    if (isSeparator)
    {
        g.setColour (kOutline);
        g.fillRect (area.toFloat().reduced (kGapS, 0).withHeight (1.0f)
                        .withY (area.toFloat().getCentreY()));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (kSelection);
        g.fillRoundedRectangle (r.reduced (2.0f, 0.0f), (float) kRadiusS);
        g.setColour (kPinkAccent);
        g.fillRoundedRectangle (r.reduced (2.0f, 0.0f).withWidth (2.0f), 1.0f);
    }

    juce::Colour c = textColour != nullptr ? *textColour
                                          : (isActive ? kText : kTextMute);
    g.setColour (c);
    g.setFont (fontText());

    auto textArea = area.withTrimmedLeft (kGapM + 4).withTrimmedRight (kGapXL);
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (kTextMute);
        g.setFont (fontMicro());
        g.drawFittedText (shortcutKeyText, area.withTrimmedRight (kGapS),
                          juce::Justification::centredRight, 1);
    }

    if (isTicked)
    {
        g.setColour (kPinkAccent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f)
                           .withCentre ({ area.getX() + kGapM * 0.5f,
                                          area.toFloat().getCentreY() }));
    }

    if (hasSubMenu)
    {
        g.setColour (kTextDim);
        juce::Path p;
        const auto cx = area.getRight() - kGapM;
        const auto cy = area.toFloat().getCentreY();
        p.startNewSubPath (cx - 3.0f, cy - 4.0f);
        p.lineTo (cx + 2.0f, cy);
        p.lineTo (cx - 3.0f, cy + 4.0f);
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    juce::ignoreUnused (icon);
}

void NymphesLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                    int standardMenuItemHeight,
                                                    int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 60;
        idealHeight = 9;
        return;
    }

    const auto f = fontText();
    idealWidth = (int) std::ceil (measureWidth (f, text)) + kGapXL + kGapXL + 4;
    idealHeight = juce::jmax (26, standardMenuItemHeight > 0 ? standardMenuItemHeight : 26);
}

juce::Font NymphesLookAndFeel::getPopupMenuFont() { return fontText(); }

//==============================================================================
int NymphesLookAndFeel::getDefaultScrollbarWidth() { return 9; }

void NymphesLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&,
                                        int x, int y, int width, int height,
                                        bool isScrollbarVertical, int thumbStartPosition,
                                        int thumbSize, bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<int> thumb;
    if (isScrollbarVertical)
        thumb = { x + 2, thumbStartPosition, width - 4, thumbSize };
    else
        thumb = { thumbStartPosition, y + 2, thumbSize, height - 4 };

    if (thumb.getWidth() <= 0 || thumb.getHeight() <= 0)
        return;

    const float radius = (float) juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f;
    g.setColour (isMouseDown ? kPinkAccent
                            : (isMouseOver ? kOutlineStrong : withAlpha (kOutlineStrong, 0.7f)));
    g.fillRoundedRectangle (thumb.toFloat(), radius);
}

//==============================================================================
void NymphesLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                      int width, int height)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (kSurfaceAlt);
    g.fillRoundedRectangle (r, (float) kRadiusS);
    g.setColour (kOutlineStrong);
    g.drawRoundedRectangle (r, (float) kRadiusS, 1.0f);

    // a 2px accent edge, so a tooltip reads as ours and not as the OS
    g.setColour (kPinkAccent);
    g.fillRoundedRectangle (r.withWidth (2.0f), 1.0f);

    g.setColour (kText);
    g.setFont (fontText());
    g.drawFittedText (text, r.toNearestInt().reduced (kGapM + 2, kGapS),
                      juce::Justification::centredLeft, 6, 1.0f);
}

juce::Rectangle<int> NymphesLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                          juce::Point<int> screenPos,
                                                          juce::Rectangle<int> parentArea)
{
    const int maxW = 380;
    const auto f = fontText();
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.append (tipText, f, kText);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, (float) maxW);

    const int w = juce::jlimit (120, maxW,
                               (int) std::ceil (layout.getWidth()) + kGapM * 2 + 6);
    const int h = juce::jlimit (28, 260,
                               (int) std::ceil (layout.getHeight()) + kGapS * 2);

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12)
                                                                      : screenPos.x + 18,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)
                                                                      : screenPos.y + 6,
                                 w, h)
               .constrainedWithin (parentArea);
}

void NymphesLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g,
                                                   const juce::Path& path, juce::Image&)
{
    g.setColour (kSurfaceAlt);
    g.fillPath (path);
    g.setColour (kOutlineStrong);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

//==============================================================================
void NymphesLookAndFeel::drawGroupComponentOutline (juce::Graphics& g, int width, int height,
                                                    const juce::String& text,
                                                    const juce::Justification&,
                                                    juce::GroupComponent&)
{
    drawPlate (g, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), text);
}

} // namespace nymphes::ui
