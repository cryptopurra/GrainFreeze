#include "SpectralEngine.h"

void SpectralEngine::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate;

    for (int i = 0; i < fftSize; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * i / (fftSize - 1)));
        analysisWin[i] = std::sqrt (w);
        synthWin[i]    = std::sqrt (w);
    }

    inRing.assign (fftSize, 0.0f);
    outRing.assign (fftSize + hop, 0.0f);
    frame.assign (fftSize, 0.0f);
    reset();
}

void SpectralEngine::reset()
{
    std::fill (inRing.begin(),  inRing.end(),  0.0f);
    std::fill (outRing.begin(), outRing.end(), 0.0f);
    heldMag.fill (0.0f);
    phaseAccum.fill (0.0f);
    inWrite = 0; sinceHop = 0; outRead = 0; outValid = 0;
    haveHeld = false;
}

void SpectralEngine::processFrame()
{
    // Assemble the most recent fftSize samples (oldest first) and window them.
    for (int i = 0; i < fftSize; ++i)
    {
        const int idx = (inWrite + i) % fftSize;   // inWrite points at oldest
        fftData[(size_t) i * 2]     = inRing[(size_t) idx] * analysisWin[i];
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

        if (! haveHeld) heldMag[(size_t) b] = mag;
        const float sm = smear * 0.98f;
        heldMag[(size_t) b] = sm * heldMag[(size_t) b] + (1.0f - sm) * mag;

        float outMag = juce::jmap (hold, mag, heldMag[(size_t) b]);

        const float binFreq = juce::MathConstants<float>::twoPi * (float) hop * (float) b / (float) fftSize;
        phaseAccum[(size_t) b] += binFreq;
        float outPhase = juce::jmap (hold, phase, phaseAccum[(size_t) b]);
        outPhase = juce::jmap (robotize, outPhase, 0.0f);

        fftData[(size_t) b * 2]     = outMag * std::cos (outPhase);
        fftData[(size_t) b * 2 + 1] = outMag * std::sin (outPhase);
    }

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

    for (int b = 1; b < fftSize / 2; ++b)
    {
        fftData[(size_t) (fftSize - b) * 2]     =  fftData[(size_t) b * 2];
        fftData[(size_t) (fftSize - b) * 2 + 1] = -fftData[(size_t) b * 2 + 1];
    }

    fft.perform (reinterpret_cast<juce::dsp::Complex<float>*> (fftData.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (fftData.data()), true);

    // Overlap-add into the output ring at the current output write position.
    const int outWrite = (outRead + outValid) % (int) outRing.size();
    for (int i = 0; i < fftSize; ++i)
    {
        const int idx = (outWrite + i) % (int) outRing.size();
        outRing[(size_t) idx] += fftData[(size_t) i * 2] * synthWin[i] / (float) fftSize;
    }
    outValid = juce::jmin ((int) outRing.size(), outValid + hop);
}

void SpectralEngine::process (juce::AudioBuffer<float>& out, int numSamples)
{
    out.clear();
    if (capture == nullptr) return;

    auto* dst  = out.getWritePointer (0);
    auto* dstR = out.getNumChannels() > 1 ? out.getWritePointer (1) : dst;

    const double anchor = capture->getAnchor();

    for (int s = 0; s < numSamples; ++s)
    {
        const float x = 0.5f * (capture->readSample (0, anchor - numSamples + s)
                              + capture->readSample (1, anchor - numSamples + s));

        inRing[(size_t) inWrite] = x;
        inWrite = (inWrite + 1) % fftSize;

        if (++sinceHop >= hop)
        {
            sinceHop = 0;
            processFrame();
        }

        float y = 0.0f;
        if (outValid > 0)
        {
            y = outRing[(size_t) outRead];
            outRing[(size_t) outRead] = 0.0f;
            outRead = (outRead + 1) % (int) outRing.size();
            --outValid;
        }
        dst[s]  = y;
        dstR[s] = y;
    }
}
