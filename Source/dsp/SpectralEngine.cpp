#include "SpectralEngine.h"

void SpectralEngine::prepare()
{
    for (int i = 0; i < fftSize; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize));
        win[(size_t) i] = std::sqrt (w);          // sqrt-Hann; analysis * synthesis = Hann
    }
    inRing.assign ((size_t) fftSize, 0.0f);
    outRing.assign ((size_t) (fftSize + hop), 0.0f);
    reset();
}

void SpectralEngine::reset()
{
    std::fill (inRing.begin(), inRing.end(), 0.0f);
    std::fill (outRing.begin(), outRing.end(), 0.0f);
    heldMag.fill (0.0f);
    phaseAccum.fill (0.0f);
    inWrite = sinceHop = outRead = outValid = 0;
    haveHeld = false;
}

void SpectralEngine::frame()
{
    for (int i = 0; i < fftSize; ++i)
    {
        timeIn[(size_t) i * 2]     = inRing[(size_t) ((inWrite + i) % fftSize)] * win[(size_t) i];
        timeIn[(size_t) i * 2 + 1] = 0.0f;
    }

    // JUCE's FFT is not in-place safe: separate input and output arrays.
    fft.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (timeIn.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (spec.data()), false);

    const float amount = juce::jmax (smear, hold);                 // how much of the held spectrum is used
    const float coef   = juce::jmax (0.98f * smear, hold);         // 1.0 = spectrum stops updating
    const int bins = fftSize / 2 + 1;

    for (int b = 0; b < bins; ++b)
    {
        const float re = spec[(size_t) b * 2], im = spec[(size_t) b * 2 + 1];
        const float mag = std::sqrt (re * re + im * im);
        const float phase = std::atan2 (im, re);

        if (! haveHeld) heldMag[(size_t) b] = mag;
        heldMag[(size_t) b] = coef * heldMag[(size_t) b] + (1.0f - coef) * mag;
        const float outMag = juce::jmap (amount, mag, heldMag[(size_t) b]);

        const float binFreq = juce::MathConstants<float>::twoPi * (float) hop * (float) b / (float) fftSize;
        phaseAccum[(size_t) b] = std::fmod (phaseAccum[(size_t) b] + binFreq, juce::MathConstants<float>::twoPi);
        float outPhase = juce::jmap (hold, phase, phaseAccum[(size_t) b]);
        outPhase = juce::jmap (robotize, outPhase, 0.0f);

        syn[(size_t) b * 2]     = outMag * std::cos (outPhase);
        syn[(size_t) b * 2 + 1] = outMag * std::sin (outPhase);
    }
    haveHeld = true;

    syn[1] = 0.0f;
    syn[(size_t) (fftSize / 2) * 2 + 1] = 0.0f;
    for (int b = 1; b < fftSize / 2; ++b)
    {
        syn[(size_t) (fftSize - b) * 2]     =  syn[(size_t) b * 2];
        syn[(size_t) (fftSize - b) * 2 + 1] = -syn[(size_t) b * 2 + 1];
    }

    fft.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (syn.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (timeOut.data()), true);   // already scaled by 1/N

    const int ringSize = (int) outRing.size();
    const int outWrite = (outRead + outValid) % ringSize;
    for (int i = 0; i < fftSize; ++i)
        outRing[(size_t) ((outWrite + i) % ringSize)] += timeOut[(size_t) i * 2] * win[(size_t) i] * 0.5f;
    outValid = juce::jmin (ringSize, outValid + hop);
}

void SpectralEngine::process (const float* in, float* out, int numSamples)
{
    const int ringSize = (int) outRing.size();
    for (int s = 0; s < numSamples; ++s)
    {
        inRing[(size_t) inWrite] = in[s];
        inWrite = (inWrite + 1) % fftSize;

        if (++sinceHop >= hop) { sinceHop = 0; frame(); }

        float y = 0.0f;
        if (outValid > 0)
        {
            y = outRing[(size_t) outRead];
            outRing[(size_t) outRead] = 0.0f;
            outRead = (outRead + 1) % ringSize;
            --outValid;
        }
        out[s] = y;
    }
}
