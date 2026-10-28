#pragma once
#include <JuceHeader.h>
#include "CaptureBuffer.h"
#include "GrainEngine.h"
#include "SpectralEngine.h"
#include "Reducer.h"

// ---------------------------------------------------------------------------
// FreezeCore  --  the whole audio chain, with no GUI / parameter-tree
// dependency so it can be tested offline:
//   input gain -> (always recording) -> grain cloud <-> STFT -> reducer
//   -> HP/LP -> volume -> LIVE/FREEZE wet switch (latency-matched dry) -> output
//   gain -> limiter
// ---------------------------------------------------------------------------
struct FreezeParams
{
    bool  frozen = false, live = true, limiter = true;
    float inGainDb = 0.f, outGainDb = 0.f;
    float grainSize = 2048.f, overlap = 0.5f, pitchSemis = 0.f, speed = 1.f;
    float volume = 1.f, reducer = 1.f;
    float specMix = 0.f, smear = 0.f, hold = 0.f, robotize = 0.f;
    float highpass = 0.f, lowpass = 1.f;
};

class FreezeCore
{
public:
    void prepare (double sampleRate, int maxBlock);
    void restartScan() { grain.restartScan(); }
    void process (juce::AudioBuffer<float>& buffer, const FreezeParams& p);

    static constexpr int latencySamples() noexcept { return SpectralEngine::latencySamples(); }
    float getInLevel (int c) const noexcept  { return inLevel[c].load(); }
    float getOutLevel (int c) const noexcept { return outLevel[c].load(); }

private:
    double sr = 44100.0;
    CaptureBuffer  capture;
    GrainEngine    grain;
    SpectralEngine spectral[2];
    Reducer        reducerFx;

    void processChunk (juce::AudioBuffer<float>& buffer, const FreezeParams& p);

    int maxBlock = 512;
    juce::AudioBuffer<float> grainBuf, specBuf, dlyBuf, dryBuf;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;
    juce::dsp::Limiter<float> limiter;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 }, grainDelay { 8192 };
    juce::SmoothedValue<float> wetSm;
    float lastIn = 1.f, lastOut = 1.f, lastVol = 1.f;
    bool wasFrozen = false, wasSpecOn = false;
    std::atomic<float> inLevel[2] { { 0.f }, { 0.f } }, outLevel[2] { { 0.f }, { 0.f } };
};
