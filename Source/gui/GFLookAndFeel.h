#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// GFLookAndFeel  --  original dark skin for GrainFreeze.
//
// Hand-drawn rotary knobs (arc + pointer), flat panels and toggle styling.
// Deliberately our own look (no bitmaps, no third-party artwork or branding).
// ---------------------------------------------------------------------------

class GFLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GFLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd8dde6));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, juce::Colour (0xffaeb4bf));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff20232b));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffd8dde6));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3f4a));
        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffaeb4bf));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  centre = bounds.getCentre();
        const float angle  = startAngle + pos * (endAngle - startAngle);

        // Track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2b2f39));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (0xffd9c24a));
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knob body
        const float br = radius * 0.62f;
        g.setColour (juce::Colour (0xff343a45));
        g.fillEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colour (0xff1c1f26));
        g.drawEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f, 1.0f);

        // Pointer
        juce::Point<float> tip (centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * br,
                                centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * br);
        g.setColour (juce::Colour (0xffe8edf5));
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.0f);
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::Font (12.0f); }
};
