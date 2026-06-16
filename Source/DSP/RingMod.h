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

#include "FastMath.h"

// ============================================================================
//  RingMod — multiplies the signal by a sine carrier. Sum/difference sidebands
//  give the classic metallic / robotic / clangorous tone: sci-fi zaps, alarms,
//  bell-like hits. A chain slot; runs per trigger-instance, retriggered with
//  the sound so the carrier phase is deterministic per shot.
// ============================================================================

class RingMod
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void retrigger() noexcept { phase = 0.0f; }

    // carrier shape: 0 = sine, 1 = triangle, 2 = square, 3 = saw
    void setWave (int waveType) noexcept { wave = waveType; }

    // freqHz: carrier 20 .. ~4 kHz   mix01: 0 = dry .. 1 = fully ring-modulated
    void setParams (float freqHz, float mix01) noexcept
    {
        freq = freqHz < 0.0f ? 0.0f : freqHz;
        mix  = clamp01 (mix01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        const float inc = freq / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float carrier = FastMath::waveCycle (wave, phase);

            left[s]  += mix * (left[s]  * carrier - left[s]);
            right[s] += mix * (right[s] * carrier - right[s]);
        }
    }

    // single-channel variant for the per-voice pre-filter path
    void processMono (float* buf, int n) noexcept
    {
        const float inc = freq / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float carrier = FastMath::waveCycle (wave, phase);
            buf[s] += mix * (buf[s] * carrier - buf[s]);
        }
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    float fs = 44100.0f;
    float freq = 200.0f;
    float mix = 1.0f;
    float phase = 0.0f;
    int   wave = 0;
};
