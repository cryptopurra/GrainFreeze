#include "PluginEditor.h"

using namespace juce;

GrainFreezeAudioProcessorEditor::Knob&
GrainFreezeAudioProcessorEditor::makeKnob (Component& parent, const String& paramID, const String& text)
{
    auto* k = new Knob();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 14);
    k->slider.setLookAndFeel (&lnf);
    parent.addAndMakeVisible (k->slider);
    k->label.setText (text, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    parent.addAndMakeVisible (k->label);
    k->attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramID, k->slider);
    mainKnobs.add (k);   // ownership sink; group pointers tracked separately where needed
    return *k;
}

GrainFreezeAudioProcessorEditor::GrainFreezeAudioProcessorEditor (GrainFreezeAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);

    // ---- zoom ----
    addAndMakeVisible (zoomBox);
    zoomBox.addItemList ({ "50%", "75%", "100%", "125%", "150%", "200%" }, 1);
    zoomBox.setSelectedId (3, dontSendNotification);
    zoomBox.onChange = [this]
    {
        const int pct[] = { 50, 75, 100, 125, 150, 200 };
        const int z = pct[jlimit (0, 5, zoomBox.getSelectedId() - 1)];
        setSize (baseW * z / 100, baseH * z / 100);
    };

    // ---- tabs ----
    const char* tabNames[] = { "Seq", "Time", "Pitch", "Misc 1", "Misc 2", "Misc 3", "Misc 4" };
    for (int i = 0; i < 7; ++i)
    {
        auto* b = new TextButton (tabNames[i]);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (100);
        b->setColour (TextButton::buttonOnColourId, Colour (0xff3b4250));
        b->onClick = [this, i] { selectTab (i); };
        canvas.addAndMakeVisible (b);
        tabs.add (b);
    }

    // ---- sequencer display + controls ----
    canvas.addChildComponent (seqGrid);
    seqLength.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    seqLength.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 50, 14);
    // seq length/offset/res/on use their own attachments
    seqLength.attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, pid::seqLength, seqLength.slider);
    seqLength.label.setText ("Length", dontSendNotification); seqLength.label.setJustificationType (Justification::centred);
    canvas.addChildComponent (seqLength.slider); canvas.addChildComponent (seqLength.label);

    seqOffset.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    seqOffset.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 50, 14);
    seqOffset.attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, pid::seqOffset, seqOffset.slider);
    seqOffset.label.setText ("Offset", dontSendNotification); seqOffset.label.setJustificationType (Justification::centred);
    canvas.addChildComponent (seqOffset.slider); canvas.addChildComponent (seqOffset.label);

    seqRes.addItemList (resolutionNames(), 1);
    seqResA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, pid::seqResolution, seqRes);
    canvas.addChildComponent (seqRes);
    seqOn.setButtonText ("On");
    seqOnA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::seqOn, seqOn);
    canvas.addChildComponent (seqOn);

    // ---- envelopes: curve editors + per-tab controls + depth knobs ----
    const auto& specs = envSpecs();
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto* ce = new CurveEditor (proc.modulator (n));
        canvas.addChildComponent (ce);
        curves.add (ce);

        auto* ec = new EnvControls();
        ec->mode.addItemList ({ "Normal", "Step" }, 1);
        ec->loop.addItemList ({ "One-Shot", "Loop", "Ping-Pong" }, 1);
        ec->bars.addItemList (barsNames(), 1);
        ec->modeA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, pid::envField (n, "mode"), ec->mode);
        ec->loopA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, pid::envField (n, "loop"), ec->loop);
        ec->barsA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, pid::envField (n, "bars"), ec->bars);
        ec->steps.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        ec->steps.setTextBoxStyle (Slider::TextBoxBelow, false, 46, 14);
        ec->stepsA = std::make_unique<APVTS::SliderAttachment> (proc.apvts, pid::envField (n, "steps"), ec->steps);
        ec->enable.setButtonText ("Enable");
        ec->enableA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::envField (n, "enable"), ec->enable);
        for (auto* c : { (Component*) &ec->mode, &ec->loop, &ec->bars, &ec->steps, &ec->enable })
            canvas.addChildComponent (c);

        // keep mode combo driving the curve editor's draw style
        ec->mode.onChange = [this, n, ec]
        { curves[n]->setMode ((EnvMode) jmax (0, ec->mode.getSelectedId() - 1)); };

        for (auto d : specs[(size_t) n].dests)
        {
            auto* k = new Knob();
            k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 54, 14);
            k->slider.setLookAndFeel (&lnf);
            k->label.setText (modDestLabel (d), dontSendNotification);
            k->label.setJustificationType (Justification::centred);
            k->attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, pid::envDepth (n, d), k->slider);
            canvas.addChildComponent (k->slider);
            canvas.addChildComponent (k->label);
            ec->depth.add (k);
        }
        envCtrls.add (ec);
    }

    // ---- main knobs ----
    struct MK { const char* id; const char* lbl; };
    const MK mk[] = {
        { pid::grainOverlap, "Overlap" }, { pid::grainSize, "Grain Size" },
        { pid::specMix, "Spec Mix" },     { pid::specSmear, "Smear" },
        { pid::scanSpeed, "Speed" },      { pid::volume, "Volume" },
        { pid::grainPitch, "Pitch" },     { pid::reducer, "Reducer" },
        { pid::inputGain, "Input" },      { pid::outputGain, "Output" }
    };
    for (auto& m : mk) makeKnob (canvas, m.id, m.lbl);

    // ---- transport ----
    freezeBtn.setClickingTogglesState (true);
    canvas.addAndMakeVisible (freezeBtn);
    freezeA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::freeze, freezeBtn);
    canvas.addAndMakeVisible (resetBtn);
    resetBtn.onClick = [this]
    {
        if (auto* pr = proc.apvts.getParameter (pid::reset))
        { pr->setValueNotifyingHost (1.0f); pr->setValueNotifyingHost (0.0f); }
    };
    canvas.addAndMakeVisible (bypassBtn);
    bypassA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::bypass, bypassBtn);
    canvas.addAndMakeVisible (limiterBtn);
    limiterA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::limiterOn, limiterBtn);

    // ---- voice ----
    canvas.addAndMakeVisible (voice);
    voice.onFileChosen = [this] (File f)
    {
        if (! proc.loadSampleFile (f))
            NativeMessageBox::showMessageBoxAsync (MessageBoxIconType::WarningIcon,
                "GrainFreeze", "Could not load this audio file.");
    };
    canvas.addAndMakeVisible (voiceEnable);
    voiceEnableA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::voiceEnable, voiceEnable);
    canvas.addAndMakeVisible (stretchSync);
    stretchSyncA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::stretchSync, stretchSync);
    voiceGain.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    voiceGain.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 54, 14);
    voiceGain.slider.setLookAndFeel (&lnf);
    voiceGain.attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, pid::voiceGain, voiceGain.slider);
    voiceGain.label.setText ("Voice Gain", dontSendNotification); voiceGain.label.setJustificationType (Justification::centred);
    canvas.addAndMakeVisible (voiceGain.slider); canvas.addAndMakeVisible (voiceGain.label);
    stretchRatio.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    stretchRatio.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 54, 14);
    stretchRatio.slider.setLookAndFeel (&lnf);
    stretchRatio.attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, pid::stretchRatio, stretchRatio.slider);
    stretchRatio.label.setText ("Stretch", dontSendNotification); stretchRatio.label.setJustificationType (Justification::centred);
    canvas.addAndMakeVisible (stretchRatio.slider); canvas.addAndMakeVisible (stretchRatio.label);

    tabs[0]->setToggleState (true, dontSendNotification);
    selectTab (0);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
        setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    }
    setSize (baseW, baseH);
    startTimerHz (30);
}

GrainFreezeAudioProcessorEditor::~GrainFreezeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GrainFreezeAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff17191e));
    // Scaled panels behind the canvas content.
    const float s = (float) getWidth() / (float) baseW;
    g.addTransform (AffineTransform::scale (s));
    g.setColour (Colour (0xff1f232b));
    g.fillRoundedRectangle (14.f, 42.f, baseW - 28.f, 234.f, 8.f);   // display
    g.setColour (Colour (0xffe8edf5).withAlpha (0.9f));
    g.setFont (16.0f);
    g.drawText ("GrainFreeze", 16, 8, 240, 24, Justification::left);
}

void GrainFreezeAudioProcessorEditor::selectTab (int tab)
{
    selectedTab = tab;
    seqGrid.setVisible (tab == 0);
    seqLength.slider.setVisible (tab == 0); seqLength.label.setVisible (tab == 0);
    seqOffset.slider.setVisible (tab == 0); seqOffset.label.setVisible (tab == 0);
    seqRes.setVisible (tab == 0); seqOn.setVisible (tab == 0);

    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        const bool on = (tab == n + 1);
        curves[n]->setVisible (on);
        auto* ec = envCtrls[n];
        ec->mode.setVisible (on); ec->loop.setVisible (on); ec->bars.setVisible (on);
        ec->steps.setVisible (on); ec->enable.setVisible (on);
        for (auto* k : ec->depth) { k->slider.setVisible (on); k->label.setVisible (on); }
    }
    layoutCanvas();
}

void GrainFreezeAudioProcessorEditor::resized()
{
    // Scale the fixed-size canvas to fill the resizable window (percentage zoom).
    const float scale = (float) getWidth() / (float) baseW;
    canvas.setTransform (AffineTransform::scale (scale));
    canvas.setBounds (0, 0, baseW, baseH);
    zoomBox.setBounds (getWidth() - 86, 6, 74, 22);
    layoutCanvas();
}

void GrainFreezeAudioProcessorEditor::layoutCanvas()
{
    auto area = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (14);

    // Title strip
    auto top = area.removeFromTop (28);
    bypassBtn.setBounds (top.removeFromRight (90));
    limiterBtn.setBounds (top.removeFromRight (90));

    // --- display panel (tabs + content) ---
    auto display = area.removeFromTop (210);
    auto tabRow = display.removeFromTop (24);
    const int tw = tabRow.getWidth() / 7;
    for (int i = 0; i < 7; ++i)
        tabs[i]->setBounds (tabRow.removeFromLeft (tw).reduced (1));

    auto content = display.reduced (2);
    auto rightCol = content.removeFromRight (120);     // per-tab mode/bars/loop/steps
    auto depthRow = content.removeFromBottom (78);     // depth knobs row

    // content (left) = grid or curve
    seqGrid.setBounds (content.reduced (4));
    for (auto* ce : curves) ce->setBounds (content.reduced (4));

    // right column controls
    if (selectedTab == 0)
    {
        auto rc = rightCol.reduced (4);
        seqOn.setBounds (rc.removeFromTop (24));
        seqRes.setBounds (rc.removeFromTop (24).reduced (0, 2));
        auto k = rc.removeFromTop (70);
        seqLength.slider.setBounds (k.removeFromLeft (56)); seqLength.label.setBounds (seqLength.slider.getX(), seqLength.slider.getBottom() - 2, 56, 12);
        seqOffset.slider.setBounds (k.removeFromLeft (56)); seqOffset.label.setBounds (seqOffset.slider.getX(), seqOffset.slider.getBottom() - 2, 56, 12);
    }
    else
    {
        const int n = selectedTab - 1;
        auto* ec = envCtrls[n];
        auto rc = rightCol.reduced (4);
        ec->enable.setBounds (rc.removeFromTop (22));
        ec->mode.setBounds (rc.removeFromTop (22).reduced (0, 1));
        ec->bars.setBounds (rc.removeFromTop (22).reduced (0, 1));
        ec->loop.setBounds (rc.removeFromTop (22).reduced (0, 1));
        ec->steps.setBounds (rc.removeFromTop (60));

        // depth knobs row
        auto dr = depthRow;
        const int cnt = ec->depth.size();
        const int kw = cnt > 0 ? dr.getWidth() / cnt : dr.getWidth();
        for (auto* k : ec->depth)
        {
            auto cell = dr.removeFromLeft (kw);
            k->slider.setBounds (cell.removeFromTop (60));
            k->label.setBounds (cell);
        }
    }

    area.removeFromTop (8);

    // --- centre: FREEZE + reset ---
    auto centre = area.removeFromTop (120);
    auto freezeArea = centre.withSizeKeepingCentre (150, 90);
    freezeBtn.setBounds (freezeArea.removeFromTop (60));
    resetBtn.setBounds (freezeArea.removeFromTop (26).reduced (24, 0));

    // --- main knobs row ---
    auto knobRow = area.removeFromTop (110);
    const int n = mainKnobs.size();
    const int kw = n > 0 ? knobRow.getWidth() / n : knobRow.getWidth();
    for (auto* k : mainKnobs)
    {
        auto cell = knobRow.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }

    // --- voice section ---
    auto vs = area.removeFromTop (90);
    auto drop = vs.removeFromLeft (vs.getWidth() - 320);
    voice.setBounds (drop.reduced (2, 8));
    auto vc = vs.reduced (4, 8);
    voiceEnable.setBounds (vc.removeFromTop (22));
    stretchSync.setBounds (vc.removeFromTop (22));
    auto vk = vc;
    voiceGain.slider.setBounds (vk.removeFromLeft (70).withHeight (60));
    voiceGain.label.setBounds (voiceGain.slider.getX(), voiceGain.slider.getBottom(), 70, 12);
    stretchRatio.slider.setBounds (vk.removeFromLeft (70).withHeight (60));
    stretchRatio.label.setBounds (stretchRatio.slider.getX(), stretchRatio.slider.getBottom(), 70, 12);
}

void GrainFreezeAudioProcessorEditor::timerCallback()
{
    seqGrid.setPlayStep (proc.currentStep.load (std::memory_order_relaxed));
    voice.setInfo (proc.getSampleName(), proc.getDetectedBpm(), proc.getProjectBpm());
}
