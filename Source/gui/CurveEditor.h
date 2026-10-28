#pragma once
#include <JuceHeader.h>
#include "../dsp/StepModulator.h"
#include "../dsp/CurveMath.h"
#include "GFLookAndFeel.h"

// ---------------------------------------------------------------------------
// CurveEditor  --  the envelope display / automation writer.
//
//  * NORMAL mode: breakpoint curve. Click empty space = add point, drag = move,
//    double-click = delete, SHIFT+drag = freehand pencil.
//  * STEP mode  : stair display; click/drag paints step heights (pencil style).
//  * Right-click = shapes menu (flat, ramps, triangle, sine, random, decay).
//  * Beat grid follows the "bars" setting, a playhead follows the host position.
//
// It edits the StepModulator the processor owns, so changes are heard at once.
// (Original component.)
// ---------------------------------------------------------------------------

class CurveEditor : public juce::Component
{
public:
    explicit CurveEditor (StepModulator& m) : mod (m) { reloadFromModulator(); }

    void reloadFromModulator()
    {
        curve = mod.getCurve();
        bends = mod.getBends();
        if (curve.size() < 2) curve = { { 0.f, 0.5f }, { 1.f, 0.5f } };
        bends.resize (curve.size() - 1, 0.0f);
        sortCurve();
        stepVals.clear();
        repaint();
    }

    void setMode (EnvMode m)      { mode = m; stepVals.clear(); repaint(); }
    void setStepCount (int n)     { steps = juce::jlimit (2, 32, n); stepVals.clear(); repaint(); }
    void setBars (double b)       { bars = b; repaint(); }
    void setBipolar (bool b)      { bipolar = b; repaint(); }
    void setActive (bool a)       { active = a; repaint(); }
    void setPhase (float p)       { if (std::abs (p - phase) > 0.0015f) { phase = p; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        auto r = plot();

        // recessed background
        juce::ColourGradient bg (juce::Colour (0xff1a1c21), 0, full.getY(), juce::Colour (0xff0b0c0e), 0, full.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (full, 4.0f);
        g.setColour (juce::Colour (0xff2c3038));
        g.drawRoundedRectangle (full.reduced (0.5f), 4.0f, 1.0f);

        // horizontal grid + y labels
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        const std::vector<std::pair<float, juce::String>> ylab = bipolar
            ? std::vector<std::pair<float, juce::String>> { { 0.75f, "+50%" }, { 0.5f, "0%" }, { 0.25f, "-50%" } }
            : std::vector<std::pair<float, juce::String>> { { 0.75f, "75%" }, { 0.5f, "50%" }, { 0.25f, "25%" }, { 0.0f, "0%" } };
        for (auto& [v, t] : ylab)
        {
            const float y = juce::jmap (v, r.getBottom(), r.getY());
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            g.setColour (juce::Colour (0xff777d89));
            g.drawText (t, juce::Rectangle<float> (full.getX() + 1.0f, y - 6.0f, r.getX() - full.getX() - 2.0f, 12.0f),
                        juce::Justification::centredRight);
        }

        // vertical beat grid + x labels
        const int beats = juce::jmax (2, (int) std::lround (bars * 4.0));
        const int every = beats > 16 ? 4 : (beats > 8 ? 2 : 1);
        for (int i = 0; i <= beats; ++i)
        {
            const float x = juce::jmap ((float) i / (float) beats, r.getX(), r.getRight());
            const bool bar = (i % 4) == 0;
            g.setColour (juce::Colours::white.withAlpha (bar ? 0.16f : 0.06f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            if (i > 0 && i < beats && (i % every) == 0)
            {
                g.setColour (juce::Colour (0xff777d89));
                g.drawText (juce::String (i), juce::Rectangle<float> (x - 10.0f, r.getBottom() + 1.0f, 20.0f, 12.0f),
                            juce::Justification::centred);
            }
        }

        if (curve.size() < 2) return;

        // curve path
        juce::Path line;
        if (mode == EnvMode::step)
        {
            for (int s = 0; s < steps; ++s)
            {
                const float x0 = juce::jmap ((float) s / steps, r.getX(), r.getRight());
                const float x1 = juce::jmap ((float) (s + 1) / steps, r.getX(), r.getRight());
                const float y  = juce::jmap (valueAt ((s + 0.5f) / steps), r.getBottom(), r.getY());
                if (s == 0) line.startNewSubPath (x0, y); else line.lineTo (x0, y);
                line.lineTo (x1, y);
            }
        }
        else
        {
            line.startNewSubPath (juce::jmap (curve[0].x, r.getX(), r.getRight()), juce::jmap (curve[0].y, r.getBottom(), r.getY()));
            for (size_t i = 1; i < curve.size(); ++i)
            {
                const int sub = std::abs (bends[i - 1]) > 1.0e-4f ? 28 : 1;
                for (int k = 1; k <= sub; ++k)
                {
                    const float xx = curve[i - 1].x + (curve[i].x - curve[i - 1].x) * (float) k / (float) sub;
                    const float yy = curvemath::segment (curve[i - 1], curve[i], bends[i - 1], xx);
                    line.lineTo (juce::jmap (xx, r.getX(), r.getRight()), juce::jmap (yy, r.getBottom(), r.getY()));
                }
            }
        }

        const auto col = active ? gf::gold() : gf::goldDim();
        juce::Path fill (line);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setColour (col.withAlpha (active ? 0.22f : 0.12f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (line, juce::PathStrokeType (2.0f));

        if (mode == EnvMode::normal)
        {
            g.setColour (active ? gf::gold() : gf::goldDim());
            for (auto& pt : curve)
            {
                const float x = juce::jmap (pt.x, r.getX(), r.getRight());
                const float y = juce::jmap (pt.y, r.getBottom(), r.getY());
                g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
                g.setColour (juce::Colour (0xff111215));
                g.drawEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.0f);
                g.setColour (active ? gf::gold() : gf::goldDim());
            }
        }

        // playhead
        if (active)
        {
            const float px = juce::jmap (phase, r.getX(), r.getRight());
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawVerticalLine ((int) px, r.getY(), r.getBottom());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showShapeMenu(); return; }

        if (mode == EnvMode::step) { lastStep = -1; paintStep (e.position); return; }

        dragIndex = nearestPoint (e.position);
        pencil = e.mods.isShiftDown() && dragIndex < 0;
        bendSeg = -1;
        if (pencil) { pencilAdd (e.position); return; }

        if (dragIndex < 0)
        {
            bendSeg = segmentAt (e.position);
            if (bendSeg >= 0) { bendStartC = bends[(size_t) bendSeg]; bendStartY = e.position.y; return; }

            // empty space: add a point inside the segment under the pointer
            const auto pt = toCurve (e.position);
            size_t i = 1;
            while (i < curve.size() - 1 && curve[i].x < pt.x) ++i;
            curve.insert (curve.begin() + (long) i, pt);
            bends.insert (bends.begin() + (long) i, bends[i - 1]);
            dragIndex = (int) i;
            push();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (mode == EnvMode::step) { paintStep (e.position); return; }
        if (pencil) { pencilAdd (e.position); return; }

        if (bendSeg >= 0)        // drag the line up / down to change its slope (curvature)
        {
            bends[(size_t) bendSeg] = juce::jlimit (-1.0f, 1.0f, bendStartC + (bendStartY - e.position.y) / 55.0f);
            push();
            return;
        }
        if (dragIndex < 0) return;

        const auto pt = toCurve (e.position);
        const bool isEnd = (dragIndex == 0 || dragIndex == (int) curve.size() - 1);
        curve[(size_t) dragIndex].y = pt.y;
        if (! isEnd)
        {
            const float lo = curve[(size_t) dragIndex - 1].x + 0.002f;
            const float hi = curve[(size_t) dragIndex + 1].x - 0.002f;
            curve[(size_t) dragIndex].x = juce::jlimit (lo, juce::jmax (lo, hi), pt.x);
        }
        push();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (mode != EnvMode::normal) { setMouseCursor (juce::MouseCursor::CrosshairCursor); return; }
        if (nearestPoint (e.position) >= 0)      setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else if (segmentAt (e.position) >= 0)    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        else                                     setMouseCursor (juce::MouseCursor::CrosshairCursor);
    }

    void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; bendSeg = -1; pencil = false; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (mode != EnvMode::normal) return;
        const int i = nearestPoint (e.position);
        if (i > 0 && i < (int) curve.size() - 1)
        {
            bends[(size_t) i - 1] = 0.0f;
            curve.erase (curve.begin() + i);
            bends.erase (bends.begin() + i);
            push();
            return;
        }
        const int sg = segmentAt (e.position);
        if (sg >= 0) { bends[(size_t) sg] = 0.0f; push(); }     // double-click a line: straighten it
    }

private:
    juce::Rectangle<float> plot() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft (30.0f).withTrimmedRight (6.0f)
                                         .withTrimmedTop (6.0f).withTrimmedBottom (15.0f);
    }

    juce::Point<float> toCurve (juce::Point<float> px) const
    {
        auto r = plot();
        return { juce::jlimit (0.0f, 1.0f, (px.x - r.getX()) / r.getWidth()),
                 juce::jlimit (0.0f, 1.0f, 1.0f - (px.y - r.getY()) / r.getHeight()) };
    }

    int nearestPoint (juce::Point<float> px) const
    {
        auto r = plot();
        int best = -1; float bestD = 11.0f;
        for (size_t i = 0; i < curve.size(); ++i)
        {
            const float x = juce::jmap (curve[i].x, r.getX(), r.getRight());
            const float y = juce::jmap (curve[i].y, r.getBottom(), r.getY());
            const float d = px.getDistanceFrom ({ x, y });
            if (d < bestD) { bestD = d; best = (int) i; }
        }
        return best;
    }

    // index of the segment whose drawn line passes within a few pixels of the pointer, or -1
    int segmentAt (juce::Point<float> px) const
    {
        auto r = plot();
        for (size_t i = 1; i < curve.size(); ++i)
        {
            const float x0 = juce::jmap (curve[i - 1].x, r.getX(), r.getRight());
            const float x1 = juce::jmap (curve[i].x, r.getX(), r.getRight());
            if (px.x < x0 || px.x > x1) continue;
            const float xx = curve[i - 1].x + (px.x - x0) / juce::jmax (1.0f, x1 - x0) * (curve[i].x - curve[i - 1].x);
            const float yy = juce::jmap (curvemath::segment (curve[i - 1], curve[i], bends[i - 1], xx), r.getBottom(), r.getY());
            if (std::abs (yy - px.y) < 7.0f) return (int) i - 1;
        }
        return -1;
    }

    int indexOfX (float x) const
    {
        for (size_t i = 0; i < curve.size(); ++i)
            if (juce::approximatelyEqual (curve[i].x, x)) return (int) i;
        return -1;
    }

    float valueAt (float t) const { return curvemath::eval (curve, bends, t); }

    void sortCurve()
    {
        std::sort (curve.begin(), curve.end(), [] (auto& a, auto& b) { return a.x < b.x; });
    }

    void push() { bends.resize (curve.size() - 1, 0.0f); mod.setCurve (curve, bends); repaint(); }

    // freehand drawing in NORMAL mode
    void pencilAdd (juce::Point<float> px)
    {
        const auto pt = toCurve (px);
        if (pt.x < 0.01f)      { curve.front().y = pt.y; push(); return; }
        if (pt.x > 0.99f)      { curve.back().y  = pt.y; push(); return; }
        for (int i = (int) curve.size() - 2; i >= 1; --i)
            if (std::abs (curve[(size_t) i].x - pt.x) < 0.008f)
            {
                curve.erase (curve.begin() + i);
                bends.erase (bends.begin() + i);
            }
        curve.push_back (pt);
        sortCurve();
        bends.assign (curve.size() - 1, 0.0f);
        push();
    }

    // paint-style editing in STEP mode
    void captureSteps()
    {
        stepVals.assign ((size_t) steps, 0.5f);
        for (int s = 0; s < steps; ++s) stepVals[(size_t) s] = valueAt ((s + 0.5f) / (float) steps);
    }

    void paintStep (juce::Point<float> px)
    {
        if (stepVals.size() != (size_t) steps) captureSteps();
        auto r = plot();
        const int s = juce::jlimit (0, steps - 1, (int) ((px.x - r.getX()) / r.getWidth() * (float) steps));
        const float v = toCurve (px).y;
        const int a = lastStep < 0 ? s : juce::jmin (lastStep, s);
        const int b = lastStep < 0 ? s : juce::jmax (lastStep, s);
        for (int i = a; i <= b; ++i) stepVals[(size_t) i] = v;
        lastStep = s;
        rebuildFromSteps();
    }

    void rebuildFromSteps()
    {
        curve.clear();
        curve.push_back ({ 0.0f, stepVals.front() });
        for (int s = 0; s < steps; ++s)
            curve.push_back ({ (s + 0.5f) / (float) steps, stepVals[(size_t) s] });
        curve.push_back ({ 1.0f, stepVals.back() });
        bends.assign (curve.size() - 1, 0.0f);
        push();
    }

    void setShape (std::function<float (float)> f, int points)
    {
        curve.clear();
        for (int i = 0; i < points; ++i)
        {
            const float x = (float) i / (float) (points - 1);
            curve.push_back ({ x, juce::jlimit (0.0f, 1.0f, f (x)) });
        }
        stepVals.clear();
        bends.assign (curve.size() - 1, 0.0f);
        push();
    }

    void showShapeMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Flat 100%");
        m.addItem (2, "Flat 50%");
        m.addItem (3, "Flat 0%");
        m.addSeparator();
        m.addItem (4, "Ramp up");
        m.addItem (5, "Ramp down");
        m.addItem (6, "Triangle");
        m.addItem (7, "Sine");
        m.addItem (8, "Decay");
        m.addItem (9, "Random steps");
        m.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<CurveEditor> (this)] (int id)
        {
            if (safe == nullptr || id == 0) return;
            auto* s = safe.getComponent();
            switch (id)
            {
                case 1: s->setShape ([] (float)   { return 1.0f; }, 2); break;
                case 2: s->setShape ([] (float)   { return 0.5f; }, 2); break;
                case 3: s->setShape ([] (float)   { return 0.0f; }, 2); break;
                case 4: s->setShape ([] (float x) { return x; }, 2); break;
                case 5: s->setShape ([] (float x) { return 1.0f - x; }, 2); break;
                case 6: s->setShape ([] (float x) { return x < 0.5f ? x * 2.0f : 2.0f - x * 2.0f; }, 3); break;
                case 7: s->setShape ([] (float x) { return 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * x); }, 49); break;
                case 8: s->setShape ([] (float x) { return std::pow (1.0f - x, 2.2f); }, 33); break;
                case 9:
                {
                    juce::Random rnd;
                    s->stepVals.assign ((size_t) s->steps, 0.0f);
                    for (auto& v : s->stepVals) v = rnd.nextFloat();
                    s->rebuildFromSteps();
                    break;
                }
                default: break;
            }
        });
    }

    StepModulator& mod;
    std::vector<juce::Point<float>> curve { { 0.f, 0.5f }, { 1.f, 0.5f } };
    std::vector<float> stepVals, bends { 0.0f };
    EnvMode mode = EnvMode::normal;
    int steps = 16;
    double bars = 2.0;
    bool bipolar = false, active = false, pencil = false;
    float phase = 0.0f;
    int dragIndex = -1, lastStep = -1, bendSeg = -1;
    float bendStartC = 0.0f, bendStartY = 0.0f;
};
