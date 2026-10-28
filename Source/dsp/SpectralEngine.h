#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// SpectralEngine  --  one channel of clean STFT processing (2048 / hop 512,
// sqrt-Hann both ways). It runs AFTER the grain engine, so Pitch, Grain Size,
// Speed and Freeze all stay active when Spectral Mix is up.
//   Smear : magnitudes are averaged over time (spectral blur).
//   Hold  : the magnitude spectrum stops updating and phases keep running.
//   Robotize : phases are zeroed.
// Latency = fftSize - hop (the caller aligns the other paths).
// ---------------------------------------------------------------------------
class SpectralEngine
{
public:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, hop = fftSize / 4;
    static constexpr int latencySamples() noexcept { return fftSize - hop; }

    void prepare();
    void reset();
    void setSmear (float v) noexcept { smear = juce::jlimit (0.f, 1.f, v); }
    void setHold (float v) noexcept { hold = juce::jlimit (0.f, 1.f, v); }
    void setRobotize (float v) noexcept { robotize = juce::jlimit (0.f, 1.f, v); }

    void process (const float* in, float* out, int numSamples);

private:
    void frame();

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> win {};
    std::vector<float> inRing, outRing;
    int inWrite = 0, sinceHop = 0, outRead = 0, outValid = 0;
    std::array<float, fftSize * 2> timeIn {}, spec {}, syn {}, timeOut {};
    std::array<float, fftSize / 2 + 1> heldMag {}, phaseAccum {};
    bool haveHeld = false;
    float smear = 0.f, hold = 0.f, robotize = 0.f;
};
