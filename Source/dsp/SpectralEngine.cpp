#include "SpectralEngine.h"

void SpectralEngine::prepare (double)
{

    // sqrt-Hann (periodic) used for both analysis and synthesis -> Hann overall,
    // which sums to 2.0 at 75% overlap (compensated by the 0.5 below).
    for (int i = 0; i < fftSize; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize));
        win[(size_t) i] = std::sqrt (w);
    }

    inRing.assign ((size_t) fftSize, 0.0f);
    outRing.assign ((size_t) (fftSize + hop), 0.0f);
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
    scanPos = 0.0;
    frozen = false; justFrozen = false;
}

// Expects fftData[0..2*fftSize) to already hold the windowed time-domain frame
// (interleaved re/im, im = 0). Performs the spectral processing and overlap-adds
// the result into the output ring.
void SpectralEngine::analyseAndSynthesise (bool frozenMode)
{
    // JUCE's FFT is NOT in-place safe: always use separate input / output arrays.
    fft.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (fftData.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (specData.data()), false);

    const int bins = fftSize / 2 + 1;
    for (int b = 0; b < bins; ++b)
    {
        const float re = specData[(size_t) b * 2];
        const float im = specData[(size_t) b * 2 + 1];
        const float mag = std::sqrt (re * re + im * im);
        const float phase = std::atan2 (im, re);

        if (! haveHeld) heldMag[(size_t) b] = mag;
        const float sm = smear * 0.98f;
        heldMag[(size_t) b] = sm * heldMag[(size_t) b] + (1.0f - sm) * mag;

        const float outMag = juce::jmap (hold, mag, heldMag[(size_t) b]);

        float outPhase;
        if (frozenMode)
        {
            outPhase = (rng.nextFloat() * 2.0f - 1.0f) * juce::MathConstants<float>::pi;
        }
        else
        {
            const float binFreq = juce::MathConstants<float>::twoPi * (float) hop * (float) b / (float) fftSize;
            phaseAccum[(size_t) b] = std::fmod (phaseAccum[(size_t) b] + binFreq, juce::MathConstants<float>::twoPi);
            outPhase = juce::jmap (hold, phase, phaseAccum[(size_t) b]);
        }
        outPhase = juce::jmap (robotize, outPhase, 0.0f);

        synData[(size_t) b * 2]     = outMag * std::cos (outPhase);
        synData[(size_t) b * 2 + 1] = outMag * std::sin (outPhase);
    }
    haveHeld = true;

    // DC and Nyquist must be real.
    synData[1] = 0.0f;
    synData[(size_t) (fftSize / 2) * 2 + 1] = 0.0f;

    for (int b = 1; b < fftSize / 2; ++b)
    {
        synData[(size_t) (fftSize - b) * 2]     =  synData[(size_t) b * 2];
        synData[(size_t) (fftSize - b) * 2 + 1] = -synData[(size_t) b * 2 + 1];
    }

    // JUCE's inverse transform is already normalised by 1/N.
    fft.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (synData.data()),
                 reinterpret_cast<juce::dsp::Complex<float>*> (timeData.data()), true);

    const int ringSize = (int) outRing.size();
    const int outWrite = (outRead + outValid) % ringSize;
    for (int i = 0; i < fftSize; ++i)
    {
        const int idx = (outWrite + i) % ringSize;
        outRing[(size_t) idx] += timeData[(size_t) i * 2] * win[(size_t) i] * 0.5f;
    }
    outValid = juce::jmin (ringSize, outValid + hop);
}

void SpectralEngine::process (juce::AudioBuffer<float>& out, int numSamples)
{
    out.clear();
    if (capture == nullptr) return;

    float* dst  = out.getWritePointer (0);
    float* dstR = out.getWritePointer (1);

    const double liveStart = (double) capture->getWriteHead() - (double) numSamples;
    const int ringSize = (int) outRing.size();
    const double span = juce::jmax (1.0, (double) capture->getFrozenLen() - (double) fftSize - 2.0);

    for (int s = 0; s < numSamples; ++s)
    {
        if (! frozen)
        {
            const double pos = liveStart + (double) s;
            inRing[(size_t) inWrite] = 0.5f * (capture->readLive (0, pos) + capture->readLive (1, pos));
            inWrite = (inWrite + 1) % fftSize;
        }

        if (++sinceHop >= hop)
        {
            sinceHop = 0;

            if (! frozen)
            {
                for (int i = 0; i < fftSize; ++i)
                {
                    const int idx = (inWrite + i) % fftSize;      // inWrite = oldest sample
                    fftData[(size_t) i * 2]     = inRing[(size_t) idx] * win[(size_t) i];
                    fftData[(size_t) i * 2 + 1] = 0.0f;
                }
            }
            else
            {
                if (justFrozen) { scanPos = span * 0.98; justFrozen = false; }
                const double start = std::fmod (scanPos, span);
                for (int i = 0; i < fftSize; ++i)
                {
                    const double pos = start + (double) i;
                    fftData[(size_t) i * 2]     = 0.5f * (capture->readFrozen (0, pos) + capture->readFrozen (1, pos)) * win[(size_t) i];
                    fftData[(size_t) i * 2 + 1] = 0.0f;
                }
                scanPos += (double) speed * (double) hop;
            }
            analyseAndSynthesise (frozen);
        }

        float y = 0.0f;
        if (outValid > 0)
        {
            y = outRing[(size_t) outRead];
            outRing[(size_t) outRead] = 0.0f;
            outRead = (outRead + 1) % ringSize;
            --outValid;
        }
        dst[s] = y; dstR[s] = y;
    }
}
