#pragma once
#include <JuceHeader.h>
#include "../Parameters.h"

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

    void setCurve (const std::vector<juce::Point<float>>& pts) { curve = pts; }
    const std::vector<juce::Point<float>>& getCurve() const { return curve; }

    // Bipolar value in [-1,1] at an absolute musical position (quarter notes).
    float evaluate (double ppqPosition) const noexcept;

private:
    float sampleSmooth (float phase01) const noexcept;
    float sampleStep   (float phase01) const noexcept;

    std::vector<juce::Point<float>> curve { {0.f, 0.5f}, {1.f, 0.5f} };
    EnvMode  mode     = EnvMode::normal;
    LoopMode loopMode = LoopMode::loop;
    double   bars     = 2.0;
    int      numSteps = 16;
    bool     enabled  = false;
};
