#pragma once
#include <JuceHeader.h>
#include "CaptureBuffer.h"

// ---------------------------------------------------------------------------
// SpectralEngine  --  the clean STFT path (our addition over the original).
//
// Overlap-add STFT with a Hann window. On each frame we can:
//   - SMEAR : exponential time-averaging of magnitudes (spectral blur in time)
//   - HOLD  : freeze the magnitude spectrum, let phases advance by bin freq
//             (true phase-vocoder style freeze, no metallic static)
//   - BLUR  : bin-to-bin magnitude diffusion (frequency smearing)
//
// It reads the same CaptureBuffer as the grain engine, so LIVE/FREEZE applies
// to both consistently. Latency = fftSize - hop; reported to the host.
// ---------------------------------------------------------------------------

class SpectralEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setCaptureBuffer (CaptureBuffer* cb) noexcept { capture = cb; }

    void setSmear (float v) noexcept { smear = juce::jlimit (0.f, 1.f, v); }
    void setHold  (float v) noexcept { hold  = juce::jlimit (0.f, 1.f, v); }
    void setBlur  (float v) noexcept { blur  = juce::jlimit (0.f, 1.f, v); }
    void setRobotize (float v) noexcept { robotize = juce::jlimit (0.f, 1.f, v); }

    int getLatencySamples() const noexcept { return fftSize - hop; }

    // Renders the spectral path for numSamples into out (replacing contents).
    void process (juce::AudioBuffer<float>& out, int numSamples);

private:
    static constexpr int fftOrder = 11;            // 2048-point FFT
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hop      = fftSize / 4;   // 75% overlap

    void processFrame();

    CaptureBuffer* capture = nullptr;
    double sr = 44100.0;

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> analysisWin;
    std::array<float, fftSize> synthWin;

    // Sliding input / output accumulators for overlap-add.
    std::vector<float> inFifo, outFifo;
    int fifoFill = 0, outReadPos = 0;

    std::array<float, fftSize * 2> fftData {};     // interleaved re/im work area
    std::array<float, fftSize / 2 + 1> heldMag {}; // smoothed / held magnitudes
    std::array<float, fftSize / 2 + 1> phaseAccum {};
    bool  haveHeld = false;

    float smear = 0.f, hold = 0.f, blur = 0.f, robotize = 0.f;
};
