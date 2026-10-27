#include "TempoDetector.h"

double TempoDetector::estimateBpm (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples < (int) sampleRate || sampleRate <= 0.0)
        return 0.0;

    // --- 1. onset-strength envelope (energy flux) ---
    const int hop = 512;
    const int frames = numSamples / hop;
    if (frames < 16) return 0.0;

    std::vector<float> flux ((size_t) frames, 0.0f);
    const int chans = buffer.getNumChannels();
    float prevEnergy = 0.0f;

    for (int f = 0; f < frames; ++f)
    {
        float e = 0.0f;
        const int start = f * hop;
        for (int i = 0; i < hop; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < chans; ++c)
                s += buffer.getSample (c, start + i);
            s /= (float) juce::jmax (1, chans);
            e += s * s;
        }
        const float d = e - prevEnergy;
        flux[(size_t) f] = d > 0.0f ? d : 0.0f;   // half-wave rectify
        prevEnergy = e;
    }

    // Normalise & remove DC.
    float mean = 0.0f;
    for (float v : flux) mean += v;
    mean /= (float) frames;
    for (auto& v : flux) v -= mean;

    // --- 2. autocorrelation over musical lag range ---
    const double fps = sampleRate / (double) hop;   // envelope frames per second
    const int minLag = (int) std::floor (fps * 60.0 / 180.0);  // 180 BPM
    const int maxLag = (int) std::ceil  (fps * 60.0 / 60.0);   // 60 BPM
    if (minLag < 1 || maxLag >= frames) return 0.0;

    double bestScore = -1.0;
    int    bestLag   = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double acc = 0.0;
        for (int f = 0; f + lag < frames; ++f)
            acc += (double) flux[(size_t) f] * (double) flux[(size_t) (f + lag)];
        if (acc > bestScore) { bestScore = acc; bestLag = lag; }
    }
    if (bestLag <= 0) return 0.0;

    double bpm = 60.0 * fps / (double) bestLag;

    // --- 3. fold into a sensible range ---
    while (bpm < 70.0)  bpm *= 2.0;
    while (bpm > 180.0) bpm /= 2.0;
    return bpm;
}
