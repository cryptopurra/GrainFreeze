#include "PluginEditor.h"

using namespace juce;

//==============================================================================
GrainFreezeAudioProcessorEditor::Knob*
GrainFreezeAudioProcessorEditor::makeKnob (Component& parent, const String& paramID, const String& text,
                                           bool big, int textBox)
{
    auto* k = new Knob();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    if (textBox == 0) k->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    else              k->slider.setTextBoxStyle (Slider::TextBoxRight, false, 58, 16);
    k->slider.getProperties().set ("big", big);
    k->slider.setLookAndFeel (&lnf);
    k->slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    parent.addAndMakeVisible (k->slider);
    k->label.setText (text, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    parent.addAndMakeVisible (k->label);
    k->attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramID, k->slider);
    return k;
}

Label* GrainFreezeAudioProcessorEditor::makeCaption (const String& text, Justification j)
{
    auto* l = new Label();
    l->setText (text, dontSendNotification);
    l->setJustificationType (j);
    l->setInterceptsMouseClicks (false, false);
    l->setFont (Font (FontOptions (11.0f)));
    canvas.addAndMakeVisible (l);
    captions.add (l);
    return l;
}

GrainFreezeAudioProcessorEditor::Param*
GrainFreezeAudioProcessorEditor::makeParam (const char* id, const String& caption, bool wantKnob, bool wantBox)
{
    auto* p = new Param();
    if (wantKnob) p->knob.reset (makeKnob (canvas, id, "", false, 0));
    if (p->knob != nullptr) p->knob->label.setVisible (false);
    if (wantBox)
    {
        p->box = std::make_unique<ValueBox> (P (id));
        canvas.addAndMakeVisible (*p->box);
    }
    p->caption.setText (caption, dontSendNotification);
    p->caption.setJustificationType (Justification::centred);
    p->caption.setFont (Font (FontOptions (11.0f)));
    p->caption.setInterceptsMouseClicks (false, false);
    canvas.addAndMakeVisible (p->caption);
    params.add (p);
    return p;
}

//==============================================================================
GrainFreezeAudioProcessorEditor::GrainFreezeAudioProcessorEditor (GrainFreezeAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);

    // ---- top bar ----
    canvas.addAndMakeVisible (zoomBox);
    zoomBox.addItemList ({ "50%", "75%", "100%", "125%", "150%", "200%" }, 1);
    zoomBox.setSelectedId (3, dontSendNotification);
    zoomBox.onChange = [this]
    {
        const int pct[] = { 50, 75, 100, 125, 150, 200 };
        const int z = pct[jlimit (0, 5, zoomBox.getSelectedId() - 1)];
        setSize (baseW * z / 100, baseH * z / 100);
    };

    liveBtn.setComponentID ("live");
    canvas.addAndMakeVisible (liveBtn);
    liveBtn.setTooltip ("Effect on live playback (when Freeze is off)");
    liveA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::live, liveBtn);
    canvas.addAndMakeVisible (bypassBtn);
    bypassA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::bypass, bypassBtn);
    canvas.addAndMakeVisible (limiterBtn);
    limiterA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::limiterOn, limiterBtn);

    // ---- tabs + LEDs ----
    const char* tabNames[] = { "Seq", "Time", "Pitch", "Misc 1", "Misc 2", "Misc 3", "Misc 4" };
    for (int i = 0; i < 7; ++i)
    {
        auto* b = new TextButton (tabNames[i]);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (100);
        b->onClick = [this, i] { selectTab (i); };
        canvas.addAndMakeVisible (b);
        tabs.add (b);
    }
    for (int i = 0; i < 7; ++i)
    {
        auto* led = new ToggleButton();
        led->setComponentID ("led");
        canvas.addAndMakeVisible (led);
        tabLeds.add (led);
        const String id = (i == 0) ? String (pid::seqOn) : pid::envField (i - 1, "enable");
        tabLedAtt.add (new APVTS::ButtonAttachment (proc.apvts, id, *led));
    }

    // ---- sequencer page ----
    canvas.addChildComponent (seqGrid);
    seqRes = std::make_unique<CycleButton> (P (pid::seqResolution), resolutionNames());
    seqLength = std::make_unique<ValueBox> (P (pid::seqLength));
    seqOffset = std::make_unique<ValueBox> (P (pid::seqOffset));
    canvas.addChildComponent (*seqRes);
    canvas.addChildComponent (*seqLength);
    canvas.addChildComponent (*seqOffset);
    for (auto* l : { &seqLengthLbl, &seqOffsetLbl })
    {
        l->setJustificationType (Justification::centred);
        l->setFont (Font (FontOptions (11.0f)));
        canvas.addChildComponent (l);
    }
    seqLengthLbl.setText ("Length", dontSendNotification);
    seqOffsetLbl.setText ("Offset", dontSendNotification);

    // ---- envelope pages ----
    const auto& specs = envSpecs();
    const double barVals[] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto* ui = new EnvUI();
        envUI.add (ui);

        ui->curve = std::make_unique<CurveEditor> (proc.modulator (n));
        ui->curve->setBipolar (n != 0);
        canvas.addChildComponent (*ui->curve);

        ui->steps = std::make_unique<ValueBox> (P (pid::envField (n, "steps")));
        canvas.addChildComponent (*ui->steps);
        ui->stepsLabel.setText ("Steps", dontSendNotification);
        ui->stepsLabel.setJustificationType (Justification::centred);
        ui->stepsLabel.setFont (Font (FontOptions (11.0f)));
        canvas.addChildComponent (ui->stepsLabel);
        ui->stepsAtt = std::make_unique<ParameterAttachment> (P (pid::envField (n, "steps")),
            [c = ui->curve.get()] (float v) { c->setStepCount ((int) std::lround (v)); });
        ui->stepsAtt->sendInitialUpdate();

        ui->mode = std::make_unique<CycleButton> (P (pid::envField (n, "mode")), StringArray { "Normal", "Step" });
        ui->mode->onChanged = [this, n, ui] (int i)
        {
            ui->curve->setMode (i == 1 ? EnvMode::step : EnvMode::normal);
            const bool vis = (selectedTab == n + 1) && i == 1;
            ui->steps->setVisible (vis);
            ui->stepsLabel.setVisible (vis);
        };
        ui->bars = std::make_unique<CycleButton> (P (pid::envField (n, "bars")), barsNames().isEmpty() ? StringArray() : [&]
        {
            StringArray a;
            for (auto& b : barsNames()) a.add (b + (b == "1/2" || b == "1" ? " Bar" : " Bars"));
            return a;
        }());
        ui->bars->onChanged = [ui, barVals] (int i) { ui->curve->setBars (barVals[jlimit (0, 5, i)]); };
        ui->mode->onChanged (ui->mode->getIndex());
        ui->bars->onChanged (ui->bars->getIndex());
        canvas.addChildComponent (*ui->mode);
        canvas.addChildComponent (*ui->bars);

        // Loop / Ping Pong are two toggles over one 3-state parameter.
        ui->loopAtt = std::make_unique<ParameterAttachment> (P (pid::envField (n, "loop")),
            [ui] (float v)
            {
                ui->loopIndex = (int) std::lround (v);
                ui->loopBtn.setToggleState (ui->loopIndex >= 1, dontSendNotification);
                ui->pingBtn.setToggleState (ui->loopIndex == 2, dontSendNotification);
            });
        ui->loopAtt->sendInitialUpdate();
        ui->loopBtn.onClick = [ui] { ui->loopAtt->setValueAsCompleteGesture (ui->loopIndex >= 1 ? 0.0f : 1.0f); };
        ui->pingBtn.onClick = [ui] { ui->loopAtt->setValueAsCompleteGesture (ui->loopIndex == 2 ? 1.0f : 2.0f); };
        canvas.addChildComponent (ui->loopBtn);
        canvas.addChildComponent (ui->pingBtn);

        for (auto d : specs[(size_t) n].dests)
        {
            auto* k = makeKnob (canvas, pid::envDepth (n, d), modDestLabel (d), false, 1);
            k->slider.setVisible (false);
            k->label.setVisible (false);
            ui->depth.add (k);
        }
    }

    // ---- main knob clusters ----
    pOverlap = makeParam (pid::grainOverlap, "Overlap",      true,  false);
    pGrain   = makeParam (pid::grainSize,    "Grain Size",   false, true);
    pSpeed   = makeParam (pid::scanSpeed,    "Speed",        true,  true);
    pPitch   = makeParam (pid::grainPitch,   "Pitch",        true,  true);
    pSpecMix = makeParam (pid::specMix,      "Spectral Mix", false, true);
    pSmear   = makeParam (pid::specSmear,    "Smear",        true,  false);
    pVolume  = makeParam (pid::volume,       "Volume",       true,  true);
    pReducer = makeParam (pid::reducer,      "Reducer",      true,  true);

    // ---- gain strips ----
    inKnob.reset (makeKnob (canvas, pid::inputGain, "", true, 0));
    outKnob.reset (makeKnob (canvas, pid::outputGain, "", true, 0));
    inKnob->label.setVisible (false);
    outKnob->label.setVisible (false);
    inBox  = std::make_unique<ValueBox> (P (pid::inputGain));
    outBox = std::make_unique<ValueBox> (P (pid::outputGain));
    canvas.addAndMakeVisible (*inBox);
    canvas.addAndMakeVisible (*outBox);
    canvas.addAndMakeVisible (meterIn);
    canvas.addAndMakeVisible (meterOut);
    makeCaption ("Input Gain");
    makeCaption ("Output Gain");

    // ---- transport ----
    freezeBtn.setComponentID ("freeze");
    freezeBtn.setClickingTogglesState (true);
    canvas.addAndMakeVisible (freezeBtn);
    freezeA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::freeze, freezeBtn);
    canvas.addAndMakeVisible (resetBtn);
    resetBtn.onClick = [this] { proc.requestReset(); };
    canvas.addAndMakeVisible (ledTime);

    // ---- voice ----
    canvas.addAndMakeVisible (voice);
    voice.onFileChosen = [this] (File f)
    {
        if (! proc.loadSampleFile (f))
            NativeMessageBox::showMessageBoxAsync (MessageBoxIconType::WarningIcon,
                                                   "GrainFreeze", "Could not load this audio file.");
        else
            if (auto* pr = proc.apvts.getParameter (pid::voiceEnable))
                pr->setValueNotifyingHost (1.0f);       // use the voice right away
    };
    canvas.addAndMakeVisible (voiceEnable);
    voiceEnableA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::voiceEnable, voiceEnable);
    canvas.addAndMakeVisible (stretchSync);
    stretchSyncA = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, pid::stretchSync, stretchSync);
    canvas.addAndMakeVisible (loadBtn);
    loadBtn.onClick = [this] { voice.browse(); };
    voiceGainBox = std::make_unique<ValueBox> (P (pid::voiceGain));
    stretchBox   = std::make_unique<ValueBox> (P (pid::stretchRatio));
    canvas.addAndMakeVisible (*voiceGainBox);
    canvas.addAndMakeVisible (*stretchBox);
    for (auto* l : { &voiceGainLbl, &stretchLbl })
    {
        l->setFont (Font (FontOptions (11.0f)));
        l->setJustificationType (Justification::centredRight);
        canvas.addAndMakeVisible (l);
    }
    voiceGainLbl.setText ("Gain", dontSendNotification);
    stretchLbl.setText ("Stretch", dontSendNotification);

    tabs[1]->setToggleState (true, dontSendNotification);
    selectTab (1);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
        setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    }
    setSize (baseW, baseH);
    lastCurvesRevision = proc.curvesRevision.load();
    startTimerHz (30);
}

GrainFreezeAudioProcessorEditor::~GrainFreezeAudioProcessorEditor()
{
    stopTimer();
    for (auto* ui : envUI)
        for (auto* k : ui->depth) k->slider.setLookAndFeel (nullptr);
    for (auto* p : params) if (p->knob != nullptr) p->knob->slider.setLookAndFeel (nullptr);
    inKnob->slider.setLookAndFeel (nullptr);
    outKnob->slider.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void GrainFreezeAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0e0f11));
    g.addTransform (AffineTransform::scale ((float) getWidth() / (float) baseW));

    // top bar
    ColourGradient top (Colour (0xff25282e), 0, 0, Colour (0xff17181c), 0, 44, false);
    g.setGradientFill (top);
    g.fillRect (0, 0, baseW, 44);
    g.setColour (Colour (0xff3a3f4a));
    g.drawHorizontalLine (43, 0, (float) baseW);
    g.setColour (Colour (0xffe9edf4));
    g.setFont (Font (FontOptions (22.0f, Font::bold)));
    g.drawText ("GRAINFREEZE", 16, 6, 220, 30, Justification::centredLeft);
    g.setColour (gf::gold());
    g.setFont (Font (FontOptions (10.5f)));
    g.drawText ("TIME-FREEZE  GRANULAR  +  STFT", 18, 28, 220, 12, Justification::centredLeft);

    // main body
    ColourGradient body (Colour (0xff2b2e35), 0, 48, Colour (0xff1b1d22), 0, 476, false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (8.0f, 48.0f, baseW - 16.0f, 426.0f, 12.0f);
    g.setColour (Colour (0xff3d424d));
    g.drawRoundedRectangle (8.5f, 48.5f, baseW - 17.0f, 425.0f, 12.0f, 1.2f);

    // display housing
    g.setColour (Colour (0xff0f1013));
    g.fillRoundedRectangle (106.0f, 52.0f, 508.0f, 240.0f, 8.0f);
    g.setColour (Colour (0xff4a505c));
    g.drawRoundedRectangle (106.5f, 52.5f, 507.0f, 239.0f, 8.0f, 1.5f);

    // voice section
    g.setColour (Colour (0xff17191d));
    g.fillRoundedRectangle (8.0f, 480.0f, baseW - 16.0f, 236.0f, 10.0f);
    g.setColour (Colour (0xff343843));
    g.drawRoundedRectangle (8.5f, 480.5f, baseW - 17.0f, 235.0f, 10.0f, 1.0f);
    g.setColour (gf::gold());
    g.setFont (Font (FontOptions (13.0f, Font::bold)));
    g.drawText ("VOICE", 20, 484, 80, 20, Justification::centredLeft);
    g.setColour (gf::textDim());
    g.setFont (Font (FontOptions (11.5f)));
    g.drawText ("drop a sample - it is stretched to the project tempo", 78, 484, 400, 20, Justification::centredLeft);
}

void GrainFreezeAudioProcessorEditor::selectTab (int tab)
{
    selectedTab = tab;
    const bool seqPage = (tab == 0);
    seqGrid.setVisible (seqPage);
    seqRes->setVisible (seqPage);
    seqLength->setVisible (seqPage);
    seqOffset->setVisible (seqPage);
    seqLengthLbl.setVisible (seqPage);
    seqOffsetLbl.setVisible (seqPage);

    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        const bool on = (tab == n + 1);
        auto* ui = envUI[n];
        ui->curve->setVisible (on);
        ui->mode->setVisible (on);
        ui->bars->setVisible (on);
        ui->loopBtn.setVisible (on);
        ui->pingBtn.setVisible (on);
        const bool stepVis = on && ui->mode->getIndex() == 1;
        ui->steps->setVisible (stepVis);
        ui->stepsLabel.setVisible (stepVis);
        for (auto* k : ui->depth) { k->slider.setVisible (on); k->label.setVisible (on); }
    }
    layoutCanvas();
}

void GrainFreezeAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    canvas.setTransform (AffineTransform::scale (scale));
    canvas.setBounds (0, 0, baseW, baseH);
    layoutCanvas();
}

void GrainFreezeAudioProcessorEditor::layoutCanvas()
{
    using R = juce::Rectangle<int>;

    // top bar
    zoomBox.setBounds (baseW - 108, 10, 96, 24);
    limiterBtn.setBounds (baseW - 188, 10, 72, 24);
    bypassBtn.setBounds (baseW - 266, 10, 72, 24);
    liveBtn.setBounds (baseW - 360, 9, 88, 26);

    // gain strips
    meterIn.setBounds (14, 62, 88, 202);
    meterOut.setBounds (baseW - 14 - 88, 62, 88, 202);
    inKnob->slider.setBounds (R (22, 270, 72, 72));
    outKnob->slider.setBounds (R (baseW - 94, 270, 72, 72));
    inBox->setBounds (22, 346, 72, 20);
    outBox->setBounds (baseW - 94, 346, 72, 20);
    captions[0]->setBounds (14, 368, 88, 14);
    captions[1]->setBounds (baseW - 102, 368, 88, 14);

    // tabs
    for (int i = 0; i < 7; ++i)
    {
        tabs[i]->setBounds (110 + i * 72, 56, 72, 24);
        tabLeds[i]->setBounds (110 + i * 72 + 4, 62, 12, 12);
    }

    const R curveArea (112, 84, 404, 152);
    seqGrid.setBounds (curveArea);
    for (auto* ui : envUI) ui->curve->setBounds (curveArea);

    // right column of the display
    const int rx = 524, rw = 84;
    seqRes->setBounds (rx, 88, rw, 22);
    seqLengthLbl.setBounds (rx, 116, rw, 13);
    seqLength->setBounds (rx, 130, rw, 22);
    seqOffsetLbl.setBounds (rx, 158, rw, 13);
    seqOffset->setBounds (rx, 172, rw, 22);

    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto* ui = envUI[n];
        ui->mode->setBounds (rx, 88, rw, 22);
        ui->bars->setBounds (rx, 114, rw, 22);
        ui->loopBtn.setBounds (rx, 140, rw, 22);
        ui->pingBtn.setBounds (rx, 166, rw, 22);
        ui->stepsLabel.setBounds (rx, 194, rw, 13);
        ui->steps->setBounds (rx, 208, rw, 22);

        // depth strip under the curve
        const int cnt = ui->depth.size();
        auto strip = R (114, 242, 492, 46);
        if (cnt == 1) strip = strip.removeFromRight (190);
        const int cw = cnt > 0 ? strip.getWidth() / cnt : strip.getWidth();
        for (auto* k : ui->depth)
        {
            auto cell = strip.removeFromLeft (cw);
            k->label.setBounds (cell.removeFromTop (13));
            k->slider.setBounds (cell.withTrimmedBottom (1));
        }
    }

    // left / right parameter clusters
    auto place = [] (Param* p, int boxX, int knobX, int y)
    {
        if (p->knob != nullptr) p->knob->slider.setBounds (knobX, y, 40, 40);
        if (p->box != nullptr)  p->box->setBounds (boxX, y + 6, 88, 22);
        if (p->box != nullptr)  p->caption.setBounds (boxX - 6, y + 29, 100, 13);
        else                    p->caption.setBounds (knobX - 24, y + 41, 88, 13);
    };
    // left: knob at x=122, box at x=176
    place (pOverlap, 0, 122, 300);                 // knob only
    place (pGrain,   176, 0, 300);                 // box only
    place (pSpeed,   176, 122, 352);
    place (pPitch,   176, 122, 404);
    // right: box at x=456, knob at x=558
    place (pSpecMix, 456, 0, 300);                 // box only
    place (pSmear,   0, 558, 300);                 // knob only
    place (pVolume,  456, 558, 352);
    place (pReducer, 456, 558, 404);

    freezeBtn.setBounds (300, 298, 120, 120);
    ledTime.setBounds (296, 424, 128, 26);
    resetBtn.setBounds (322, 454, 76, 18);

    // voice section
    voice.setBounds (18, 508, baseW - 36, 154);
    voiceEnable.setBounds (18, 672, 72, 24);
    stretchSync.setBounds (96, 672, 108, 24);
    voiceGainLbl.setBounds (212, 674, 36, 20);
    voiceGainBox->setBounds (252, 674, 84, 22);
    stretchLbl.setBounds (346, 674, 48, 20);
    stretchBox->setBounds (398, 674, 84, 22);
    loadBtn.setBounds (baseW - 18 - 104, 672, 104, 24);
}

void GrainFreezeAudioProcessorEditor::timerCallback()
{
    meterIn.setLevels  (proc.inLevel[0].load(),  proc.inLevel[1].load());
    meterOut.setLevels (proc.outLevel[0].load(), proc.outLevel[1].load());
    seqGrid.setPlayStep (proc.currentStep.load (std::memory_order_relaxed));
    ledTime.set (proc.frozenUI.load() ? proc.freezeSeconds.load() : 0.0);

    // envelopes: activity + playheads, and re-read curves after a preset/session load
    const int rev = proc.curvesRevision.load();
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto* ui = envUI[n];
        if (rev != lastCurvesRevision) ui->curve->reloadFromModulator();
        ui->curve->setActive (proc.apvts.getRawParameterValue (pid::envField (n, "enable"))->load() > 0.5f);
        if (selectedTab == n + 1) ui->curve->setPhase (proc.modulator (n).getPhase());
    }
    lastCurvesRevision = rev;

    // voice
    if (const int v = proc.getSampleVersion(); v != lastSampleVersion)
    {
        lastSampleVersion = v;
        double dur = 0.0;
        auto pk = proc.getPeaks (dur);
        voice.setPeaks (std::move (pk), dur);
    }
    const bool sync = proc.apvts.getRawParameterValue (pid::stretchSync)->load() > 0.5f;
    voice.setInfo (proc.getSampleName(), proc.getDetectedBpm(), proc.getProjectBpm(), sync, proc.frozenUI.load());
    voice.setPlayhead (proc.samplePlayNorm.load (std::memory_order_relaxed));
    stretchBox->setEnabled (! sync);
}
