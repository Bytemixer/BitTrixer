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

// ============================================================================
//  Delay — a feedback delay line with fractional read (linear interpolation).
//  Short times give slap / metallic resonance; longer times give rhythmic
//  repeats and echo tails. A chain slot; runs per trigger-instance and is
//  retriggered (buffer cleared) so each SFX starts with a clean tail.
// ============================================================================

class Delay
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
    }

    // timeMs 1 .. ~340   feedback01 0 .. 1   mix01 0 = dry .. 1 = wet
    void setParams (float timeMs, float feedback01, float mix01) noexcept
    {
        float t = timeMs < 1.0f ? 1.0f : timeMs;
        delaySamps = (t * 0.001f) * fs;
        if (delaySamps > (float) (kSize - 2)) delaySamps = (float) (kSize - 2);
        fb  = clamp01 (feedback01) * 0.98f;   // headroom under runaway
        mix = clamp01 (mix01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        for (int s = 0; s < n; ++s)
        {
            left[s]  = stage (bufL, left[s]);
            right[s] = stage (bufR, right[s]);
            writePos = (writePos + 1) & kMask;
        }
    }

private:
    static constexpr int kSize = 16384;        // ~340 ms @ 48 kHz
    static constexpr int kMask = kSize - 1;

    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    float stage (std::array<float, kSize>& buf, float x) noexcept
    {
        const float readPos = (float) writePos - delaySamps;
        const int i0 = ((int) std::floor (readPos)) & kMask;
        const int i1 = (i0 + 1) & kMask;
        const float frac = readPos - std::floor (readPos);
        const float delayed = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

        buf[(size_t) writePos] = x + delayed * fb;
        return x * (1.0f - mix) + delayed * mix;
    }

    float fs = 44100.0f;
    float delaySamps = 4800.0f;
    float fb = 0.4f;
    float mix = 0.4f;
    std::array<float, kSize> bufL {}, bufR {};
    int writePos = 0;
};
