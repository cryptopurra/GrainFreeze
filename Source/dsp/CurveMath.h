#pragma once
#include <JuceHeader.h>
#include <vector>

// Shared envelope maths. A curve is a list of (x, y) points in 0..1 plus one
// "bend" per segment (-1..1). bend > 0 pushes the segment upward, < 0 downward
// (what you get by dragging the line up or down in the editor).
namespace curvemath
{
    inline float shape (float t, float bend, bool rising)
    {
        if (std::abs (bend) < 1.0e-4f) return t;
        const float k = std::exp2 (3.0f * (rising ? -bend : bend));
        return std::pow (juce::jlimit (0.0f, 1.0f, t), k);
    }

    inline float segment (const juce::Point<float>& a, const juce::Point<float>& b, float bend, float x)
    {
        const float t = (b.x > a.x) ? (x - a.x) / (b.x - a.x) : 0.0f;
        return a.y + (b.y - a.y) * shape (t, bend, b.y >= a.y);
    }

    inline float eval (const std::vector<juce::Point<float>>& p, const std::vector<float>& bends, float x)
    {
        if (p.size() < 2) return 0.5f;
        for (size_t i = 1; i < p.size(); ++i)
            if (x <= p[i].x)
                return segment (p[i - 1], p[i], i - 1 < bends.size() ? bends[i - 1] : 0.0f, x);
        return p.back().y;
    }
}
