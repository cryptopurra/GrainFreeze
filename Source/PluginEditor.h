#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/GFLookAndFeel.h"
#include "gui/SequencerGrid.h"
#include "gui/CurveEditor.h"
#include "gui/ValueBox.h"
#include "gui/CycleButton.h"
#include "gui/Meter.h"

// ---------------------------------------------------------------------------
// Editor: TimeFreeze-style functional layout (meters + gain knobs left/right,
// tabbed envelope display in the middle, knob clusters around a big FREEZE
// button). Original skin; resizable and percentage-zoomable (the fixed-size canvas is
// scaled to the window).
// ---------------------------------------------------------------------------

class GrainFreezeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit GrainFreezeAudioProcessorEditor (GrainFreezeAudioProcessor&);
    ~GrainFreezeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    static constexpr int baseW = 720, baseH = 484;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };

    struct EnvUI
    {
        std::unique_ptr<CurveEditor> curve;
        std::unique_ptr<CycleButton> mode, bars;
        juce::TextButton loopBtn { "Loop" }, pingBtn { "Ping Pong" };
        std::unique_ptr<juce::ParameterAttachment> loopAtt, stepsAtt;
        int loopIndex = 1;
        std::unique_ptr<ValueBox> steps;
        juce::Label stepsLabel;
        juce::OwnedArray<Knob> depth;
    };

    struct Param          // knob + optional value box + caption
    {
        std::unique_ptr<Knob> knob;
        std::unique_ptr<ValueBox> box;
        juce::Label caption;
    };

    Knob* makeKnob (juce::Component& parent, const juce::String& paramID, const juce::String& text,
                    bool big, int textBox);
    Param* makeParam (const char* id, const juce::String& caption, bool wantKnob, bool wantBox);
    juce::Label* makeCaption (const juce::String& text, juce::Justification j = juce::Justification::centred);
    void selectTab (int tab);
    void layoutCanvas();
    juce::RangedAudioParameter& P (const juce::String& id) { return *proc.apvts.getParameter (id); }

    GrainFreezeAudioProcessor& proc;
    GFLookAndFeel lnf;

    juce::Component canvas;
    juce::ComboBox zoomBox;

    // tabs: 0=Seq 1=Time 2=Pitch 3..6=Misc1..4
    juce::OwnedArray<juce::TextButton>   tabs;
    juce::OwnedArray<juce::ToggleButton> tabLeds;
    juce::OwnedArray<APVTS::ButtonAttachment> tabLedAtt;
    int selectedTab = 0;
    int lastCurvesRevision = 0;

    // display contents
    SequencerGrid seqGrid { proc.sequencer() };
    std::unique_ptr<CycleButton> seqRes;
    std::unique_ptr<ValueBox> seqLength;
    juce::Label seqLengthLbl;
    juce::OwnedArray<EnvUI> envUI;

    // main clusters
    juce::OwnedArray<Param> params;
    juce::OwnedArray<juce::Label> captions;
    Param *pOverlap = nullptr, *pGrain = nullptr, *pSpeed = nullptr, *pPitch = nullptr,
          *pSpecMix = nullptr, *pSmear = nullptr, *pVolume = nullptr, *pReducer = nullptr;

    // gain strips
    Meter meterIn { true }, meterOut { false };
    std::unique_ptr<Knob> inKnob, outKnob;
    std::unique_ptr<ValueBox> inBox, outBox;

    // transport
    juce::ToggleButton freezeBtn { "FREEZE" }, bypassBtn { "Bypass" }, limiterBtn { "Limiter" }, liveBtn { "LIVE" };
    juce::TextButton   resetBtn { "Reset" };
    std::unique_ptr<APVTS::ButtonAttachment> freezeA, bypassA, limiterA, liveA;

    struct LedTime : juce::Component
    {
        void set (double sec)
        {
            const int ms = (int) (sec * 1000.0);
            auto t = juce::String::formatted ("-%02d:%02d:%03d", (ms / 60000) % 100, (ms / 1000) % 60, ms % 1000);
            if (t != text) { text = t; repaint(); }
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff070708)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colour (0xff2c3038)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (juce::Colour (0xffff5a1f));
            g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), r.getHeight() * 0.72f, juce::Font::bold)));
            g.drawText (text, r, juce::Justification::centred);
        }
        juce::String text { "-00:00:000" };
    } ledTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeAudioProcessorEditor)
};
