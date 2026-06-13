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
};

class ModMatrix
{
public:
    static constexpr float kPitchRangeSemis = 48.0f;
    static constexpr float kCutoffRangeOct  = 6.0f;
    static constexpr float kLfoRateRangeOct = 4.0f;
    static constexpr float kRingFreqRangeOct  = 4.0f;
    static constexpr float kDelayTimeRangeOct = 2.0f;

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
                case Params::ModDest::Osc1Pitch:  mv.oscPitchSemis[0]   += v * kPitchRangeSemis; break;
                case Params::ModDest::Osc2Pitch:  mv.oscPitchSemis[1]   += v * kPitchRangeSemis; break;
                case Params::ModDest::Osc3Pitch:  mv.oscPitchSemis[2]   += v * kPitchRangeSemis; break;
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
                default: break;
            }
        }
        return mv;
    }
};
