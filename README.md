# GrainFreeze

An original granular + STFT "time-freeze" audio effect (VST3 / AU / Standalone),
built with JUCE. Clean-room design — no third-party plugin code or resources are
reused. The architecture was informed by a behavioural study (what the effect
does), not by any proprietary source.

## What it does

A stereo **capture buffer** is always recording the input. Two engines read from it:

- **Grain engine** — a granular cloud (size, overlap, pitch ±36 st, scan speed
  incl. reverse, jitter, stereo spread).
- **Spectral engine** — a clean overlap-add **STFT** (2048 / 75% overlap) with
  magnitude **smear**, phase-vocoder **hold**, bin **blur**, and **robotize**
  (phase lock). This is the "extra over the original" you asked for.

A single **Freeze** flag is a *source switch*, not an on/off: 

- **Live**  → grains/STFT read the moving input (live granular effect).
- **Freeze** → the read window is parked (classic time-freeze).

After the engines: **crossfade** (Grain ↔ Spectral), **reducer** (lo-fi),
**HP/LP filters**, a **multi-level step gate** (sequencer), output gain and a
safety **limiter**, then a dry/wet against the original.

Six tempo-synced **modulation envelopes** (smooth *or* step, 1/2–16 bars, loop /
ping-pong / one-shot) route freely to any destination (volume, grain size,
pitch, speed, spectral mix/smear/hold, HP, LP, robotize, reducer).

## Build

```bash
git clone https://github.com/juce-framework/JUCE extern/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3/AU is copied to your user plugin folder (`COPY_PLUGIN_AFTER_BUILD`).
Rescan in Cubase and load **GrainFreeze**.

## Source map

| File | Role |
|------|------|
| `Source/Parameters.*` | parameter IDs + APVTS layout |
| `Source/dsp/CaptureBuffer.*` | always-on ring buffer, Live/Freeze anchor |
| `Source/dsp/GrainEngine.*` | granular cloud |
| `Source/dsp/SpectralEngine.*` | clean STFT (smear/hold/blur/robotize) |
| `Source/dsp/StepModulator.*` | one tempo-synced envelope (smooth/step) |
| `Source/dsp/StepSequencer.*` | multi-level gate grid |
| `Source/dsp/Reducer.*` | bit/rate reduction |
| `Source/PluginProcessor.*` | signal flow + modulation matrix |
| `Source/PluginEditor.*` | UI: knobs + freeze + curve editor + step grid |
| `Source/gui/SequencerGrid.h` | clickable multi-level step grid (live playhead) |
| `Source/gui/CurveEditor.h` | breakpoint envelope editor (smooth/step) |

## Next steps (open TODOs)

1. Curve editor component for the 6 envelopes + the sequencer grid UI.
2. Stereo STFT (two FFTs) instead of mono-summed.
3. Factory presets (see `PRESETS.md`) wired as `AudioProcessor` programs.
4. Oversampling option for the reducer/robotize to tame aliasing.
