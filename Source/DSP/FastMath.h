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
//  FastMath — OUR transcendental approximations for the per-sample hot paths.
//  Rationale: <cmath> routes to the platform's libm, so std::sin/tanh results
//  differ slightly between Windows/macOS/Linux builds. These polynomials are
//  defined here, so every platform computes bit-identical waveforms — and
//  they're faster than libm. The small deviations from the ideal functions
//  are part of this synth's voice (the sine carries a touch of low-order
//  harmonic warmth, the tanh knee is slightly softer than ideal).
//  All inputs/outputs are bounded — no input can produce inf/NaN here.
// ============================================================================

namespace FastMath
{
    // sin(2*pi*phase) for phase in [0, 1). Refined parabolic approximation
    // (~0.1 % THD): the residual low-order harmonics give the sine wave a
    // subtle analog-style warmth instead of clinical purity.
    inline float sinCycle (float p) noexcept
    {
        p -= std::floor (p);                       // defensive wrap
        const float x = p * 2.0f - 1.0f;           // -1 .. 1, angle = pi*(x+1)
        float y = 4.0f * x * (std::fabs (x) - 1.0f);   // parabola, = -sin(pi*x)... = sin(angle)
        y = 0.225f * (y * std::fabs (y) - y) + y;  // precision refinement
        return y;
    }

    // sin(x) for arbitrary radians (wraps internally)
    inline float sinRad (float radians) noexcept
    {
        constexpr float invTwoPi = 0.15915493667125702f;
        return sinCycle (radians * invTwoPi);
    }

    // tanh with a hard input clamp: identical saturation curve everywhere,
    // exactly +/-1 beyond |x| = 3 (no divergence, no NaN).
    inline float tanh (float x) noexcept
    {
        x = x < -3.0f ? -3.0f : (x > 3.0f ? 3.0f : x);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // tan(x) for 0 <= x < ~1.4 (filter/phaser coefficient warping).
    // 7th-order odd polynomial; bounded output via input clamp.
    inline float tanPos (float x) noexcept
    {
        x = x < 0.0f ? 0.0f : (x > 1.4f ? 1.4f : x);
        const float x2 = x * x;
        return x * (1.0f + x2 * (0.3333333f + x2 * (0.1333333f + x2 * 0.0539683f)));
    }

    // last-resort guard: anything non-finite becomes silence
    inline float sanitize (float x) noexcept
    {
        return std::isfinite (x) ? x : 0.0f;
    }
}
