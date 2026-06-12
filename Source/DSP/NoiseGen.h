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

#include <cstdint>

// ============================================================================
//  NoiseGen — the dedicated noise "oscillator".
//  xorshift white noise + Paul Kellet pink filter; the color control
//  crossfades continuously white -> pink (gain-compensated).
// ============================================================================

class NoiseGen
{
public:
    void prepare (double /*sampleRate*/) noexcept { reset(); }

    void reset() noexcept
    {
        b0 = b1 = b2 = 0.0f;
    }

    void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9e3779b9u; }

    void setColor (float whiteToPink01) noexcept
    {
        color = whiteToPink01 < 0.0f ? 0.0f : (whiteToPink01 > 1.0f ? 1.0f : whiteToPink01);
    }

    float tick() noexcept
    {
        const float white = nextWhite();

        // Paul Kellet "economy" pink filter
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        const float pink = (b0 + b1 + b2 + white * 0.1848f) * 0.28f; // ~unity peak

        return white + color * (pink - white);
    }

private:
    float nextWhite() noexcept
    {
        // xorshift32 -> [-1, 1)
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }

    uint32_t state = 0x12345678u;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float color = 0.0f;
};
