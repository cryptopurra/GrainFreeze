#pragma once
#include <JuceHeader.h>
#include "../dsp/StepSequencer.h"
#include "GFLookAndFeel.h"

// Two-level step grid: click toggles a step between 1 (live) and 2 (frozen);
// drag across steps to paint the same value. The playhead step is outlined.
class SequencerGrid : public juce::Component
{
public:
    explicit SequencerGrid (StepSequencer& s) : seq (s) {}
    void setPlayStep (int s) { if (s != playStep) { playStep = s; repaint(); } }
    void setActive (bool a)  { if (a != active)   { active = a;   repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        juce::ColourGradient bg (juce::Colour (0xff1a1c21), 0, 0, juce::Colour (0xff0b0c0e), 0, full.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (full, 4.0f);
        g.setColour (juce::Colour (0xff2c3038));
        g.drawRoundedRectangle (full.reduced (0.5f), 4.0f, 1.0f);

        const int len = seq.getLength();
        auto area = full.reduced (6.0f, 8.0f);
        area.removeFromBottom (14.0f);
        const float cw = area.getWidth() / (float) len;

        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        for (int i = 0; i < len; ++i)
        {
            auto cell = juce::Rectangle<float> (area.getX() + (float) i * cw + 1.0f, area.getY(), cw - 2.0f, area.getHeight());
            g.setColour (juce::Colour (0xff17191d));
            g.fillRoundedRectangle (cell, 2.0f);

            const bool hi = seq.getStep (i) == StepSequencer::high;
            auto bar = hi ? cell : cell.withTop (cell.getBottom() - cell.getHeight() * 0.28f);
            g.setColour (hi ? (active ? gf::gold() : gf::goldDim()) : juce::Colour (0xff3a3f4a));
            g.fillRoundedRectangle (bar, 2.0f);

            if (active && i == playStep)
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawRoundedRectangle (cell, 2.0f, 1.5f);
            }
            if (i % 4 == 0)
            {
                g.setColour (juce::Colour (0xff777d89));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (cell.getX() - 4.0f, area.getBottom() + 1.0f, cw + 8.0f, 12.0f),
                            juce::Justification::centred);
            }
        }
        g.setColour (juce::Colour (0xff777d89));
        g.drawText ("2 = frozen    1 = live", full.withTrimmedRight (8.0f).withHeight (14.0f).withY (full.getBottom() - 14.0f),
                    juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.x);
        if (i < 0) return;
        paintValue = seq.getStep (i) == StepSequencer::high ? StepSequencer::low : StepSequencer::high;
        seq.setStep (i, paintValue);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.x);
        if (i >= 0 && seq.getStep (i) != paintValue) { seq.setStep (i, paintValue); repaint(); }
    }

private:
    int cellAt (int x) const
    {
        const int len = seq.getLength();
        auto area = getLocalBounds().toFloat().reduced (6.0f, 8.0f);
        const int i = (int) ((float) x - area.getX()) * len / juce::jmax (1, (int) area.getWidth());
        return juce::isPositiveAndBelow (i, len) ? i : -1;
    }
    StepSequencer& seq;
    int playStep = -1, paintValue = StepSequencer::high;
    bool active = false;
};
