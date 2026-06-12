# RetroForge

**Retro game SFX synthesizer** — VST3 + Standalone (JUCE 8, C++17).

An sfxr/bfxr-inspired sound-effect design tool built as a real subtractive synth,
modeled on classic VCO / VCF / VCA voltage-control architecture. Not a musical
keyboard instrument: a one-shot SFX generator for 8-bit/16-bit arcade-style game
audio, with hand-tuned category randomizers and an editable front panel.

> Working title — final name TBD.

## Features

- **3 oscillators**: sine, triangle, square (PWM), saw, reverse saw, supersaw;
  per-osc pitch/fine, wavefolder ("Tone Mod" flavour), level, on/off
- **Noise oscillator**: continuous white↔pink color blend
- **VCF**: zero-delay-feedback ladder LPF, switchable 2-pole/4-pole, resonance to
  near-self-osc, dedicated (invertible) envelope amount; switchable 2-pole HPF
- **2 ADSR envelopes** (filter + amp): variable curve shape, invertible,
  analog gate semantics (re-attack from current level)
- **2 LFOs**: sine/tri/saw/rev-saw/square/S&H/S&G, rate + delay (fade-in)
- **6-slot mod matrix**: LFOs + envelopes → pitch, PWM, fold, noise level,
  cutoff, resonance, LFO rates, VCA
- **VCA drive**: musical tanh "preamp push"
- **Unison stack**: 1–16 voices with detune + stereo spread, per-voice analog drift
- **Trigger workflow**: momentary TRIGGER pad, MIDI note gating (optional pitch
  tracking), loop mode for tweak-while-listening, gate time
- **Generators**: full RANDOM + 7 sfxr-style categories (Pickup, Laser, Explosion,
  Powerup, Hit, Jump, Blip), VARIATE (perturb current patch), one-step UNDO,
  auto-variate per trigger (ephemeral, kills machine-gun repetition)
- **Presets**: save/load `.rfxp` files; **Export WAV**: offline render straight to disk

## Build

```
cmake -S . -B build -G "Visual Studio 17 2022" -DRF_JUCE_PATH=G:/JUCE
cmake --build build --config Release --parallel
```

Leave `RF_JUCE_PATH` empty to fetch JUCE 8.0.4 automatically.
Artefacts land in `build/RetroForge_artefacts/Release/` (VST3 + Standalone).

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
