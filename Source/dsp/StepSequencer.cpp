#include "StepSequencer.h"

double StepSequencer::stepsPerQuarter() const noexcept
{
    switch (resolution)
    {
        case q:  return 1.0;          // 1/4
        case e:  return 2.0;          // 1/8
        case eT: return 3.0;          // 1/8 triplet
        case s:  return 4.0;          // 1/16
        case sT: return 6.0;          // 1/16 triplet
        case ts: return 8.0;          // 1/32
        default: return 4.0;
    }
}

int StepSequencer::stepIndexAt (double ppqPosition) const noexcept
{
    const double spq = stepsPerQuarter();
    double stepPos = ppqPosition * spq + (double) offset;
    int idx = ((int) std::floor (stepPos)) % length;
    if (idx < 0) idx += length;
    return idx;
}

float StepSequencer::gateAt (double ppqPosition) const noexcept
{
    if (! enabled) return 1.0f;       // bypassed -> fully open

    const int idx = stepIndexAt (ppqPosition);
    const int level = pattern[(size_t) idx];
    if (level <= 0) return 0.0f;                  // closed
    return (float) level / (float) maxLevel;      // 0.5 = normal, 1.0 = accent
}
