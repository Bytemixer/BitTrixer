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
#include "FastMath.h"

// ============================================================================
//  NoiseGen — the dedicated noise "oscillator", four switchable characters.
//  The COLOR control changes meaning per type:
//   * Analog    : white -> pink crossfade (smooth analog hiss/rumble)
//   * LFSR Hiss : long shift-register console noise; COLOR divides the
//                 clock (0 = full-rate bright hiss, 1 = crunchy low rattle —
//                 how 80s console noise channels were "pitched")
//   * LFSR Buzz : short shift register — periodic, buzzy, tonal grit;
//                 COLOR divides the clock the same way
//   * Rasp      : random telegraph noise — clocked white noise slammed to
//                 near-binary; COLOR sets the grit rate
// ============================================================================

class NoiseGen
{
public:
    enum class Type { Analog = 0, LfsrHiss, LfsrBuzz, Rasp };

    void prepare (double /*sampleRate*/) noexcept { reset(); }

    void reset() noexcept
    {
        b0 = b1 = b2 = 0.0f;
        clockCount = 1.0e9f;       // force a fresh value immediately
        held = 0.0f;
        lfsr = 0x4D21u;
    }

    void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9e3779b9u; }

    void setType (Type t) noexcept { type = t; }

    void setColor (float c01) noexcept
    {
        color = c01 < 0.0f ? 0.0f : (c01 > 1.0f ? 1.0f : c01);
        // LFSR/Rasp clock divider: 1 (bright) .. 128 (crunchy low)
        clockStep = std::exp2 (color * 7.0f);
    }

    float tick() noexcept
    {
        switch (type)
        {
            case Type::Analog:
            {
                const float white = nextWhite();
                // Paul Kellet "economy" pink filter
                b0 = 0.99765f * b0 + white * 0.0990460f;
                b1 = 0.96300f * b1 + white * 0.2965164f;
                b2 = 0.57000f * b2 + white * 1.0526913f;
                const float pink = (b0 + b1 + b2 + white * 0.1848f) * 0.28f;
                return white + color * (pink - white);
            }

            case Type::LfsrHiss:
                if (advanceClock())
                    held = stepLfsr (14, 0, 1) ? 0.8f : -0.8f;   // 15-bit, taps 1+2
                return held;

            case Type::LfsrBuzz:
                if (advanceClock())
                    held = stepLfsr (5, 0, 5) ? 0.8f : -0.8f;    // 6-bit, short period buzz
                return held;

            case Type::Rasp:
                if (advanceClock())
                    held = FastMath::tanh (nextWhite() * 6.0f) * 0.9f;
                return held;
        }
        return 0.0f;
    }

private:
    bool advanceClock() noexcept
    {
        clockCount += 1.0f;
        if (clockCount >= clockStep)
        {
            clockCount -= clockStep;
            if (clockCount >= clockStep)   // after reset()
                clockCount = 0.0f;
            return true;
        }
        return false;
    }

    // Galois-style shift with two XOR taps; topBit = register width - 1
    bool stepLfsr (int topBit, int tapA, int tapB) noexcept
    {
        const uint32_t bit = ((lfsr >> tapA) ^ (lfsr >> tapB)) & 1u;
        lfsr = (lfsr >> 1) | (bit << topBit);
        if (lfsr == 0)
            lfsr = 0x4D21u;                // never let the register lock at zero
        return (lfsr & 1u) != 0;
    }

    float nextWhite() noexcept
    {
        // xorshift32 -> [-1, 1)
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }

    Type type = Type::Analog;
    uint32_t state = 0x12345678u;
    uint32_t lfsr = 0x4D21u;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float color = 0.0f;
    float clockStep = 1.0f;
    float clockCount = 1.0e9f;
    float held = 0.0f;
};
