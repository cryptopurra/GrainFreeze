#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// StepSequencer  --  a tempo-synced multi-level gate.
//
// A pattern of N steps, each holding a level 0..maxLevel:
//   0 = silent (gate closed)
//   1 = normal gate
//   2 = accent (full / variant)
// (More levels can be added; the audio mapping lives in the processor.)
//
// The current step is derived from the host musical position and the chosen
// resolution, so it locks to the grid. Offset shifts the phase; Reset
// restarts the pattern on the next bar.
// ---------------------------------------------------------------------------

class StepSequencer
{
public:
    enum Resolution { q = 0, e, eT, s, sT, ts };   // 1/4,1/8,1/8T,1/16,1/16T,1/32

    void setEnabled    (bool e)        noexcept { enabled = e; }
    void setLength     (int n)         noexcept { length = juce::jlimit (1, 32, n); }
    void setResolution (Resolution r)  noexcept { resolution = r; }
    void setOffset     (float o)       noexcept { offset = o; }

    bool isEnabled() const noexcept { return enabled; }
    int  getLength() const noexcept { return length; }

    void setStep (int i, int level) noexcept
    {
        if (juce::isPositiveAndBelow (i, (int) pattern.size()))
            pattern[(size_t) i] = juce::jlimit (0, maxLevel, level);
    }
    int getStep (int i) const noexcept
    {
        return juce::isPositiveAndBelow (i, (int) pattern.size()) ? pattern[(size_t) i] : 0;
    }

    // Gate value 0..1 at a given musical position; smoothed edges applied in
    // the processor. Returns the raw level's amplitude.
    float gateAt (double ppqPosition) const noexcept;

    // The step index the given musical position falls on (for the UI playhead).
    int stepIndexAt (double ppqPosition) const noexcept;

    static constexpr int maxLevel = 2;

    double stepsPerQuarter() const noexcept;

private:

    std::array<int, 32> pattern {};   // default all 0 (silent) until set
    bool  enabled   = false;
    int   length    = 16;
    Resolution resolution = s;        // 1/16 default
    float offset    = 0.0f;
};
