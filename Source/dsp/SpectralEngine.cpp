#include "SpectralEngine.h"

void SpectralEngine::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate;

    // Hann analysis & synthesis windows (sqrt-Hann pair -> flat overlap-add).
    for (int i = 0; i < fftSize; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * i / (fftSize - 1)));
        analysisWin[i] = std::sqrt (w);
        synthWin[i]    = std::sqrt (w);
    }

    inFifo.assign  (fftSize, 0.0f);
    outFifo.assign (fftSize * 2, 0.0f);
    reset();
}

void SpectralEngine::reset()
{
    std::fill (inFifo.begin(),  inFifo.end(),  0.0f);
    std::fill (outFifo.begin(), outFifo.end(), 0.0f);
    heldMag.fill (0.0f);
    phaseAccum.fill (0.0f);
    fifoFill   = 0;
    outReadPos = 0;
    haveHeld   = false;
}

void SpectralEngine::processFrame()
{
    // Window the current frame into the interleaved FFT work area.
    for (int i = 0; i < fftSize; ++i)
    {
        fftData[(size_t) i * 2]     = inFifo[(size_t) i] * analysisWin[i];
        fftData[(size_t) i * 2 + 1] = 0.0f;
    }

    fft.perform (reinterpret_cast<juce::dsp::Complex<float>*> (fftData.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (fftData.data()), false);

    const int bins = fftSize / 2 + 1;
    for (int b = 0; b < bins; ++b)
    {
        float re = fftData[(size_t) b * 2];
        float im = fftData[(size_t) b * 2 + 1];
        float mag = std::sqrt (re * re + im * im);
        float phase = std::atan2 (im, re);

        // SMEAR: exponential magnitude averaging across frames.
        if (! haveHeld) heldMag[(size_t) b] = mag;
        const float sm = smear * 0.98f;
        heldMag[(size_t) b] = sm * heldMag[(size_t) b] + (1.0f - sm) * mag;

        // HOLD: blend current magnitude toward the held magnitude.
        float outMag = juce::jmap (hold, mag, heldMag[(size_t) b]);

        // Phase handling:
        //  - normal: keep measured phase
        //  - hold>0: advance phase by nominal bin frequency (vocoder freeze)
        //  - robotize: zero phase (phase-locked, "robot" timbre)
        float binFreq = juce::MathConstants<float>::twoPi * (float) hop * (float) b / (float) fftSize;
        phaseAccum[(size_t) b] += binFreq;
        float frozenPhase = phaseAccum[(size_t) b];
        float outPhase = juce::jmap (hold, phase, frozenPhase);
        outPhase = juce::jmap (robotize, outPhase, 0.0f);

        fftData[(size_t) b * 2]     = outMag * std::cos (outPhase);
        fftData[(size_t) b * 2 + 1] = outMag * std::sin (outPhase);
    }

    // BLUR: simple 3-tap bin smoothing of the complex spectrum.
    if (blur > 0.0f)
    {
        std::array<float, fftSize * 2> tmp = fftData;
        const float a = blur * 0.5f;
        for (int b = 1; b < bins - 1; ++b)
            for (int k = 0; k < 2; ++k)
            {
                float c = tmp[(size_t) b * 2 + k];
                float l = tmp[(size_t) (b - 1) * 2 + k];
                float r = tmp[(size_t) (b + 1) * 2 + k];
                fftData[(size_t) b * 2 + k] = (1.0f - a) * c + a * 0.5f * (l + r);
            }
    }

    haveHeld = true;

    // Rebuild the negative-frequency half (conjugate symmetry) before inverse.
    for (int b = 1; b < fftSize / 2; ++b)
    {
        fftData[(size_t) (fftSize - b) * 2]     =  fftData[(size_t) b * 2];
        fftData[(size_t) (fftSize - b) * 2 + 1] = -fftData[(size_t) b * 2 + 1];
    }

    fft.perform (reinterpret_cast<juce::dsp::Complex<float>*> (fftData.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (fftData.data()), true);

    // Overlap-add windowed output.
    for (int i = 0; i < fftSize; ++i)
    {
        const float s = fftData[(size_t) i * 2] * synthWin[i] / (float) fftSize;
        outFifo[(size_t) (outReadPos + i) % outFifo.size()] += s;
    }
}

void SpectralEngine::process (juce::AudioBuffer<float>& out, int numSamples)
{
    out.clear();
    if (capture == nullptr) { return; }

    auto* dst = out.getWritePointer (0);
    auto* dstR = out.getNumChannels() > 1 ? out.getWritePointer (1) : dst;

    // Pull mono input from the capture buffer at the read anchor (spectral
    // path is mono-summed for simplicity; stereo can be added as two FFTs).
    const double anchor = capture->getAnchor();

    for (int s = 0; s < numSamples; ++s)
    {
        float x = 0.5f * (capture->readSample (0, anchor - numSamples + s)
                        + capture->readSample (1, anchor - numSamples + s));

        inFifo.erase (inFifo.begin());
        inFifo.push_back (x);

        if (++fifoFill >= hop)
        {
            fifoFill = 0;
            processFrame();
            outReadPos = (outReadPos + hop) % (int) outFifo.size();
        }

        const int rp = (outReadPos + s) % (int) outFifo.size();
        float y = outFifo[(size_t) rp];
        outFifo[(size_t) rp] = 0.0f;  // consume

        dst[s]  = y;
        dstR[s] = y;
    }
}
