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
//  Tremolo — periodic amplitude modulation by a selectable LFO shape. The rate
//  spans a deliberately wide 0.01 .. 70 Hz: slow swells at the bottom, buzzy
//  AM/ring-ish tones near the top. A chain slot; runs per trigger-instance,
//  retriggered so the modulation starts in phase every shot.
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

    // LFO shape: 0 = sine, 1 = triangle, 2 = square, 3 = saw
    void setWave (int waveType) noexcept { wave = waveType; }

    // rateHz 0.01 .. 70   depth01 0 = none .. 1 = full
    void setParams (float rateHz, float depth01) noexcept
    {
        rate  = rateHz < 0.0f ? 0.0f : rateHz;
        depth = clamp01 (depth01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        const float inc = rate / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;

            const float lfo = FastMath::waveCycle (wave, phase);   // -1 .. 1
            // unipolar gain in [1 - depth, 1]; depth sets how deep it dips
            const float gain = 1.0f - depth * (0.5f - 0.5f * lfo);
            left[s]  *= gain;
            right[s] *= gain;
        }
    }

    // single-channel variant for the per-voice pre-filter path
    void processMono (float* buf, int n) noexcept
    {
        const float inc = rate / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = FastMath::waveCycle (wave, phase);
            buf[s] *= 1.0f - depth * (0.5f - 0.5f * lfo);
        }
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    float fs = 44100.0f;
    float rate = 5.0f;
    float depth = 0.5f;
    float phase = 0.0f;
    int   wave = 0;
};
