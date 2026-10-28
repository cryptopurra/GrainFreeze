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

static double goertzel (const std::vector<float>& v, size_t a, size_t b, double f)
{ double w = 2 * M_PI * f / SR, c = 2 * std::cos (w), s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
  return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a); }

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
    { // 13. Freeze very early (0.15 s after start) still sustains a tone
        auto in = [] (size_t i) { return i < 6615 ? 0.5f * (float) std::sin (2 * M_PI * 440 * (double) i / SR) : 0.0f; };
        auto r = run (3.0, in, [&] (int b) { auto p = d; p.frozen = (b * B >= 6615); return p; });
        const float rf = rms (r.out, 60000, 120000);
        check ("early freeze: sustains (rms>0.1)", rf > 0.1f, rf);
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
    { // 9. Pitch still works with Spectral Mix at 100%
        auto p = d; p.specMix = 1.0f; p.pitchSemis = 12.f;
        auto r = run (3.0, sine (440, 0.5f), [&] (int) { return p; });
        check ("pitch +12 st with spectral mix 100%: ~880 Hz", std::abs (freqOf (r.out, 66150, 132000) - 880) < 70, freqOf (r.out, 66150, 132000));
    }
    { // 10. Smear really smears: 440 Hz switches to 880 Hz at 1.5 s
        auto in = [] (size_t i) { double f = i < 66150 ? 440.0 : 880.0; return 0.5f * (float) std::sin (2 * M_PI * f * (double) i / SR); };
        auto pa = d; pa.specMix = 1.0f; pa.smear = 0.0f;
        auto pb = pa; pb.smear = 1.0f;
        auto ra = run (2.2, in, [&] (int) { return pa; });
        auto rb = run (2.2, in, [&] (int) { return pb; });
        const size_t a = 66150 + 1536 + 6000, b = a + 8192;       // shortly after the switch (+latency)
        const double ea = goertzel (ra.out, a, b, 440), eb = goertzel (rb.out, a, b, 440);
        check ("smear: old 440 Hz tail lingers (smear1 > 5x smear0)", eb > 5 * ea + 1e-4, ea, eb);
    }
    { // 11. Hold freezes the spectrum while input changes
        auto in = [] (size_t i) { double f = i < 66150 ? 440.0 : 880.0; return 0.5f * (float) std::sin (2 * M_PI * f * (double) i / SR); };
        auto p = d; p.specMix = 1.0f;
        auto r = run (2.6, in, [&] (int b) { auto q = p; q.hold = (b * B >= 44100) ? 1.0f : 0.0f; return q; });
        const size_t a = 90000, b = 110000;
        check ("hold: spectrum stays at 440 Hz", goertzel (r.out, a, b, 440) > 3 * goertzel (r.out, a, b, 880), goertzel (r.out, a, b, 440), goertzel (r.out, a, b, 880));
    }
    { // 12. Host block bigger than prepared block must not break
        FreezeCore core; core.prepare (SR, 256);
        AudioBuffer<float> buf (2, 2048); double e = 0;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 2048; ++i) { float x = 0.5f * (float) std::sin (2 * M_PI * 440 * (double) (b * 2048 + i) / SR); buf.setSample (0, i, x); buf.setSample (1, i, x); }
            core.process (buf, d);
            if (b > 20) e += buf.getRMSLevel (0, 0, 2048);
        }
        check ("oversized host block: still audible", e / 19 > 0.2, e / 19);
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
