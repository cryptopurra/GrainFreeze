#include "GrainEngine.h"

void GrainEngine::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate;
    reset();
}

void GrainEngine::reset()
{
    for (auto& g : grains) g = Grain{};
    scanPhase    = 0.0;
    spawnCounter = 0.0;
    grainSizeSmooth = grainSizeTarget;
}

float GrainEngine::window (float phase01) const noexcept
{
    // Tukey-ish window: raised-cosine fades, flat centre. (Original implementation.)
    constexpr float edge = 0.25f;
    if (phase01 < edge)
        return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * phase01 / edge));
    if (phase01 > 1.0f - edge)
        return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (1.0f - phase01) / edge));
    return 1.0f;
}

void GrainEngine::spawnGrain()
{
    if (capture == nullptr) return;

    for (auto& g : grains)
    {
        if (g.active) continue;

        const int    len  = juce::jmax (16, (int) grainSizeSmooth);
        const double rate = (double) pitchRatio;
        const double reach = (double) len * juce::jmax (1.0, rate);   // how far a grain reads

        double origin;
        if (! capture->isFrozen())
        {
            // LIVE: read a little behind "now". The delay is at least the distance the
            // grain will travel, so pitched-up grains never overtake the write head.
            origin = curAnchor - (reach + 64.0);
        }
        else
        {
            // FROZEN: the input is parked. Grains start inside the last `windowSize`
            // samples before the freeze point, and the start point scans through that
            // window at `Speed`. 0 = static freeze, 1 = real-time passage.
            const double windowSize = juce::jmax (sr * 1.5, reach * 2.0 + 64.0);
            const double span = juce::jmax (1.0, windowSize - reach);
            const double jit  = jitter * (double) len * (double) rng.nextFloat();
            origin = curAnchor - windowSize + std::fmod (scanPhase + jit, span);
        }

        const float panPos = 0.5f + spread * (rng.nextFloat() - 0.5f);

        g.active  = true;
        g.age     = 0;
        g.length  = len;
        g.readPos = origin;
        g.rate    = rate;
        g.panL    = std::sqrt (1.0f - panPos);
        g.panR    = std::sqrt (panPos);
        return;
    }
}

void GrainEngine::process (juce::AudioBuffer<float>& out, int numSamples)
{
    out.clear();
    if (capture == nullptr) return;

    auto* L = out.getWritePointer (0);
    auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : L;

    const bool frozen = capture->isFrozen();
    const double rateA = frozen ? 0.0 : anchorRate;
    // getAnchor() is the END of this block for a moving source, so step back.
    const double anchorStart = capture->getAnchor() - rateA * (double) numSamples;

    if (! frozen) scanPhase = 0.0;

    const float density = juce::jmax (1.0f, 1.0f + overlap * 8.0f);
    const float norm = 1.0f / std::sqrt (density);

    for (int s = 0; s < numSamples; ++s)
    {
        grainSizeSmooth += (grainSizeTarget - grainSizeSmooth) * 0.001f;
        curAnchor = anchorStart + rateA * (double) s;

        if (--spawnCounter <= 0.0)
        {
            spawnGrain();
            spawnCounter += juce::jmax (8.0, (double) grainSizeSmooth / (double) density);
        }

        float sl = 0.0f, sr_ = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;

            const float w = window ((float) g.age / (float) g.length);
            sl  += capture->readSample (0, g.readPos) * w * g.panL;
            sr_ += capture->readSample (1, g.readPos) * w * g.panR;

            g.readPos += g.rate;
            if (++g.age >= g.length)
                g.active = false;
        }

        L[s] = sl * norm;
        R[s] = sr_ * norm;

        if (frozen) scanPhase += (double) scanSpeed;
    }
}
