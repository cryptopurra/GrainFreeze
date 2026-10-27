#pragma once
#include <JuceHeader.h>
#include "CaptureBuffer.h"

// ---------------------------------------------------------------------------
// GrainEngine
//
// A polyphonic granular cloud. Each active grain reads a windowed snippet from
// the CaptureBuffer at a pitch-shifted rate, with its own stereo pan. Grain
// density is derived from size + overlap; new grains are scheduled as old ones
// finish, so the cloud is continuous.
//
// Per-block control values (set before process()) are the smoothed parameter
// targets; the modulation layer writes into them, so the engine itself has no
// knowledge of envelopes or the sequencer.
// ---------------------------------------------------------------------------

class GrainEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void restartScan() noexcept { scanPhase = 0.0; }

    void setCaptureBuffer (CaptureBuffer* cb) noexcept { capture = cb; }

    // --- per-block controls (already modulated) ---
    void setGrainSize   (float samples) noexcept { grainSizeTarget = samples; }
    void setOverlap     (float v)       noexcept { overlap = juce::jlimit (0.f, 1.f, v); }
    void setPitch       (float semis)   noexcept { pitchRatio = std::pow (2.0f, semis / 12.0f); }
    void setScanSpeed   (float v)       noexcept { scanSpeed = v; }
    void setJitter      (float v)       noexcept { jitter = juce::jlimit (0.f, 1.f, v); }
    void setSpread      (float v)       noexcept { spread = juce::jlimit (0.f, 1.f, v); }
    // How fast the live anchor moves per output sample (1 = live input, ratio = voice).
    void setAnchorRate  (double r)      noexcept { anchorRate = r; }

    // Renders the grain cloud into out (replacing its contents).
    void process (juce::AudioBuffer<float>& out, int numSamples);

private:
    struct Grain
    {
        bool   active = false;
        double readPos = 0.0;   // absolute position into the capture buffer
        double rate    = 1.0;   // read increment per output sample
        int    age     = 0;
        int    length  = 0;
        float  panL = 1.0f, panR = 1.0f;
    };

    static constexpr int kMaxGrains = 64;

    void spawnGrain();
    inline float window (float phase01) const noexcept; // Hann/Tukey window

    CaptureBuffer* capture = nullptr;
    double sr = 44100.0;

    std::array<Grain, kMaxGrains> grains;

    float  grainSizeTarget = 2048.f, grainSizeSmooth = 2048.f;
    float  overlap = 0.75f;
    float  pitchRatio = 1.0f;
    float  scanSpeed = 1.0f;
    float  jitter = 0.0f, spread = 0.3f;

    double anchorRate = 1.0;
    double curAnchor  = 0.0;
    double scanPhase = 0.0;     // advances the cloud's read origin
    double spawnCounter = 0.0;  // fractional grain-scheduler accumulator

    juce::Random rng;
};
