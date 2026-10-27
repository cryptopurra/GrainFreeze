#pragma once
#include <JuceHeader.h>
#include "CaptureBuffer.h"

// ---------------------------------------------------------------------------
// SpectralEngine  --  the clean STFT path (our addition over the original).
//
// LIVE   : overlap-add STFT of the incoming signal (transparent at Hold = 0).
//          SMEAR averages magnitudes over time, HOLD freezes magnitudes and lets
//          phases run, ROBOTIZE zeroes phases.
// FROZEN : frames are analysed straight from the parked capture window at a
//          scan position that moves with Speed, phases are randomised
//          (paulstretch-style) so a still spectrum sounds smooth, not buzzy.
//
// Latency = fftSize - hop; reported to the host by the processor.
// ---------------------------------------------------------------------------

class SpectralEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void restartScan() noexcept { scanPos = 0.0; }

    void setCaptureBuffer (CaptureBuffer* cb) noexcept { capture = cb; }

    void setSmear (float v) noexcept { smear = juce::jlimit (0.f, 1.f, v); }
    void setHold  (float v) noexcept { hold  = juce::jlimit (0.f, 1.f, v); }
    void setBlur  (float v) noexcept { blur  = juce::jlimit (0.f, 1.f, v); }
    void setRobotize (float v) noexcept { robotize = juce::jlimit (0.f, 1.f, v); }
    void setScanSpeed (float v) noexcept { scanSpeed = juce::jlimit (0.f, 2.f, v); }
    void setAnchorRate (double r) noexcept { anchorRate = r; }

    static constexpr int latencySamples() noexcept { return fftSize - hop; }
    int getLatencySamples() const noexcept { return latencySamples(); }

    void process (juce::AudioBuffer<float>& out, int numSamples);

private:
    static constexpr int fftOrder = 11;            // 2048-point FFT
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hop      = fftSize / 4;   // 75% overlap

    void analyseAndSynthesise (bool frozenMode);

    CaptureBuffer* capture = nullptr;
    double sr = 44100.0;

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> win;

    std::vector<float> inRing, outRing;
    int inWrite = 0, sinceHop = 0, outRead = 0, outValid = 0;

    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2 + 1> heldMag {};
    std::array<float, fftSize / 2 + 1> phaseAccum {};
    bool haveHeld = false;

    double scanPos = 0.0;
    bool   wasFrozen = false;
    juce::Random rng;

    float smear = 0.f, hold = 0.f, blur = 0.f, robotize = 0.f, scanSpeed = 1.f;
    double anchorRate = 1.0;
};
