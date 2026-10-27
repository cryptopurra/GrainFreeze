#pragma once
#include <JuceHeader.h>
#include "GFLookAndFeel.h"

// Stereo peak meter with a dB scale (+6 .. -48). Feed linear peaks via setLevels().
class Meter : public juce::Component
{
public:
    explicit Meter (bool scaleOnLeft_) : scaleOnLeft (scaleOnLeft_) {}

    void setLevels (float l, float r)
    {
        auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -60.0f); };
        auto fall = [&] (float& cur, float target)
        { cur = target > cur ? target : cur - 1.8f; cur = juce::jmax (-60.0f, cur); };
        fall (dbL, toDb (l));
        fall (dbR, toDb (r));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float scaleW = 24.0f;
        auto scaleArea = scaleOnLeft ? r.removeFromLeft (scaleW) : r.removeFromRight (scaleW);
        auto bars = r.reduced (2.0f, 0.0f);

        g.setColour (gf::inset());
        g.fillRoundedRectangle (bars, 3.0f);

        auto yFor = [&] (float db)
        { return juce::jmap (juce::jlimit (-48.0f, 6.0f, db), -48.0f, 6.0f, bars.getBottom() - 2.0f, bars.getY() + 2.0f); };

        const float bw = (bars.getWidth() - 6.0f) * 0.5f;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float x = bars.getX() + 2.0f + ch * (bw + 2.0f);
            const float top = yFor (ch == 0 ? dbL : dbR);
            juce::Rectangle<float> col (x, top, bw, bars.getBottom() - 2.0f - top);
            juce::ColourGradient grad (juce::Colour (0xffe14b3b), 0, yFor (6.0f),
                                       juce::Colour (0xff3fbf6a), 0, yFor (-18.0f), false);
            grad.addColour (0.25, juce::Colour (0xffe3bb35));
            g.setGradientFill (grad);
            g.fillRect (col);
        }

        g.setColour (juce::Colour (0xff6c727e));
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        for (int db = 6; db >= -48; db -= (db > -6 ? 6 : 6))
        {
            const float y = yFor ((float) db);
            g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db),
                        scaleArea.withY (y - 6.0f).withHeight (12.0f), scaleOnLeft ? juce::Justification::centredRight
                                                                                      : juce::Justification::centredLeft);
        }
        g.drawText ("dB", scaleArea.withY (bars.getBottom() + 1.0f).withHeight (12.0f),
                    scaleOnLeft ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }

private:
    bool scaleOnLeft;
    float dbL = -60.0f, dbR = -60.0f;
};
