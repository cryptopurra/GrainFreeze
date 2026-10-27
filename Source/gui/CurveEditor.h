#pragma once
#include <JuceHeader.h>
#include "../dsp/StepModulator.h"

// ---------------------------------------------------------------------------
// CurveEditor
//
// A breakpoint curve editor bound to one StepModulator. Points are (time, value)
// in 0..1. Click empty space to add a point, drag to move, double-click a point
// to delete it (never the two endpoints). In STEP mode the curve is drawn as a
// stair; in SMOOTH mode as connected ramps. Changes push straight into the
// modulator the processor owns, so they are heard immediately.
//
// (Original component.)
// ---------------------------------------------------------------------------

class CurveEditor : public juce::Component
{
public:
    explicit CurveEditor (StepModulator& m) : mod (m) {}

    void setMode (EnvMode m) { mode = m; repaint(); }
    void setStepCount (int n) { steps = juce::jlimit (2, 32, n); repaint(); }

    // Supply the current curve to draw/edit (editor owns a working copy).
    void setCurve (std::vector<juce::Point<float>> pts)
    {
        curve = std::move (pts);
        sortCurve();
        pushToModulator();
        repaint();
    }

    const std::vector<juce::Point<float>>& getCurve() const { return curve; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        g.setColour (juce::Colour (0xff23262e));
        g.fillRoundedRectangle (r, 4.0f);

        // Midline (bipolar 0).
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        if (curve.size() < 2) return;

        juce::Path p;
        if (mode == EnvMode::step)
        {
            for (int s = 0; s < steps; ++s)
            {
                const float x0 = juce::jmap ((float) s / steps, r.getX(), r.getRight());
                const float x1 = juce::jmap ((float) (s + 1) / steps, r.getX(), r.getRight());
                const float v  = valueAt ((s + 0.5f) / steps);
                const float y  = juce::jmap (v, r.getBottom(), r.getY());
                if (s == 0) p.startNewSubPath (x0, y);
                else        p.lineTo (x0, y);
                p.lineTo (x1, y);
            }
        }
        else
        {
            for (size_t i = 0; i < curve.size(); ++i)
            {
                const float x = juce::jmap (curve[i].x, r.getX(), r.getRight());
                const float y = juce::jmap (curve[i].y, r.getBottom(), r.getY());
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
        }
        g.setColour (juce::Colour (0xffd9c24a));
        g.strokePath (p, juce::PathStrokeType (2.0f));

        // Draw the handles.
        g.setColour (juce::Colours::white);
        for (auto& pt : curve)
        {
            const float x = juce::jmap (pt.x, r.getX(), r.getRight());
            const float y = juce::jmap (pt.y, r.getBottom(), r.getY());
            g.fillEllipse (x - 4, y - 4, 8, 8);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragIndex = nearestPoint (e.position);
        if (dragIndex < 0)
        {
            // add a point
            auto pt = toCurve (e.position);
            curve.push_back (pt);
            sortCurve();
            dragIndex = indexOfX (pt.x);
            pushToModulator();
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragIndex < 0) return;
        auto pt = toCurve (e.position);
        // Endpoints keep their x; interior points move freely but stay ordered.
        const bool isEnd = (dragIndex == 0 || dragIndex == (int) curve.size() - 1);
        curve[(size_t) dragIndex].y = juce::jlimit (0.0f, 1.0f, pt.y);
        if (! isEnd)
            curve[(size_t) dragIndex].x = juce::jlimit (0.001f, 0.999f, pt.x);
        sortCurve();
        pushToModulator();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = nearestPoint (e.position);
        if (i > 0 && i < (int) curve.size() - 1)   // never delete endpoints
        {
            curve.erase (curve.begin() + i);
            pushToModulator();
            repaint();
        }
    }

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (4.0f); }

    juce::Point<float> toCurve (juce::Point<float> px) const
    {
        auto r = plot();
        return { juce::jlimit (0.0f, 1.0f, (px.x - r.getX()) / r.getWidth()),
                 juce::jlimit (0.0f, 1.0f, 1.0f - (px.y - r.getY()) / r.getHeight()) };
    }

    int nearestPoint (juce::Point<float> px) const
    {
        auto r = plot();
        int best = -1; float bestD = 12.0f;
        for (size_t i = 0; i < curve.size(); ++i)
        {
            const float x = juce::jmap (curve[i].x, r.getX(), r.getRight());
            const float y = juce::jmap (curve[i].y, r.getBottom(), r.getY());
            const float d = px.getDistanceFrom ({ x, y });
            if (d < bestD) { bestD = d; best = (int) i; }
        }
        return best;
    }

    int indexOfX (float x) const
    {
        for (size_t i = 0; i < curve.size(); ++i)
            if (juce::approximatelyEqual (curve[i].x, x)) return (int) i;
        return -1;
    }

    float valueAt (float t) const
    {
        if (curve.size() < 2) return 0.5f;
        for (size_t i = 1; i < curve.size(); ++i)
            if (t <= curve[i].x)
            {
                auto& a = curve[i - 1]; auto& b = curve[i];
                const float f = (b.x > a.x) ? (t - a.x) / (b.x - a.x) : 0.0f;
                return a.y + (b.y - a.y) * f;
            }
        return curve.back().y;
    }

    void sortCurve()
    {
        std::sort (curve.begin(), curve.end(),
                   [] (auto& a, auto& b) { return a.x < b.x; });
    }

    void pushToModulator() { mod.setCurve (curve); }

    StepModulator& mod;
    std::vector<juce::Point<float>> curve { {0.f, 0.5f}, {1.f, 0.5f} };
    EnvMode mode = EnvMode::normal;
    int steps = 16;
    int dragIndex = -1;
};
