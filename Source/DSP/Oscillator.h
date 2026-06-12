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
#include <array>

// ============================================================================
//  Oscillator — one "VCO".
//  PolyBLEP anti-aliased square (with PWM) and saw; sine/triangle are
//  bandlimited by nature. SuperSaw is a fixed 5-saw detuned stack.
//  A reflect wavefolder ("Tone Mod" flavour) sits after the wave, fully
//  transparent at fold = 0.
//  No JUCE dependencies; prepare/reset/tick interface.
// ============================================================================

class Oscillator
{
public:
    enum class Wave { Sine = 0, Triangle, Square, Saw, RevSaw, SuperSaw };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        reset();
    }

    void reset (float startPhase = 0.0f) noexcept
    {
        phase = startPhase;
        for (size_t i = 0; i < superPhase.size(); ++i)
            superPhase[i] = startPhase * (float) (i + 1) * 0.61803f; // decorrelate
        superPhase[2] = startPhase;
    }

    void setWave (Wave w) noexcept   { wave = w; }
    void setPwm  (float duty) noexcept { pwm = clamp (duty, 0.05f, 0.95f); }
    void setFold (float amount) noexcept { fold = clamp (amount, 0.0f, 1.0f); }

    // freqHz is the already-modulated per-voice frequency.
    float tick (float freqHz) noexcept
    {
        const float inc = clamp (freqHz / fs, 0.0f, 0.49f);
        float out;

        switch (wave)
        {
            case Wave::Sine:     out = std::sin (kTwoPi * phase); advance (inc); break;
            case Wave::Triangle: out = triangle (phase);          advance (inc); break;
            case Wave::Square:   out = square (phase, inc, pwm);  advance (inc); break;
            case Wave::Saw:      out = saw (phase, inc);          advance (inc); break;
            case Wave::RevSaw:   out = -saw (phase, inc);         advance (inc); break;
            case Wave::SuperSaw: out = superSaw (inc);            advance (inc); break;
            default:             out = 0.0f;                      advance (inc); break;
        }

        if (fold > 0.0001f)
        {
            const float driven = out * (1.0f + fold * 7.0f);
            out += fold * (reflectFold (driven) - out);   // dry/wet mix, transparent at 0
        }
        return out;
    }

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    static float clamp (float v, float lo, float hi) noexcept
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    void advance (float inc) noexcept
    {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }

    // ---- polyBLEP residual for a discontinuity at phase wrap ----
    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    static float saw (float p, float dt) noexcept
    {
        return (2.0f * p - 1.0f) - polyBlep (p, dt);
    }

    static float square (float p, float dt, float duty) noexcept
    {
        float v = p < duty ? 1.0f : -1.0f;
        v += polyBlep (p, dt);
        float p2 = p - duty;
        if (p2 < 0.0f) p2 += 1.0f;
        v -= polyBlep (p2, dt);
        return v;
    }

    static float triangle (float p) noexcept
    {
        return 4.0f * std::fabs (p - 0.5f) - 1.0f;
    }

    float superSaw (float inc) noexcept
    {
        // fixed mild detune fan, cents: -14, -7, 0, +7, +14
        static constexpr std::array<float, 5> ratios {
            0.991947f, 0.995965f, 1.0f, 1.004052f, 1.008121f };
        static constexpr std::array<float, 5> gains {
            0.6f, 0.8f, 1.0f, 0.8f, 0.6f };

        float sum = 0.0f;
        for (size_t i = 0; i < ratios.size(); ++i)
        {
            const float di = clamp (inc * ratios[i], 0.0f, 0.49f);
            sum += gains[i] * saw (superPhase[i], di);
            superPhase[i] += di;
            if (superPhase[i] >= 1.0f) superPhase[i] -= 1.0f;
        }
        return sum * 0.32f;   // normalize the stack
    }

    // reflect ("triangle") folding: identity for |x| <= 1
    static float reflectFold (float x) noexcept
    {
        const float t = (x - 1.0f) * 0.25f;
        const float f = t - std::floor (t);
        return 4.0f * std::fabs (f - 0.5f) - 1.0f;
    }

    float fs = 44100.0f;
    float phase = 0.0f;
    std::array<float, 5> superPhase {};
    Wave  wave = Wave::Square;
    float pwm  = 0.5f;
    float fold = 0.0f;
};
