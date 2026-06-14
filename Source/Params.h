/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// ============================================================================
//  Params
//  Single source of truth for every parameter ID, range and default, plus
//  the plain-data Patch snapshot the DSP engine consumes. The engine never
//  touches juce parameter objects directly.
// ============================================================================

namespace Params
{
    inline constexpr int   kNumOscs     = 3;
    inline constexpr int   kNumLfos     = 2;     // LFO 2 is the Step LFO
    inline constexpr int   kNumModSlots = 6;
    inline constexpr int   kMaxUnison   = 16;
    inline constexpr int   kMaxSteps    = 8;
    inline constexpr int   kFxSlots     = 6;     // reorderable FX chain slots (mirror FxChain::kSlots)
    inline constexpr int   kFxChainLen  = 7;     // FX chain effects/order length (mirror FxChain::kCount)

    // ---- enum orderings (must match the choice arrays below) ----
    enum class OscWave  { Sine = 0, Triangle, Square, Saw, RevSaw, SuperSaw, Tan, Breaker };
    enum class NoiseType { Analog = 0, LfsrHiss, LfsrBuzz, Rasp };
    enum class LfoWave  { Sine = 0, Triangle, Saw, RevSaw, Square, SampleHold, SampleGlide };
    // Osc sync: OSC 1 is always the master; each other oscillator has its own
    // sync switch that hard-syncs it to OSC 1 (per-osc bool, see OscPatch).
    enum class ModSrc   { Off = 0, Lfo1, Lfo2, FilterEnv, AmpEnv };
    enum class ModDest  { Off = 0, AllPitch, Osc1Pitch, Osc2Pitch, Osc3Pitch,
                          Pwm, Fold, NoiseLevel, Cutoff, Resonance,
                          Lfo1Rate, Lfo2Rate, VcaLevel,
                          FormVowel, RingFreq, TremDepth, DelayTime };

    inline const juce::StringArray oscWaveNames  { "Sine", "Triangle", "Square", "Saw", "Rev Saw", "SuperSaw", "Tan", "Breaker" };
    inline const juce::StringArray noiseTypeNames { "Analog W>P", "LFSR Hiss", "LFSR Buzz", "Rasp" };
    inline const juce::StringArray lfoWaveNames  { "Sine", "Triangle", "Saw", "Rev Saw", "Square", "S&H", "S&G" };
    inline const juce::StringArray modSrcNames   { "Off", "LFO 1", "LFO 2", "Filt Env", "Amp Env" };
    inline const juce::StringArray modDestNames  { "Off", "All Pitch", "Osc1 Pitch", "Osc2 Pitch", "Osc3 Pitch",
                                                   "PWM", "Fold", "Noise Lvl", "Cutoff", "Resonance",
                                                   "LFO1 Rate", "LFO2 Rate", "VCA Level",
                                                   "Form Vowel", "Ring Freq", "Trem Depth", "Delay Time" };
    inline const juce::StringArray polesNames    { "2-Pole", "4-Pole" };
    inline const juce::StringArray rateNames     { "48 kHz", "44.1 kHz", "22 kHz", "11 kHz", "8 kHz" };
    inline const juce::StringArray fxTypeNames   { "Off", "Crush", "Phaser", "Flanger", "Ring Mod", "Tremolo", "Formant", "Delay" };  // mirror FxChain::Type
    inline const juce::StringArray fxWaveNames   { "Sine", "Tri", "Square", "Saw" };   // RingMod carrier / Tremolo LFO shape

    inline float rateChoiceToHz (int choice) noexcept
    {
        switch (choice) { case 1: return 44100.0f; case 2: return 22050.0f;
                          case 3: return 11025.0f; case 4: return 8000.0f;
                          default: return 48000.0f; }
    }

    // hard-sync routing: slave oscillators reset phase when the master wraps

    // ---- ID helpers ----
    inline juce::String oscId  (int osc1Based, const char* suffix)  { return "osc"  + juce::String (osc1Based) + "_" + suffix; }
    inline juce::String lfoId  (int lfo1Based, const char* suffix)  { return "lfo"  + juce::String (lfo1Based) + "_" + suffix; }
    inline juce::String modId  (int slot1Based, const char* suffix) { return "mod"  + juce::String (slot1Based) + "_" + suffix; }
    inline juce::String stepValId (int step1Based)                  { return "step_val" + juce::String (step1Based); }
    inline juce::String fxId   (int slot1Based, const char* suffix) { return "fxslot" + juce::String (slot1Based) + "_" + suffix; }
    inline juce::String fxOrderId (int pos)                         { return "fxorder" + juce::String (pos); }

    namespace id
    {
        inline constexpr const char* baseFreq    = "base_freq";
        inline constexpr const char* midiTrack   = "midi_track";

        // discrete pitch jumps (the sfxr/bfxr/jfxr "arpeggio" ingredient)
        inline constexpr const char* pj1Amt      = "pj1_amt";
        inline constexpr const char* pj1Time     = "pj1_time";
        inline constexpr const char* pj2Amt      = "pj2_amt";
        inline constexpr const char* pj2Time     = "pj2_time";

        inline constexpr const char* noiseOn     = "noise_on";
        inline constexpr const char* noiseType   = "noise_type";
        inline constexpr const char* noiseColor  = "noise_color";
        inline constexpr const char* noiseLevel  = "noise_level";

        inline constexpr const char* lpfCutoff   = "lpf_cutoff";
        inline constexpr const char* lpfRes      = "lpf_res";
        inline constexpr const char* lpfPoles    = "lpf_poles";
        inline constexpr const char* lpfEnv      = "lpf_env";
        inline constexpr const char* hpfOn       = "hpf_on";
        inline constexpr const char* hpfCutoff   = "hpf_cutoff";

        inline constexpr const char* envFAttack  = "envf_attack";
        inline constexpr const char* envFDecay   = "envf_decay";
        inline constexpr const char* envFSustain = "envf_sustain";
        inline constexpr const char* envFRelease = "envf_release";
        inline constexpr const char* envFCurve   = "envf_curve";
        inline constexpr const char* envFInvert  = "envf_invert";

        inline constexpr const char* envAAttack  = "enva_attack";
        inline constexpr const char* envADecay   = "enva_decay";
        inline constexpr const char* envASustain = "enva_sustain";
        inline constexpr const char* envARelease = "enva_release";
        inline constexpr const char* envACurve   = "enva_curve";
        inline constexpr const char* envAInvert  = "enva_invert";

        inline constexpr const char* vcaDrive    = "vca_drive";

        inline constexpr const char* uniVoices   = "uni_voices";
        inline constexpr const char* uniDetune   = "uni_detune";
        inline constexpr const char* uniSpread   = "uni_spread";

        inline constexpr const char* gateTime    = "gate_time";
        inline constexpr const char* loopOn      = "loop_on";
        inline constexpr const char* loopRate    = "loop_rate";
        inline constexpr const char* autoVarOn   = "autovar_on";
        inline constexpr const char* autoVarAmt  = "autovar_amt";
        inline constexpr const char* retrigRate  = "retrig_rate";
        inline constexpr const char* stepCount   = "step_count";
        inline constexpr const char* stepGlide   = "step_glide";
        inline constexpr const char* stepSkew    = "step_skew";

        inline constexpr const char* masterVol   = "master_vol";
        inline constexpr const char* comp        = "comp";
        inline constexpr const char* outRate     = "out_rate";
        inline constexpr const char* outBits     = "out_bits";

        inline constexpr const char* crushOn     = "fxcrush_on";
        inline constexpr const char* crushBits   = "fxcrush_bits";
        inline constexpr const char* crushDown   = "fxcrush_down";
        inline constexpr const char* phaseOn     = "fxphase_on";
        inline constexpr const char* phaseRate   = "fxphase_rate";
        inline constexpr const char* phaseDepth  = "fxphase_depth";
        inline constexpr const char* phaseFb     = "fxphase_fb";
        inline constexpr const char* flangeOn    = "fxflange_on";
        inline constexpr const char* flangeRate  = "fxflange_rate";
        inline constexpr const char* flangeDepth = "fxflange_depth";
        inline constexpr const char* flangeFb    = "fxflange_fb";
        inline constexpr const char* ringOn      = "fxring_on";
        inline constexpr const char* ringFreq    = "fxring_freq";
        inline constexpr const char* ringMix     = "fxring_mix";
        inline constexpr const char* ringWave    = "fxring_wave";
        inline constexpr const char* tremOn      = "fxtrem_on";
        inline constexpr const char* tremRate    = "fxtrem_rate";
        inline constexpr const char* tremDepth   = "fxtrem_depth";
        inline constexpr const char* tremWave    = "fxtrem_wave";
        inline constexpr const char* formOn      = "fxform_on";
        inline constexpr const char* formVowel   = "fxform_vowel";
        inline constexpr const char* formReso    = "fxform_reso";
        inline constexpr const char* formMix     = "fxform_mix";
        inline constexpr const char* delayOn     = "fxdelay_on";
        inline constexpr const char* delayTime   = "fxdelay_time";
        inline constexpr const char* delayFb     = "fxdelay_fb";
        inline constexpr const char* delayMix    = "fxdelay_mix";
        inline constexpr const char* fxSplit     = "fx_split";   // mono subgroup pre-filter
    }

    // ------------------------------------------------------------------
    //  Plain-data snapshot of the whole front panel, built once per block.
    // ------------------------------------------------------------------
    struct OscPatch
    {
        bool    on = false;
        bool    sync = false;        // hard-sync this osc to OSC 1 (ignored for OSC 1)
        OscWave wave = OscWave::Square;
        float   pitchSemis = 0.0f;   // coarse, semitones
        float   fineCents  = 0.0f;
        float   pwm        = 0.5f;   // 0.05 .. 0.95
        float   fold       = 0.0f;
        float   level      = 0.8f;
    };

    struct LfoPatch
    {
        LfoWave wave = LfoWave::Triangle;
        float   rateHz  = 5.0f;
        float   delaySec = 0.0f;
    };

    struct ModSlot
    {
        ModSrc  src  = ModSrc::Off;
        ModDest dest = ModDest::Off;
        float   depth = 0.0f;        // -1 .. +1
    };

    struct EnvPatch
    {
        float attack  = 0.001f;      // seconds
        float decay   = 0.3f;
        float sustain = 0.7f;
        float release = 0.2f;
        float curve   = 0.0f;        // -1 exp .. 0 lin .. +1 log
        bool  invert  = false;
    };

    struct Patch
    {
        std::array<OscPatch, kNumOscs> osc;
        float baseFreqHz = 440.0f;
        bool  midiTrack  = false;

        float pj1AmtSemis = 0.0f;   // 0 = jump disabled
        float pj1TimeSec  = 0.08f;
        float pj2AmtSemis = 0.0f;
        float pj2TimeSec  = 0.16f;

        bool  noiseOn    = false;
        NoiseType noiseType = NoiseType::Analog;
        float noiseColor = 0.0f;     // Analog: white..pink | LFSR: clock divide | Rasp: grit rate
        float noiseLevel = 0.5f;

        std::array<LfoPatch, kNumLfos>   lfo;   // lfo[1] = Step LFO (uses rate/delay)
        int   stepCount = 4;
        float stepGlide = 0.0f;     // 0 = hard steps .. 1 = fully glided
        float stepSkew  = 0.0f;     // -1..+1 stagger of step durations (swing)
        std::array<float, kMaxSteps> stepVals { -0.6f, -0.2f, 0.2f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<ModSlot, kNumModSlots> mod;

        float lpfCutoff = 20000.0f;
        float lpfRes    = 0.0f;
        bool  lpf4Pole  = true;
        float lpfEnvAmt = 0.0f;      // -1 .. +1
        bool  hpfOn     = false;
        float hpfCutoff = 20.0f;

        EnvPatch envF, envA;

        float vcaDrive = 0.0f;

        int   uniVoices = 1;
        float uniDetuneCents = 12.0f;
        float uniSpread = 0.5f;

        float gateTime  = 0.25f;     // seconds
        bool  loopOn    = false;
        float loopRate  = 1.0f;      // seconds between triggers
        bool  autoVarOn = false;
        float autoVarAmt = 0.15f;
        float retrigHz  = 0.0f;      // 0 = off; else re-strike rate within a sound

        float masterVolDb = -6.0f;
        float compAmount = 0.0f;     // 0 = off; power-law density/punch
        float outRateHz = 48000.0f;  // target output sample rate (lo-fi decimation)
        bool  out8bit   = false;     // 8-bit output quantization

        // integrated FX
        bool  crushOn = false;
        float crushBits = 8.0f;       // 2..16
        float crushDown = 1.0f;       // 1..40 sample-hold factor
        bool  phaseOn = false;
        float phaseRate = 1.0f;
        float phaseDepth = 0.5f;
        float phaseFb = 0.3f;
        bool  flangeOn = false;
        float flangeRate = 0.5f;
        float flangeDepth = 0.5f;
        float flangeFb = 0.4f;
        bool  ringOn = false;
        float ringFreq = 200.0f;
        float ringMix = 1.0f;
        int   ringWave = 0;
        bool  tremOn = false;
        float tremRate = 5.0f;
        float tremDepth = 0.5f;
        int   tremWave = 0;
        bool  formOn = false;
        float formVowel = 0.5f;       // centred so bipolar mod sweeps both ways
        float formReso = 0.5f;
        float formMix = 1.0f;
        bool  delayOn = false;
        float delayTime = 0.12f;      // seconds (1 ms .. 400 ms)
        float delayFb = 0.4f;
        float delayMix = 0.4f;

        // FX chain processing order (position -> FxChain::Effect index). Mono
        // pre-capable effects first, Flanger(2)/Delay(6) last (right) so they
        // sit where they land when the pre-filter split engages.
        std::array<int, kFxChainLen> fxOrder { 0, 1, 3, 4, 5, 2, 6 };
        bool fxSplit = false;   // true => mono subgroup runs per-voice pre-filter

        // reorderable FX chain: per-slot type index (FxChain::Type) + 3 generic params A/B/C
        std::array<int,   kFxSlots> fxSlotType { 0, 0, 0, 0, 0, 0 };
        std::array<float, kFxSlots> fxSlotA { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, kFxSlots> fxSlotB { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, kFxSlots> fxSlotC { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    };

    // ------------------------------------------------------------------
    //  Cached raw-value pointers for lock-free per-block snapshotting.
    // ------------------------------------------------------------------
    struct Cache
    {
        using APVTS = juce::AudioProcessorValueTreeState;

        explicit Cache (APVTS& s)
        {
            auto get = [&s] (const juce::String& pid)
            {
                auto* p = s.getRawParameterValue (pid);
                jassert (p != nullptr);
                return p;
            };

            for (int i = 0; i < kNumOscs; ++i)
            {
                oscOn[i]    = get (oscId (i + 1, "on"));
                oscWave[i]  = get (oscId (i + 1, "wave"));
                oscPitch[i] = get (oscId (i + 1, "pitch"));
                oscFine[i]  = get (oscId (i + 1, "fine"));
                oscPwm[i]   = get (oscId (i + 1, "pwm"));
                oscFold[i]  = get (oscId (i + 1, "fold"));
                oscLevel[i] = get (oscId (i + 1, "level"));
                // OSC 1 is the master; only OSC 2/3 carry a sync param
                oscSync[i]  = i == 0 ? nullptr : get (oscId (i + 1, "sync"));
            }

            baseFreq  = get (id::baseFreq);
            midiTrack = get (id::midiTrack);
            pj1Amt    = get (id::pj1Amt);
            pj1Time   = get (id::pj1Time);
            pj2Amt    = get (id::pj2Amt);
            pj2Time   = get (id::pj2Time);

            noiseOn    = get (id::noiseOn);
            noiseType  = get (id::noiseType);
            noiseColor = get (id::noiseColor);
            noiseLevel = get (id::noiseLevel);

            for (int j = 0; j < kNumLfos; ++j)
            {
                lfoWave[j]  = get (lfoId (j + 1, "wave"));
                lfoRate[j]  = get (lfoId (j + 1, "rate"));
                lfoDelay[j] = get (lfoId (j + 1, "delay"));
            }

            stepCount  = get (id::stepCount);
            stepGlide  = get (id::stepGlide);
            stepSkew   = get (id::stepSkew);
            for (int k = 0; k < kMaxSteps; ++k)
                stepVal[k] = get (stepValId (k + 1));

            for (int k = 0; k < kNumModSlots; ++k)
            {
                modSrc[k]   = get (modId (k + 1, "src"));
                modDest[k]  = get (modId (k + 1, "dest"));
                modDepth[k] = get (modId (k + 1, "depth"));
            }

            lpfCutoff = get (id::lpfCutoff);
            lpfRes    = get (id::lpfRes);
            lpfPoles  = get (id::lpfPoles);
            lpfEnv    = get (id::lpfEnv);
            hpfOn     = get (id::hpfOn);
            hpfCutoff = get (id::hpfCutoff);

            envF[0] = get (id::envFAttack);  envF[1] = get (id::envFDecay);
            envF[2] = get (id::envFSustain); envF[3] = get (id::envFRelease);
            envF[4] = get (id::envFCurve);   envF[5] = get (id::envFInvert);

            envA[0] = get (id::envAAttack);  envA[1] = get (id::envADecay);
            envA[2] = get (id::envASustain); envA[3] = get (id::envARelease);
            envA[4] = get (id::envACurve);   envA[5] = get (id::envAInvert);

            vcaDrive  = get (id::vcaDrive);
            uniVoices = get (id::uniVoices);
            uniDetune = get (id::uniDetune);
            uniSpread = get (id::uniSpread);

            gateTime  = get (id::gateTime);
            loopOn    = get (id::loopOn);
            loopRate  = get (id::loopRate);
            autoVarOn = get (id::autoVarOn);
            autoVarAmt = get (id::autoVarAmt);
            retrigRate = get (id::retrigRate);

            masterVol = get (id::masterVol);
            comp      = get (id::comp);
            outRate   = get (id::outRate);
            outBits   = get (id::outBits);

            crushOn    = get (id::crushOn);
            crushBits  = get (id::crushBits);
            crushDown  = get (id::crushDown);
            phaseOn    = get (id::phaseOn);
            phaseRate  = get (id::phaseRate);
            phaseDepth = get (id::phaseDepth);
            phaseFb    = get (id::phaseFb);
            flangeOn    = get (id::flangeOn);
            flangeRate  = get (id::flangeRate);
            flangeDepth = get (id::flangeDepth);
            flangeFb    = get (id::flangeFb);
            ringOn   = get (id::ringOn);   ringFreq  = get (id::ringFreq);   ringMix   = get (id::ringMix);   ringWave = get (id::ringWave);
            tremOn   = get (id::tremOn);   tremRate  = get (id::tremRate);   tremDepth = get (id::tremDepth); tremWave = get (id::tremWave);
            formOn   = get (id::formOn);   formVowel = get (id::formVowel);  formReso  = get (id::formReso);  formMix  = get (id::formMix);
            delayOn  = get (id::delayOn);  delayTime = get (id::delayTime);  delayFb   = get (id::delayFb);   delayMix = get (id::delayMix);
            for (int k = 0; k < kFxChainLen; ++k)
                fxOrder[k] = get (fxOrderId (k));
            fxSplit = get (id::fxSplit);

            for (int k = 0; k < kFxSlots; ++k)
            {
                fxSlotType[k] = get (fxId (k + 1, "type"));
                fxSlotA[k]    = get (fxId (k + 1, "a"));
                fxSlotB[k]    = get (fxId (k + 1, "b"));
                fxSlotC[k]    = get (fxId (k + 1, "c"));
            }
        }

        Patch read() const
        {
            Patch p;
            for (int i = 0; i < kNumOscs; ++i)
            {
                auto& o = p.osc[(size_t) i];
                o.on         = oscOn[i]->load() > 0.5f;
                o.sync       = oscSync[i] != nullptr && oscSync[i]->load() > 0.5f;
                o.wave       = (OscWave) (int) oscWave[i]->load();
                o.pitchSemis = oscPitch[i]->load();
                o.fineCents  = oscFine[i]->load();
                o.pwm        = oscPwm[i]->load() * 0.01f;   // % -> 0..1
                o.fold       = oscFold[i]->load();
                o.level      = oscLevel[i]->load();
            }

            p.baseFreqHz = baseFreq->load();
            p.midiTrack  = midiTrack->load() > 0.5f;
            p.pj1AmtSemis = pj1Amt->load();
            p.pj1TimeSec  = pj1Time->load();
            p.pj2AmtSemis = pj2Amt->load();
            p.pj2TimeSec  = pj2Time->load();

            p.noiseOn    = noiseOn->load() > 0.5f;
            p.noiseType  = (NoiseType) (int) noiseType->load();
            p.noiseColor = noiseColor->load();
            p.noiseLevel = noiseLevel->load();

            for (int j = 0; j < kNumLfos; ++j)
            {
                auto& l = p.lfo[(size_t) j];
                l.wave     = (LfoWave) (int) lfoWave[j]->load();
                l.rateHz   = lfoRate[j]->load();
                l.delaySec = lfoDelay[j]->load();
            }

            p.stepCount  = (int) stepCount->load();
            p.stepGlide  = stepGlide->load();
            p.stepSkew   = stepSkew->load();
            for (int k = 0; k < kMaxSteps; ++k)
                p.stepVals[(size_t) k] = stepVal[k]->load();

            for (int k = 0; k < kNumModSlots; ++k)
            {
                auto& m = p.mod[(size_t) k];
                m.src   = (ModSrc)  (int) modSrc[k]->load();
                m.dest  = (ModDest) (int) modDest[k]->load();
                m.depth = modDepth[k]->load();
            }

            p.lpfCutoff = lpfCutoff->load();
            p.lpfRes    = lpfRes->load();
            p.lpf4Pole  = lpfPoles->load() > 0.5f;
            p.lpfEnvAmt = lpfEnv->load();
            p.hpfOn     = hpfOn->load() > 0.5f;
            p.hpfCutoff = hpfCutoff->load();

            auto readEnv = [] (std::atomic<float>* const* a)
            {
                EnvPatch e;
                e.attack  = a[0]->load();  e.decay   = a[1]->load();
                e.sustain = a[2]->load();  e.release = a[3]->load();
                e.curve   = a[4]->load();  e.invert  = a[5]->load() > 0.5f;
                return e;
            };
            p.envF = readEnv (envF);
            p.envA = readEnv (envA);

            p.vcaDrive       = vcaDrive->load();
            p.uniVoices      = (int) uniVoices->load();
            p.uniDetuneCents = uniDetune->load();
            p.uniSpread      = uniSpread->load();

            p.gateTime   = gateTime->load();
            p.loopOn     = loopOn->load() > 0.5f;
            p.loopRate   = loopRate->load();
            p.autoVarOn  = autoVarOn->load() > 0.5f;
            p.autoVarAmt = autoVarAmt->load();
            p.retrigHz   = retrigRate->load();

            p.masterVolDb = masterVol->load();
            p.compAmount = comp->load();
            p.outRateHz  = rateChoiceToHz ((int) outRate->load());
            p.out8bit    = outBits->load() > 0.5f;

            p.crushOn     = crushOn->load() > 0.5f;
            p.crushBits   = crushBits->load();
            p.crushDown   = crushDown->load();
            p.phaseOn     = phaseOn->load() > 0.5f;
            p.phaseRate   = phaseRate->load();
            p.phaseDepth  = phaseDepth->load();
            p.phaseFb     = phaseFb->load();
            p.flangeOn    = flangeOn->load() > 0.5f;
            p.flangeRate  = flangeRate->load();
            p.flangeDepth = flangeDepth->load();
            p.flangeFb    = flangeFb->load();
            p.ringOn  = ringOn->load() > 0.5f;  p.ringFreq  = ringFreq->load();  p.ringMix  = ringMix->load();  p.ringWave = (int) ringWave->load();
            p.tremOn  = tremOn->load() > 0.5f;  p.tremRate  = tremRate->load();  p.tremDepth = tremDepth->load(); p.tremWave = (int) tremWave->load();
            p.formOn  = formOn->load() > 0.5f;  p.formVowel = formVowel->load(); p.formReso = formReso->load(); p.formMix  = formMix->load();
            p.delayOn = delayOn->load() > 0.5f; p.delayTime = delayTime->load(); p.delayFb  = delayFb->load();  p.delayMix = delayMix->load();
            for (int k = 0; k < kFxChainLen; ++k)
                p.fxOrder[(size_t) k] = (int) fxOrder[k]->load();
            p.fxSplit = fxSplit->load() > 0.5f;

            for (int k = 0; k < kFxSlots; ++k)
            {
                p.fxSlotType[(size_t) k] = (int) fxSlotType[k]->load();
                p.fxSlotA[(size_t) k]    = fxSlotA[k]->load();
                p.fxSlotB[(size_t) k]    = fxSlotB[k]->load();
                p.fxSlotC[(size_t) k]    = fxSlotC[k]->load();
            }
            return p;
        }

        std::atomic<float>* oscOn[kNumOscs] {};
        std::atomic<float>* oscSync[kNumOscs] {};   // [0] null (OSC 1 master)
        std::atomic<float>* oscWave[kNumOscs] {};
        std::atomic<float>* oscPitch[kNumOscs] {};
        std::atomic<float>* oscFine[kNumOscs] {};
        std::atomic<float>* oscPwm[kNumOscs] {};
        std::atomic<float>* oscFold[kNumOscs] {};
        std::atomic<float>* oscLevel[kNumOscs] {};

        std::atomic<float>* baseFreq {};
        std::atomic<float>* midiTrack {};
        std::atomic<float>* pj1Amt {};
        std::atomic<float>* pj1Time {};
        std::atomic<float>* pj2Amt {};
        std::atomic<float>* pj2Time {};

        std::atomic<float>* noiseOn {};
        std::atomic<float>* noiseType {};
        std::atomic<float>* noiseColor {};
        std::atomic<float>* noiseLevel {};

        std::atomic<float>* lfoWave[kNumLfos] {};
        std::atomic<float>* lfoRate[kNumLfos] {};
        std::atomic<float>* lfoDelay[kNumLfos] {};

        std::atomic<float>* stepCount {};
        std::atomic<float>* stepGlide {};
        std::atomic<float>* stepSkew {};
        std::atomic<float>* stepVal[kMaxSteps] {};

        std::atomic<float>* modSrc[kNumModSlots] {};
        std::atomic<float>* modDest[kNumModSlots] {};
        std::atomic<float>* modDepth[kNumModSlots] {};

        std::atomic<float>* lpfCutoff {};
        std::atomic<float>* lpfRes {};
        std::atomic<float>* lpfPoles {};
        std::atomic<float>* lpfEnv {};
        std::atomic<float>* hpfOn {};
        std::atomic<float>* hpfCutoff {};

        std::atomic<float>* envF[6] {};
        std::atomic<float>* envA[6] {};

        std::atomic<float>* vcaDrive {};
        std::atomic<float>* uniVoices {};
        std::atomic<float>* uniDetune {};
        std::atomic<float>* uniSpread {};

        std::atomic<float>* gateTime {};
        std::atomic<float>* loopOn {};
        std::atomic<float>* loopRate {};
        std::atomic<float>* autoVarOn {};
        std::atomic<float>* autoVarAmt {};
        std::atomic<float>* retrigRate {};

        std::atomic<float>* masterVol {};
        std::atomic<float>* comp {};
        std::atomic<float>* outRate {};
        std::atomic<float>* outBits {};

        std::atomic<float>* crushOn {};
        std::atomic<float>* crushBits {};
        std::atomic<float>* crushDown {};
        std::atomic<float>* phaseOn {};
        std::atomic<float>* phaseRate {};
        std::atomic<float>* phaseDepth {};
        std::atomic<float>* phaseFb {};
        std::atomic<float>* flangeOn {};
        std::atomic<float>* flangeRate {};
        std::atomic<float>* flangeDepth {};
        std::atomic<float>* flangeFb {};
        std::atomic<float>* ringOn {};  std::atomic<float>* ringFreq {};  std::atomic<float>* ringMix {};   std::atomic<float>* ringWave {};
        std::atomic<float>* tremOn {};  std::atomic<float>* tremRate {};  std::atomic<float>* tremDepth {}; std::atomic<float>* tremWave {};
        std::atomic<float>* formOn {};  std::atomic<float>* formVowel {}; std::atomic<float>* formReso {};  std::atomic<float>* formMix {};
        std::atomic<float>* delayOn {}; std::atomic<float>* delayTime {}; std::atomic<float>* delayFb {};   std::atomic<float>* delayMix {};
        std::atomic<float>* fxOrder[kFxChainLen] {};
        std::atomic<float>* fxSplit {};

        std::atomic<float>* fxSlotType[kFxSlots] {};
        std::atomic<float>* fxSlotA[kFxSlots] {};
        std::atomic<float>* fxSlotB[kFxSlots] {};
        std::atomic<float>* fxSlotC[kFxSlots] {};
    };

    // ------------------------------------------------------------------
    //  Layout builder
    // ------------------------------------------------------------------
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto freqRange = [] (float lo, float hi)
        {
            NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (std::sqrt (lo * hi));   // log-ish midpoint
            return r;
        };
        // centre = the value at the fader's halfway point; picked per-param so
        // the control feels musically linear instead of bottom-heavy.
        auto timeRange = [] (float lo, float hi, float centre)
        {
            NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (centre);
            return r;
        };

        // ---- value display formatting (keeps the tiny text boxes readable) ----
        using FAttr = AudioParameterFloatAttributes;
        const auto hzAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz"
                                : String (v, v < 100.0f ? 1 : 0) + " Hz";
        });
        const auto secAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms"
                            : String (v, 2) + " s";
        });
        const auto unitAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return String (v, 2);
        });
        const auto dbAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return String (v, 1) + " dB";
        });
        const auto centAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return String (v, 1) + " ct";
        });
        const auto semiAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return String ((int) v) + " st";
        });
        const auto pctAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return String ((int) v) + " %";
        });

        // ---- oscillators ----
        for (int i = 1; i <= kNumOscs; ++i)
        {
            const auto n = "Osc " + String (i) + " ";
            layout.add (std::make_unique<AudioParameterBool>  (ParameterID { oscId (i, "on"), 1 },    n + "On", i == 1));
            if (i > 1)   // OSC 1 is the sync master; OSC 2/3 can sync to it
                layout.add (std::make_unique<AudioParameterBool> (ParameterID { oscId (i, "sync"), 1 }, n + "Sync", false));
            layout.add (std::make_unique<AudioParameterChoice>(ParameterID { oscId (i, "wave"), 1 },  n + "Wave", oscWaveNames, (int) OscWave::Square));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { oscId (i, "pitch"), 1 }, n + "Pitch",
                            NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, semiAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { oscId (i, "fine"), 1 },  n + "Fine",
                            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, centAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { oscId (i, "pwm"), 1 },   n + "PWM",
                            NormalisableRange<float> (5.0f, 95.0f, 0.1f), 50.0f, pctAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { oscId (i, "fold"), 1 },  n + "Fold",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, unitAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { oscId (i, "level"), 1 }, n + "Level",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f, unitAttr));
        }

        // ---- pitch ----
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::baseFreq, 1 }, "Base Freq",
                        freqRange (20.0f, 4000.0f), 440.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::midiTrack, 1 }, "MIDI Pitch Track", false));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::pj1Amt, 1 },  "Pitch Jump 1 Amount",
                        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, semiAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::pj1Time, 1 }, "Pitch Jump 1 Onset",
                        timeRange (0.01f, 2.0f, 0.15f), 0.08f, secAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::pj2Amt, 1 },  "Pitch Jump 2 Amount",
                        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, semiAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::pj2Time, 1 }, "Pitch Jump 2 Onset",
                        timeRange (0.01f, 2.0f, 0.15f), 0.16f, secAttr));

        // ---- noise ----
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::noiseOn, 1 },    "Noise On", false));
        layout.add (std::make_unique<AudioParameterChoice>(ParameterID { id::noiseType, 1 },  "Noise Type", noiseTypeNames, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::noiseColor, 1 }, "Noise Color",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::noiseLevel, 1 }, "Noise Level",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));

        // ---- LFOs ----
        for (int j = 1; j <= kNumLfos; ++j)
        {
            const auto n = "LFO " + String (j) + " ";
            layout.add (std::make_unique<AudioParameterChoice>(ParameterID { lfoId (j, "wave"), 1 },  n + "Wave", lfoWaveNames, (int) LfoWave::Triangle));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lfoId (j, "rate"), 1 },  n + "Rate",
                            freqRange (0.02f, 60.0f), 5.0f, hzAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lfoId (j, "delay"), 1 }, n + "Delay",
                            NormalisableRange<float> (0.0f, 5.0f, 0.001f), 0.0f, secAttr));
        }

        // ---- step LFO (LFO 2) ----
        layout.add (std::make_unique<AudioParameterInt>   (ParameterID { id::stepCount, 1 }, "Step Count", 2, kMaxSteps, 4));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::stepGlide, 1 }, "Step Glide",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::stepSkew, 1 }, "Step Skew",
                        NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        const float stepDefaults[kMaxSteps] = { -0.6f, -0.2f, 0.2f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f };
        for (int k = 1; k <= kMaxSteps; ++k)
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { stepValId (k), 1 },
                            "Step " + String (k),
                            NormalisableRange<float> (-1.0f, 1.0f, 0.001f), stepDefaults[k - 1], unitAttr));

        // ---- mod matrix ----
        for (int k = 1; k <= kNumModSlots; ++k)
        {
            const auto n = "Mod " + String (k) + " ";
            layout.add (std::make_unique<AudioParameterChoice>(ParameterID { modId (k, "src"), 1 },   n + "Source", modSrcNames, 0));
            layout.add (std::make_unique<AudioParameterChoice>(ParameterID { modId (k, "dest"), 1 },  n + "Dest",   modDestNames, 0));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { modId (k, "depth"), 1 }, n + "Depth",
                            NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        }

        // ---- filter ----
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::lpfCutoff, 1 }, "LPF Cutoff",
                        freqRange (20.0f, 20000.0f), 20000.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::lpfRes, 1 },    "LPF Resonance",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        layout.add (std::make_unique<AudioParameterChoice>(ParameterID { id::lpfPoles, 1 },  "LPF Poles", polesNames, 1));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::lpfEnv, 1 },    "Filter Env Amt",
                        NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::hpfOn, 1 },     "HPF On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::hpfCutoff, 1 }, "HPF Cutoff",
                        freqRange (20.0f, 2000.0f), 20.0f, hzAttr));

        // ---- envelopes ----
        auto addEnv = [&] (const char* a, const char* d, const char* s, const char* r,
                           const char* c, const char* inv, const String& name,
                           float defS)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { a, 1 }, name + " Attack",
                            timeRange (0.0001f, 5.0f, 0.08f), 0.001f, secAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { d, 1 }, name + " Decay",
                            timeRange (0.001f, 8.0f, 0.4f), 0.3f, secAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { s, 1 }, name + " Sustain",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), defS, unitAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { r, 1 }, name + " Release",
                            timeRange (0.001f, 8.0f, 0.4f), 0.2f, secAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { c, 1 }, name + " Curve",
                            NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, unitAttr));
            layout.add (std::make_unique<AudioParameterBool>  (ParameterID { inv, 1 }, name + " Invert", false));
        };
        addEnv (id::envFAttack, id::envFDecay, id::envFSustain, id::envFRelease,
                id::envFCurve, id::envFInvert, "Filter Env", 0.5f);
        addEnv (id::envAAttack, id::envADecay, id::envASustain, id::envARelease,
                id::envACurve, id::envAInvert, "Amp Env", 0.7f);

        // ---- VCA ----
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::vcaDrive, 1 }, "VCA Drive",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, unitAttr));

        // ---- voices ----
        layout.add (std::make_unique<AudioParameterInt>   (ParameterID { id::uniVoices, 1 }, "Voices", 1, kMaxUnison, 1));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::uniDetune, 1 }, "Voice Detune",
                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 12.0f, centAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::uniSpread, 1 }, "Stereo Spread",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));

        // ---- trigger ----
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::gateTime, 1 }, "Gate Time",
                        timeRange (0.01f, 5.0f, 0.4f), 0.25f, secAttr));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::loopOn, 1 },   "Loop", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::loopRate, 1 }, "Loop Interval",
                        timeRange (0.1f, 4.0f, 1.0f), 1.0f, secAttr));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::autoVarOn, 1 }, "Auto Variate", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::autoVarAmt, 1 }, "Variate Amount",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f, unitAttr));
        const auto hzOffAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return v < 0.25f ? juce::String ("Off") : juce::String (v, 1) + " Hz";
        });
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::retrigRate, 1 }, "Retrigger",
                        NormalisableRange<float> (0.0f, 50.0f, 0.1f), 0.0f, hzOffAttr));

        // ---- master / output ----
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::masterVol, 1 }, "Master Volume",
                        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -6.0f, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::comp, 1 }, "Compression",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, unitAttr));
        layout.add (std::make_unique<AudioParameterChoice>(ParameterID { id::outRate, 1 }, "Output Rate", rateNames, 0));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::outBits, 1 }, "Output 8-bit", false));

        // ---- integrated FX ----
        const auto bitsAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return String (v, 1) + " bit";
        });
        const auto downAttr = FAttr().withStringFromValueFunction ([] (float v, int)
        {
            return "/" + String (v, 1);
        });

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::crushOn, 1 },   "Crush On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::crushBits, 1 }, "Crush Bits",
                        NormalisableRange<float> (2.0f, 16.0f, 0.1f), 8.0f, bitsAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::crushDown, 1 }, "Crush Rate Div",
                        timeRange (1.0f, 40.0f, 6.0f), 1.0f, downAttr));

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::phaseOn, 1 },    "Phaser On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::phaseRate, 1 },  "Phaser Rate",
                        freqRange (0.05f, 8.0f), 1.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::phaseDepth, 1 }, "Phaser Depth",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::phaseFb, 1 },    "Phaser Feedback",
                        NormalisableRange<float> (0.0f, 0.9f, 0.001f), 0.3f, unitAttr));

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::flangeOn, 1 },    "Flanger On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::flangeRate, 1 },  "Flanger Rate",
                        freqRange (0.05f, 5.0f), 0.5f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::flangeDepth, 1 }, "Flanger Depth",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::flangeFb, 1 },    "Flanger Feedback",
                        NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.4f, unitAttr));

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::ringOn, 1 },   "Ring Mod On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::ringFreq, 1 }, "Ring Freq",
                        freqRange (20.0f, 4000.0f), 200.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::ringMix, 1 },  "Ring Wet/Dry",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, unitAttr));
        layout.add (std::make_unique<AudioParameterChoice>(ParameterID { id::ringWave, 1 }, "Ring Wave", fxWaveNames, 0));

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::tremOn, 1 },    "Tremolo On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::tremRate, 1 },  "Tremolo Speed",
                        freqRange (0.01f, 70.0f), 5.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::tremDepth, 1 }, "Tremolo Depth",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
        layout.add (std::make_unique<AudioParameterChoice>(ParameterID { id::tremWave, 1 }, "Tremolo Wave", fxWaveNames, 0));

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::formOn, 1 },    "Formant On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::formVowel, 1 }, "Formant Vowel",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::formReso, 1 },  "Formant Reso",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::formMix, 1 },   "Formant Wet/Dry",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, unitAttr));

        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::delayOn, 1 },    "Delay On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::delayTime, 1 },  "Delay Time",
                        timeRange (0.001f, 0.4f, 0.12f), 0.12f, secAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::delayFb, 1 },    "Delay Feedback",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, unitAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::delayMix, 1 },   "Delay Wet/Dry",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, unitAttr));

        // FX chain order: one int per chain position. Default = mono effects
        // first, Flanger/Delay last (right), matching the pre-filter split.
        const int defOrder[kFxChainLen] = { 0, 1, 3, 4, 5, 2, 6 };
        for (int k = 0; k < kFxChainLen; ++k)
            layout.add (std::make_unique<AudioParameterInt> (ParameterID { fxOrderId (k), 1 },
                            "FX Order " + String (k + 1), 0, kFxChainLen - 1, defOrder[k]));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::fxSplit, 1 }, "FX Pre-filter Split", false));

        // ---- reorderable FX chain slots (type + 3 generic A/B/C params each) ----
        for (int k = 1; k <= kFxSlots; ++k)
        {
            const auto n = "FX " + String (k) + " ";
            layout.add (std::make_unique<AudioParameterChoice>(ParameterID { fxId (k, "type"), 1 }, n + "Type", fxTypeNames, 0));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { fxId (k, "a"), 1 }, n + "A",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { fxId (k, "b"), 1 }, n + "B",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { fxId (k, "c"), 1 }, n + "C",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, unitAttr));
        }

        return layout;
    }
} // namespace Params
