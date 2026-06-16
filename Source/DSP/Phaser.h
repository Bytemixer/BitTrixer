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
//  Phaser — 4-stage first-order allpass chain with feedback, sine-swept
//  between ~200 Hz and up to ~7 kHz. Runs per trigger-instance after the
//  voice sum (retriggered with the sound, integrated into the SFX).
// ============================================================================

class Phaser
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void retrigger() noexcept
    {
        for (auto& st : stagesL) st = 0.0f;
        for (auto& st : stagesR) st = 0.0f;
        lastL = lastR = 0.0f;
        phase = 0.0f;
    }

    void setParams (float rateHz, float depth01, float feedback01) noexcept
    {
        rate = rateHz;
        depth = depth01 < 0.0f ? 0.0f : (depth01 > 1.0f ? 1.0f : depth01);
        fb = feedback01 < 0.0f ? 0.0f : (feedback01 > 0.9f ? 0.9f : feedback01);
    }

    void process (float* left, float* right, int n) noexcept
    {
        const float inc = rate / fs;
        for (int s = 0; s < n; ++s)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = 0.5f + 0.5f * FastMath::sinCycle (phase);

            const float sweepHz = 200.0f + lfo * (800.0f + 6200.0f * depth);
            const float t = FastMath::tanPos (kPi * (sweepHz < 0.45f * fs ? sweepHz : 0.45f * fs) / fs);
            const float a = (t - 1.0f) / (t + 1.0f);

            left[s]  = channel (stagesL, lastL, left[s], a);
            right[s] = channel (stagesR, lastR, right[s], a);
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
            const float lfo = 0.5f + 0.5f * FastMath::sinCycle (phase);
            const float sweepHz = 200.0f + lfo * (800.0f + 6200.0f * depth);
            const float t = FastMath::tanPos (kPi * (sweepHz < 0.45f * fs ? sweepHz : 0.45f * fs) / fs);
            const float a = (t - 1.0f) / (t + 1.0f);
            buf[s] = channel (stagesL, lastL, buf[s], a);
        }
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kTwoPi = 2.0f * kPi;

    float channel (std::array<float, 4>& st, float& last, float x, float a) noexcept
    {
        float v = x + last * fb;
        for (auto& z : st)
        {
            const float y = a * v + z;
            z = v - a * y;
            v = y;
        }
        last = v;
        return 0.5f * (x + v);
    }

    float fs = 44100.0f;
    std::array<float, 4> stagesL {}, stagesR {};
    float lastL = 0.0f, lastR = 0.0f;
    float phase = 0.0f;
    float rate = 1.0f, depth = 0.5f, fb = 0.3f;
};
