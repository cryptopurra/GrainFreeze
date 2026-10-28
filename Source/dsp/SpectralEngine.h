#pragma once
#include <JuceHeader.h>
#include "CaptureBuffer.h"

// STFT path (2048 / hop 512, sqrt-Hann both ways).
//  LIVE   : transparent at Hold = 0; Smear averages magnitudes over time.
//  FROZEN : frames are analysed from the frozen snapshot at a scan position
//           that moves with Speed; phases are randomised so a still spectrum
//           sounds smooth instead of buzzing.
class SpectralEngine
{
public:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, hop = fftSize / 4;
    static constexpr int latencySamples() noexcept { return fftSize - hop; }

    void prepare (double sampleRate);
    void reset();
    void setCapture (CaptureBuffer* cb) noexcept { capture = cb; }
    void setSmear (float v) noexcept { smear = juce::jlimit (0.f, 1.f, v); }
    void setHold (float v) noexcept { hold = juce::jlimit (0.f, 1.f, v); }
    void setRobotize (float v) noexcept { robotize = juce::jlimit (0.f, 1.f, v); }
    void setSpeed (float v) noexcept { speed = juce::jmax (0.0f, v); }
    void setFrozen (bool f) noexcept { if (f && ! frozen) justFrozen = true; frozen = f; }
    void restartScan() noexcept { justFrozen = true; }

    void process (juce::AudioBuffer<float>& out, int numSamples);

private:
    void analyseAndSynthesise (bool frozenMode);

    CaptureBuffer* capture = nullptr;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> win {};
    std::vector<float> inRing, outRing;
    int inWrite = 0, sinceHop = 0, outRead = 0, outValid = 0;
    std::array<float, fftSize * 2> fftData {}, specData {}, synData {}, timeData {};
    std::array<float, fftSize / 2 + 1> heldMag {}, phaseAccum {};
    bool haveHeld = false, frozen = false, justFrozen = false;
    double scanPos = 0.0;
    juce::Random rng;
    float smear = 0.f, hold = 0.f, robotize = 0.f, speed = 1.f;
};
