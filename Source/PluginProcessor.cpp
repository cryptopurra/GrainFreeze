#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/TempoDetector.h"

using namespace juce;

GrainFreezeAudioProcessor::GrainFreezeAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

bool GrainFreezeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void GrainFreezeAudioProcessor::prepareToPlay (double sr, int maxBlock)
{
    sampleRate = sr;

    capture.prepare (sr, maxBlock, 8.0);
    grain.prepare (sr, maxBlock);
    grain.setCaptureBuffer (&capture);
    spectral.prepare (sr, maxBlock);
    spectral.setCaptureBuffer (&capture);
    reducerFx.prepare (sr);

    grainBuf.setSize (2, maxBlock);
    specBuf.setSize  (2, maxBlock);
    dryBuf.setSize   (2, maxBlock);

    dsp::ProcessSpec spec { sr, (uint32) maxBlock, 2 };
    hpFilter.prepare (spec); hpFilter.setType (dsp::StateVariableTPTFilterType::highpass);
    lpFilter.prepare (spec); lpFilter.setType (dsp::StateVariableTPTFilterType::lowpass);
    limiter.prepare (spec);  limiter.setThreshold (-0.5f); limiter.setRelease (50.0f);

    gateSmoothed.reset (sr, 0.003);
    inGainSm.reset (sr, 0.01);
    outGainSm.reset (sr, 0.01);
    voiceGainSm.reset (sr, 0.01);

    setLatencySamples (spectral.getLatencySamples());
}

bool GrainFreezeAudioProcessor::loadSampleFile (const File& file)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    AudioBuffer<float> tmp ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&tmp, 0, (int) reader->lengthInSamples, 0, true, true);
    const double fileRate = reader->sampleRate;

    const double bpm = TempoDetector::estimateBpm (tmp, fileRate);

    // Briefly suspend the audio thread to install the resampled sample safely.
    suspendProcessing (true);
    capture.loadSample (tmp, fileRate);
    suspendProcessing (false);

    if (bpm > 0.0) detectedBpm.store (bpm);
    sampleName = file.getFileName();
    return true;
}

void GrainFreezeAudioProcessor::applyModulation (double ppq)
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    tgt.grainSize  = get (pid::grainSize);
    tgt.overlap    = get (pid::grainOverlap);
    tgt.pitchSemis = get (pid::grainPitch) / 100.0f * 24.0f;   // -100..100 -> +/-24 semis
    tgt.scanSpeed  = get (pid::scanSpeed);
    tgt.volume     = get (pid::volume);
    tgt.reducer    = get (pid::reducer);
    tgt.specMix    = get (pid::specMix);
    tgt.specSmear  = get (pid::specSmear);
    tgt.specHold   = get (pid::specHold);
    tgt.highpass   = get (pid::highpass);
    tgt.lowpass    = get (pid::lowpass);
    tgt.robotize   = get (pid::robotize);

    const auto& specs = envSpecs();
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto& m = mods[(size_t) n];
        // sync per-block control from params
        m.setEnabled ((bool) (get (pid::envField (n, "enable").toRawUTF8()) > 0.5f));
        if (! m.isEnabled()) continue;
        m.setMode     ((EnvMode)  (int) get (pid::envField (n, "mode").toRawUTF8()));
        m.setLoopMode ((LoopMode) (int) get (pid::envField (n, "loop").toRawUTF8()));
        m.setSteps    ((int) get (pid::envField (n, "steps").toRawUTF8()));
        const int barsIdx = (int) get (pid::envField (n, "bars").toRawUTF8());
        const double barsVals[] = { 0.5, 1, 2, 4, 8, 16 };
        m.setBars (barsVals[jlimit (0, 5, barsIdx)]);

        const float base = m.evaluate (ppq);   // [-1,1]

        for (auto d : specs[(size_t) n].dests)
        {
            const float depth = get (pid::envDepth (n, d).toRawUTF8());
            const float v = base * depth;
            switch (d)
            {
                case ModDest::speed:     tgt.scanSpeed  += v;                     break;
                case ModDest::pitch:     tgt.pitchSemis += v * 24.0f;             break;
                case ModDest::volume:    tgt.volume     += v;                     break;
                case ModDest::overlap:   tgt.overlap    += v;                     break;
                case ModDest::grainSize: tgt.grainSize  *= std::pow (2.0f, v);    break;
                case ModDest::specMix:   tgt.specMix    += v;                     break;
                case ModDest::specHold:  tgt.specHold   += v;                     break;
                case ModDest::highpass:  tgt.highpass   += v;                     break;
                case ModDest::lowpass:   tgt.lowpass    += v;                     break;
                case ModDest::robotize:  tgt.robotize   += v;                     break;
                default: break;
            }
        }
    }

    tgt.grainSize = jlimit (128.f, 4096.f, tgt.grainSize);
    tgt.overlap   = jlimit (0.f, 1.f, tgt.overlap);
    tgt.scanSpeed = jlimit (0.f, 2.f, tgt.scanSpeed);
    tgt.volume    = jlimit (0.f, 2.f, tgt.volume);
    tgt.reducer   = jlimit (0.f, 1.f, tgt.reducer);
    tgt.specMix   = jlimit (0.f, 1.f, tgt.specMix);
    tgt.specSmear = jlimit (0.f, 1.f, tgt.specSmear);
    tgt.specHold  = jlimit (0.f, 1.f, tgt.specHold);
    tgt.highpass  = jlimit (0.f, 1.f, tgt.highpass);
    tgt.lowpass   = jlimit (0.f, 1.f, tgt.lowpass);
    tgt.robotize  = jlimit (0.f, 1.f, tgt.robotize);
}

void GrainFreezeAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    if (apvts.getRawParameterValue (pid::bypass)->load() > 0.5f)
        return;

    double ppq = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto b = pos->getBpm())         bpm = *b;
        }
    projectBpm.store (bpm);

    // --- input gain + always-on capture ---
    inGainSm.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (pid::inputGain)->load()));
    dryBuf.makeCopyOf (buffer, true);
    buffer.applyGain (inGainSm.getCurrentValue());
    capture.writeBlock (buffer);

    // --- voice source + time-stretch + tempo sync ---
    const bool voiceOn = apvts.getRawParameterValue (pid::voiceEnable)->load() > 0.5f;
    capture.setUseSample (voiceOn);
    if (voiceOn && capture.hasSample())
    {
        const bool  sync  = apvts.getRawParameterValue (pid::stretchSync)->load() > 0.5f;
        double ratio = sync ? (projectBpm.load() / jmax (1.0, detectedBpm.load()))
                            : apvts.getRawParameterValue (pid::stretchRatio)->load();
        ratio = jlimit (0.1, 8.0, ratio);
        capture.advanceSamplePlayhead (ratio * (double) n);
    }

    // --- freeze / reset ---
    capture.setFrozen (apvts.getRawParameterValue (pid::freeze)->load() > 0.5f);
    if (apvts.getRawParameterValue (pid::reset)->load() > 0.5f)
        capture.reset();

    // --- modulation ---
    applyModulation (ppq);

    // --- grain engine ---
    grain.setGrainSize (tgt.grainSize);
    grain.setOverlap   (tgt.overlap);
    grain.setPitch     (tgt.pitchSemis);
    // In live-unfrozen voice mode the playhead supplies the time motion, so the
    // grain scan stays near the anchor; otherwise Speed scans the window.
    grain.setScanSpeed ((voiceOn && ! capture.isFrozen()) ? 0.0f : tgt.scanSpeed);
    grain.setJitter    (0.0f);
    grain.setSpread    (0.3f);
    grain.process (grainBuf, n);

    // --- spectral engine ---
    spectral.setSmear (tgt.specSmear);
    spectral.setHold  (tgt.specHold);
    spectral.setBlur  (0.0f);
    spectral.setRobotize (tgt.robotize);
    spectral.process (specBuf, n);

    // --- crossfade grain <-> spectral ---
    buffer.clear();
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* o = buffer.getWritePointer (c);
        auto* g = grainBuf.getReadPointer (jmin (c, grainBuf.getNumChannels() - 1));
        auto* s = specBuf.getReadPointer  (jmin (c, specBuf.getNumChannels()  - 1));
        const float mix = tgt.specMix;
        for (int i = 0; i < n; ++i)
            o[i] = g[i] * (1.0f - mix) + s[i] * mix;
    }

    // --- reducer ---
    reducerFx.setAmount (tgt.reducer);
    reducerFx.process (buffer, n);

    // --- filters ---
    hpFilter.setCutoffFrequency (jmap (tgt.highpass, 20.0f, 2000.0f));
    lpFilter.setCutoffFrequency (jmap (tgt.lowpass, 500.0f, 20000.0f));
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        if (tgt.highpass > 0.001f) hpFilter.process (ctx);
        if (tgt.lowpass  < 0.999f) lpFilter.process (ctx);
    }

    // --- step gate + volume ---
    seq.setEnabled    (apvts.getRawParameterValue (pid::seqOn)->load() > 0.5f);
    seq.setLength     ((int) apvts.getRawParameterValue (pid::seqLength)->load());
    seq.setResolution ((StepSequencer::Resolution) (int) apvts.getRawParameterValue (pid::seqResolution)->load());
    seq.setOffset     (apvts.getRawParameterValue (pid::seqOffset)->load());

    for (int i = 0; i < n; ++i)
    {
        const double localPpq = ppq + (double) i * (bpm / 60.0) / sampleRate;
        gateSmoothed.setTargetValue (seq.gateAt (localPpq));
        const float g = gateSmoothed.getNextValue() * tgt.volume;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.getWritePointer (c)[i] *= g;
    }
    currentStep.store (seq.isEnabled()
        ? seq.stepIndexAt (ppq + (double) (n - 1) * (bpm / 60.0) / sampleRate)
        : -1, std::memory_order_relaxed);

    // --- output gain + limiter ---
    outGainSm.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (pid::outputGain)->load()));
    buffer.applyGain (outGainSm.getCurrentValue());

    if (apvts.getRawParameterValue (pid::limiterOn)->load() > 0.5f)
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        limiter.process (ctx);
    }
}

void GrainFreezeAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void GrainFreezeAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* GrainFreezeAudioProcessor::createEditor()
{
    return new GrainFreezeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainFreezeAudioProcessor();
}
