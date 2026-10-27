#pragma once
#include <JuceHeader.h>
#include "../Parameters.h"

// ---------------------------------------------------------------------------
// StepModulator  --  one tempo-synced modulation envelope.
//
// Each modulator owns a breakpoint curve (points over a 0..1 cycle) that can
// be interpreted in two modes:
//   SMOOTH : linear interpolation between points (continuous ramp)
//   STEP   : held value per step (stair-stepped)
//
// The cycle length is expressed in bars and advances from the host's musical
// position, so it stays locked whatever the buffer size. LoopMode picks loop
// vs ping-pong vs one-shot. The output (-1..+1 * depth) is applied to a
// ModDest by the processor's modulation matrix.
// ---------------------------------------------------------------------------

class StepModulator
{
public:
    void setMode     (EnvMode m)  noexcept { mode = m; }
    void setLoopMode (LoopMode m) noexcept { loopMode = m; }
    void setBars     (double b)   noexcept { bars = juce::jmax (0.125, b); }
    void setSteps    (int n)      noexcept { numSteps = juce::jlimit (2, 32, n); }
    void setDepth    (float d)    noexcept { depth = d; }
    void setDest     (ModDest d)  noexcept { dest = d; }
    void setEnabled  (bool e)     noexcept { enabled = e; }

    bool     isEnabled() const noexcept { return enabled; }
    ModDest  destination() const noexcept { return dest; }

    // Replace the breakpoint curve (values in 0..1, times in 0..1 ascending).
    void setCurve (const std::vector<juce::Point<float>>& pts) { curve = pts; }

    // Evaluate the modulator at an absolute musical position (in quarter notes).
    // Returns depth-scaled value in roughly [-1, +1].
    float evaluate (double ppqPosition) const noexcept;

private:
    float sampleCurveSmooth (float phase01) const noexcept;
    float sampleCurveStep    (float phase01) const noexcept;

    std::vector<juce::Point<float>> curve { {0.f, 0.f}, {1.f, 1.f} };
    EnvMode  mode     = EnvMode::smooth;
    LoopMode loopMode = LoopMode::loop;
    double   bars     = 2.0;
    int      numSteps = 16;
    float    depth    = 1.0f;
    ModDest  dest     = ModDest::none;
    bool     enabled  = false;
};
