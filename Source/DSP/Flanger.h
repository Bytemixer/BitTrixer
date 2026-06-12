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

#include <array>
#include <cmath>
#include "FastMath.h"

// ============================================================================
//  Flanger — short LFO-modulated delay with feedback (the sfxr "phaser" is
//  really this). Runs per trigger-instance after the voice sum, retriggered
//  with each sound so the sweep is part of the effect, not a bus afterthought.
// ============================================================================

class Flanger
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void retrigger() noexcept
    {
        bufL.fill (0.0f);
        bufR.fill (0.0f);
        writePos = 0;
        phase = 0.0f;
    }

    void setParams (float rateHz, float depth01, float feedback01) noexcept
    {
        rate = rateHz;
        depth = depth01 < 0.0f ? 0.0f : (depth01 > 1.0f ? 1.0f : depth01);
        fb = feedback01 < 0.0f ? 0.0f : (feedback01 > 0.95f ? 0.95f : feedback01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        const float inc = rate / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = 0.5f + 0.5f * FastMath::sinCycle (phase);

            // sweep 0.5 ms .. 0.5 + 7*depth ms
            const float delaySamps = (0.0005f + 0.007f * depth * lfo) * fs;

            left[s]  = stage (bufL, left[s], delaySamps);
            right[s] = stage (bufR, right[s], delaySamps);

            writePos = (writePos + 1) & kMask;
        }
    }

private:
    static constexpr int kSize = 2048;     // ~46 ms headroom at 44.1k
    static constexpr int kMask = kSize - 1;
    static constexpr float kTwoPi = 6.28318530717958647692f;

    float stage (std::array<float, kSize>& buf, float x, float delaySamps) noexcept
    {
        const float readPos = (float) writePos - delaySamps;
        const int i0 = ((int) std::floor (readPos)) & kMask;
        const int i1 = (i0 + 1) & kMask;
        const float frac = readPos - std::floor (readPos);
        const float delayed = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

        buf[(size_t) writePos] = x + delayed * fb;
        return x + delayed * 0.7f;
    }

    float fs = 44100.0f;
    std::array<float, kSize> bufL {}, bufR {};
    int writePos = 0;
    float phase = 0.0f;
    float rate = 0.5f, depth = 0.5f, fb = 0.4f;
};
