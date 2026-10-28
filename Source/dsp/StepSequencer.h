#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// StepSequencer  --  tempo-synced two-level step pattern.
// Every step holds 1 or 2 (the two values TimeFreeze's sequencer uses):
//   2 = frozen step, 1 = live step.
// The current step follows the host musical position and the resolution.
// ---------------------------------------------------------------------------
class StepSequencer
{
public:
    enum Resolution { q = 0, e, eT, s, sT, ts };   // 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
    static constexpr int maxSteps = 32, low = 1, high = 2;

    StepSequencer()
    {
        for (int i = 0; i < maxSteps; ++i) pattern[(size_t) i] = ((i % 4) < 2) ? high : low;
    }

    void setEnabled (bool v) noexcept { enabled = v; }
    void setLength (int n) noexcept { length = juce::jlimit (1, maxSteps, n); }
    void setResolution (Resolution r) noexcept { resolution = r; }

    bool isEnabled() const noexcept { return enabled; }
    int  getLength() const noexcept { return length; }

    void setStep (int i, int level) noexcept
    {
        if (juce::isPositiveAndBelow (i, maxSteps)) pattern[(size_t) i] = level >= high ? high : low;
    }
    int getStep (int i) const noexcept
    {
        return juce::isPositiveAndBelow (i, maxSteps) ? pattern[(size_t) i] : low;
    }

    double stepsPerQuarter() const noexcept
    {
        switch (resolution) { case q: return 1.0; case e: return 2.0; case eT: return 3.0;
                              case s: return 4.0; case sT: return 6.0; case ts: return 8.0; }
        return 4.0;
    }

    int stepIndexAt (double ppq) const noexcept
    {
        int idx = ((int) std::floor (ppq * stepsPerQuarter())) % length;
        if (idx < 0) idx += length;
        return idx;
    }
    int levelAt (double ppq) const noexcept { return getStep (stepIndexAt (ppq)); }

private:
    std::array<int, maxSteps> pattern {};
    bool enabled = false;
    int length = 16;
    Resolution resolution = s;
};
