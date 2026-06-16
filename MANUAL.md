# BitTrixer — User Manual

**BitTrixer** is a retro game **SFX synthesizer**: a compact subtractive + FM synth built for designing 8/16‑bit arcade sound effects — coins, lasers, explosions, power‑ups, sword swooshes, zaps, UI blips — and exporting them as ready‑to‑drop WAV files. It is inspired by sfxr / bfxr / ChipTone, but with a full modern voice underneath: two oscillators, a 4‑operator FM voice, noise, a resonant filter, two envelopes, two LFOs (one a step sequencer), a 6‑slot modulation matrix, and a reorderable 7‑effect chain.

- **Formats:** VST3 + Standalone (Windows; built with JUCE 8).
- **In one line:** dial a sound (or hit a category/RANDOM button), audition it, then export a small lo‑fi WAV — or generate subtle variations for a whole batch.
- **License:** GNU AGPL v3.0 or later. Copyright © 2026 Bytemixer.

---

## 1. How it works (signal flow)

```
  OSC 1  ┐
  OSC 2  ┤
  FM     ┼ ──► (mix) ──► FILTER (LP + HP) ──► VCA (drive ► comp) ──► FX CHAIN ──► master (gain ► lo‑fi) ──► out
  NOISE  ┘                 ▲                      ▲                   ▲
                           │                      │                   │
                 FILTER ENV, AMP ENV, LFO 1, LFO 2 (step) ─── routed by the MOD MATRIX
```

Every sound is **triggered** as a one‑shot (or held/looped). The two envelopes, two LFOs and the mod matrix shape it over the length of the shot. Per‑effect, any of the buffer‑free FX can be moved **before** the filter (pre‑filter) instead of after.

**Determinism:** for a given patch, every trigger — and the WAV export — renders **identically** (fixed voice seed + clean envelope state). The only intentional variation comes from **Auto‑Variate** or a free‑running (non‑retriggered) LFO. This is what makes "what you hear is what you export" true.

---

## 2. Installation

**VST3:** copy `BitTrixer.vst3` into your VST3 folder (Windows: `C:\Program Files\Common Files\VST3\`), then rescan in your DAW.

**Standalone:** run `BitTrixer.exe`. Use its menu bar (top of the window, outside the plugin UI) to pick your audio device and sample rate.

**Building from source:** it's a JUCE 8 / CMake project (AGPL). `cmake -B build && cmake --build build --config Release`. JUCE is fetched automatically (or point `RF_JUCE_PATH` at a local checkout).

---

## 3. Quick start — make a sound and export it

1. Click a **category** button at the bottom (e.g. **LASER**, **EXPLODE**, **SHIMMER**). A fresh sound of that archetype is generated and auditioned.
2. Press **RANDOM** for more of the same archetype, or hit the **Trigger** button (in the TRIGGER panel) to re‑audition.
3. Tweak knobs to taste — the **Scope** (center) re‑renders the whole sound as you edit.
4. Happy with it? **EXPORT WAV** (top bar) → choose a name → done. The file is written at the patch's chosen output rate / bit depth (small, game‑ready).
5. Want a batch of subtle variations? See §14 (Variate).

---

## 4. The top bar

Left to right:

- **BIT‑TRIXER** wordmark.
- **Preset display** (center): shows the current patch name. **`<` / `>`** step through presets; **click the display** to open a browse menu (categories are submenus — see §15).
- **SAVE / LOAD** — write/read a `.rfxp` preset file.
- **CH** — MIDI channel to listen on (`All`, or `1`–`16`). See §17.
- **MIDI** — MIDI‑Learn (see §17). Right‑click to clear all CC mappings.
- **RATE** — global output sample rate (48 / 44.1 / 22 / 11 / 8 kHz). Lower = lo‑fi grit, and a smaller exported file.
- **THEME** — color scheme (see §18).
- **EXPORT WAV** — render the current patch to a WAV file.
- **ABOUT** — version + license notice.

Hover any control for a one‑line tooltip.

---

## 5. Sound Generators

Three tonal sources plus noise, mixed before the filter.

### OSC 1 / OSC 2
Classic subtractive oscillators.
- **Wave** — Sine, Triangle, Square, Saw, Rev Saw, SuperSaw, Tan, Breaker.
- **Tune** — coarse pitch (semitones).
- **Fine** — fine pitch (cents).
- **PWM** — pulse width / square duty cycle.
- **Fold** — wavefolder; adds harmonics by folding the wave.
- **Level** — this oscillator's level in the mix.
- **Sync** — hard‑sync this oscillator to OSC 1 (OSC 1 is always the master).

### FM (4‑operator FM voice)
The 3rd generator slot is a **4‑operator FM synth** (sine operators), great for bells, coins, metallic clangs and inharmonic timbres.
- **Enable** + **Algorithm** — choose one of **12 operator routings** (the 8 classic YM2612/Genesis algorithms plus 4 extras). The small **glyph** diagrams the selected algorithm: boxes are operators, highlighted ones are carriers.
- **Feedback** — operator‑1 feedback: adds brightness, then collapses toward noise at high settings.
- **Op 1–4 Ratio / Level** — each operator's frequency ratio (harmonic of the note) and level (carrier gain or modulation depth, depending on the algorithm).
- The FM voice reuses OSC 3's **Pitch / Fine / Level** as its transpose and output level.

### NOISE
- **Type** — Analog (white→pink), LFSR Hiss, LFSR Buzz, Rasp.
- **Color** — noise tone (white→pink, or the LFSR clock).
- **Level** — noise level in the mix.

---

## 6. Filter

A resonant low‑pass with an optional high‑pass.

- **Cutoff** — low‑pass cutoff (overall brightness).
- **Resonance** — emphasis at the cutoff.
- **Key** — key tracking (cutoff follows pitch).
- **Env Amount** — how much the **Filter Envelope** opens/closes the cutoff (bipolar — positive opens on attack, negative closes).
- **Poles** — 2‑pole (gentle) or 4‑pole (steeper).
- **HPF** — high‑pass on/off + **HPF Freq** to remove the low end.

---

## 7. Envelopes

Two ADSR envelopes with a continuously variable curve and an invert switch.

- **Filter Envelope** — drives the filter cutoff (via the filter **Env Amount**), and is also a mod‑matrix source.
- **Amp Envelope** — the VCA shape; when it finishes, the voice ends.

Per envelope: **A / D / S / R** faders, **Curve** (exponential ← linear → logarithmic), **Invert** (flips the shape). The live **curve display** shows the actual ADSR shape; faint lines mark the A/D/S boundaries. An instant (0) attack reads near‑vertical.

> Note: each trigger starts the envelopes from a **clean** state — re‑triggers don't carry over a residual level, so the sound is consistent and the export matches the preview.

---

## 8. VCA / Output

- **Drive** — saturation / grit at the output.
- **Comp** — bfxr‑style compression: density and punch (boosts the quiet parts).
- **Master** — output level.

The final master stage applies gain → compression → a safety soft‑clip → the **lo‑fi stage** (anti‑aliased rate reduction + bit‑depth quantization, set by the top‑bar **RATE** and the bit‑depth option).

---

## 9. Pitch & Voices

- **Base Hz** — the fundamental pitch of the sound.
- **Pitch Jumps (JUMP 1 / JUMP 2)** — sfxr‑style arpeggio steps: at a set time after the trigger, the pitch jumps by N semitones (the classic coin "ding‑ding").
- **Unison** — **Voices** (stacked detuned copies), **Detune** (spread in cents), **Spread** (stereo width).

---

## 10. LFO 1 & LFO 2 (Step Sequencer)

### LFO 1 — classic LFO
- **Rate** (Hz), **Amount**, **Phase**, **Wave** (Sine, Triangle, Saw, Rev Saw, Square, S&H, S&G), **Delay** (fade‑in), **Trig** (retrigger per note) / **Sync**.

### LFO 2 — step sequencer (the BitTrixer twist)
A draggable **2–8 step** bar graph of bipolar levels. Route it through the mod matrix: to pitch it's an arpeggio; to cutoff a rhythmic filter; to a vowel it "talks."
- **Steps** — number of steps. **Rate** — advance rate (Hz). **Glide** — glide between steps (0 = hard). **Skew** — stagger the step timing (swing). Drag the bars to set each step's level.

---

## 11. Mod Matrix

Six routing slots. Each slot: **Source → Destination** at a bipolar **Depth**.

- **Sources:** Off, LFO 1, LFO 2, Filt Env, Amp Env.
- **Destinations** (56, grouped by section in the dropdown):
  - **All Pitch / Base Hz / All PWM / All Fold** (global) — All Pitch is musical (semitones); Base Hz is linear (frequency‑domain sweep).
  - **OSC 1 / OSC 2:** Pitch, Fine, PWM, Fold, Level.
  - **FM:** Pitch, Fine, Out, Feedback, and per‑operator Ratio + Level.
  - **Noise:** Level, Color.
  - **Filter:** Cutoff, Resonance, Filter Env amount, HPF Freq.
  - **Amp:** VCA Level.
  - **LFO 1 / LFO 2:** Rate; plus LFO 2 Glide & Skew.
  - **FX:** every effect's parameters (Crush, Phaser, Flanger, Ring, Tremolo, Formant, Delay).

Sources are sampled once per audio sub‑block ("CV" snapshot). LFOs are bipolar; envelopes unipolar (and respect their Invert switch).

---

## 12. FX Chain

Seven effects, each in its own named strip, **drag to reorder**:
**Crush · Phaser · Flanger · Ring Mod · Tremolo · Formant · Delay.**

Each strip has an **on/off** and a **PRE** switch. The five buffer‑free effects (Crush, Phaser, Ring Mod, Tremolo, Formant) can run **pre‑filter** (before the cutoff shapes them) by flipping **PRE**; Flanger and Delay are always post (they use delay buffers). The chain order is reflected in the signal traces.

- **Crush** — Bits (lower = crunchier) + Sample‑rate divider.
- **Phaser / Flanger** — Rate, Depth, Feedback.
- **Ring Mod** — carrier Freq, Wet, Wave.
- **Tremolo** — Speed, Depth, Wave.
- **Formant** — Vowel (A‑E‑I‑O‑U morph), Reso, Wet.
- **Delay** — Time, Feedback, Wet (+ stereo options).

---

## 13. Trigger panel

- **TRIGGER** — fire the sound once (or hold).
- **Gate** — auto note length for one‑shots.
- **Interval** — loop interval (when LOOP is on).
- **Retrig** — re‑strike rate (stutter / arpeggio) while gated.
- **Var Amt** — the Variate amount (see §14). *Note: this knob scales both VARIATE and MUTATE — at 0, neither changes the sound.*
- **LOOP** — auto‑retrigger at the Interval rate.
- **AUTO** — apply an Auto‑Variate nudge on every trigger (ephemeral, for live auditioning — see §14 / §16).
- **8‑BIT** — export bit depth (8‑bit vs 16‑bit).
- **VARIATE / UNDO** — see §14.

---

## 14. Generate — Random, Mutate, Variate, Categories

The colorful **GENERATE** panel is the heart of fast SFX creation.

- **RANDOM** — a full anything‑goes (but musically constrained) patch.
- **MUTATE** — *explore*: a compounding random walk from the current sound. Each press builds on the last, progressively wandering toward new territory.
- **Category buttons** (15) — hand‑tuned recipes per archetype, each biased toward its sound but able to occasionally reach any parameter:
  - **PICKUP · LASER · EXPLODE · POWERUP · HIT · JUMP · BLIP · 1‑UP · LOSE** (classic sfxr family)
  - **CLANG · SLASH · GUST · FLAME · SPARK · SHIMMER** (metal, sword swoosh, wind, fire, electric, ice/crystal)

### VARIATE — subtle siblings of a dialed‑in sound
**VARIATE** (TRIGGER panel) is *anchored*: it captures your current sound as an anchor and every press makes a fresh, subtly‑different **sibling** of it — it never wanders off, no matter how many times you press. This is the game‑audio workflow for avoiding "machine‑gun, identical sound" ear fatigue: dial in a sound, then generate a handful of variations.

- **Var Amt** controls how large each variation is.
- **UNDO** reverts the last Random / Mutate / Variate / category press (one deep).

> **Mutate vs Variate:** Mutate *drifts* (explore new sounds); Variate *stays in the family* (subtle variations of what you have).

---

## 15. Presets

Presets are `.rfxp` files stored in **`Documents\BitTrixer Presets`**.

- **SAVE / LOAD** use a file dialog (defaulting to that folder).
- **Browse:** click the preset display in the top bar for a menu of everything in the folder; **`<` / `>`** step through them.
- **Categories = subfolders.** Drop presets into subfolders (`Sweeps`, `Explosions`, `Shots`, …) and each becomes a **submenu**, auto‑grouped on next open. The browser rescans every time you open the menu and after every Save.

---

## 16. Exporting WAV

**EXPORT WAV** renders the current patch offline and writes a stereo WAV at the patch's **output rate** (top‑bar RATE) and **bit depth** (8/16), so the file is small and game‑ready (jsfxr‑style), not a fat host‑rate container.

- The render is **deterministic**: the exported file matches the live Trigger exactly for a given patch.
- **Batch variations:** to make a folder of subtly‑different renders of the same sound, use the **VARIATE button** → **EXPORT** → repeat. Each Variate press commits a new sibling to the parameters, and the export captures it exactly.
- **Auto‑Variate is for live auditioning, not export** — its per‑trigger nudges are ephemeral (they never touch the parameters) and are not baked into the exported file. Use the **Variate button** when you want exportable variations.

---

## 17. MIDI

- **Channel (CH):** listen on `All` channels or a specific `1`–`16`. Notes and CC outside the chosen channel are ignored.
- **MIDI‑Learn:** click **MIDI** to arm, touch any control on the UI, then move a hardware knob/fader — the CC binds to that parameter. Right‑click **MIDI** to clear all mappings. Mappings persist in the plugin state.
- Notes trigger the sound (pitch‑tracked if MIDI‑track is enabled); the Trigger button is a manual one‑shot.

---

## 18. Themes

Pick a color scheme from the **THEME** dropdown — the whole UI (including the preset display and signal traces) reskins to match, with several dark schemes plus light and Pride options. Themes are purely visual and have no effect on the sound.

---

## 19. Workflow tips

- **Fast SFX:** hit a category → RANDOM a few times until one grabs you → tweak → export. The category buttons are tuned but randomized, so press a few times to see the range.
- **The Scope** re‑renders the whole sound on every change — use it to eyeball attack, length and shape.
- **Pre‑filter FX** (the PRE switches) put grit/ring/tremolo *into* the filter sweep rather than on top — great for the filter to shape the effect.
- **Step LFO → pitch** makes instant arpeggios; **Step LFO → Formant Vowel** makes a sound "talk."
- **Lo‑fi on purpose:** drop the RATE to 11 kHz / 8‑bit for authentic crunch (and a tiny file).
- **Variation batch:** dial a hero sound, then Variate → Export, Variate → Export… for a set of non‑repetitive in‑game instances.

---

## 20. Credits & license

BitTrixer is **free software** under the **GNU Affero General Public License v3.0 or later**. It comes with **absolutely no warranty**; see the `LICENSE` file for details. Built with [JUCE 8](https://juce.com). Copyright © 2026 Bytemixer.
