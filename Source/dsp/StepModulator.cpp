#include "StepModulator.h"

float StepModulator::sampleSmooth (float phase01) const noexcept
{
    if (curve.size() < 2) return 0.5f;
    for (size_t i = 1; i < curve.size(); ++i)
        if (phase01 <= curve[i].x)
        {
            auto& a = curve[i - 1]; auto& b = curve[i];
            const float t = (b.x > a.x) ? (phase01 - a.x) / (b.x - a.x) : 0.0f;
            return a.y + (b.y - a.y) * t;
        }
    return curve.back().y;
}

float StepModulator::sampleStep (float phase01) const noexcept
{
    const int step = juce::jlimit (0, numSteps - 1, (int) (phase01 * numSteps));
    return sampleSmooth ((step + 0.5f) / (float) numSteps);
}

float StepModulator::evaluate01 (double ppqPosition) const noexcept
{
    const double quartersPerCycle = bars * 4.0;
    const double raw = ppqPosition / quartersPerCycle;
    double cyclePhase = raw - std::floor (raw);

    switch (loopMode)
    {
        case LoopMode::oneShot:
            if (raw >= 1.0 || raw < 0.0) cyclePhase = (raw < 0.0) ? 0.0 : 1.0;
            break;
        case LoopMode::pingPong:
        {
            const int cyc = (int) std::floor (raw);
            if (cyc & 1) cyclePhase = 1.0 - cyclePhase;
            break;
        }
        case LoopMode::loop:
        default: break;
    }
    lastPhase.store ((float) cyclePhase, std::memory_order_relaxed);

    juce::SpinLock::ScopedTryLockType tl (lock);
    if (! tl.isLocked())
        return lastValue;                    // UI is editing the curve: hold the last value

    lastValue = (mode == EnvMode::step) ? sampleStep ((float) cyclePhase)
                                        : sampleSmooth ((float) cyclePhase);
    return lastValue;
}

float StepModulator::evaluate (double ppqPosition) const noexcept
{
    if (! enabled) return 0.0f;
    return evaluate01 (ppqPosition) * 2.0f - 1.0f;   // 0..1 curve -> -1..+1
}
