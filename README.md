# BitTrixer

**Retro game SFX synthesizer** — VST3 + Standalone (JUCE 8, C++17).

An sfxr/bfxr-inspired sound-effect design tool built as a real subtractive synth,
modeled on classic VCO / VCF / VCA voltage-control architecture. Not a musical
keyboard instrument: a one-shot SFX generator for 8-bit/16-bit arcade-style game
audio, with hand-tuned category randomizers and an editable front panel.

> 📖 Full usage guide: **[MANUAL.md](MANUAL.md)**.

## Features

- **2 oscillators** (sine, triangle, square/PWM, saw, rev-saw, supersaw, tan, breaker):
  per-osc pitch/fine, wavefolder, level, hard-sync
- **4-operator FM voice** in the 3rd slot: 12 algorithms, per-op ratio + level, feedback
- **Noise**: analog white↔pink + LFSR (hiss/buzz) + rasp, with color
- **Filter**: resonant ladder LPF (2/4-pole) with invertible envelope amount; switchable HPF
- **2 ADSR envelopes** (filter + amp): variable curve shape, invertible
- **LFO 1** (classic) + **LFO 2 step sequencer** (draggable 2–8 steps, glide, skew)
- **6-slot, 56-destination mod matrix** grouped by section (oscillators, FM, noise,
  filter, amp, LFOs, and every FX parameter)
- **7-effect chain**, drag-reorderable, with per-effect pre/post-filter routing
  (Crush · Phaser · Flanger · Ring Mod · Tremolo · Formant · Delay)
- **VCA**: drive + bfxr-style compression + master; lo-fi rate/bit-depth output stage
- **Unison** (1–16 voices, detune, stereo spread); sfxr-style pitch jumps
- **Generators**: RANDOM, MUTATE (explore), 15 category recipes (Pickup, Laser,
  Explode, Powerup, Hit, Jump, Blip, 1-Up, Lose, Clang, Slash, Gust, Flame, Spark,
  Shimmer), VARIATE (anchored siblings), one-step UNDO
- **Preset browser**: `.rfxp` files, subfolders = categories, ◄ ► step + dropdown
- **Export WAV** at the patch's lo-fi rate/bit-depth — deterministic (export == preview)
- **MIDI-learn** (CC mapping) + channel select; **themeable** UI (dark / light / pride)

## Build

```
cmake -S . -B build -G "Visual Studio 17 2022" -DRF_JUCE_PATH=G:/JUCE
cmake --build build --config Release --parallel
```

Leave `RF_JUCE_PATH` empty to fetch JUCE 8.0.4 automatically.
Artefacts land in `build/BitTrixer_artefacts/Release/` (VST3 + Standalone).

## Source layout

```
Source/
  PluginProcessor.*    JUCE shell: APVTS, MIDI gating, block render
  Params.h             every parameter ID/range/default + Patch snapshot
  Randomizer.*         full random / categories / variate / undo
  PresetManager.h      .rfxp save/load
  WavExporter.h        offline render to 16-bit WAV
  DSP/                 single-purpose blocks composed by Voice + SynthEngine
  UI/                  RetroLookAndFeel + one Component per panel section
docs/                  reference material (subtractive synthesis)
```

## License

AGPL-3.0-or-later — see [LICENSE](LICENSE). Copyright (C) 2026 Bytemixer.
