#pragma once
#include <JuceHeader.h>
#include "../Parameters.h"
#include "CurveMath.h"

// ---------------------------------------------------------------------------
// StepModulator  --  one tempo-synced modulation envelope.
//
// Owns a breakpoint curve over a 0..1 cycle, interpreted either SMOOTH (linear
// ramps) or STEP (held per step). The cycle length is in bars and advances from
// the host musical position. Destinations/depths are applied by the processor's
// modulation matrix, so this class just produces a bipolar [-1,1] value.
// ---------------------------------------------------------------------------

class StepModulator
{
public:
    void setMode     (EnvMode m)  noexcept { mode = m; }
    void setLoopMode (LoopMode m) noexcept { loopMode = m; }
    void setBars     (double b)   noexcept { bars = juce::jmax (0.125, b); }
    void setSteps    (int n)      noexcept { numSteps = juce::jlimit (2, 32, n); }
    void setEnabled  (bool e)     noexcept { enabled = e; }

    bool isEnabled() const noexcept { return enabled; }

    void setCurve (const std::vector<juce::Point<float>>& pts, const std::vector<float>& segmentBends = {})
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        curve = pts;
        bends = segmentBends;
        bends.resize (pts.size() > 0 ? pts.size() - 1 : 0, 0.0f);
    }
    std::vector<float> getBends() const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return bends;
    }
    std::vector<juce::Point<float>> getCurve() const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return curve;
    }

    // Bipolar value in [-1,1] at an absolute musical position (quarter notes).
    float evaluate (double ppqPosition) const noexcept;
    // Unipolar curve value 0..1 (used for absolute targets such as Speed).
    float evaluate01 (double ppqPosition) const noexcept;
    // Current position inside the cycle (0..1), for the UI playhead.
    float getPhase() const noexcept { return lastPhase.load (std::memory_order_relaxed); }

private:
    float sampleSmooth (float phase01) const noexcept;
    float sampleStep   (float phase01) const noexcept;

    mutable juce::SpinLock lock;
    mutable std::atomic<float> lastPhase { 0.0f };
    mutable float lastValue = 0.5f;
    std::vector<juce::Point<float>> curve { {0.f, 0.5f}, {1.f, 0.5f} };
    std::vector<float> bends { 0.0f };
    EnvMode  mode     = EnvMode::normal;
    LoopMode loopMode = LoopMode::loop;
    double   bars     = 2.0;
    int      numSteps = 16;
    bool     enabled  = false;
};
