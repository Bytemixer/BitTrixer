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

// ============================================================================
//  LadderFilter — zero-delay-feedback (TPT) 4-stage transistor-ladder LPF
//  with a switchable 2-pole tap, plus a separate TPT 2-pole highpass (the
//  switchable HPF section). Gentle tanh on the feedback input gives the
//  "VCF being pushed" character without full circuit modeling.
// ============================================================================

class LadderFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        reset();
        setCutoff (20000.0f);
        setResonance (0.0f);
    }

    void reset() noexcept
    {
        s1 = s2 = s3 = s4 = 0.0f;
        hp1 = hp2 = 0.0f;
    }

    void setFourPole (bool fourPole) noexcept { use4Pole = fourPole; }

    void setCutoff (float hz) noexcept
    {
        cutoff = hz < 10.0f ? 10.0f : (hz > 0.45f * fs ? 0.45f * fs : hz);
        const float g0 = std::tan (kPi * cutoff / fs);
        g = g0 / (1.0f + g0);
        gDen = 1.0f / (1.0f + g0);
    }

    // res 0..1 -> feedback k 0..~4 (self-osc threshold)
    void setResonance (float res01) noexcept
    {
        if (res01 < 0.0f) res01 = 0.0f;
        if (res01 > 1.0f) res01 = 1.0f;
        k = res01 * 3.98f;
        // passband loss compensation as resonance rises
        comp = 1.0f + k * 0.4f;
    }

    void setHpf (bool on, float hz) noexcept
    {
        hpfOn = on;
        if (hz < 10.0f) hz = 10.0f;
        if (hz > 0.45f * fs) hz = 0.45f * fs;
        const float gh0 = std::tan (kPi * hz / fs);
        gh = gh0 / (1.0f + gh0);
    }

    float tick (float x) noexcept
    {
        // ---- ZDF ladder ----
        const float G  = g;
        const float G2 = G * G;
        const float S  = (G2 * G * s1 + G2 * s2 + G * s3 + s4) * gDen;

        float u = (x * comp - k * S) / (1.0f + k * G2 * G2);
        u = std::tanh (u);                          // input-stage warmth

        // 4 cascaded TPT one-poles
        const float y1 = lp (u,  s1);
        const float y2 = lp (y1, s2);
        const float y3 = lp (y2, s3);
        const float y4 = lp (y3, s4);

        float out = use4Pole ? y4 : y2;

        // ---- optional 2-pole TPT highpass ----
        if (hpfOn)
        {
            const float l1 = gh * (out - hp1) + hp1;  // one-pole LP
            hp1 = 2.0f * l1 - hp1;
            const float h1 = out - l1;
            const float l2 = gh * (h1 - hp2) + hp2;
            hp2 = 2.0f * l2 - hp2;
            out = h1 - l2;
        }

        return out;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    float lp (float x, float& state) noexcept
    {
        const float v = g * (x - state);
        const float y = v + state;
        state = y + v;
        return y;
    }

    float fs = 44100.0f;
    float cutoff = 20000.0f;
    float g = 0.5f, gDen = 1.0f, k = 0.0f, comp = 1.0f;
    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
    bool  use4Pole = true;

    bool  hpfOn = false;
    float gh = 0.0f;
    float hp1 = 0.0f, hp2 = 0.0f;
};
