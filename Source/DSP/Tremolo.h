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

#include "FastMath.h"

// ============================================================================
//  Tremolo — periodic amplitude modulation. The shape control morphs the LFO
//  from a smooth sine (gentle wobble) to a hard square (on/off gating), good
//  for stutter, pulse, helicopter and "powering up" SFX. A chain slot; runs
//  per trigger-instance, retriggered so the gate starts in phase every shot.
// ============================================================================

class Tremolo
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void retrigger() noexcept { phase = 0.0f; }

    // rateHz 0.1 .. 40   depth01 0 = none .. 1 = full   shape01 0 = sine .. 1 = square
    void setParams (float rateHz, float depth01, float shape01) noexcept
    {
        rate  = rateHz < 0.0f ? 0.0f : rateHz;
        depth = clamp01 (depth01);
        shape = clamp01 (shape01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        const float inc = rate / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;

            const float sine = FastMath::sinCycle (phase);     // -1 .. 1
            const float sq   = sine >= 0.0f ? 1.0f : -1.0f;
            const float lfo  = sine + shape * (sq - sine);     // morph sine -> square

            // unipolar gain in [1 - depth, 1]; depth sets how deep it dips
            const float gain = 1.0f - depth * (0.5f - 0.5f * lfo);
            left[s]  *= gain;
            right[s] *= gain;
        }
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    float fs = 44100.0f;
    float rate = 5.0f;
    float depth = 0.5f;
    float shape = 0.0f;
    float phase = 0.0f;
};
