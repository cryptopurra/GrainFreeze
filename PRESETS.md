# Starting-point preset recipes

These are *original* sound-design starting points for GrainFreeze, written as
directions you dial in yourself — not copied from any other plugin's presets.
They map the four families of behaviour we identified during analysis onto our
own parameter set. Tweak to taste; set your own exact values.

## 1. Frozen Pad (continuous, breathing)
- Freeze **on**, Sequencer **off**
- Grain Size **large** (~4096), Overlap **high**, Pitch 0, Speed 100%
- Spec Mix 0 (pure granular), Reducer clean
- Modulators: 3–4 **smooth** envelopes, long cycles (8 / 16 bars), **loop**,
  gentle depth on Grain Size, Speed and Pitch → a slow, living pad.

## 2. Slow Riser (filtered fade-in)
- Freeze **on**, Sequencer **sparse** (a few steps)
- Grain Size **medium** (~1200)
- Modulators on **16 bars**, smooth/loop:
  - Volume ramp up
  - Lowpass opening (dark → bright)
  - Pitch rising, Speed dipping then recovering
- One faster **step** modulator (2 bars) on filters/robotize for texture.

## 3. Syncopated Gate (rhythmic)
- Freeze **on**, Sequencer **on**, Resolution **1/16**, length 16
- Two gate levels: normal + accent, placed syncopated
  (e.g. normal on 3,6,7,11,14,15 — accent on 0,4,8,12)
- Medium grains; little/no envelope modulation → groove comes from the gate.

## 4. Crazy Drop (glitch / stutter)
- Freeze **on**, Sequencer **on**, Resolution **1/4**, dense two-level pattern
- Grain Size ~1600
- Several **step** modulators, short cycles (1/2 and 2 bars), **loop**:
  Volume stabs, Grain Size stair-steps, Lowpass/Highpass/Robotize jumps
  → abrupt, glitchy drops.

## Extra (beyond the original)
- **True Spectral Freeze**: Spec Mix toward 1.0, Spectral Hold high, Smear mid.
  The clean STFT holds the spectrum with advancing phases — a glassy, non-grainy
  freeze the original's time-domain path can't do. Add Robotize for a vocoder-like
  phase-locked tone.
