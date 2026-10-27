#include "StepModulator.h"

float StepModulator::sampleCurveSmooth (float phase01) const noexcept
{
    if (curve.size() < 2) return 0.0f;
    for (size_t i = 1; i < curve.size(); ++i)
    {
        if (phase01 <= curve[i].x)
        {
            auto& a = curve[i - 1];
            auto& b = curve[i];
            const float t = (b.x > a.x) ? (phase01 - a.x) / (b.x - a.x) : 0.0f;
            return a.y + (b.y - a.y) * t;
        }
    }
    return curve.back().y;
}

float StepModulator::sampleCurveStep (float phase01) const noexcept
{
    const int step = juce::jlimit (0, numSteps - 1, (int) (phase01 * numSteps));
    const float center = (step + 0.5f) / (float) numSteps;
    return sampleCurveSmooth (center);
}

float StepModulator::evaluate (double ppqPosition) const noexcept
{
    if (! enabled || dest == ModDest::none) return 0.0f;

    const double quartersPerCycle = bars * 4.0;
    double raw = ppqPosition / quartersPerCycle;
    double cyclePhase = raw - std::floor (raw);   // 0..1

    switch (loopMode)
    {
        case LoopMode::oneShot:
            if (raw >= 1.0) cyclePhase = 1.0;
            break;
        case LoopMode::pingPong:
        {
            const int cyc = (int) std::floor (raw);
            if (cyc & 1) cyclePhase = 1.0 - cyclePhase;  // reverse odd cycles
            break;
        }
        case LoopMode::loop:
        default: break;
    }

    const float v = (mode == EnvMode::step)
                      ? sampleCurveStep ((float) cyclePhase)
                      : sampleCurveSmooth ((float) cyclePhase);

    // Map curve value 0..1 to bipolar -1..+1, then scale by depth.
    return (v * 2.0f - 1.0f) * depth;
}
