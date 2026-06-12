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
#include <cstdint>

// ============================================================================
//  LFO — bipolar (-1..+1) modulation oscillator.
//  Waves: sine, triangle, saw, reverse saw, square, S&H (stepped random),
//  S&G (sample & glide — random targets with smooth glide between them).
//  "Delay" fades the LFO in over the delay time after (re)trigger, like the
//  DeepMind's LFO delay.
// ============================================================================

class LFO
{
public:
    enum class Wave { Sine = 0, Triangle, Saw, RevSaw, Square, SampleHold, SampleGlide };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void seed (uint32_t s) noexcept { rng = s != 0 ? s : 0xCAFEBABEu; }

    void setWave  (Wave w) noexcept       { wave = w; }
    void setRate  (float hz) noexcept     { rateHz = hz < 0.0f ? 0.0f : hz; }
    void setDelay (float seconds) noexcept { delaySec = seconds < 0.0f ? 0.0f : seconds; }

    void retrigger() noexcept
    {
        phase = 0.0f;
        age = 0.0f;
        shValue = nextRand();
        sgTarget = nextRand();
        sgValue = 0.0f;
    }

    float value() const noexcept { return lastOut; }

    float tick() noexcept
    {
        const float inc = rateHz / fs;
        float v;

        switch (wave)
        {
            case Wave::Sine:     v = std::sin (kTwoPi * phase); break;
            case Wave::Triangle: v = 4.0f * std::fabs (phase - 0.5f) - 1.0f; break;
            case Wave::Saw:      v = 2.0f * phase - 1.0f; break;
            case Wave::RevSaw:   v = 1.0f - 2.0f * phase; break;
            case Wave::Square:   v = phase < 0.5f ? 1.0f : -1.0f; break;

            case Wave::SampleHold:
                v = shValue;
                break;

            case Wave::SampleGlide:
            {
                // one-pole glide toward the current random target,
                // time constant ~ half a period
                const float tau = 0.5f / (rateHz > 0.01f ? rateHz : 0.01f);
                const float a = 1.0f - std::exp (-1.0f / (tau * fs));
                sgValue += a * (sgTarget - sgValue);
                v = sgValue;
                break;
            }

            default: v = 0.0f; break;
        }

        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            shValue  = nextRand();
            sgTarget = nextRand();
        }

        // delay fade-in
        age += 1.0f / fs;
        float amp = 1.0f;
        if (delaySec > 0.001f)
        {
            amp = age / delaySec;
            if (amp > 1.0f) amp = 1.0f;
        }

        lastOut = v * amp;
        return lastOut;
    }

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    float nextRand() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    float fs = 44100.0f;
    Wave  wave = Wave::Triangle;
    float rateHz = 5.0f;
    float delaySec = 0.0f;

    float phase = 0.0f;
    float age = 0.0f;
    float shValue = 0.0f;
    float sgValue = 0.0f, sgTarget = 0.0f;
    float lastOut = 0.0f;
    uint32_t rng = 0xCAFEBABEu;
};
