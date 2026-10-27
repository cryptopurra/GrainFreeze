#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

GrainFreezeAudioProcessor::GrainFreezeAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
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

    // The spectral path adds latency; report it so the host compensates.
    setLatencySamples (spectral.getLatencySamples());
}

void GrainFreezeAudioProcessor::applyModulation (double ppq)
{
    // Base (unmodulated) values from the parameter tree.
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    tgt.grainSize = get (pid::grainSize);
    tgt.overlap   = get (pid::grainOverlap);
    tgt.pitch     = get (pid::grainPitch);
    tgt.scanSpeed = get (pid::scanSpeed);
    tgt.jitter    = get (pid::grainJitter);
    tgt.spread    = get (pid::grainSpread);
    tgt.specMix   = get (pid::specMix);
    tgt.specSmear = get (pid::specSmear);
    tgt.specHold  = get (pid::specHold);
    tgt.specBlur  = get (pid::specBlur);
    tgt.highpass  = get (pid::highpass);
    tgt.lowpass   = get (pid::lowpass);
    tgt.robotize  = get (pid::robotize);
    tgt.reducer   = get (pid::reducer);
    tgt.volume    = 1.0f;

    // Modulation matrix: each enabled modulator adds to its destination.
    for (auto& m : mods)
    {
        if (! m.isEnabled()) continue;
        const float v = m.evaluate (ppq);   // ~[-1,+1]
        switch (m.destination())
        {
            case ModDest::volume:       tgt.volume    += v;               break;
            case ModDest::grainSize:    tgt.grainSize *= std::pow (2.0f, v); break;
            case ModDest::grainOverlap: tgt.overlap   += v;               break;
            case ModDest::grainPitch:   tgt.pitch     += v * 36.0f;       break;
            case ModDest::scanSpeed:    tgt.scanSpeed += v * 2.0f;        break;
            case ModDest::specMix:      tgt.specMix   += v;               break;
            case ModDest::specSmear:    tgt.specSmear += v;               break;
            case ModDest::specHold:     tgt.specHold  += v;               break;
            case ModDest::highpass:     tgt.highpass  += v;               break;
            case ModDest::lowpass:      tgt.lowpass   += v;               break;
            case ModDest::robotize:     tgt.robotize  += v;               break;
            case ModDest::reducer:      tgt.reducer   += v;               break;
            default: break;
        }
    }

    // Clamp to valid ranges.
    tgt.grainSize = jlimit (64.f, 8192.f, tgt.grainSize);
    tgt.overlap   = jlimit (0.f, 1.f, tgt.overlap);
    tgt.specMix   = jlimit (0.f, 1.f, tgt.specMix);
    tgt.specSmear = jlimit (0.f, 1.f, tgt.specSmear);
    tgt.specHold  = jlimit (0.f, 1.f, tgt.specHold);
    tgt.highpass  = jlimit (0.f, 1.f, tgt.highpass);
    tgt.lowpass   = jlimit (0.f, 1.f, tgt.lowpass);
    tgt.robotize  = jlimit (0.f, 1.f, tgt.robotize);
    tgt.reducer   = jlimit (0.f, 1.f, tgt.reducer);
    tgt.volume    = jlimit (0.f, 2.f, tgt.volume);
}

void GrainFreezeAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // --- transport for musical sync ---
    double ppq = 0.0, bpm = 120.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto p = pos->getPpqPosition())    ppq = *p;
            if (auto b = pos->getBpm())            bpm = *b;
            playing = pos->getIsPlaying();
        }
    ignoreUnused (bpm, playing);

    // --- input gain + always-on capture ---
    inGainSm.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (pid::inputGain)->load()));
    dryBuf.makeCopyOf (buffer, true);
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.applyGain (c, 0, n, inGainSm.getCurrentValue());

    // Freeze / reset handling.
    const bool freeze = apvts.getRawParameterValue (pid::freeze)->load() > 0.5f;
    capture.setFrozen (freeze);
    if (apvts.getRawParameterValue (pid::reset)->load() > 0.5f)
        capture.reset();
    capture.writeBlock (buffer);

    // --- modulation for this block ---
    applyModulation (ppq);

    // --- grain engine ---
    grain.setGrainSize (tgt.grainSize);
    grain.setOverlap   (tgt.overlap);
    grain.setPitch     (tgt.pitch);
    grain.setScanSpeed (tgt.scanSpeed);
    grain.setJitter    (tgt.jitter);
    grain.setSpread    (tgt.spread);
    grain.process (grainBuf, n);

    // --- spectral engine (our addition) ---
    spectral.setSmear (tgt.specSmear);
    spectral.setHold  (tgt.specHold);
    spectral.setBlur  (tgt.specBlur);
    spectral.setRobotize (tgt.robotize);
    spectral.process (specBuf, n);

    // --- crossfade grain <-> spectral ---
    buffer.clear();
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* out = buffer.getWritePointer (c);
        auto* g   = grainBuf.getReadPointer (jmin (c, grainBuf.getNumChannels() - 1));
        auto* s   = specBuf.getReadPointer  (jmin (c, specBuf.getNumChannels()  - 1));
        const float mix = tgt.specMix;
        for (int i = 0; i < n; ++i)
            out[i] = g[i] * (1.0f - mix) + s[i] * mix;
    }

    // --- reducer (lo-fi) ---
    reducerFx.setAmount (tgt.reducer);
    reducerFx.process (buffer, n);

    // --- HP/LP filters ---
    hpFilter.setCutoffFrequency (jmap (tgt.highpass, 20.0f, 2000.0f));
    lpFilter.setCutoffFrequency (jmap (tgt.lowpass, 500.0f, 20000.0f));
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        if (tgt.highpass > 0.001f) hpFilter.process (ctx);
        if (tgt.lowpass  < 0.999f) lpFilter.process (ctx);
    }

    // --- step gate ---
    seq.setEnabled    (apvts.getRawParameterValue (pid::seqOn)->load() > 0.5f);
    seq.setLength     ((int) apvts.getRawParameterValue (pid::seqLength)->load());
    seq.setResolution ((StepSequencer::Resolution) (int) apvts.getRawParameterValue (pid::seqResolution)->load());
    seq.setOffset     (apvts.getRawParameterValue (pid::seqOffset)->load());

    const double spqPpq = ppq;
    for (int i = 0; i < n; ++i)
    {
        const double localPpq = spqPpq + (double) i * (bpm / 60.0) / sampleRate;
        gateSmoothed.setTargetValue (seq.gateAt (localPpq));
        const float gate = gateSmoothed.getNextValue() * tgt.volume;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.getWritePointer (c)[i] *= gate;
    }
    // Publish the step under the last sample for the editor's playhead.
    currentStep.store (seq.isEnabled()
        ? seq.stepIndexAt (spqPpq + (double) (n - 1) * (bpm / 60.0) / sampleRate)
        : -1, std::memory_order_relaxed);

    // --- output gain + limiter ---
    outGainSm.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (pid::outputGain)->load()));
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.applyGain (c, 0, n, outGainSm.getCurrentValue());

    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        limiter.process (ctx);
    }

    // --- dry/wet (extra vs original) ---
    const float wet = apvts.getRawParameterValue (pid::dryWet)->load();
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* out = buffer.getWritePointer (c);
        auto* dry = dryBuf.getReadPointer (jmin (c, dryBuf.getNumChannels() - 1));
        for (int i = 0; i < n; ++i)
            out[i] = out[i] * wet + dry[i] * (1.0f - wet);
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

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainFreezeAudioProcessor();
}
