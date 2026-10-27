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
    // Tukey-ish window: raised-cosine fades, flat centre. Keeps grains smooth
    // without over-softening transients. (Original implementation.)
    constexpr float edge = 0.25f; // fraction of grain spent fading in/out
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

        const int   len   = juce::jmax (16, (int) grainSizeSmooth);
        const double anchor = capture->getAnchor();

        // Read origin: scan position behind the anchor, plus optional jitter.
        double jit = jitter * (rng.nextFloat() * 2.0f - 1.0f) * (double) len;
        double origin = anchor - scanPhase - jit;

        const float panPos = 0.5f + spread * (rng.nextFloat() - 0.5f);

        g.active  = true;
        g.age     = 0;
        g.length  = len;
        g.readPos = origin;
        g.rate    = (double) pitchRatio;
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

    // Grain spacing from size & overlap: higher overlap -> grains closer.
    const float sizeNow = grainSizeSmooth;
    const float density = juce::jmax (1.0f, 1.0f + overlap * 8.0f);  // grains overlapping
    const double spawnInterval = juce::jmax (8.0, (double) sizeNow / density);

    for (int s = 0; s < numSamples; ++s)
    {
        // Smooth grain size to avoid zipper noise when modulated fast.
        grainSizeSmooth += (grainSizeTarget - grainSizeSmooth) * 0.001f;

        // Schedule new grains.
        if (--spawnCounter <= 0.0)
        {
            spawnGrain();
            spawnCounter += spawnInterval;
        }

        float sl = 0.0f, sr_ = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;

            const float ph = (float) g.age / (float) g.length;
            const float w  = window (ph);
            const float smp = capture->readSample (0, g.readPos);
            const float smpR = capture->getNumChannels() > 1
                                 ? capture->readSample (1, g.readPos) : smp;

            sl += smp  * w * g.panL;
            sr_ += smpR * w * g.panR;

            g.readPos += g.rate;
            if (++g.age >= g.length)
                g.active = false;
        }

        // Normalise roughly by sqrt of overlap so level stays stable.
        const float norm = 1.0f / std::sqrt (density);
        L[s] = sl * norm;
        R[s] = sr_ * norm;

        // Advance the cloud's scan origin (negative speed = reverse read).
        scanPhase += (double) scanSpeed;
    }
}
