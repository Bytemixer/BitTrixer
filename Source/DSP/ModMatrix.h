/*  This file is part of the BitTrixer audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#pragma once

#include "../Params.h"

// ============================================================================
//  ModMatrix — evaluates the 6 routing slots. Sources are "CV" values
//  (LFOs bipolar, envelopes unipolar); the result is a set of summed
//  offsets per destination, in musically scaled units:
//    * pitch destinations   : semitones (depth ±1 -> ±48 st, big SFX sweeps)
//    * cutoff               : octaves   (depth ±1 -> ±6 oct)
//    * LFO rates            : octaves   (depth ±1 -> ±4 oct)
//    * pwm/fold/levels/vca  : plain unit offsets
// ============================================================================

struct ModValues
{
    float allPitchSemis = 0.0f;
    float baseHz = 0.0f;        // linear Hz offset on the base frequency
    float oscPitchSemis[Params::kNumOscs] { 0.0f, 0.0f, 0.0f };
    float pwm = 0.0f;
    float fold = 0.0f;
    float noiseLevel = 0.0f;
    float cutoffOct = 0.0f;
    float resonance = 0.0f;
    float lfoRateOct[Params::kNumLfos] { 0.0f, 0.0f };
    float vca = 0.0f;
    float formVowel = 0.0f;     // 0..1 offset
    float ringFreqOct = 0.0f;   // octaves
    float tremDepth = 0.0f;     // 0..1 offset
    float delayTimeOct = 0.0f;  // octaves

    // granular per-oscillator + per-FX-knob offsets
    float oscPwm[Params::kNumOscs]   { 0.0f, 0.0f, 0.0f };
    float oscFold[Params::kNumOscs]  { 0.0f, 0.0f, 0.0f };
    float oscLevel[Params::kNumOscs] { 0.0f, 0.0f, 0.0f };
    float crushBits   = 0.0f;   // +/- bits
    float crushDivOct = 0.0f;   // octaves on the sample-hold divisor
    float phaseRateOct = 0.0f;  // octaves
    float phaseDepth   = 0.0f;
    float phaseFb      = 0.0f;
    float flangeRateOct = 0.0f; // octaves
    float flangeDepth   = 0.0f;
    float flangeFb      = 0.0f;
    float ringMix    = 0.0f;
    float tremRateOct = 0.0f;   // octaves
    float formReso   = 0.0f;
    float formMix    = 0.0f;
    float delayFb    = 0.0f;
    float delayMix   = 0.0f;
    float fmOpLevel[4] { 0.0f, 0.0f, 0.0f, 0.0f };   // FM operator level offsets
    float fmOpRatio[4] { 0.0f, 0.0f, 0.0f, 0.0f };   // FM operator ratio offsets
    float fmFeedback = 0.0f;
    float noiseColor = 0.0f;    // 0..1 offset
    float lpfEnvAmt  = 0.0f;    // adds to the filter-env -> cutoff amount
    float hpfOct     = 0.0f;    // octaves on the HPF cutoff
    float stepGlide  = 0.0f;    // 0..1 offset
    float stepSkew   = 0.0f;    // -1..1 offset
    // note: osc/FM "fine" destinations reuse oscPitchSemis with a small range
};

class ModMatrix
{
public:
    static constexpr float kPitchRangeSemis = 48.0f;
    static constexpr float kCutoffRangeOct  = 6.0f;
    static constexpr float kLfoRateRangeOct = 4.0f;
    static constexpr float kRingFreqRangeOct  = 4.0f;
    static constexpr float kDelayTimeRangeOct = 2.0f;
    static constexpr float kFxRateRangeOct    = 4.0f;   // phaser/flanger/trem speed
    static constexpr float kCrushBitsRange    = 8.0f;   // +/- bits
    static constexpr float kCrushDivRangeOct  = 4.0f;   // sample-hold divisor
    static constexpr float kFineRangeSemis    = 1.0f;   // fine-pitch dests (subtle vibrato)
    static constexpr float kFmRatioRange      = 4.0f;   // +/- on an FM operator ratio
    static constexpr float kHpfRangeOct       = 4.0f;   // octaves on the HPF cutoff
    static constexpr float kBaseHzRange       = 2000.0f; // linear Hz on the base frequency

    // srcLfo1/2 are bipolar, srcEnvF/A unipolar (already inverted if set so)
    static ModValues compute (const Params::Patch& p,
                              float srcLfo1, float srcLfo2,
                              float srcEnvF, float srcEnvA) noexcept
    {
        ModValues mv;

        for (const auto& slot : p.mod)
        {
            if (slot.src == Params::ModSrc::Off || slot.dest == Params::ModDest::Off
                || slot.depth == 0.0f)
                continue;

            float src = 0.0f;
            switch (slot.src)
            {
                case Params::ModSrc::Lfo1:      src = srcLfo1; break;
                case Params::ModSrc::Lfo2:      src = srcLfo2; break;
                case Params::ModSrc::FilterEnv: src = srcEnvF; break;
                case Params::ModSrc::AmpEnv:    src = srcEnvA; break;
                default: break;
            }

            const float v = src * slot.depth;

            switch (slot.dest)
            {
                case Params::ModDest::AllPitch:   mv.allPitchSemis      += v * kPitchRangeSemis; break;
                case Params::ModDest::BaseHz:     mv.baseHz             += v * kBaseHzRange; break;
                case Params::ModDest::Osc1Pitch:  mv.oscPitchSemis[0]   += v * kPitchRangeSemis; break;
                case Params::ModDest::Osc2Pitch:  mv.oscPitchSemis[1]   += v * kPitchRangeSemis; break;
                case Params::ModDest::FmPitch:    mv.oscPitchSemis[2]   += v * kPitchRangeSemis; break;
                case Params::ModDest::Pwm:        mv.pwm                += v * 0.45f; break;
                case Params::ModDest::Fold:       mv.fold               += v; break;
                case Params::ModDest::NoiseLevel: mv.noiseLevel         += v; break;
                case Params::ModDest::Cutoff:     mv.cutoffOct          += v * kCutoffRangeOct; break;
                case Params::ModDest::Resonance:  mv.resonance          += v; break;
                case Params::ModDest::Lfo1Rate:   mv.lfoRateOct[0]      += v * kLfoRateRangeOct; break;
                case Params::ModDest::Lfo2Rate:   mv.lfoRateOct[1]      += v * kLfoRateRangeOct; break;
                case Params::ModDest::VcaLevel:   mv.vca                += v; break;
                case Params::ModDest::FormVowel:  mv.formVowel          += v; break;
                case Params::ModDest::RingFreq:   mv.ringFreqOct        += v * kRingFreqRangeOct; break;
                case Params::ModDest::TremDepth:  mv.tremDepth          += v; break;
                case Params::ModDest::DelayTime:  mv.delayTimeOct       += v * kDelayTimeRangeOct; break;

                case Params::ModDest::Osc1Pwm:    mv.oscPwm[0]   += v * 0.45f; break;
                case Params::ModDest::Osc2Pwm:    mv.oscPwm[1]   += v * 0.45f; break;
                case Params::ModDest::Osc1Fold:   mv.oscFold[0]  += v; break;
                case Params::ModDest::Osc2Fold:   mv.oscFold[1]  += v; break;
                case Params::ModDest::Osc1Level:  mv.oscLevel[0] += v; break;
                case Params::ModDest::Osc2Level:  mv.oscLevel[1] += v; break;
                case Params::ModDest::FmOut:      mv.oscLevel[2] += v; break;
                case Params::ModDest::CrushBits:    mv.crushBits     += v * kCrushBitsRange; break;
                case Params::ModDest::CrushDiv:     mv.crushDivOct   += v * kCrushDivRangeOct; break;
                case Params::ModDest::PhaserRate:   mv.phaseRateOct  += v * kFxRateRangeOct; break;
                case Params::ModDest::PhaserDepth:  mv.phaseDepth    += v; break;
                case Params::ModDest::PhaserFb:     mv.phaseFb       += v; break;
                case Params::ModDest::FlangerRate:  mv.flangeRateOct += v * kFxRateRangeOct; break;
                case Params::ModDest::FlangerDepth: mv.flangeDepth   += v; break;
                case Params::ModDest::FlangerFb:    mv.flangeFb      += v; break;
                case Params::ModDest::RingMix:      mv.ringMix       += v; break;
                case Params::ModDest::TremRate:     mv.tremRateOct   += v * kFxRateRangeOct; break;
                case Params::ModDest::FormReso:     mv.formReso      += v; break;
                case Params::ModDest::FormMix:      mv.formMix       += v; break;
                case Params::ModDest::DelayFb:      mv.delayFb       += v; break;
                case Params::ModDest::DelayMix:     mv.delayMix      += v; break;
                case Params::ModDest::FmOp1Level:   mv.fmOpLevel[0]  += v; break;
                case Params::ModDest::FmOp2Level:   mv.fmOpLevel[1]  += v; break;
                case Params::ModDest::FmOp3Level:   mv.fmOpLevel[2]  += v; break;
                case Params::ModDest::FmOp4Level:   mv.fmOpLevel[3]  += v; break;
                case Params::ModDest::FmFeedback:   mv.fmFeedback    += v; break;

                // ---- fine pitch (reuse the per-osc pitch offset, small range) ----
                case Params::ModDest::Osc1Fine:     mv.oscPitchSemis[0] += v * kFineRangeSemis; break;
                case Params::ModDest::Osc2Fine:     mv.oscPitchSemis[1] += v * kFineRangeSemis; break;
                case Params::ModDest::FmFine:       mv.oscPitchSemis[2] += v * kFineRangeSemis; break;
                // ---- FM operator ratios ----
                case Params::ModDest::FmOp1Ratio:   mv.fmOpRatio[0]  += v * kFmRatioRange; break;
                case Params::ModDest::FmOp2Ratio:   mv.fmOpRatio[1]  += v * kFmRatioRange; break;
                case Params::ModDest::FmOp3Ratio:   mv.fmOpRatio[2]  += v * kFmRatioRange; break;
                case Params::ModDest::FmOp4Ratio:   mv.fmOpRatio[3]  += v * kFmRatioRange; break;
                // ---- noise / filter / step-LFO ----
                case Params::ModDest::NoiseColor:   mv.noiseColor    += v; break;
                case Params::ModDest::FilterEnvAmt: mv.lpfEnvAmt     += v; break;
                case Params::ModDest::HpfFreq:      mv.hpfOct        += v * kHpfRangeOct; break;
                case Params::ModDest::Lfo2Glide:    mv.stepGlide     += v; break;
                case Params::ModDest::Lfo2Skew:     mv.stepSkew      += v; break;
                default: break;
            }
        }
        return mv;
    }
};
