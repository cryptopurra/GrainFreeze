#include "GrainEngine.h"

void GrainEngine::spawn (double liveNow)
{
    for (auto& g : grains)
    {
        if (g.active) continue;

        const int    len   = juce::jmax (16, (int) grainSizeSmooth);
        const double rate  = (double) pitchRatio;
        const double reach = (double) len * juce::jmax (1.0, rate);

        if (! frozen)
        {
            g.fromFrozen = false;
            g.readPos = liveNow - (reach + 64.0);      // never overtakes the write head
        }
        else
        {
            g.fromFrozen = true;
            const double span = juce::jmax (1.0, (double) capture->getFrozenLen() - reach - 2.0);
            if (justFrozen) { scanPhase = span * 0.98; justFrozen = false; }   // start at the newest audio
            const double jit = 0.05 * (double) len * (double) rng.nextFloat();
            g.readPos = std::fmod (scanPhase + jit, span);
        }

        const float pan = 0.5f + 0.3f * (rng.nextFloat() - 0.5f);
        g.active = true; g.age = 0; g.length = len; g.rate = rate;
        g.panL = std::sqrt (1.0f - pan); g.panR = std::sqrt (pan);
        return;
    }
}

void GrainEngine::process (juce::AudioBuffer<float>& out, int numSamples)
{
    out.clear();
    if (capture == nullptr) return;

    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (1);

    const double liveStart = (double) capture->getWriteHead() - (double) numSamples;
    const float density = 1.0f + overlap * 8.0f;
    const float norm = 1.0f / std::sqrt (density);

    for (int s = 0; s < numSamples; ++s)
    {
        grainSizeSmooth += (grainSizeTarget - grainSizeSmooth) * 0.002f;

        if (--spawnCounter <= 0.0)
        {
            spawn (liveStart + (double) s);
            spawnCounter += juce::jmax (8.0, (double) grainSizeSmooth / (double) density);
        }

        float sl = 0.0f, sr_ = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float w = window ((float) g.age / (float) g.length);
            if (g.fromFrozen)
            {
                sl  += capture->readFrozen (0, g.readPos) * w * g.panL;
                sr_ += capture->readFrozen (1, g.readPos) * w * g.panR;
            }
            else
            {
                sl  += capture->readLive (0, g.readPos) * w * g.panL;
                sr_ += capture->readLive (1, g.readPos) * w * g.panR;
            }
            g.readPos += g.rate;
            if (++g.age >= g.length) g.active = false;
        }
        L[s] = sl * norm;
        R[s] = sr_ * norm;

        if (frozen) scanPhase += (double) speed;
    }
}
