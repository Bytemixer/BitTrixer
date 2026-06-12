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
    inline constexpr int   kNumLfos     = 2;
    inline constexpr int   kNumModSlots = 6;
    inline constexpr int   kMaxUnison   = 16;

    // ---- enum orderings (must match the choice arrays below) ----
    enum class OscWave  { Sine = 0, Triangle, Square, Saw, RevSaw, SuperSaw };
    enum class LfoWave  { Sine = 0, Triangle, Saw, RevSaw, Square, SampleHold, SampleGlide };
    enum class ModSrc   { Off = 0, Lfo1, Lfo2, FilterEnv, AmpEnv };
    enum class ModDest  { Off = 0, AllPitch, Osc1Pitch, Osc2Pitch, Osc3Pitch,
                          Pwm, Fold, NoiseLevel, Cutoff, Resonance,
                          Lfo1Rate, Lfo2Rate, VcaLevel };

    inline const juce::StringArray oscWaveNames  { "Sine", "Triangle", "Square", "Saw", "Rev Saw", "SuperSaw" };
    inline const juce::StringArray lfoWaveNames  { "Sine", "Triangle", "Saw", "Rev Saw", "Square", "S&H", "S&G" };
    inline const juce::StringArray modSrcNames   { "Off", "LFO 1", "LFO 2", "Filt Env", "Amp Env" };
    inline const juce::StringArray modDestNames  { "Off", "All Pitch", "Osc1 Pitch", "Osc2 Pitch", "Osc3 Pitch",
                                                   "PWM", "Fold", "Noise Lvl", "Cutoff", "Resonance",
                                                   "LFO1 Rate", "LFO2 Rate", "VCA Level" };
    inline const juce::StringArray polesNames    { "2-Pole", "4-Pole" };

    // ---- ID helpers ----
    inline juce::String oscId  (int osc1Based, const char* suffix)  { return "osc"  + juce::String (osc1Based) + "_" + suffix; }
    inline juce::String lfoId  (int lfo1Based, const char* suffix)  { return "lfo"  + juce::String (lfo1Based) + "_" + suffix; }
    inline juce::String modId  (int slot1Based, const char* suffix) { return "mod"  + juce::String (slot1Based) + "_" + suffix; }

    namespace id
    {
        inline constexpr const char* baseFreq    = "base_freq";
        inline constexpr const char* midiTrack   = "midi_track";

        inline constexpr const char* noiseOn     = "noise_on";
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

        inline constexpr const char* masterVol   = "master_vol";
    }

    // ------------------------------------------------------------------
    //  Plain-data snapshot of the whole front panel, built once per block.
    // ------------------------------------------------------------------
    struct OscPatch
    {
        bool    on = false;
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

        bool  noiseOn    = false;
        float noiseColor = 0.0f;     // 0 white .. 1 pink
        float noiseLevel = 0.5f;

        std::array<LfoPatch, kNumLfos>   lfo;
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

        float masterVolDb = -6.0f;
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
            }

            baseFreq  = get (id::baseFreq);
            midiTrack = get (id::midiTrack);

            noiseOn    = get (id::noiseOn);
            noiseColor = get (id::noiseColor);
            noiseLevel = get (id::noiseLevel);

            for (int j = 0; j < kNumLfos; ++j)
            {
                lfoWave[j]  = get (lfoId (j + 1, "wave"));
                lfoRate[j]  = get (lfoId (j + 1, "rate"));
                lfoDelay[j] = get (lfoId (j + 1, "delay"));
            }

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

            masterVol = get (id::masterVol);
        }

        Patch read() const
        {
            Patch p;
            for (int i = 0; i < kNumOscs; ++i)
            {
                auto& o = p.osc[(size_t) i];
                o.on         = oscOn[i]->load() > 0.5f;
                o.wave       = (OscWave) (int) oscWave[i]->load();
                o.pitchSemis = oscPitch[i]->load();
                o.fineCents  = oscFine[i]->load();
                o.pwm        = oscPwm[i]->load() * 0.01f;   // % -> 0..1
                o.fold       = oscFold[i]->load();
                o.level      = oscLevel[i]->load();
            }

            p.baseFreqHz = baseFreq->load();
            p.midiTrack  = midiTrack->load() > 0.5f;

            p.noiseOn    = noiseOn->load() > 0.5f;
            p.noiseColor = noiseColor->load();
            p.noiseLevel = noiseLevel->load();

            for (int j = 0; j < kNumLfos; ++j)
            {
                auto& l = p.lfo[(size_t) j];
                l.wave     = (LfoWave) (int) lfoWave[j]->load();
                l.rateHz   = lfoRate[j]->load();
                l.delaySec = lfoDelay[j]->load();
            }

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

            p.masterVolDb = masterVol->load();
            return p;
        }

        std::atomic<float>* oscOn[kNumOscs] {};
        std::atomic<float>* oscWave[kNumOscs] {};
        std::atomic<float>* oscPitch[kNumOscs] {};
        std::atomic<float>* oscFine[kNumOscs] {};
        std::atomic<float>* oscPwm[kNumOscs] {};
        std::atomic<float>* oscFold[kNumOscs] {};
        std::atomic<float>* oscLevel[kNumOscs] {};

        std::atomic<float>* baseFreq {};
        std::atomic<float>* midiTrack {};

        std::atomic<float>* noiseOn {};
        std::atomic<float>* noiseColor {};
        std::atomic<float>* noiseLevel {};

        std::atomic<float>* lfoWave[kNumLfos] {};
        std::atomic<float>* lfoRate[kNumLfos] {};
        std::atomic<float>* lfoDelay[kNumLfos] {};

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

        std::atomic<float>* masterVol {};
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
        auto timeRange = [] (float lo, float hi)
        {
            NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (std::sqrt (lo * hi));
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

        // ---- noise ----
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::noiseOn, 1 },    "Noise On", false));
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
                            timeRange (0.0001f, 5.0f), 0.001f, secAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { d, 1 }, name + " Decay",
                            timeRange (0.001f, 8.0f), 0.3f, secAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { s, 1 }, name + " Sustain",
                            NormalisableRange<float> (0.0f, 1.0f, 0.001f), defS, unitAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { r, 1 }, name + " Release",
                            timeRange (0.001f, 8.0f), 0.2f, secAttr));
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
                        timeRange (0.01f, 5.0f), 0.25f, secAttr));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::loopOn, 1 },   "Loop", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::loopRate, 1 }, "Loop Interval",
                        timeRange (0.1f, 4.0f), 1.0f, secAttr));
        layout.add (std::make_unique<AudioParameterBool>  (ParameterID { id::autoVarOn, 1 }, "Auto Variate", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::autoVarAmt, 1 }, "Variate Amount",
                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f, unitAttr));

        // ---- master ----
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::masterVol, 1 }, "Master Volume",
                        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -6.0f, dbAttr));

        return layout;
    }
} // namespace Params
