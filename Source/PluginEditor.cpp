#include "PluginEditor.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

//==============================================================================
namespace pal
{
const juce::Colour ironDark  { 0xff1d1815 };
const juce::Colour iron      { 0xff3a322c };
const juce::Colour rustDeep  { 0xff4a1d0c };
const juce::Colour rust      { 0xff9a4518 };
const juce::Colour rustLight { 0xffc0702a };
const juce::Colour paint     { 0xffd6c7a6 };   // dirty cream stencil paint
const juce::Colour paintDim  { 0xffa8997c };
const juce::Colour signal    { 0xffd2452a };   // rusty red for active things
const juce::Colour signalHot { 0xffff6a3d };
const juce::Colour hazard    { 0xffc99a22 };   // worn safety yellow
const juce::Colour screen    { 0xff0c0a08 };
}

//==============================================================================
namespace tex
{
inline float hash (int x, int y, int seed)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffffu) / 16777215.0f;
}
inline float smooth (float t) { return t * t * (3.0f - 2.0f * t); }
inline float smoothstep (float a, float b, float x) { return smooth (juce::jlimit (0.0f, 1.0f, (x - a) / (b - a))); }
inline float vnoise (float x, float y, int seed)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed), c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
    const float u = smooth (fx), v = smooth (fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm (float x, float y, int seed, int octaves)
{
    float s = 0, amp = 0.5f, f = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) { s += amp * vnoise (x * f, y * f, seed + i * 31); norm += amp; f *= 2.03f; amp *= 0.5f; }
    return s / norm;
}
struct RGB { float r, g, b; };
inline RGB rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }
inline RGB mix (RGB a, RGB b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
inline RGB mul (RGB a, float k) { return { a.r * k, a.g * k, a.b * k }; }

inline RGB rustAt (float x, float y, float rustBias, int seed)
{
    const float n      = fbm (x / 170.0f, y / 170.0f, seed, 4);
    const float detail = fbm (x / 36.0f, y / 36.0f, seed + 7, 3);
    const float fine   = vnoise (x / 7.0f, y / 7.0f, seed + 13);
    const float mask   = smoothstep (0.45f, 0.62f, n + (detail - 0.5f) * 0.35f + rustBias);
    RGB base = mix (rgb (pal::ironDark), rgb (pal::iron), detail);
    RGB r    = mix (rgb (pal::rustDeep), rgb (pal::rust), fbm (x / 20.0f, y / 20.0f, seed + 3, 2));
    if (fine > 0.72f) r = mix (r, rgb (pal::rustLight), (fine - 0.72f) * 2.5f);
    return mix (base, r, mask);
}

// Paint-chip a rendered image: knock out chunks of alpha with noise, plus grain.
inline void distress (juce::Image& img, float scale, float wear, int seed)
{
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
        for (int x = 0; x < bd.width; ++x)
        {
            auto c = bd.getPixelColour (x, y);
            if (c.getAlpha() == 0) continue;
            const float n = fbm ((float) x / (7.0f * scale), (float) y / (7.0f * scale), seed, 3);
            const float chip = n < wear ? 0.08f : 1.0f;
            const float grain = 0.7f + 0.3f * hash (x, y, seed + 99);
            bd.setPixelColour (x, y, c.withMultipliedAlpha (chip * grain));
        }
}

inline juce::Font stencil (float size) { return juce::Font (juce::FontOptions ("Impact", size, juce::Font::plain)); }
inline juce::Font plainBold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

inline juce::Image distressedText (const juce::String& text, int w, int h, juce::Font font, juce::Colour col,
                                   juce::Justification just, float scale, float wear, int seed, float kerning = 0.0f)
{
    juce::Image img (juce::Image::ARGB, juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setColour (col);
        g.setFont (font.withExtraKerningFactor (kerning));
        g.drawFittedText (text, 0, 0, w, h, just, 1, 0.55f);   // squeeze rather than clip if the font runs wide
    }
    distress (img, scale, wear, seed);
    return img;
}
} // namespace tex

//==============================================================================
// The Castle Danger crest: a castle silhouette under a blood moon, on a riveted iron medallion.
static void drawCastleCrest (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;

    // iron ring
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (r.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff1a1410), c.x + R, c.y + R, true));
    g.fillEllipse (r);
    g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (r.reduced (1.0f), 1.6f);
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i / 8.0f * juce::MathConstants<float>::twoPi + 0.39f;
        const float rx = c.x + std::cos (a) * R * 0.88f, ry = c.y + std::sin (a) * R * 0.88f;
        g.setColour (juce::Colour (0xff8a7a68)); g.fillEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f, 0.6f);
    }

    // night sky + blood moon
    const auto inner = r.reduced (R * 0.2f);
    const float ir = inner.getWidth() * 0.5f;
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addEllipse (inner);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0c08), c.x, inner.getY(), juce::Colour (0xff080505), c.x, inner.getBottom(), false));
    g.fillEllipse (inner);
    const juce::Rectangle<float> moon (c.x - ir * 0.55f, inner.getY() + ir * 0.12f, ir * 1.1f, ir * 1.1f);
    juce::DropShadow (pal::signalHot.withAlpha (0.55f), (int) (ir * 0.45f), {}).drawForPath (g, [&] { juce::Path p; p.addEllipse (moon); return p; }());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7a4a), moon.getX() + moon.getWidth() * 0.35f, moon.getY() + moon.getHeight() * 0.3f,
                                             juce::Colour (0xff8a1408), moon.getRight(), moon.getBottom(), true));
    g.fillEllipse (moon);
    g.setColour (juce::Colour (0x40400000));
    g.fillEllipse (moon.getX() + moon.getWidth() * 0.55f, moon.getY() + moon.getHeight() * 0.25f, moon.getWidth() * 0.16f, moon.getWidth() * 0.16f);
    g.fillEllipse (moon.getX() + moon.getWidth() * 0.25f, moon.getY() + moon.getHeight() * 0.55f, moon.getWidth() * 0.12f, moon.getWidth() * 0.12f);

    // castle silhouette
    const float x0 = inner.getX(), y0 = inner.getY(), w = inner.getWidth(), h = inner.getHeight();
    auto X = [&] (float f) { return x0 + f * w; };
    auto Y = [&] (float f) { return y0 + f * h; };
    juce::Path castle;
    auto tower = [&] (float l, float rgt, float top, int merlons)
    {
        castle.addRectangle (X (l), Y (top), X (rgt) - X (l), Y (1.05f) - Y (top));
        const float mw = (rgt - l) / (float) (merlons * 2 - 1);
        for (int i = 0; i < merlons; ++i)
            castle.addRectangle (X (l + (float) i * 2.0f * mw), Y (top - 0.06f), X (l + mw) - X (l), Y (0.06f) - Y (0.0f) + 0.5f);
    };
    tower (0.08f, 0.30f, 0.50f, 3);           // left tower
    tower (0.70f, 0.92f, 0.50f, 3);           // right tower
    tower (0.38f, 0.62f, 0.36f, 3);           // keep
    tower (0.30f, 0.70f, 0.64f, 5);           // curtain wall
    castle.addTriangle (X (0.08f), Y (0.44f), X (0.19f), Y (0.27f), X (0.30f), Y (0.44f));   // spires
    castle.addTriangle (X (0.70f), Y (0.44f), X (0.81f), Y (0.27f), X (0.92f), Y (0.44f));
    castle.setUsingNonZeroWinding (true);
    g.setColour (juce::Colour (0xff070505)); g.fillPath (castle);
    // gate + lit windows
    juce::Path gate;
    gate.addRectangle (X (0.44f), Y (0.84f), X (0.56f) - X (0.44f), Y (0.2f) - Y (0.0f));
    gate.addEllipse (X (0.44f), Y (0.79f), X (0.56f) - X (0.44f), Y (0.1f) - Y (0.0f));
    g.setColour (juce::Colour (0xff2a0c06)); g.fillPath (gate);
    g.setColour (pal::signalHot.withAlpha (0.9f));
    for (auto p : { juce::Point<float> (0.19f, 0.62f), { 0.81f, 0.62f }, { 0.5f, 0.5f } })
        g.fillRect (X (p.x) - w * 0.018f, Y (p.y), w * 0.036f, h * 0.07f);
}

//==============================================================================
ChopLookAndFeel::ChopLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, pal::paintDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::Label::textColourId, pal::paintDim);
    setColour (juce::ComboBox::textColourId, pal::paint);
    setColour (juce::ComboBox::arrowColourId, pal::signal);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::PopupMenu::textColourId, pal::paint);
    setColour (juce::PopupMenu::headerTextColourId, pal::rustLight);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::rust);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, pal::paint);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f0b09));
    setColour (juce::TextEditor::textColourId, pal::paint);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a3420));
    setColour (juce::TextEditor::focusedOutlineColourId, pal::rustLight);
    setColour (juce::TextEditor::highlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, pal::signalHot);

    rustDisc = juce::Image (juce::Image::ARGB, 128, 128, true);
    juce::Image::BitmapData bd (rustDisc, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const float n = tex::fbm ((float) x / 22.0f, (float) y / 22.0f, 501, 4);
            const float a = tex::smoothstep (0.5f, 0.7f, n);
            const auto c = tex::rustAt ((float) x * 2.0f, (float) y * 2.0f, 0.3f, 777);
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.r, c.g, c.b, a * 0.85f));
        }
}

juce::Font ChopLookAndFeel::getComboBoxFont (juce::ComboBox&) { return tex::plainBold (13.0f); }
juce::Font ChopLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return tex::stencil (juce::jmin (15.0f, (float) h * 0.62f)).withExtraKerningFactor (0.08f); }

void ChopLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (down ? pal::rustDeep : juce::Colour (0xff3a2a20), 0, r.getY(),
                                             down ? juce::Colour (0xff2a120a) : juce::Colour (0xff17110e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hi ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
}

void ChopLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0f0b09)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.isMouseOver (true) ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
    juce::Path arrow; const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (pal::signal); g.fillPath (arrow);
}

void ChopLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (2.5f, r * 0.11f);
    const float alpha = s.isEnabled() ? 1.0f : 0.3f;

    // tick marks stamped round the dial
    g.setColour (pal::paintDim.withAlpha (0.45f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
        const float r1 = r + 1.0f, r0 = r - (i % 5 == 0 ? 5.0f : 3.0f);
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 5 == 0 ? 1.4f : 0.9f);
    }

    const float ar = r - thick - 4.0f;
    juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff120e0c).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3a2418).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, angle, true);
    const auto arcCol = (s.isMouseOverOrDragging() ? pal::signalHot : pal::signal).withAlpha (alpha);
    g.setColour (arcCol.withAlpha (0.25f * alpha));
    g.strokePath (val, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = ar - thick * 1.4f;
    if (br < 4.0f) return;
    const juce::Rectangle<float> body (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (body.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c).withAlpha (alpha), c.x - br * 0.5f, c.y - br * 0.7f,
                                             juce::Colour (0xff17120f), c.x + br * 0.6f, c.y + br, true));
    g.fillEllipse (body);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        const float off = (float) (((juce::uint64) (juce::pointer_sized_int) &s >> 4) % 64);
        g.setOpacity (0.9f * alpha);
        g.drawImage (rustDisc, body.expanded (br * 0.4f).translated (-off * 0.3f, off * 0.2f), juce::RectanglePlacement::stretchToFit);
    }
    g.setColour (juce::Colour (0xff0f0b09).withAlpha (0.8f));
    for (int i = 0; i < 32; ++i)
    {
        const float a = (float) i / 32.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * br * 0.87f, c.y + std::sin (a) * br * 0.87f,
                    c.x + std::cos (a) * br, c.y + std::sin (a) * br, 1.0f);
    }
    g.setColour (juce::Colour (0xff0b0807)); g.drawEllipse (body, 1.2f);

    juce::Path p; p.addRoundedRectangle (-1.8f, -br * 0.92f, 3.6f, br * 0.6f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x + 0.8f, c.y + 1.0f));
    g.setColour (pal::paint.withAlpha (alpha));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

// toggles: an indicator lamp + stencilled label on a rusty plate
void ChopLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool warn = b.getName() == "scraps";
    const auto lit = warn ? pal::hazard : pal::signalHot;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff3a1a10) : juce::Colour (0xff2a221d), 0, r.getY(),
                                             juce::Colour (0xff120d0b), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? lit.withAlpha (0.8f) : (hi ? pal::rustLight : juce::Colour (0xff5a3420))); g.drawRoundedRectangle (r, 3.0f, 1.2f);

    const float d = r.getHeight() * 0.42f;
    auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
    if (on)
    {
        juce::Path lp; lp.addEllipse (led);
        juce::DropShadow (lit.withAlpha (0.9f), 10, {}).drawForPath (g, lp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c0), led.getCentreX(), led.getCentreY(), lit, led.getX(), led.getY(), true));
    }
    else g.setColour (juce::Colour (0xff2a1c16));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (led, 1.0f);

    g.setColour (on ? pal::paint : pal::paintDim);
    g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.1f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (r.getHeight() * 1.0f).withTrimmedRight (6), juce::Justification::centredLeft);
}

//==============================================================================
Selector::Selector (juce::RangedAudioParameter& p, juce::StringArray l, juce::StringArray s)
    : labels (std::move (l)), subs (std::move (s)),
      att (p, [this] (float v) { index = juce::roundToInt (v); repaint(); if (onChange) onChange(); })
{
    att.sendInitialUpdate();
}

int Selector::segmentAt (juce::Point<int> p) const
{
    if (labels.isEmpty()) return -1;
    return juce::jlimit (0, labels.size() - 1, p.x * labels.size() / juce::jmax (1, getWidth()));
}

void Selector::mouseDown (const juce::MouseEvent& e) { att.setValueAsCompleteGesture ((float) segmentAt (e.getPosition())); }
void Selector::mouseMove (const juce::MouseEvent& e) { const int h = segmentAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }

void Selector::paint (juce::Graphics& g)
{
    const int n = labels.size();
    const float sw = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto r = juce::Rectangle<float> ((float) i * sw, 0, sw, (float) getHeight()).reduced (3.0f, 2.0f);
        const bool on = i == index, hi = i == hover;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (1.5f, 2.0f), 3.0f);
        if (on)
        {
            juce::Path pth; pth.addRoundedRectangle (r, 3.0f);
            juce::DropShadow (pal::signalHot.withAlpha (0.45f), 12, {}).drawForPath (g, pth);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a2a14), r.getCentreX(), r.getY(), juce::Colour (0xff3a0e06), r.getCentreX(), r.getBottom(), false));
        }
        else
            g.setGradientFill (juce::ColourGradient (hi ? juce::Colour (0xff43332a) : juce::Colour (0xff332822), r.getCentreX(), r.getY(),
                                                     juce::Colour (0xff15100d), r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? pal::signalHot : (hi ? pal::rustLight : juce::Colour (0xff5a3420)));
        g.drawRoundedRectangle (r, 3.0f, on ? 1.6f : 1.1f);
        // little lamp in the corner
        auto lamp = juce::Rectangle<float> (6, 6).withCentre ({ r.getX() + 9, r.getY() + 9 });
        g.setColour (on ? juce::Colour (0xffffd0a0) : juce::Colour (0xff2a1c16)); g.fillEllipse (lamp);
        // grime
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (r.getRight() - 18, r.getBottom() - 3, r.getRight() - 4, r.getBottom() - 10, 1.0f);

        const bool hasSub = subs.size() > i && subs[i].isNotEmpty();
        auto tr = r.reduced (6, 2);
        g.setColour (on ? juce::Colour (0xfffff0d8) : pal::paint.withAlpha (0.85f));
        g.setFont (tex::stencil (hasSub ? 19.0f : 18.0f).withExtraKerningFactor (0.08f));
        g.drawFittedText (labels[i], (hasSub ? tr.removeFromTop (tr.getHeight() * 0.62f) : tr).toNearestInt(),
                          hasSub ? juce::Justification::centredBottom : juce::Justification::centred, 1, 0.7f);
        if (hasSub)
        {
            g.setColour (on ? pal::paint : pal::paintDim.withAlpha (0.7f));
            g.setFont (tex::plainBold (9.5f).withExtraKerningFactor (0.12f));
            g.drawFittedText (subs[i], tr.toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
        }
    }
}

//==============================================================================
void SawBlade::advance (float chopDb, float dt)
{
    heat = std::max (juce::jlimit (0.0f, 1.0f, chopDb / 9.0f), heat * 0.9f);
    angle += dt * (0.25f + heat * 9.0f);
    if (angle > juce::MathConstants<float>::twoPi) angle -= juce::MathConstants<float>::twoPi;
}

void SawBlade::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    const int teeth = 22;
    juce::Path blade;
    for (int i = 0; i < teeth; ++i)
    {
        const float a0 = (float) i / (float) teeth * juce::MathConstants<float>::twoPi;
        const float a1 = a0 + juce::MathConstants<float>::twoPi / (float) teeth * 0.72f;
        const float a2 = a0 + juce::MathConstants<float>::twoPi / (float) teeth;
        auto pt = [&] (float a, float rad) { return juce::Point<float> (std::cos (a) * rad, std::sin (a) * rad); };
        if (i == 0) blade.startNewSubPath (pt (a0, R * 0.80f));
        blade.lineTo (pt (a1, R));
        blade.lineTo (pt (a1 + 0.02f, R * 0.84f));
        blade.lineTo (pt (a2, R * 0.80f));
    }
    blade.closeSubPath();
    const auto xf = juce::AffineTransform::rotation (angle).translated (c);
    juce::Path b2 = blade; b2.applyTransform (xf);

    if (heat > 0.02f) juce::DropShadow (pal::signalHot.withAlpha (0.7f * heat), 14, {}).drawForPath (g, b2);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillPath (b2, juce::AffineTransform::translation (2.0f, 3.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a8c7c), c.x - R * 0.6f, c.y - R * 0.6f, juce::Colour (0xff2a221c), c.x + R, c.y + R, true));
    g.fillPath (b2);
    {
        // rust blotches that rotate with the blade
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (b2);
        juce::Random rnd (77);
        for (int i = 0; i < 16; ++i)
        {
            const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi + angle, d = rnd.nextFloat() * R * 0.85f, s = R * (0.12f + rnd.nextFloat() * 0.22f);
            g.setColour ((rnd.nextBool() ? pal::rust : pal::rustDeep).withAlpha (0.35f + rnd.nextFloat() * 0.4f));
            g.fillEllipse (c.x + std::cos (a) * d - s * 0.5f, c.y + std::sin (a) * d - s * 0.5f, s, s * (0.6f + rnd.nextFloat() * 0.6f));
        }
        // machining rings
        for (float k = 0.3f; k < 0.8f; k += 0.08f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f)); g.drawEllipse (c.x - R * k, c.y - R * k, R * k * 2, R * k * 2, 0.8f);
        }
        // heated tips
        if (heat > 0.02f)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c.x, c.y, pal::signalHot.withAlpha (0.85f * heat), c.x + R, c.y, true));
            g.fillEllipse (r);
        }
    }
    g.setColour (juce::Colour (0xff0c0807)); g.strokePath (b2, juce::PathStrokeType (1.0f));
    // expansion slots
    for (int i = 0; i < 4; ++i)
    {
        const float a = angle + (float) i * juce::MathConstants<float>::halfPi;
        juce::Path slot; slot.addRoundedRectangle (R * 0.42f, -1.4f, R * 0.26f, 2.8f, 1.4f);
        g.setColour (juce::Colour (0xff0a0706)); g.fillPath (slot, juce::AffineTransform::rotation (a).translated (c));
    }
    // hub + bolt
    const float hr = R * 0.28f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7a6a5a), c.x - hr * 0.5f, c.y - hr * 0.5f, juce::Colour (0xff1a1410), c.x + hr, c.y + hr, true));
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    g.setColour (juce::Colour (0xff0c0807)); g.drawEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2, 1.2f);
    juce::Path hex; hex.addPolygon ({}, 6, hr * 0.5f, angle);
    g.setColour (juce::Colour (0xff3a3028)); g.fillPath (hex, juce::AffineTransform::translation (c));
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (hex, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (c));
}

//==============================================================================
void CurveDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (pal::screen); g.fillRoundedRectangle (r, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x22ff6a3d), r.getCentreX(), r.getCentreY(), juce::Colours::transparentBlack, r.getX(), r.getY(), true));
    g.fillRoundedRectangle (r, 4.0f);

    auto plot = r.reduced (14.0f, 30.0f).withTrimmedBottom (-6.0f);
    const float xr = 2.0f, yr = 1.3f;
    auto toX = [&] (float u) { return plot.getCentreX() + u / xr * plot.getWidth() * 0.5f; };
    auto toY = [&] (float v) { return plot.getCentreY() - v / yr * plot.getHeight() * 0.5f; };

    // grid
    g.setColour (juce::Colour (0xff2a1d16));
    for (float u = -2.0f; u <= 2.01f; u += 0.5f) g.drawVerticalLine ((int) toX (u), plot.getY(), plot.getBottom());
    for (float v = -1.0f; v <= 1.01f; v += 0.5f) g.drawHorizontalLine ((int) toY (v), plot.getX(), plot.getRight());
    g.setColour (juce::Colour (0xff3f2a1e));
    g.drawVerticalLine ((int) toX (0), plot.getY(), plot.getBottom());
    g.drawHorizontalLine ((int) toY (0), plot.getX(), plot.getRight());
    // unity line
    {
        juce::Path unity; unity.startNewSubPath (toX (-1.3f), toY (-1.3f)); unity.lineTo (toX (1.3f), toY (1.3f));
        juce::Path dashed; const float dl[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dl, 2);
        g.setColour (pal::paintDim.withAlpha (0.25f)); g.fillPath (dashed);
    }
    // ceiling lines
    for (float v : { -1.0f, 1.0f })
    {
        juce::Path l; l.startNewSubPath (plot.getX(), toY (v)); l.lineTo (plot.getRight(), toY (v));
        juce::Path dashed; const float dl[] = { 6.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, l, dl, 2);
        g.setColour (pal::signal.withAlpha (0.55f)); g.fillPath (dashed);
    }
    g.setColour (pal::signal.withAlpha (0.8f)); g.setFont (tex::plainBold (9.0f));
    g.drawText ("CEILING", (int) plot.getRight() - 50, (int) toY (1.0f) - 13, 50, 12, juce::Justification::centredRight);

    // the curve
    const auto p = proc.currentParams();
    const float col = p.color;
    juce::Path curvePath;
    for (int i = 0; i <= 240; ++i)
    {
        const float u = -xr + 2.0f * xr * (float) i / 240.0f;
        const float v = chop::shapeSample (u, p.mode, p.shape, col);
        if (i == 0) curvePath.startNewSubPath (toX (u), toY (v)); else curvePath.lineTo (toX (u), toY (v));
    }
    g.setColour (pal::signalHot.withAlpha (0.18f)); g.strokePath (curvePath, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (pal::signalHot); g.strokePath (curvePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // live drive level
    if (level > 0.003f)
    {
        const float u = std::min (level, xr);
        const float v = chop::shapeSample (u, p.mode, p.shape, col);
        const bool chopping = level > 1.0f;
        const auto dotCol = chopping ? pal::hazard : pal::paint;
        g.setColour (dotCol.withAlpha (0.25f)); g.drawVerticalLine ((int) toX (u), toY (v), toY (0));
        juce::Path dot; dot.addEllipse (toX (u) - 4.5f, toY (v) - 4.5f, 9.0f, 9.0f);
        juce::DropShadow (dotCol.withAlpha (0.9f), 10, {}).drawForPath (g, dot);
        g.setColour (dotCol); g.fillPath (dot);
    }

    // labels
    static const char* modeNames[] = { "MASTER", "AGGRESSIVE" };
    g.setColour (pal::paint); g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.08f));
    juce::String head = juce::String (modeNames[juce::jlimit (0, 1, p.mode)]) + "  /  " + (p.shape == chop::hard ? "HARD" : "SOFT");
    if (p.color > 0.0f) head << "  +  " << juce::roundToInt (p.color * 100.0f) << "% COLOR";
    g.drawFittedText (head, r.reduced (12, 6).removeFromTop (18).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    g.setColour (pal::paintDim.withAlpha (0.7f)); g.setFont (tex::plainBold (9.0f).withExtraKerningFactor (0.1f));
    g.drawText ("INPUT  >", r.reduced (12, 6).removeFromBottom (12), juce::Justification::centredRight);
    g.drawText ("TRANSFER CURVE", r.reduced (12, 6).removeFromBottom (12), juce::Justification::centredLeft);

    // glass + bezel
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 4.0f, 2.0f);
    g.setColour (pal::rust.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (2.0f), 3.0f, 1.0f);
}

//==============================================================================
ChopShopEditor::ChopShopEditor (ChopShopProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      modeSel (*p.apvts.getParameter ("mode"), { "MASTER", "AGGRESSIVE" }, { "TRANSPARENT", "DRUMS & TRACKS" }),
      shapeSel (*p.apvts.getParameter ("shape"), { "HARD", "SOFT" }, { "BRICKWALL", "ROUNDED KNEE" }),
      osSel (*p.apvts.getParameter ("oversample"), { "OFF", "2x", "4x", "8x" }),
      curve (p)
{
    setLookAndFeel (&lnf);

    inputKnob = &makeKnob ("inputDb", "INPUT");
    ceilKnob  = &makeKnob ("ceilingDb", "CEILING");
    colorKnob = &makeKnob ("color", "COLOR");
    mixKnob   = &makeKnob ("mix", "MIX");
    outKnob   = &makeKnob ("outputDb", "OUTPUT");
    inputKnob->label.setFont (tex::stencil (20.0f).withExtraKerningFactor (0.12f));

    for (auto* s : { &modeSel, &shapeSel, &osSel }) addAndMakeVisible (*s);
    modeSel.setTooltip ("MASTER: transparent clipping for the mix bus.  AGGRESSIVE: crunchy, dense clipping for drums and tracks.  Add COLOR to either one.");
    modeSel.onChange = [this] { curve.repaint(); };
    shapeSel.onChange = [this] { curve.repaint(); };

    gainMatchButton.setName ("gainMatch");
    scrapsButton.setName ("scraps");
    gainMatchButton.setTooltip ("Turns the output down by as much as INPUT turns it up, so you hear the clipping, not just more volume.");
    scrapsButton.setTooltip ("Listen to only what the clipper chops off. Turn it off before you bounce!");
    for (auto* b : { &gainMatchButton, &scrapsButton }) addAndMakeVisible (*b);
    gainMatchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "gainMatch", gainMatchButton);
    scrapsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "scraps", scrapsButton);

    addAndMakeVisible (saw);
    addAndMakeVisible (curve);
    addAndMakeVisible (meters);
    meters.setInterceptsMouseClicks (false, false);
    saw.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (presetBox);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id >= kUserIdBase)
        {
            if (id - kUserIdBase < userFiles.size()) proc.loadUserPreset (userFiles[id - kUserIdBase]);
        }
        else if (id - 1 != proc.getCurrentProgram() || proc.getCurrentPresetName() != proc.getProgramName (id - 1))
            proc.setCurrentProgram (id - 1);
    };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton }) addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { promptSave(); };
    deleteButton.onClick = [this] { confirmDelete(); };

    refreshPresetBox();
    colorKnob->slider.setTooltip ("Adds harmonic saturation in either mode: 0 = none, 100 = full colour.");
    setSize (940, 580);
    startTimerHz (30);
}

ChopShopEditor::~ChopShopEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ChopShopEditor::Knob& ChopShopEditor::makeKnob (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.1f));
    k->label.setColour (juce::Label::textColourId, pal::paint);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k->slider.setColour (juce::Slider::textBoxTextColourId, pal::paint.withAlpha (0.85f));
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a2418));
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x90000000));
    k->slider.setMouseDragSensitivity (220);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    k->slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void ChopShopEditor::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    k.label.setBounds (cell.removeFromTop (20));
    k.slider.setBounds (cell);
}


void ChopShopEditor::resized()
{
    plates.clear();
    logoArea  = { 18, 12, 430, 92 };
    saw.setBounds (20, 14, 86, 86);
    badgeArea = { 460, 14, 252, 86 };
    crestArea = { 468, 19, 76, 76 };
    presetArea = { 724, 14, 198, 86 };
    presetBox.setBounds (724, 36, 198, 26);
    prevButton.setBounds (724, 70, 28, 24);
    nextButton.setBounds (756, 70, 28, 24);
    saveButton.setBounds (790, 70, 62, 24);
    deleteButton.setBounds (858, 70, 64, 24);
    stripeArea = { 0, 110, getWidth(), 10 };

    plates.push_back ({ "MODE", { 16, 130, 448, 86 }, true });
    modeSel.setBounds (26, 154, 428, 54);
    plates.push_back ({ "CLIP SHAPE", { 474, 130, 222, 86 } });
    shapeSel.setBounds (484, 154, 202, 54);
    plates.push_back ({ "OVERSAMPLING", { 706, 130, 218, 86 } });
    osSel.setBounds (716, 160, 198, 44);

    plates.push_back ({ "THE CURVE", { 16, 226, 300, 338 } });
    curve.setBounds (28, 252, 276, 300);
    plates.push_back ({ "CONTROLS", { 326, 226, 410, 338 } });
    placeKnob (*inputKnob, { 336, 262, 180, 250 });
    const int gx = 522, gw = 102, gh = 148;
    placeKnob (*ceilKnob,  { gx,      252, gw, gh });
    placeKnob (*colorKnob, { gx + gw, 252, gw, gh });
    placeKnob (*mixKnob,   { gx,      252 + gh + 8, gw, gh });
    placeKnob (*outKnob,   { gx + gw, 252 + gh + 8, gw, gh });
    plates.push_back ({ "METERS", { 746, 226, 178, 338 } });
    meterArea = { 758, 254, 154, 210 };
    meters.setBounds (meterArea);
    gainMatchButton.setBounds (758, 478, 154, 34);
    scrapsButton.setBounds (758, 520, 154, 34);

    buildBackdrop();
}

//==============================================================================
void ChopShopEditor::buildBackdrop()
{
    const float scale = 2.0f;
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    const int PW = (int) ((float) W * scale), PH = (int) ((float) H * scale);
    juce::Image base (juce::Image::RGB, W, H, false);

    std::vector<juce::Rectangle<float>> pr;
    for (auto& s : plates) pr.push_back (s.bounds.toFloat());
    pr.push_back (badgeArea.toFloat());
    const auto stripe = stripeArea.toFloat();

    {
        juce::Image::BitmapData bd (base, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < H; ++py)
        {
            const float y = (float) py;
            for (int px = 0; px < W; ++px)
            {
                const float x = (float) px;
                tex::RGB col;
                int inside = -1; float edge = 1e9f; bool shadow = false;
                for (int i = 0; i < (int) pr.size(); ++i)
                {
                    const auto& p = pr[(size_t) i];
                    if (p.contains (x, y))
                    {
                        inside = i;
                        edge = std::min ({ x - p.getX(), p.getRight() - x, y - p.getY(), p.getBottom() - y });
                        break;
                    }
                    if (p.translated (3.0f, 4.0f).contains (x, y)) shadow = true;
                }
                if (stripe.contains (x, y))
                {
                    // worn hazard stripes
                    const bool yellow = std::fmod (x + y * 1.0f + 1000.0f, 28.0f) < 14.0f;
                    const float wear = tex::fbm (x / 12.0f, y / 4.0f, 2024, 3);
                    col = yellow ? tex::rgb (pal::hazard) : tex::rgb (juce::Colour (0xff141010));
                    col = tex::mix (col, tex::rustAt (x, y, 0.3f, 99), tex::smoothstep (0.5f, 0.68f, wear));
                    col = tex::mul (col, 0.85f + 0.15f * tex::hash (px, py, 3));
                }
                else if (inside >= 0)
                {
                    // dark diamond-plate steel, rust creeping in from the edges
                    const float d = tex::fbm (x / 30.0f, y / 30.0f, 211, 3);
                    tex::RGB steel = tex::mix (tex::rgb (juce::Colour (0xff1e1a17)), tex::rgb (juce::Colour (0xff332c27)), d);
                    // diamond tread
                    const float u = std::fmod (x + y + 2000.0f, 16.0f) / 16.0f, v = std::fmod (x - y + 2000.0f, 16.0f) / 16.0f;
                    const float tread = (std::abs (u - 0.5f) < 0.08f && std::abs (v - 0.25f) < 0.18f) || (std::abs (v - 0.5f) < 0.08f && std::abs (u - 0.75f) < 0.18f) ? 1.0f : 0.0f;
                    steel = tex::mix (steel, tex::rgb (juce::Colour (0xff4a4038)), tread * 0.55f);
                    const float edgeRust = (1.0f - juce::jlimit (0.0f, 1.0f, edge / 14.0f)) * tex::smoothstep (0.35f, 0.6f, tex::vnoise (x / 6.0f, y / 6.0f, 17));
                    const float bloom = tex::smoothstep (0.6f, 0.75f, tex::fbm (x / 55.0f, y / 55.0f, 404, 3)) * 0.7f;
                    col = tex::mix (steel, tex::rustAt (x, y, 0.15f, 1337), juce::jlimit (0.0f, 1.0f, edgeRust + bloom));
                    col = tex::mul (col, 0.88f);
                }
                else
                {
                    col = tex::rustAt (x, y, 0.1f, 42);
                    if (shadow) col = tex::mul (col, 0.45f);
                }

                const float streak = tex::fbm (x / 9.0f, y / 240.0f, 909, 3);
                const float drip = tex::smoothstep (0.58f, 0.78f, streak) * (0.25f + 0.5f * y / (float) H);
                col = tex::mul (col, 1.0f - drip * 0.6f);
                const float runoff = tex::smoothstep (0.66f, 0.82f, tex::fbm (x / 5.0f, y / 120.0f, 31337, 2));
                col = tex::mix (col, tex::rgb (pal::rustDeep), runoff * 0.35f);
                // oil stains
                const float oil = tex::smoothstep (0.7f, 0.8f, tex::fbm (x / 70.0f, y / 50.0f, 555, 3));
                col = tex::mul (col, 1.0f - oil * 0.45f);
                if (tex::hash (px, py, 5) > 0.988f) col = tex::mul (col, 0.45f);
                const float grime = 0.72f + 0.28f * tex::fbm (x / 260.0f, y / 260.0f, 77, 2);
                const float dx = (x / (float) W - 0.5f) * 2.0f, dy = (y / (float) H - 0.5f) * 2.0f;
                const float vig = 1.0f - 0.45f * std::min (1.0f, (dx * dx + dy * dy) * 0.6f);
                col = tex::mul (col, grime * vig);
                bd.setPixelColour (px, py, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, col.r), juce::jlimit (0.0f, 1.0f, col.g),
                                                                        juce::jlimit (0.0f, 1.0f, col.b), 1.0f));
            }
        }
    }

    backdrop = juce::Image (juce::Image::RGB, PW, PH, false);
    juce::Graphics g (backdrop);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (base, 0, 0, PW, PH, 0, 0, W, H);
    g.addTransform (juce::AffineTransform::scale (scale));
    juce::Random rnd (1991);

    auto rivet = [&] (float cx, float cy, float rad)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - rad + 1, cy - rad + 1.5f, rad * 2, rad * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a68), cx - rad * 0.4f, cy - rad * 0.5f, juce::Colour (0xff2a201a), cx + rad, cy + rad, true));
        g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (cx - rad, cy - rad, rad * 2, rad * 2, 1.2f);
        g.setColour (pal::rustDeep.withAlpha (0.35f)); g.fillRect (cx - 0.8f, cy + rad, 1.6f, 3.0f + rnd.nextFloat() * 9.0f);
    };
    auto plateFrame = [&] (juce::Rectangle<float> p, bool hot)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawLine (p.getX() + 2, p.getY() + 1, p.getRight() - 2, p.getY() + 1, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (p, 1.0f);
        if (hot) { g.setColour (pal::signal.withAlpha (0.45f)); g.drawRect (p.reduced (2.0f), 1.5f); }
        rivet (p.getX() + 6, p.getY() + 6, 3.0f); rivet (p.getRight() - 6, p.getY() + 6, 3.0f);
        rivet (p.getX() + 6, p.getBottom() - 6, 3.0f); rivet (p.getRight() - 6, p.getBottom() - 6, 3.0f);
    };

    // scratches + weld spatter
    for (int i = 0; i < 300; ++i)
    {
        const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
        const float len = 6.0f + rnd.nextFloat() * 70.0f, ang = rnd.nextFloat() * juce::MathConstants<float>::pi;
        g.setColour ((rnd.nextBool() ? juce::Colour (0xffd0c0a8) : juce::Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.1f));
        g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.5f + rnd.nextFloat() * 0.6f);
    }

    int seed = 10;
    for (auto& s : plates)
    {
        const auto p = s.bounds.toFloat();
        plateFrame (p, s.hot);
        auto title = tex::distressedText (s.title, s.bounds.getWidth() - 40, 18, tex::stencil (15.0f), s.hot ? pal::signalHot : pal::paint,
                                          juce::Justification::centredLeft, scale, 0.2f, seed++, 0.12f);
        g.drawImage (title, juce::Rectangle<float> (p.getX() + 14, p.getY() + 3, (float) s.bounds.getWidth() - 40, 18.0f));
    }
    // knob plate: a stamped divider between INPUT and the small knobs
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawLine (518.0f, 252.0f, 518.0f, 552.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawLine (519.5f, 252.0f, 519.5f, 552.0f, 1.0f);
    // hint under the big knob
    {
        auto hint = tex::distressedText ("PUSH IT INTO THE CEILING", 180, 14, tex::plainBold (9.5f), pal::paint.withAlpha (0.75f), juce::Justification::centred, scale, 0.18f, 61, 0.14f);
        g.drawImage (hint, juce::Rectangle<float> (336.0f, 530.0f, 180.0f, 14.0f));
    }
    // meter screen well
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (meterArea.toFloat().expanded (4.0f), 4.0f);

    // hazard stripe edges
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (0, stripe.getY(), (float) W, stripe.getY(), 1.5f);
    g.drawLine (0, stripe.getBottom(), (float) W, stripe.getBottom(), 1.5f);

    // CASTLE DANGER MUSIC COMPANY badge with the castle crest
    const auto badge = badgeArea.toFloat();
    plateFrame (badge, false);
    g.setColour (pal::paint.withAlpha (0.22f)); g.drawRect (badge.reduced (5.0f), 1.0f);
    drawCastleCrest (g, crestArea.toFloat());
    const float tx = (float) crestArea.getRight() + 8.0f, tw = badge.getRight() - tx - 10.0f;
    auto l1 = tex::distressedText ("CASTLE DANGER", (int) tw, 28, tex::stencil (25.0f), juce::Colour (0xffeadfc4), juce::Justification::centredLeft, scale, 0.24f, 4242, 0.08f);
    auto l2 = tex::distressedText ("MUSIC COMPANY", (int) tw, 20, tex::stencil (17.0f), pal::rustLight, juce::Justification::centredLeft, scale, 0.24f, 4343, 0.2f);
    auto l3 = tex::distressedText ("WE MAKE MUSIC", (int) tw, 14, tex::plainBold (9.0f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 4444, 0.3f);
    g.setColour (juce::Colours::black);
    g.setOpacity (0.7f); g.drawImage (l1, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 13.0f, tw, 28.0f));
    g.setOpacity (1.0f); g.drawImage (l1, juce::Rectangle<float> (tx, badge.getY() + 11.0f, tw, 28.0f));
    g.setOpacity (0.7f); g.drawImage (l2, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 40.0f, tw, 20.0f));
    g.setOpacity (1.0f); g.drawImage (l2, juce::Rectangle<float> (tx, badge.getY() + 38.0f, tw, 20.0f));
    g.drawImage (l3, juce::Rectangle<float> (tx + 1.0f, badge.getY() + 60.0f, tw, 14.0f));

    // stamped subtitle under the logo
    auto sub = tex::distressedText ("HARD & SOFT CLIPPER  //  V" + juce::String (JucePlugin_VersionString),
                                    320, 14, tex::plainBold (10.5f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 99, 0.14f);
    g.drawImage (sub, juce::Rectangle<float> (118.0f, 80.0f, 320.0f, 14.0f));

    // PRESET label
    auto pl = tex::distressedText ("PRESET", 120, 16, tex::stencil (14.0f), pal::paint, juce::Justification::centredLeft, scale, 0.2f, 7, 0.15f);
    g.drawImage (pl, juce::Rectangle<float> ((float) presetArea.getX() + 2, (float) presetArea.getY() + 2, 120.0f, 16.0f));

    // the logo: CHOP in rusty red, SHOP in cream (drawn live so it can flicker)
    {
        const int lw = 330, lh = 66;
        logoImage = juce::Image (juce::Image::ARGB, (int) (lw * scale), (int) (lh * scale), true);
        juce::Graphics lg (logoImage);
        lg.addTransform (juce::AffineTransform::scale (scale));
        auto f = tex::stencil (62.0f).withExtraKerningFactor (0.06f);
        {
            juce::GlyphArrangement all; all.addLineOfText (f, "CHOP SHOP", 0, 0);
            const float wAll = all.getBoundingBox (0, -1, true).getWidth();
            if (wAll > (float) lw - 4.0f) f = f.withHeight (62.0f * ((float) lw - 4.0f) / wAll);
        }
        lg.setFont (f);
        lg.setColour (pal::signal);
        lg.drawText ("CHOP", 0, 0, lw, lh, juce::Justification::centredLeft, false);
        juce::GlyphArrangement ga; ga.addLineOfText (f, "CHOP ", 0, 0);
        const float shopX = ga.getBoundingBox (0, -1, true).getWidth();
        lg.setColour (pal::paint);
        lg.drawText ("SHOP", (int) shopX, 0, lw - (int) shopX, lh, juce::Justification::centredLeft, false);
        tex::distress (logoImage, scale, 0.3f, 666);
    }
}

void ChopShopEditor::paint (juce::Graphics& g)
{
    if (backdrop.isValid()) g.drawImage (backdrop, getLocalBounds().toFloat());
    else g.fillAll (pal::ironDark);

    if (logoImage.isValid())
    {
        const float alpha = juce::jlimit (0.45f, 1.0f, 1.0f - flicker * 0.6f);
        const auto la = juce::Rectangle<float> (116.0f, 12.0f, 330.0f, 66.0f);
        g.setColour (juce::Colours::black);
        g.setOpacity (0.65f); g.drawImage (logoImage, la.translated (2.5f, 3.5f));
        g.setOpacity (alpha); g.drawImage (logoImage, la);
        g.setOpacity (1.0f);
    }
}

void MeterPanel::paint (juce::Graphics& g)
{
    // meters: IN | CHOP | OUT
    auto m = getLocalBounds().toFloat();
    g.setColour (pal::screen); g.fillRoundedRectangle (m, 3.0f);
    auto bars = m.reduced (8.0f, 8.0f);
    auto labels = bars.removeFromBottom (30.0f);
    const float bw = 26.0f, gap = (bars.getWidth() - bw * 3.0f) / 2.0f;
    auto scaleX = bars.getX();
    juce::ignoreUnused (scaleX);

    auto drawLevel = [&] (juce::Rectangle<float> bar, float lvl, const juce::String& name, juce::Rectangle<float> lab)
    {
        g.setColour (juce::Colour (0xff171210)); g.fillRect (bar);
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto inner = bar.reduced (2.0f);
        const int segs = 24;
        const float sh = inner.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) (i + 1) / (float) segs;
            auto seg = juce::Rectangle<float> (inner.getX(), inner.getBottom() - (float) (i + 1) * sh + 1.0f, inner.getWidth(), sh - 1.5f);
            const auto c = t > 0.94f ? pal::signalHot : (t > 0.8f ? pal::hazard : pal::paint.darker (0.15f));
            g.setColour (t <= frac ? c : c.withAlpha (0.07f));
            g.fillRect (seg);
        }
        g.setColour (pal::paint); g.setFont (tex::stencil (13.0f));
        g.drawText (name, lab.removeFromTop (15), juce::Justification::centred);
        g.setColour (pal::paintDim); g.setFont (tex::plainBold (9.5f));
        g.drawText (lvl > 0.00002f ? juce::String (db, 1) : juce::String ("-inf"), lab, juce::Justification::centred);
    };

    auto barIn = bars.removeFromLeft (bw);  bars.removeFromLeft (gap);
    auto barCh = bars.removeFromLeft (bw);  bars.removeFromLeft (gap);
    auto barOut = bars.removeFromLeft (bw);
    auto labIn = labels.withX (barIn.getX() - 14).withWidth (bw + 28);
    auto labCh = labels.withX (barCh.getX() - 14).withWidth (bw + 28);
    auto labOut = labels.withX (barOut.getX() - 14).withWidth (bw + 28);
    drawLevel (barIn, in, "IN", labIn);
    drawLevel (barOut, out, "OUT", labOut);

    // CHOP: dB clipped, hangs down from the top (0..18 dB)
    {
        g.setColour (juce::Colour (0xff171210)); g.fillRect (barCh);
        auto inner = barCh.reduced (2.0f);
        const int segs = 18;
        const float sh = inner.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            auto seg = juce::Rectangle<float> (inner.getX(), inner.getY() + (float) i * sh + 0.5f, inner.getWidth(), sh - 1.5f);
            const bool lit = chop > (float) i + 0.05f;
            g.setColour (lit ? (i < 3 ? pal::hazard : pal::signalHot) : pal::signal.withAlpha (0.08f));
            g.fillRect (seg);
        }
        if (hold > 0.05f)
        {
            const float hy = inner.getY() + std::min (hold, 18.0f) / 18.0f * inner.getHeight();
            g.setColour (juce::Colour (0xfffff0d8)); g.fillRect (inner.getX(), hy - 1.0f, inner.getWidth(), 2.0f);
        }
        g.setColour (pal::signalHot); g.setFont (tex::stencil (13.0f));
        auto lab = labCh;
        g.drawText ("CHOP", lab.removeFromTop (15), juce::Justification::centred);
        g.setColour (pal::paintDim); g.setFont (tex::plainBold (9.5f));
        g.drawText (hold > 0.05f ? "-" + juce::String (hold, 1) : juce::String ("0.0"), lab, juce::Justification::centred);
    }
    // scale ticks between bars
    g.setFont (tex::plainBold (8.0f));
    for (int db : { 0, -6, -12, -24, -36, -48 })
    {
        const float yy = barIn.reduced (2.0f).getY() + (float) -db / 48.0f * barIn.reduced (2.0f).getHeight();
        g.setColour (pal::paintDim.withAlpha (0.55f));
        g.drawText (juce::String (db), (int) barIn.getRight() + 1, (int) yy - 5, (int) gap - 2, 10, juce::Justification::centred);
    }
    for (int db : { 3, 6, 9, 12, 15 })
    {
        const float yy = barCh.reduced (2.0f).getY() + (float) db / 18.0f * barCh.reduced (2.0f).getHeight();
        g.setColour (pal::paintDim.withAlpha (0.55f));
        g.drawText (juce::String (db), (int) barCh.getRight() + 1, (int) yy - 5, (int) gap - 2, 10, juce::Justification::centred);
    }

    // SCRAPS warning across the meters
    if (scraps)
    {
        auto w = m.withHeight (16.0f);
        g.setColour (pal::hazard.withAlpha (0.92f)); g.fillRect (w);
        g.setColour (juce::Colours::black); g.setFont (tex::stencil (12.0f).withExtraKerningFactor (0.1f));
        g.drawFittedText ("LISTENING TO SCRAPS", w.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.6f);
    }
}

void ChopShopEditor::timerCallback()
{
    auto& e = proc.getEngine();
    const float in = e.inPeak.exchange (0.0f), out = e.outPeak.exchange (0.0f);
    const float ch = e.chopDb.exchange (0.0f), drive = e.drivePeak.exchange (0.0f);
    inMeter = std::max (in, inMeter * 0.86f);
    outMeter = std::max (out, outMeter * 0.86f);
    chopMeter = std::max (ch, chopMeter - 0.6f);
    if (ch >= chopHold) { chopHold = ch; holdTimer = 45; }
    else if (--holdTimer <= 0) chopHold = std::max (0.0f, chopHold - 0.4f);
    saw.advance (chopMeter, 1.0f / 30.0f);
    saw.repaint();
    curve.setLevel (std::max (drive, 0.0f));
    curve.repaint();

    auto& rnd = juce::Random::getSystemRandom();
    const float before = flicker;
    flicker = (rnd.nextFloat() < 0.02f + juce::jlimit (0.0f, 0.3f, chopMeter * 0.03f)) ? rnd.nextFloat() : flicker * 0.5f;
    if (std::abs (flicker - before) > 0.01f) repaint (116, 10, 336, 72);

    if (proc.presetVersion.load() != lastPresetVersion) refreshPresetBox();
    meters.in = inMeter; meters.out = outMeter; meters.chop = chopMeter; meters.hold = chopHold;
    meters.scraps = scrapsButton.getToggleState();
    meters.repaint();
}

//==============================================================================
void ChopShopEditor::refreshPresetBox()
{
    lastPresetVersion = proc.presetVersion.load();
    userFiles = ChopShopProcessor::getUserPresets();
    const auto current = proc.getCurrentPresetName();

    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.addSeparator();
    presetBox.addSectionHeading ("User Presets");
    if (userFiles.isEmpty()) { presetBox.addItem ("(none saved yet)", 999); presetBox.setItemEnabled (999, false); }
    for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), kUserIdBase + i);

    int sel = 0;
    for (int i = 0; i < userFiles.size(); ++i)
        if (userFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (current)) sel = kUserIdBase + i;
    if (sel == 0 && proc.getProgramName (proc.getCurrentProgram()) == current) sel = proc.getCurrentProgram() + 1;
    if (sel != 0) presetBox.setSelectedId (sel, juce::dontSendNotification);
    else presetBox.setText (current, juce::dontSendNotification);
    deleteButton.setEnabled (sel >= kUserIdBase);
}

void ChopShopEditor::stepPreset (int delta)
{
    std::vector<int> ids;
    for (int i = 0; i < proc.getNumPrograms(); ++i) ids.push_back (i + 1);
    for (int i = 0; i < userFiles.size(); ++i) ids.push_back (kUserIdBase + i);
    auto it = std::find (ids.begin(), ids.end(), presetBox.getSelectedId());
    const int pos = it == ids.end() ? 0 : (int) (it - ids.begin());
    const int n = (int) ids.size();
    presetBox.setSelectedId (ids[(size_t) (((pos + delta) % n + n) % n)], juce::sendNotificationSync);
}

void ChopShopEditor::showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                                 juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult)
{
    juce::Component::SafePointer<ChopShopEditor> safe (this);
    dialog = std::make_unique<ChopDialog> (title, message, withTextField, initialText, buttons,
        [safe, onResult] (int index, const juce::String& text)
        {
            juce::MessageManager::callAsync ([safe, onResult, index, text]
            {
                if (safe == nullptr) return;
                safe->dialog.reset();
                if (onResult) onResult (index, text);
            });
        });
    addAndMakeVisible (*dialog);
    dialog->setBounds (getLocalBounds());
}

void ChopShopEditor::showMessage (const juce::String& title, const juce::String& text)
{
    showDialog (title, text, false, {}, { "OK" }, {});
}

void ChopShopEditor::promptSave()
{
    const bool onUser = presetBox.getSelectedId() >= kUserIdBase;
    juce::Component::SafePointer<ChopShopEditor> safe (this);
    auto doSave = [safe] (const juce::String& name)
    {
        if (safe == nullptr) return;
        if (! safe->proc.saveUserPreset (name))
            safe->showMessage ("COULDN'T SAVE", "Chop Shop couldn't write the preset file to:\n" + ChopShopProcessor::getUserPresetFolder().getFullPathName());
        else safe->refreshPresetBox();
    };
    showDialog ("SAVE PRESET", "Name your preset:", true, onUser ? proc.getCurrentPresetName() : juce::String(), { "Save", "Cancel" },
        [safe, doSave] (int button, const juce::String& text)
        {
            if (button != 0 || safe == nullptr) return;
            const auto name = text.trim();
            if (name.isEmpty()) { safe->showMessage ("SAVE PRESET", "Please enter a name for the preset."); return; }
            for (int i = 0; i < safe->proc.getNumPrograms(); ++i)
                if (safe->proc.getProgramName (i).equalsIgnoreCase (name))
                {
                    safe->showMessage ("SAVE PRESET", "\"" + name + "\" is a factory preset name. Please choose another name.");
                    return;
                }
            if (ChopShopProcessor::fileForPresetName (name).existsAsFile())
            {
                safe->showDialog ("REPLACE PRESET?", "A preset named \"" + name + "\" already exists. Replace it with the current settings?",
                                  false, {}, { "Replace", "Cancel" },
                                  [doSave, name] (int b, const juce::String&) { if (b == 0) doSave (name); });
                return;
            }
            doSave (name);
        });
}

void ChopShopEditor::confirmDelete()
{
    const int id = presetBox.getSelectedId();
    if (id < kUserIdBase || id - kUserIdBase >= userFiles.size()) return;
    const auto file = userFiles[id - kUserIdBase];
    juce::Component::SafePointer<ChopShopEditor> safe (this);
    showDialog ("DELETE PRESET?", "Delete \"" + file.getFileNameWithoutExtension() + "\"? This can't be undone.", false, {},
                { "Delete", "Cancel" }, [safe, file] (int b, const juce::String&)
                {
                    if (b != 0 || safe == nullptr) return;
                    file.deleteFile();
                    safe->proc.setCurrentProgram (0);
                    safe->refreshPresetBox();
                });
}

//==============================================================================
ChopDialog::ChopDialog (const juce::String& t, const juce::String& m, bool withTextField, const juce::String& initialText,
                        juce::StringArray names, std::function<void (int, const juce::String&)> cb)
    : title (t), message (m), hasField (withTextField), onClose (std::move (cb))
{
    setWantsKeyboardFocus (true);
    if (hasField)
    {
        field.setText (initialText, false);
        field.setInputRestrictions (60);
        field.setSelectAllWhenFocused (true);
        field.setFont (tex::plainBold (15.0f));
        field.setIndents (8, 6);
        field.onReturnKey = [this] { finish (0); };
        field.onEscapeKey = [this] { finish ((int) buttons.size() - 1); };
        addAndMakeVisible (field);
    }
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (names[i]));
        b->onClick = [this, i] { finish (i); };
        addAndMakeVisible (b);
    }
}

void ChopDialog::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ChopDialog> (this)]
        {
            if (sp == nullptr) return;
            if (sp->hasField) sp->field.grabKeyboardFocus(); else sp->grabKeyboardFocus();
        });
}

void ChopDialog::finish (int index)
{
    if (onClose == nullptr) return;
    auto cb = std::move (onClose);
    onClose = nullptr;
    cb (index, field.getText());
}

bool ChopDialog::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey) { finish (0); return true; }
    if (k == juce::KeyPress::escapeKey) { finish ((int) buttons.size() - 1); return true; }
    return true;
}

void ChopDialog::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (400, hasField ? 196 : 166);
    auto r = panel.reduced (22);
    r.removeFromTop (30);
    r.removeFromTop (hasField ? 22 : 50);
    if (hasField) { field.setBounds (r.removeFromTop (32)); r.removeFromTop (16); }
    auto row = r.removeFromBottom (28);
    for (int i = (int) buttons.size() - 1; i >= 0; --i) { buttons[i]->setBounds (row.removeFromRight (96)); row.removeFromRight (8); }
}

void ChopDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto p = panel.toFloat();
    g.setColour (juce::Colour (0xff1a1411)); g.fillRect (p);
    g.setGradientFill (juce::ColourGradient (pal::rustDeep.withAlpha (0.5f), p.getX(), p.getY(), juce::Colours::transparentBlack, p.getX(), p.getY() + p.getHeight() * 0.6f, false));
    g.fillRect (p);
    g.setColour (pal::rust); g.drawRect (p, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRect (p.reduced (2.0f), 1.0f);
    for (auto pt : { juce::Point<float> (p.getX() + 8, p.getY() + 8), { p.getRight() - 8, p.getY() + 8 }, { p.getX() + 8, p.getBottom() - 8 }, { p.getRight() - 8, p.getBottom() - 8 } })
    {
        g.setColour (juce::Colour (0xff6a5a4c)); g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6);
        g.setColour (pal::rust.withAlpha (0.6f)); g.drawEllipse (pt.x - 3, pt.y - 3, 6, 6, 1.0f);
    }
    auto r = panel.reduced (22);
    g.setColour (pal::signalHot); g.setFont (tex::stencil (19.0f).withExtraKerningFactor (0.1f));
    g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (pal::paint); g.setFont (tex::plainBold (13.0f));
    g.drawFittedText (message, r.removeFromTop (hasField ? 22 : 50), juce::Justification::topLeft, 3);
}
