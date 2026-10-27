#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/GFLookAndFeel.h"
#include "gui/SequencerGrid.h"
#include "gui/CurveEditor.h"
#include "gui/VoicePanel.h"

// Resizable, percentage-zoomable editor with a TimeFreeze-style functional
// layout (original skin, no proprietary artwork or branding).
class GrainFreezeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit GrainFreezeAudioProcessorEditor (GrainFreezeAudioProcessor&);
    ~GrainFreezeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };
    struct EnvControls
    {
        juce::ComboBox   mode, loop, bars;
        juce::Slider     steps;
        juce::ToggleButton enable;
        std::unique_ptr<APVTS::ComboBoxAttachment> modeA, loopA, barsA;
        std::unique_ptr<APVTS::SliderAttachment>   stepsA;
        std::unique_ptr<APVTS::ButtonAttachment>   enableA;
        juce::OwnedArray<Knob> depth;   // one per destination
    };

    Knob& makeKnob (juce::Component& parent, const juce::String& paramID, const juce::String& text);
    void  selectTab (int tab);
    void  layoutCanvas();

    GrainFreezeAudioProcessor& proc;
    GFLookAndFeel lnf;

    juce::Component canvas;            // fixed-size; scaled by resized()
    static constexpr int baseW = 980, baseH = 640;

    juce::ComboBox zoomBox;

    // tabs: 0=Seq 1=Time 2=Pitch 3..6=Misc1..4
    juce::OwnedArray<juce::TextButton> tabs;
    int selectedTab = 0;

    // display area
    SequencerGrid seqGrid { proc.sequencer() };
    juce::OwnedArray<CurveEditor> curves;         // 6
    juce::OwnedArray<EnvControls> envCtrls;       // 6

    // seq controls
    Knob seqLength, seqOffset;
    juce::ComboBox seqRes; std::unique_ptr<APVTS::ComboBoxAttachment> seqResA;
    juce::ToggleButton seqOn; std::unique_ptr<APVTS::ButtonAttachment> seqOnA;

    // main knobs
    juce::OwnedArray<Knob> mainKnobs;

    // transport buttons
    juce::ToggleButton freezeBtn { "FREEZE" }, bypassBtn { "Bypass" }, limiterBtn { "Limiter" };
    juce::TextButton   resetBtn  { "Reset" };
    std::unique_ptr<APVTS::ButtonAttachment> freezeA, bypassA, limiterA;

    // voice section
    VoicePanel voice;
    juce::ToggleButton voiceEnable { "Voice" }, stretchSync { "Sync Tempo" };
    std::unique_ptr<APVTS::ButtonAttachment> voiceEnableA, stretchSyncA;
    Knob voiceGain, stretchRatio;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeAudioProcessorEditor)
};
