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

#include <array>
#include <cmath>
#include "FastMath.h"

// ============================================================================
//  Formant — imposes vowel-like resonances on the signal: three parallel
//  bandpass filters tuned to the F1/F2/F3 formants of a vowel, summed. The
//  vowel control morphs ee -> eh -> ah -> oh -> oo (ordered so F2 falls
//  monotonically -- the sweep never doubles back through "ah"); sweeping it
//  (e.g. an LFO via the mod matrix) makes the sound "talk". A chain
//  slot; per trigger-instance, retriggered so each shot starts clean.
// ============================================================================

class Formant
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
        setParams (vowel, reso, mix);
    }

    void retrigger() noexcept
    {
        lpL.fill (0.0f); bpL.fill (0.0f);
        lpR.fill (0.0f); bpR.fill (0.0f);
    }

    // vowel01: 0 = "ee" .. 1 = "oo" (smooth front-to-back sweep ee-eh-ah-oh-oo)
    // reso01:  0 = soft .. 1 = sharp/vocal     mix01: 0 = dry .. 1 = wet
    void setParams (float vowel01, float reso01, float mix01) noexcept
    {
        vowel = clamp01 (vowel01);
        reso  = clamp01 (reso01);
        mix   = clamp01 (mix01);

        // interpolate the formant table between the two nearest vowels
        const float x  = vowel * (float) (kVowels - 1);   // 0 .. 4
        int   i0 = (int) x;
        if (i0 > kVowels - 2) i0 = kVowels - 2;
        const float fr = x - (float) i0;

        for (int k = 0; k < 3; ++k)
        {
            const float hz = kFormants[i0][k] + fr * (kFormants[i0 + 1][k] - kFormants[i0][k]);
            const float fc = hz < 50.0f ? 50.0f : (hz > 0.45f * fs ? 0.45f * fs : hz);
            f[k] = 2.0f * FastMath::sinCycle (fc / (2.0f * fs));   // SVF tuning coef
        }
        damp = 1.0f - 0.93f * reso;                                // 0.07 .. 1
    }

    void process (float* left, float* right, int n) noexcept
    {
        for (int s = 0; s < n; ++s)
        {
            left[s]  = sample (left[s],  lpL, bpL);
            right[s] = sample (right[s], lpR, bpR);
        }
    }

    // single-channel variant for the per-voice pre-filter path
    void processMono (float* buf, int n) noexcept
    {
        for (int s = 0; s < n; ++s)
            buf[s] = sample (buf[s], lpL, bpL);
    }

private:
    static constexpr int kVowels = 5;   // ee eh ah oh oo -- ordered along the
                                        // vowel path so F2 falls monotonically
                                        // (no doubling back through "ah")
    static constexpr float kFormants[kVowels][3] =
    {
        { 270.0f, 2290.0f, 3010.0f },   // ee
        { 530.0f, 1840.0f, 2480.0f },   // eh
        { 730.0f, 1090.0f, 2440.0f },   // ah
        { 570.0f,  840.0f, 2410.0f },   // oh
        { 300.0f,  870.0f, 2240.0f },   // oo
    };
    static constexpr float kGain[3] = { 1.0f, 0.6f, 0.35f };   // higher formants softer

    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    float sample (float in, std::array<float, 3>& lp, std::array<float, 3>& bp) noexcept
    {
        float wet = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            lp[(size_t) k] += f[k] * bp[(size_t) k];
            const float hp = in - lp[(size_t) k] - damp * bp[(size_t) k];
            bp[(size_t) k] += f[k] * hp;
            wet += bp[(size_t) k] * kGain[k];
        }
        return in + mix * (wet * 0.9f - in);
    }

    float fs = 44100.0f;
    float vowel = 0.0f, reso = 0.5f, mix = 1.0f;
    float f[3] = { 0.1f, 0.1f, 0.1f };
    float damp = 0.5f;
    std::array<float, 3> lpL {}, bpL {}, lpR {}, bpR {};
};
