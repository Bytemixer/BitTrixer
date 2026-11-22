# BitTrixer User Manual (v0.2.0)

**BitTrixer** is a retro game **SFX synthesizer**: a compact subtractive and FM synth for designing 8/16-bit arcade sound effects (coins, lasers, explosions, power-ups, sword swooshes, zaps, voices, fire, UI blips) and exporting them as ready-to-drop WAV files. It is inspired by sfxr, bfxr and ChipTone, with a full modern voice underneath: two oscillators, a 4-operator FM voice, noise, a resonant filter, two envelopes, two LFOs (one a step sequencer), a 6-slot modulation matrix and a reorderable 7-effect chain.

- **Formats:** VST3 and Standalone (Windows; built with JUCE 8).
- **In one line:** dial a sound (or press a category or RANDOM), audition it, then export a small lo-fi WAV, or a seeded batch of level-matched variations.
- **License:** GNU AGPL v3.0 or later. Copyright (C) 2026 Bytemixer.

---

## 1. How it works (signal flow)

```
  OSC 1  ┐
  OSC 2  ┤
  FM     ┼ ──► (mix) ──► FILTER (LP + HP) ──► VCA (drive) ──► FX CHAIN ──► master ──► out
  NOISE  ┘                 ▲                      ▲                ▲
                           │                      │                │
                 FILTER ENV, AMP ENV, LFO 1, LFO 2 (step), routed by the MOD MATRIX

  master = gain ► soft clip ► compression ► lo-fi (rate reduction + 8-bit)
```

Every sound is **triggered** as a one-shot (or held or looped). The envelopes, LFOs and mod matrix shape it over the length of the shot. Any of the five buffer-free effects can run **before** the filter (PRE) instead of after.

When the amp envelope ends, the voices stop but the **FX chain keeps ringing** until it falls silent (delay echoes, flanger swirl), up to 6 seconds with a short fade.

**Determinism:** for a given patch, every trigger and the WAV export render **identically**: the voices, noise and LFO 1 start from the same state on every trigger. The only intended variation comes from **AUTO** (auto-variate). At host buffer sizes that are not a multiple of 16 samples, sample-and-hold modulation can land up to a third of a millisecond differently in the live preview; the export is unaffected.

---

## 2. Installation

**VST3:** copy `BitTrixer.vst3` into your VST3 folder (Windows: `C:\Program Files\Common Files\VST3\`), then rescan in your DAW.

**Standalone:** run `BitTrixer.exe`. Its **Options** menu (top of the window, outside the plugin UI) picks the audio device and sample rate.

**Building from source:** a JUCE 8 / CMake project (AGPL). `cmake -B build && cmake --build build --config Release`. JUCE is fetched automatically, or point `RF_JUCE_PATH` at a local checkout. The factory presets in `Presets/Factory` are compiled in.

---

## 3. Quick start: make a sound and export it

1. Click a **category** button in the GENERATE panel (e.g. **LASER**, **EXPLODE**, **VOICE**). A fresh sound of that type is generated and played.
2. Press it again for another take, or **TRIGGER** (in the TRIGGER panel) to hear it again.
3. Tweak knobs to taste. The **Scope** re-renders the whole sound as you edit.
4. **EXPORT WAV** (top bar) opens the export panel: type a file name, pick a directory, press **EXPORT**.
5. Want a set of variations? Set **Count** above 1 in the export panel (see section 16).

---

## 4. The top bar

Left to right:

- **BIT-TRIXER** wordmark.
- **Preset display:** the current sound's name. **`<` / `>`** step through the presets in the current preset's folder. **Click the display**, or **BROWSE**, to open the preset library (section 15).
- **SAVE** opens the save dialog. **LOAD** opens any `.rfxp` file from disk.
- **THEME:** color scheme (section 19).
- **RATE:** global output sample rate (48 / 44.1 / 22 / 11 / 8 kHz). Lower = lo-fi grit and a smaller exported file.
- **CH:** MIDI channel (`All`, or `1` to `16`). See section 18.
- **MIDI:** MIDI Learn. Right-click to clear all CC mappings.
- **EXPORT WAV:** the export panel (section 16).
- **SETTINGS:** export locations and About (section 17).

Hover any control for a one-line tooltip.

---

## 5. Sound Generators

Three tonal sources plus noise, mixed before the filter.

### OSC 1 / OSC 2
- **Wave:** Sine, Triangle, Square, Saw, Rev Saw, SuperSaw, Tan, Breaker.
- **Tune:** coarse pitch (semitones). **Fine:** fine pitch (cents).
- **PWM:** pulse width. **Fold:** wavefolder, adds harmonics.
- **Level:** this oscillator's level in the mix.
- **Sync:** hard-sync OSC 2 to OSC 1 (OSC 1 is always the master).

### FM (4-operator FM voice)
The third generator is a **4-operator FM synth** (sine operators): bells, coins, metallic clangs, inharmonic tones, formant voices.
- **Enable** and **Algorithm:** one of **12 operator routings** (the 8 classic YM2612 algorithms plus 4 extras). The glyph diagrams the routing; highlighted boxes are carriers.
- **Feedback:** operator 1 feedback; brighter, then collapses toward noise.
- **Op 1 to 4 Ratio / Level:** each operator's frequency ratio and level (carrier gain or modulation depth).
- The FM voice uses OSC 3's **Pitch / Fine / Level** as its transpose and output level.

### NOISE
- **Type** and what **Color** does for it:
  - **Analog:** white to pink blend.
  - **LFSR Hiss / LFSR Buzz:** console-style shift-register noise; Color slows the clock (bright hiss to crunchy rattle).
  - **Rasp:** clocked near-binary noise; Color sets the grit rate.
  - **Dust:** sparse random clicks, each a 0.2 to 3 ms burst, mostly quiet with the odd loud pop. Color sets the rate, about 2 to 2000 clicks per second: embers and vinyl at the low end, campfire in the middle, crinkling paper and sizzle at the top.
- **Level:** noise level in the mix.

---

## 6. Filter

A resonant low-pass with an optional high-pass.

- **Cutoff:** low-pass cutoff (overall brightness).
- **Resonance:** emphasis at the cutoff.
- **Env Amount:** how much the Filter Envelope moves the cutoff (bipolar: positive opens on the attack, negative closes).
- **Poles:** 2-pole (gentle) or 4-pole (steeper).
- **HPF:** high-pass on/off, and **HPF Freq** to remove the low end.

---

## 7. Envelopes

Two ADSR envelopes with a continuously variable curve and an invert switch.

- **Filter Envelope:** drives the cutoff (through Env Amount) and is a mod-matrix source.
- **Amp Envelope:** the VCA shape. When it finishes, the voices stop and the FX chain rings out.

Per envelope: **A / D / S / R** faders, **Curve** (exponential, linear, logarithmic) and **Invert**. The curve display shows the actual shape; faint lines mark the stage boundaries.

---

## 8. VCA / Output

- **Drive:** saturation at the VCA.
- **Comp:** bfxr-style compression: density and punch (lifts the quiet parts).
- **Master:** output level.

The master stage runs **gain, soft clip, compression, then the lo-fi stage**: sample-and-hold rate reduction to the RATE setting (deliberately **not** anti-aliased, for the authentic grit) and optional 8-bit quantisation.

---

## 9. Pitch & Voices

- **Base Hz:** the fundamental pitch. **MIDI PITCH:** follow incoming MIDI notes instead.
- **Pitch jumps (JUMP 1 / JUMP 2):** sfxr-style steps: at a set time after the trigger, the pitch jumps by N semitones (the coin "ding-ding").
- **Unison:** **Voices** (stacked detuned copies), **Detune** (cents), **Spread** (stereo width).

---

## 10. LFO 1 & LFO 2 (Step Sequencer)

Both LFOs restart with every trigger. Their depth is set per routing in the mod matrix.

### LFO 1
- **Wave:** Sine, Triangle, Saw, Rev Saw, Square, S&H (stepped random), S&G (gliding random).
- **Rate** (Hz) and **Delay** (fade-in time after the trigger).

### LFO 2: step sequencer
A draggable bar graph of **2 to 8** bipolar steps. Routed to pitch it is an arpeggio, to cutoff a rhythmic filter, to the formant vowel it "talks", to the VCA it gates.
- **Steps**, **Rate** (step advance rate), **Glide** (0 = hard steps), **Skew** (staggers the step timing, like swing). Drag the bars to set each step.

---

## 11. Mod Matrix

Six slots, each **Source to Destination** at a bipolar **Depth**.

- **Sources:** Off, LFO 1, LFO 2, Filt Env, Amp Env.
- **Destinations** (55, grouped by section):
  - **All Pitch / Base Hz / All PWM / All Fold.** All Pitch is in semitones; Base Hz is a linear frequency sweep.
  - **OSC 1 / OSC 2:** Pitch, Fine, PWM, Fold, Level.
  - **FM:** Pitch, Fine, Out, Feedback, and each operator's Ratio and Level.
  - **Noise:** Level, Color. **Filter:** Cutoff, Resonance, Filter Env amount, HPF Freq. **Amp:** VCA Level.
  - **LFO 1 / LFO 2:** Rate, plus LFO 2 Glide and Skew.
  - **FX:** the effects' parameters.

Sources are sampled every 16 samples. LFOs are bipolar; envelopes are unipolar and follow their Invert switch.

---

## 12. FX Chain

Seven effects in named strips; **drag a strip by its grip to reorder**:
**Crush, Phaser, Flanger, Ring Mod, Tremolo, Formant, Delay.**

Each strip has **on/off**. Crush, Phaser, Ring Mod, Tremolo and Formant also have **PRE**, which runs them per voice before the filter; Flanger and Delay always run after it.

- **Crush:** Bits (lower = crunchier) and sample-rate divider.
- **Phaser / Flanger:** Rate, Depth, Feedback.
- **Ring Mod:** carrier Freq, Wet, Wave.
- **Tremolo:** Speed, Depth, Wave.
- **Formant:** Vowel (ee, eh, ah, oh, oo), Reso, Wet.
- **Delay:** Time (1 to 400 ms), Feedback, Wet.

---

## 13. Trigger panel

- **TRIGGER:** play the sound once.
- **Gate:** note length for one-shots.
- **Interval:** loop interval (when LOOP is on).
- **Retrig:** re-strike rate while gated (stutter).
- **Var Amt:** the VARIATE and MUTATE amount. At 0, neither changes the sound.
- **LOOP:** retrigger at the Interval rate.
- **AUTO:** a fresh auto-variate nudge on every trigger, for live auditioning only (never exported).
- **8-BIT:** 8-bit output (sound and export).
- **VARIATE / UNDO:** see section 14.

---

## 14. Generate: Random, Mutate, Variate, Categories

The **GENERATE** panel, in four rows grouped by family:

| Row | Buttons |
|---|---|
| 1 | RANDOM, MUTATE, PICKUP, POWERUP, 1-UP, LOSE |
| 2 | BLIP, JUMP, LASER, EXPLODE, VOICE |
| 3 | HIT, IMPACT, SLASH, CLANG, TWANG |
| 4 | GUST, FLAME, CRACKLE, SPARK, SHIMMER |

- **RANDOM:** an anything-goes (but musically constrained) patch.
- **MUTATE:** explore: a compounding random walk from the current sound.
- **Categories:** hand-tuned recipes, each with several takes picked at random per press, and a small chance of reaching any parameter:
  - **PICKUP, POWERUP, 1-UP, LOSE, BLIP, JUMP, LASER:** the classic sfxr family.
  - **EXPLODE:** a rolling explosion, or a short concussive blast.
  - **VOICE:** shout or alarm, bark (sometimes "woof-woof"), growl, roar, death cry, screech, yelp or whimper, moan. Formant filter, FM formant voice, throat rattle and optional colour effects.
  - **HIT:** punches and pierces. **IMPACT:** thump, thud, body fall, heavy door; a high-passed lighter variant.
  - **SLASH:** a short blade swish. **CLANG:** metal. **TWANG:** bow string, sproing, plucked wire.
  - **GUST:** wind (sometimes sand and debris). **FLAME:** flamethrower or rocket, firewall, ignition, torch.
  - **CRACKLE:** campfire, embers, vinyl, crinkling paper, static and electric crackle.
  - **SPARK:** electric arcs. **SHIMMER:** ice, crystal, magic sparkle.

### VARIATE: siblings of a dialed-in sound
**VARIATE** (TRIGGER panel) is *anchored*: it captures your current sound and every press makes a fresh, slightly different **sibling** of it. It never wanders off, however often you press.

- **Var Amt** sets the size of each variation.
- **UNDO** reverts the last Random / Mutate / Variate / category press (one deep).

> Mutate drifts to new sounds; Variate stays in the family.

---

## 15. Presets

Presets are `.rfxp` files. **USER** presets live in **`Documents\BitTrixer Presets`** (one level of folders); **FACTORY** presets are built in and read-only.

**The library** (click the preset display, or **BROWSE**):
- **Left:** search (matches name, folder, description and tags), then **All presets, Favourites, Recent**, the USER folders (**Unsorted** is the top level) and the FACTORY folders. **+ FOLDER / RENAME / DELETE** manage user folders.
- **Middle:** the presets in the selected view. **Click** loads and plays one; **up/down** steps and plays (also from the search box). **Double-click or Enter** keeps the sound and closes. **CANCEL or Esc** closes and restores the sound you had before browsing. **Drag** a user preset onto a user folder to move it.
- **Right:** name, author, tags, **Favourite** and description. **SAVE INFO** writes them (and renames the file if you changed the name). **DELETE** moves the preset to the recycle bin. Factory presets are read-only.

**SAVE** opens a dialog: name, folder (it offers a folder named after the Generate category that made the sound), author (remembered), tags and description. It asks before overwriting.

The factory bank has 4 presets for each Generate category plus a **Random** folder. Favourites and Recent work for factory presets too.

---

## 16. Exporting WAV

**EXPORT WAV** opens the export panel:

| Field | What it does |
|---|---|
| File name | The name to write |
| Directory | Your export locations (the default is pre-selected), or BROWSE for a one-off folder |
| Create a folder named after the file | Writes into `Directory\Name\` |
| Count | 1 writes `Name.wav`; N writes `Name_0.wav` to `Name_{N-1}.wav` |
| Amount | How far the variations stray (the VARIATE scale) |
| _0 is the unchanged sound | Keeps the original as `_0` |
| Seed / NEW SEED | The same seed always renders the same batch |
| Level match | Off, Match peak, Match loudness, or Loudness target |
| Export to | Lists the directory and every file about to be written |

- Files are written at the patch's **output rate** (top-bar RATE) and **bit depth**, so they are small and game-ready.
- The render is **deterministic**: it matches the live trigger, and a batch never touches your knobs, undo or the VARIATE anchor.
- **Level match** measures loudness as **LUFS (ungated, loudest 100 ms)**, K-weighted as in ITU BS.1770, which suits short sound effects. True peak never exceeds **-1 dBTP**: a file that would go over is held at the ceiling and flagged.
- After exporting, the panel lists each file's LUFS, true peak and the gain applied. Existing files are listed and confirmed before they are overwritten.
- **AUTO** (auto-variate) only affects live triggers, never exports. Use **Count** for exportable variations.

---

## 17. Settings

**SETTINGS** (top bar):
- **Export locations:** **ADD...** a folder, **REMOVE** one, **SET DEFAULT** (or double-click a row). EXPORT WAV opens on the default.
- **ABOUT:** version and license notice.

Settings, favourites and recent presets are kept in `BitTrixer.prefs` in your user AppData folder, shared by the plugin and the standalone.

**Known limit:** each running program (the standalone, or a DAW hosting the plugin) keeps its own copy of these settings. If two run at once, the one that saves last overwrites the other's changes to favourites, recent presets and export locations. Make those changes in one program at a time.

---

## 18. MIDI

- **Channel (CH):** listen on all channels or one of `1` to `16`.
- **MIDI Learn:** click **MIDI** to arm, touch a control, then move a hardware knob or fader. Right-click **MIDI** to clear all mappings. Mappings are saved with the plugin state.
- Notes trigger the sound (pitch-tracked if MIDI PITCH is on); the TRIGGER button is a manual one-shot.

---

## 19. Themes

Pick a color scheme from **THEME**: **Slate**, **Light**, **Cute**, **Cute Dark**, **Pride**, **Pride Light**, or **Custom** with eight editable colors. Themes are purely visual.

---

## 20. Workflow tips

- **Fast SFX:** press a category a few times until one grabs you, tweak, export. The factory presets are a good starting point too.
- **The Scope** re-renders the whole sound on every change: use it to check attack, length and tail.
- **PRE effects** put grit, ring or tremolo *into* the filter sweep rather than on top.
- **Step LFO to pitch** makes arpeggios; **to Formant Vowel** makes a sound talk; **to VCA Level** gates it into stutters and crackles.
- **Lo-fi on purpose:** RATE 11 kHz with 8-BIT for authentic crunch and a tiny file.
- **Game-ready variations:** dial a hero sound, then export Count 6 with **Match loudness**: six siblings at one loudness, named `_0` to `_5`.

---

## 21. Credits & license

BitTrixer is **free software** under the **GNU Affero General Public License v3.0 or later**. It comes with **absolutely no warranty**; see the `LICENSE` file. Built with [JUCE 8](https://juce.com). Copyright (C) 2026 Bytemixer.
