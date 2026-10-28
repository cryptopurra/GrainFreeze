#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

GrainFreezeAudioProcessor::GrainFreezeAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Default Time envelope: the classic 100% -> 0% decay.
    std::vector<Point<float>> decay;
    for (float x : { 0.0f, 0.08f, 0.18f, 0.3f, 0.45f, 0.62f, 0.8f, 1.0f })
        decay.push_back ({ x, std::pow (1.0f - x, 2.2f) });
    mods[0].setCurve (decay);
}

void GrainFreezeAudioProcessor::loadPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& pr = presets[(size_t) index];

    // 1. everything back to default except the performance/global switches and gains
    const StringArray keep { pid::freeze, pid::bypass, pid::live, pid::limiterOn, pid::inputGain, pid::outputGain };
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (! keep.contains (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());

    // 2. curves + sequencer pattern back to default
    std::vector<Point<float>> dec;
    for (float x : { 0.0f, 0.08f, 0.18f, 0.3f, 0.45f, 0.62f, 0.8f, 1.0f })
        dec.push_back ({ x, std::pow (1.0f - x, 2.2f) });
    for (int n = 0; n < pid::numEnvelopes; ++n)
        mods[(size_t) n].setCurve (n == 0 ? dec : std::vector<Point<float>> { { 0.f, 0.5f }, { 1.f, 0.5f } });
    for (int i = 0; i < StepSequencer::maxSteps; ++i)
        seq.setStep (i, ((i % 4) < 2) ? StepSequencer::high : StepSequencer::low);

    // 3. preset content
    for (auto& kv : pr.values)
        if (auto* rp = apvts.getParameter (kv.first))
            rp->setValueNotifyingHost (rp->convertTo0to1 (kv.second));
    for (auto& e : pr.envs)
        if (isPositiveAndBelow (e.index, pid::numEnvelopes)) mods[(size_t) e.index].setCurve (e.pts);
    for (int i = 0; i < pr.seqPattern.length(); ++i)
        seq.setStep (i, pr.seqPattern[i] == '2' ? StepSequencer::high : StepSequencer::low);

    currentPreset.store (index);
    curvesRevision.fetch_add (1);
}

bool GrainFreezeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void GrainFreezeAudioProcessor::prepareToPlay (double sr, int maxBlock)
{
    sampleRate = sr;
    core.prepare (sr, jmax (64, maxBlock));
    setLatencySamples (FreezeCore::latencySamples());
}

void GrainFreezeAudioProcessor::buildParams (FreezeParams& fp, double ppq)
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    fp.limiter  = get (pid::limiterOn) > 0.5f;
    fp.live     = get (pid::live) > 0.5f;
    fp.inGainDb  = get (pid::inputGain);
    fp.outGainDb = get (pid::outputGain);

    float grainSize = get (pid::grainSize), overlap = get (pid::grainOverlap);
    float pitch = get (pid::grainPitch) / 100.0f * 24.0f, speed = get (pid::scanSpeed);
    float volume = get (pid::volume), reducer = get (pid::reducer);
    float specMix = get (pid::specMix), smear = get (pid::specSmear), hold = get (pid::specHold);
    float hp = get (pid::highpass), lp = get (pid::lowpass), robo = get (pid::robotize);

    const auto& specs = envSpecs();
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto& m = mods[(size_t) n];
        const bool en = get (pid::envField (n, "enable").toRawUTF8()) > 0.5f;
        m.setEnabled (en);
        if (! en) continue;
        m.setMode     ((EnvMode)  (int) get (pid::envField (n, "mode").toRawUTF8()));
        m.setLoopMode ((LoopMode) (int) get (pid::envField (n, "loop").toRawUTF8()));
        m.setSteps    ((int) get (pid::envField (n, "steps").toRawUTF8()));
        const double barsVals[] = { 0.5, 1, 2, 4, 8, 16 };
        m.setBars (barsVals[jlimit (0, 5, (int) get (pid::envField (n, "bars").toRawUTF8()))]);

        const float y = m.evaluate01 (ppq);          // 0..1 curve value
        const float bi = y * 2.0f - 1.0f;            // -1..1
        for (auto d : specs[(size_t) n].dests)
        {
            const float depth = get (pid::envDepth (n, d).toRawUTF8());
            switch (d)
            {
                case ModDest::speed:     speed     *= 1.0f - std::abs (depth) * (1.0f - y); break;
                case ModDest::pitch:     pitch     += bi * depth * 24.0f;                    break;
                case ModDest::volume:    volume    += bi * depth;                            break;
                case ModDest::overlap:   overlap   += bi * depth;                            break;
                case ModDest::grainSize: grainSize *= std::pow (2.0f, bi * depth);           break;
                case ModDest::specMix:   specMix   += bi * depth;                            break;
                case ModDest::specHold:  hold      += bi * depth;                            break;
                case ModDest::highpass:  hp        += bi * depth;                            break;
                case ModDest::lowpass:   lp        += bi * depth;                            break;
                case ModDest::robotize:  robo      += bi * depth;                            break;
                default: break;
            }
        }
    }

    fp.grainSize  = jlimit (128.f, 4096.f, grainSize);
    fp.overlap    = jlimit (0.f, 1.f, overlap);
    fp.pitchSemis = jlimit (-24.f, 24.f, pitch);
    fp.speed      = jlimit (0.f, 1.f, speed);
    fp.volume     = jlimit (0.f, 2.f, volume);
    fp.reducer    = jlimit (0.f, 1.f, reducer);
    fp.specMix    = jlimit (0.f, 1.f, specMix);
    fp.smear      = jlimit (0.f, 1.f, smear);
    fp.hold       = jlimit (0.f, 1.f, hold);
    fp.highpass   = jlimit (0.f, 1.f, hp);
    fp.lowpass    = jlimit (0.f, 1.f, lp);
    fp.robotize   = jlimit (0.f, 1.f, robo);
}

void GrainFreezeAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    if (get (pid::bypass) > 0.5f) return;

    double ppq = 0.0;
    bool havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }

    FreezeParams fp;
    buildParams (fp, ppq);

    // Freeze: the button, or (when the sequencer is on) the pattern: step 2 = frozen, step 1 = live.
    seq.setEnabled    (get (pid::seqOn) > 0.5f);
    seq.setLength     ((int) get (pid::seqLength));
    seq.setResolution ((StepSequencer::Resolution) (int) get (pid::seqResolution));
    bool frozen = get (pid::freeze) > 0.5f;
    if (seq.isEnabled() && havePpq)
    {
        frozen = seq.levelAt (ppq) == StepSequencer::high;
        currentStep.store (seq.stepIndexAt (ppq), std::memory_order_relaxed);
    }
    else
        currentStep.store (-1, std::memory_order_relaxed);
    fp.frozen = frozen;

    if (resetRequested.exchange (false)) { core.restartScan(); freezeSeconds.store (0.0); }

    core.process (buffer, fp);

    frozenUI.store (frozen, std::memory_order_relaxed);
    freezeSeconds.store (frozen ? freezeSeconds.load() + (double) n / sampleRate : 0.0, std::memory_order_relaxed);
}

void GrainFreezeAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return;

    auto* curves = xml->createNewChildElement ("CURVES");
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto* e = curves->createNewChildElement ("ENV");
        e->setAttribute ("n", n);
        String pts;
        const auto cv = mods[(size_t) n].getCurve();
        const auto bd = mods[(size_t) n].getBends();
        for (size_t i = 0; i < cv.size(); ++i)
            pts << String (cv[i].x, 5) << "," << String (cv[i].y, 5) << "," << String (i < bd.size() ? bd[i] : 0.0f, 4) << ";";
        e->setAttribute ("pts", pts);
    }
    String lv;
    for (int i = 0; i < StepSequencer::maxSteps; ++i) lv << seq.getStep (i);
    xml->createNewChildElement ("SEQ")->setAttribute ("levels", lv);
    xml->createNewChildElement ("PRESET")->setAttribute ("index", currentPreset.load());
    copyXmlToBinary (*xml, dest);
}

void GrainFreezeAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;

    if (auto* curves = xml->getChildByName ("CURVES"))
    {
        for (auto* e : curves->getChildIterator())
        {
            const int n = e->getIntAttribute ("n", -1);
            if (! isPositiveAndBelow (n, pid::numEnvelopes)) continue;
            std::vector<Point<float>> pts;
            std::vector<float> bends;
            for (auto& tok : StringArray::fromTokens (e->getStringAttribute ("pts"), ";", ""))
            {
                auto xy = StringArray::fromTokens (tok, ",", "");
                if (xy.size() >= 2)
                {
                    pts.push_back ({ jlimit (0.0f, 1.0f, xy[0].getFloatValue()), jlimit (0.0f, 1.0f, xy[1].getFloatValue()) });
                    bends.push_back (xy.size() > 2 ? jlimit (-1.0f, 1.0f, xy[2].getFloatValue()) : 0.0f);
                }
            }
            if (pts.size() >= 2) mods[(size_t) n].setCurve (pts, bends);
        }
        xml->removeChildElement (curves, true);
    }
    if (auto* sq = xml->getChildByName ("SEQ"))
    {
        const auto lv = sq->getStringAttribute ("levels");
        for (int i = 0; i < jmin (StepSequencer::maxSteps, lv.length()); ++i)
            seq.setStep (i, lv[i] - '0');
        xml->removeChildElement (sq, true);
    }
    if (auto* pe = xml->getChildByName ("PRESET"))
    {
        currentPreset.store (jlimit (0, (int) factoryPresets().size() - 1, pe->getIntAttribute ("index", 0)));
        xml->removeChildElement (pe, true);
    }
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));
    curvesRevision.fetch_add (1);
}

AudioProcessorEditor* GrainFreezeAudioProcessor::createEditor() { return new GrainFreezeAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GrainFreezeAudioProcessor(); }
