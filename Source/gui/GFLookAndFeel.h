#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// GFLookAndFeel  --  original dark "hardware" skin for GrainFreeze.
//
// Everything is drawn with vector code (no bitmaps, no third-party artwork or
// branding): metal-ish knobs, a round FREEZE button, LED toggles, flat tabs.
// ---------------------------------------------------------------------------

namespace gf
{
    inline juce::Colour gold()     { return juce::Colour (0xffe3bb35); }
    inline juce::Colour goldDim()  { return juce::Colour (0xff8d7524); }
    inline juce::Colour inset()    { return juce::Colour (0xff0d0e11); }
    inline juce::Colour panel()    { return juce::Colour (0xff1c1e23); }
    inline juce::Colour textDim()  { return juce::Colour (0xffa4a9b4); }
    inline juce::Colour accent()   { return juce::Colour (0xff59a8ff); }
}

class GFLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GFLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, gf::gold());
        setColour (juce::Slider::textBoxBackgroundColourId, gf::inset());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff30343d));
        setColour (juce::Label::textColourId, gf::textDim());
        setColour (juce::ComboBox::backgroundColourId, gf::inset());
        setColour (juce::ComboBox::textColourId, gf::gold());
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff30343d));
        setColour (juce::ComboBox::arrowColourId, gf::gold());
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff16181c));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffd8dde6));
        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffd0d5de));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23262d));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a4458));
        setColour (juce::TextButton::textColourOffId, gf::textDim());
        setColour (juce::TextButton::textColourOnId, gf::gold());
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        const bool big = (bool) s.getProperties().getWithDefault ("big", false);
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto area = juce::Rectangle<float> (dia, dia).withCentre (bounds.getCentre()).reduced (2.0f);
        const auto c = area.getCentre();
        const float r = area.getWidth() * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        // value arc (outer)
        const float arcR = r - 1.0f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a2d35));
        g.strokePath (track, juce::PathStrokeType (big ? 3.0f : 2.0f));
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (gf::gold());
        g.strokePath (val, juce::PathStrokeType (big ? 3.0f : 2.0f));

        // body
        const float br = r * (big ? 0.80f : 0.74f);
        juce::ColourGradient rim (juce::Colour (0xffc9ced8), c.x, c.y - br, juce::Colour (0xff4a4f59), c.x, c.y + br, false);
        g.setGradientFill (rim);
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        const float ir = br * 0.84f;
        juce::ColourGradient face (juce::Colour (0xff3b3f48), c.x, c.y - ir, juce::Colour (0xff15171b), c.x, c.y + ir, false);
        g.setGradientFill (face);
        g.fillEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f);

        // pointer
        juce::Path p;
        const float pl = ir * 0.9f, pw = juce::jmax (2.0f, ir * 0.12f);
        p.addRoundedRectangle (-pw * 0.5f, -pl, pw, pl * 0.55f, pw * 0.4f);
        g.setColour (juce::Colour (0xfff0f3f8));
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool down) override
    {
        const auto id = b.getComponentID();
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        if (id == "freeze")
        {
            const float d = juce::jmin (r.getWidth(), r.getHeight());
            auto cr = juce::Rectangle<float> (d, d).withCentre (r.getCentre()).reduced (2.0f);
            const auto c = cr.getCentre();
            const float R = cr.getWidth() * 0.5f;

            if (on)   // glow
            {
                g.setColour (gf::accent().withAlpha (0.35f));
                g.fillEllipse (cr.expanded (4.0f));
            }
            juce::ColourGradient outer (juce::Colour (0xff0b0c0e), c.x, c.y - R, juce::Colour (0xff3a3d45), c.x, c.y + R, false);
            g.setGradientFill (outer);
            g.fillEllipse (cr);
            auto ring = cr.reduced (R * 0.07f);
            g.setColour (on ? gf::accent() : juce::Colour (0xff7d838f));
            g.drawEllipse (ring, 2.0f);

            auto face = cr.reduced (R * 0.14f);
            juce::ColourGradient metal (juce::Colour (0xfff3f5f8), face.getX(), face.getY(),
                                        juce::Colour (0xff8e939c), face.getRight(), face.getBottom(), false);
            metal.addColour (0.5, juce::Colour (0xffc9ccd3));
            if (down) metal = juce::ColourGradient (juce::Colour (0xffb7bbc4), face.getX(), face.getY(),
                                                    juce::Colour (0xff6d727b), face.getRight(), face.getBottom(), false);
            g.setGradientFill (metal);
            g.fillEllipse (face);
            g.setColour (juce::Colour (0xff1a1c20));
            g.drawEllipse (face, 1.0f);

            g.setColour (on ? juce::Colour (0xff0b3a78) : juce::Colour (0xff2a2d33));
            g.setFont (juce::Font (juce::FontOptions ((float) R * 0.36f, juce::Font::bold)));
            g.drawText ("FREEZE", face, juce::Justification::centred);
            return;
        }

        if (id == "led")
        {
            auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre (r.getCentre());
            g.setColour (juce::Colour (0xff08090b));
            g.fillRoundedRectangle (led.expanded (1.5f), 2.0f);
            if (on)
            {
                g.setColour (gf::gold().withAlpha (0.35f));
                g.fillRoundedRectangle (led.expanded (3.0f), 3.0f);
            }
            g.setColour (on ? gf::gold() : juce::Colour (0xff3a3d45));
            g.fillRoundedRectangle (led, 1.5f);
            return;
        }

        // "pill" (default)
        const bool isLive = (id == "live");
        const auto onCol = isLive ? juce::Colour (0xff3fbf6a) : gf::accent();
        auto pr = r.reduced (1.0f);
        g.setColour (on ? onCol.withAlpha (0.20f) : juce::Colour (0xff1d2026));
        if (hover) g.setColour (juce::Colour (on ? onCol.withAlpha (0.28f) : juce::Colour (0xff262a31)));
        g.fillRoundedRectangle (pr, 4.0f);
        g.setColour (on ? onCol : juce::Colour (0xff3a3f4a));
        g.drawRoundedRectangle (pr, 4.0f, 1.2f);
        g.setColour (on ? onCol.brighter (0.4f) : gf::textDim());
        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        g.drawText (b.getButtonText(), pr, juce::Justification::centred);
        juce::ignoreUnused (down);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        juce::Colour c = on ? juce::Colour (0xff3d4960) : juce::Colour (0xff23262d);
        if (hover) c = c.brighter (0.08f);
        if (down)  c = c.brighter (0.15f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff3a3f4a));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (11.0f)); }
};
