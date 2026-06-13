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

#include <cmath>
#include "FastMath.h"

// ============================================================================
//  AutoWah — an envelope follower opens a resonant bandpass as the signal gets
//  louder: the sound sweeps its own "wah" from its dynamics. Chamberlin state-
//  variable bandpass per channel, fast-attack / slow-release follower. A chain
//  slot; runs per trigger-instance, retriggered so each shot starts closed.
// ============================================================================

class AutoWah
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void retrigger() noexcept
    {
        env = 0.0f;
        lpL = bpL = lpR = bpR = 0.0f;
    }

    // baseHz: resting cutoff ~100 .. 1500   q01: 0 = soft .. 1 = sharp/resonant
    // sens01: 0 = static .. 1 = wide envelope sweep
    void setParams (float baseHz, float q01, float sens01) noexcept
    {
        base = baseHz;
        q    = clamp01 (q01);
        sens = clamp01 (sens01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        const float atk = std::exp (-1.0f / (0.005f * fs));   // ~5 ms attack
        const float rel = std::exp (-1.0f / (0.080f * fs));   // ~80 ms release
        const float res = 1.0f - 0.9f * q;                    // damping -> resonance (0.1 .. 1)

        for (int s = 0; s < n; ++s)
        {
            const float mono = 0.5f * (left[s] + right[s]);
            const float rect = std::fabs (mono);
            const float c = rect > env ? atk : rel;
            env = c * env + (1.0f - c) * rect;

            float fc = base + sens * 4000.0f * env;
            fc = fc < 50.0f ? 50.0f : (fc > 0.45f * fs ? 0.45f * fs : fc);
            const float f = 2.0f * FastMath::sinCycle (fc / (2.0f * fs));  // SVF tuning coef

            lpL += f * bpL;
            const float hpL = left[s] - lpL - res * bpL;
            bpL += f * hpL;
            left[s] = bpL;

            lpR += f * bpR;
            const float hpR = right[s] - lpR - res * bpR;
            bpR += f * hpR;
            right[s] = bpR;
        }
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    float fs = 44100.0f;
    float base = 400.0f;
    float q = 0.5f;
    float sens = 0.5f;
    float env = 0.0f;
    float lpL = 0.0f, bpL = 0.0f, lpR = 0.0f, bpR = 0.0f;
};
