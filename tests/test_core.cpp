#include <JuceHeader.h>
#include "dsp/FreezeCore.h"
#include <cstdio>
using namespace juce;
static const double SR = 44100.0; static const int B = 512;
struct Run { std::vector<float> out, in; };

static float rms (const std::vector<float>& v, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return (float) std::sqrt (s / (double) (b - a)); }
static double freqOf (const std::vector<float>& v, size_t a, size_t b)   // zero-crossing estimate
{ int z = 0; for (size_t i = a + 1; i < b; ++i) if (v[i-1] < 0 && v[i] >= 0) ++z; return z / ((b - a) / SR); }

// input(t,i) -> sample; paramsAt(block) -> params
template <typename In, typename Pf>
static Run run (double seconds, In input, Pf params)
{
    FreezeCore core; core.prepare (SR, B);
    Run r; AudioBuffer<float> buf (2, B);
    const int blocks = (int) (seconds * SR / B);
    size_t pos = 0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < B; ++i) { float x = input (pos + (size_t) i); buf.setSample (0, i, x); buf.setSample (1, i, x); r.in.push_back (x); }
        core.process (buf, params (b));
        for (int i = 0; i < B; ++i) { float y = buf.getSample (0, i); r.out.push_back (y); if (! std::isfinite (y)) { std::printf ("NaN!\n"); } }
        pos += B;
    }
    return r;
}
static int fails = 0;
static void check (const char* name, bool ok, double v1, double v2 = 0)
{ std::printf ("%-52s %s  (%.4f %.4f)\n", name, ok ? "PASS" : "FAIL", v1, v2); if (! ok) ++fails; }

int main()
{
    auto sine = [] (double f, float a) { return [=] (size_t i) { return a * (float) std::sin (2 * M_PI * f * (double) i / SR); }; };
    FreezeParams d; d.limiter = false;

    { // 1. LIVE on, defaults, 440 Hz
        auto r = run (3.0, sine (440, 0.5f), [&] (int) { return d; });
        const float ri = rms (r.in, 44100, 132000), ro = rms (r.out, 44100, 132000);
        check ("live: output level close to input (0.5..1.6x)", ro / ri > 0.5 && ro / ri < 1.6, ro / ri);
        check ("live: pitch preserved (~440 Hz)", std::abs (freqOf (r.out, 44100, 132000) - 440) < 30, freqOf (r.out, 44100, 132000));
    }
    { // 2. LIVE off -> clean latency-matched passthrough
        auto p = d; p.live = false;
        auto r = run (2.0, sine (440, 0.5f), [&] (int) { return p; });
        const float ri = rms (r.in, 44100, 88000), ro = rms (r.out, 44100, 88000);
        check ("live off: passthrough level == input", std::abs (ro / ri - 1.0) < 0.02, ro / ri);
    }
    { // 3. Freeze holds the sound after the input goes silent
        auto in = [] (size_t i) { return i < 66150 ? 0.5f * (float) std::sin (2 * M_PI * 440 * (double) i / SR) : 0.0f; };
        auto r = run (4.0, in, [&] (int b) { auto p = d; p.frozen = (b * B >= 66150); return p; });
        const float rf = rms (r.out, 110000, 170000);
        check ("freeze: sustains after input stops (rms>0.15)", rf > 0.15f, rf);
        check ("freeze: pitch kept (~440 Hz)", std::abs (freqOf (r.out, 110000, 170000) - 440) < 40, freqOf (r.out, 110000, 170000));
        auto p0 = d; p0.live = false; p0.frozen = false;
        auto r2 = run (4.0, in, [&] (int b) { auto p = d; p.live = true; p.frozen = false; return p; });
        check ("no freeze: silence after input stops (rms<0.02)", rms (r2.out, 110000, 170000) < 0.02f, rms (r2.out, 110000, 170000));
    }
    { // 4. Pitch +12 -> one octave up (live)
        auto p = d; p.pitchSemis = 12.f;
        auto r = run (3.0, sine (440, 0.5f), [&] (int) { return p; });
        check ("pitch +12 st: ~880 Hz", std::abs (freqOf (r.out, 44100, 132000) - 880) < 60, freqOf (r.out, 44100, 132000));
    }
    { // 5. Speed 0 frozen is stable
        auto in = [] (size_t i) { return i < 66150 ? 0.5f * (float) std::sin (2 * M_PI * 440 * (double) i / SR) : 0.0f; };
        auto r = run (4.0, in, [&] (int b) { auto p = d; p.frozen = (b * B >= 66150); p.speed = 0.f; return p; });
        check ("freeze speed 0: sustained", rms (r.out, 110000, 170000) > 0.15f, rms (r.out, 110000, 170000));
    }
    { // 6. Spectral path
        auto p = d; p.specMix = 1.0f;
        auto r = run (3.0, sine (440, 0.5f), [&] (int) { return p; });
        const float ri = rms (r.in, 66150, 132000), ro = rms (r.out, 66150, 132000);
        check ("spectral live: level 0.5..1.6x", ro / ri > 0.5 && ro / ri < 1.6, ro / ri);
        check ("spectral live: pitch kept", std::abs (freqOf (r.out, 66150, 132000) - 440) < 40, freqOf (r.out, 66150, 132000));
        auto in = [] (size_t i) { return i < 66150 ? 0.5f * (float) std::sin (2 * M_PI * 440 * (double) i / SR) : 0.0f; };
        auto r2 = run (4.0, in, [&] (int b) { auto q = p; q.frozen = (b * B >= 66150); return q; });
        const float rf = rms (r2.out, 110000, 170000);
        check ("spectral freeze: sustained (rms>0.1)", rf > 0.1f, rf);
    }
    { // 7. Gains
        auto p = d; p.inGainDb = -6.f; p.outGainDb = -6.f; p.live = false;
        auto r = run (2.0, sine (440, 0.5f), [&] (int) { return p; });
        check ("gains -6/-6 dB => ~0.25x", std::abs (rms (r.out, 44100, 88000) / rms (r.in, 44100, 88000) - 0.251) < 0.02,
               rms (r.out, 44100, 88000) / rms (r.in, 44100, 88000));
    }
    { // 8. Loud noise never exceeds limiter ceiling / no NaN
        auto p = d; p.limiter = true; p.overlap = 1.f;
        Random rnd (3);
        auto r = run (2.0, [&] (size_t) { return rnd.nextFloat() * 1.8f - 0.9f; }, [&] (int) { return p; });
        float pk = 0; for (auto v : r.out) pk = std::max (pk, std::abs (v));
        check ("limiter: peak <= 1.0", pk <= 1.0f, pk);
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
