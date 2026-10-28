#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "dsp/FreezeCore.h"
#include "dsp/StepModulator.h"
#include "dsp/StepSequencer.h"

class GrainFreezeAudioProcessor : public juce::AudioProcessor
{
public:
    GrainFreezeAudioProcessor();
    ~GrainFreezeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GrainFreeze"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    StepSequencer& sequencer()       noexcept { return seq; }
    StepModulator& modulator (int i) noexcept { return mods[(size_t) i]; }
    void requestReset() noexcept { resetRequested.store (true); }

    // UI feedback written by the audio thread
    std::atomic<int>    currentStep { -1 };
    std::atomic<bool>   frozenUI { false };
    std::atomic<double> freezeSeconds { 0.0 };
    std::atomic<int>    curvesRevision { 0 };
    float getInLevel (int c) const noexcept  { return core.getInLevel (c); }
    float getOutLevel (int c) const noexcept { return core.getOutLevel (c); }

private:
    void buildParams (FreezeParams& fp, double ppq);

    FreezeCore core;
    StepSequencer seq;
    std::array<StepModulator, pid::numEnvelopes> mods;
    std::atomic<bool> resetRequested { false };
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeAudioProcessor)
};
