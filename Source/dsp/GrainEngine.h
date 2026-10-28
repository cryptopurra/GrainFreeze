#pragma once
#include <JuceHeader.h>
#include "CaptureBuffer.h"

// ---------------------------------------------------------------------------
// GrainEngine  --  windowed grain cloud.
//   LIVE   : grains read the running input a little behind "now".
//   FROZEN : grains read the frozen snapshot; their start point scans through
//            the snapshot at `Speed` (0 = standing still, 1 = real time).
// A grain remembers which source it was born from, so switching Freeze never
// cuts a grain in half.
// ---------------------------------------------------------------------------
class GrainEngine
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        for (auto& g : grains) g = Grain{};
        spawnCounter = 0.0; scanPhase = 0.0; justFrozen = false; frozen = false;
        grainSizeSmooth = grainSizeTarget;
    }

    void setCapture (CaptureBuffer* cb) noexcept { capture = cb; }
    void setGrainSize (float samples) noexcept { grainSizeTarget = samples; }
    void setOverlap   (float v) noexcept { overlap = juce::jlimit (0.f, 1.f, v); }
    void setPitch     (float semis) noexcept { pitchRatio = std::pow (2.0f, semis / 12.0f); }
    void setSpeed     (float v) noexcept { speed = juce::jmax (0.0f, v); }
    void setFrozen (bool f) noexcept { if (f && ! frozen) justFrozen = true; frozen = f; }
    void restartScan() noexcept { justFrozen = true; }

    void process (juce::AudioBuffer<float>& out, int numSamples);

private:
    struct Grain
    {
        bool active = false, fromFrozen = false;
        double readPos = 0.0, rate = 1.0;
        int age = 0, length = 0;
        float panL = 1.0f, panR = 1.0f;
    };
    static constexpr int kMaxGrains = 96;

    static float window (float ph) noexcept
    {
        constexpr float edge = 0.25f;
        if (ph < edge)        return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * ph / edge));
        if (ph > 1.0f - edge) return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (1.0f - ph) / edge));
        return 1.0f;
    }
    void spawn (double liveNow);

    CaptureBuffer* capture = nullptr;
    double sr = 44100.0;
    std::array<Grain, kMaxGrains> grains;
    float grainSizeTarget = 2048.f, grainSizeSmooth = 2048.f;
    float overlap = 0.5f, pitchRatio = 1.0f, speed = 1.0f;
    double spawnCounter = 0.0, scanPhase = 0.0;
    bool frozen = false, justFrozen = false;
    juce::Random rng;
};
