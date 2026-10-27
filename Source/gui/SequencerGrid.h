#pragma once
#include <JuceHeader.h>
#include "../dsp/StepSequencer.h"

// ---------------------------------------------------------------------------
// SequencerGrid
//
// A clickable multi-level step grid bound to a StepSequencer. Each cell cycles
// through levels on click (0 -> 1 -> 2 -> 0): silent / normal / accent.
// Right-click clears a cell. The playhead column is highlighted from the
// processor's current musical position (set via setPlayStep).
//
// It edits the StepSequencer the processor owns, so changes take effect live.
// (Original component.)
// ---------------------------------------------------------------------------

class SequencerGrid : public juce::Component
{
public:
    explicit SequencerGrid (StepSequencer& s) : seq (s) {}

    void setPlayStep (int s) { if (s != playStep) { playStep = s; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const int len = seq.getLength();
        if (len <= 0) return;

        auto area = getLocalBounds().reduced (2);
        const float cw = (float) area.getWidth() / (float) len;
        const float h  = (float) area.getHeight();

        for (int i = 0; i < len; ++i)
        {
            juce::Rectangle<float> cell ((float) area.getX() + i * cw,
                                         (float) area.getY(), cw - 2.0f, h);

            const int level = seq.getStep (i);
            juce::Colour base = (i == playStep) ? juce::Colour (0xff4a9eff)
                                                : juce::Colour (0xff2c2f38);
            g.setColour (base);
            g.fillRoundedRectangle (cell, 3.0f);

            // Fill height encodes the level (normal = half, accent = full).
            if (level > 0)
            {
                const float frac = (float) level / (float) StepSequencer::maxLevel;
                auto fill = cell.withTrimmedTop (cell.getHeight() * (1.0f - frac));
                g.setColour (level >= StepSequencer::maxLevel
                               ? juce::Colour (0xffffc24a)   // accent
                               : juce::Colour (0xff8ad17a));  // normal
                g.fillRoundedRectangle (fill.reduced (2.0f), 2.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.x);
        if (i < 0) return;

        if (e.mods.isRightButtonDown())
            seq.setStep (i, 0);
        else
            seq.setStep (i, (seq.getStep (i) + 1) % (StepSequencer::maxLevel + 1));

        repaint();
    }

private:
    int cellAt (int x) const
    {
        const int len = seq.getLength();
        if (len <= 0) return -1;
        auto area = getLocalBounds().reduced (2);
        const int i = (int) ((float) (x - area.getX()) / (float) area.getWidth() * len);
        return juce::isPositiveAndBelow (i, len) ? i : -1;
    }

    StepSequencer& seq;
    int playStep = -1;
};
