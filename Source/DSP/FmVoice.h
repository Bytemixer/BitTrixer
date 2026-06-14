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
//  FmUnit — a 4-operator FM voice in the Sega Genesis / YM2612 mould, taking
//  the third generator slot (OSC 3's replacement).
//
//  Each operator is a sine whose "level" is its output gain when it is a
//  carrier, or its modulation depth when it feeds another operator. The eight
//  YM2612 algorithms are stored as data (a modulator bitmask per operator plus
//  a carrier bitmask) and run by one generic loop. Operator 1 (index 0) is the
//  only one with feedback, averaged over its last two outputs like the chip.
//
//  No envelopes here by design: movement comes from the mod matrix driving the
//  operator levels / feedback (see ModMatrix). No JUCE deps; prepare/tick.
// ============================================================================

class FmUnit
{
public:
    static constexpr int kOps = 4;
    static constexpr int kNumAlgos = 8;

    // mod[i]: bitmask of operators (bit j) that modulate operator i.
    // carriers: bitmask of operators summed to the output. Feedback is fixed
    // to operator 0. In every algorithm a modulator's index is < its target,
    // so a single S1->S4 (0->3) pass evaluates them in dependency order.
    struct Algo { uint8_t mod[kOps]; uint8_t carriers; };

    static constexpr Algo kAlgos[kNumAlgos] = {
        // 0  four serial            0->1->2->3            carrier 3
        { { 0u, 0b0001u, 0b0010u, 0b0100u }, 0b1000u },
        // 1  three double serial    (0,1)->2->3           carrier 3
        { { 0u, 0u,      0b0011u, 0b0100u }, 0b1000u },
        // 2  double modulation 1    1->2 ; (0,2)->3       carrier 3
        { { 0u, 0u,      0b0010u, 0b0101u }, 0b1000u },
        // 3  double modulation 2    0->1 ; (1,2)->3       carrier 3
        { { 0u, 0b0001u, 0u,      0b0110u }, 0b1000u },
        // 4  two serial + two par   0->1 ; 2->3           carriers 1,3
        { { 0u, 0b0001u, 0u,      0b0100u }, 0b1010u },
        // 5  common mod 3 parallel  0->1 ; 0->2 ; 0->3    carriers 1,2,3
        { { 0u, 0b0001u, 0b0001u, 0b0001u }, 0b1110u },
        // 6  two serial + two sine  0->1                  carriers 1,2,3
        { { 0u, 0b0001u, 0u,      0u      }, 0b1110u },
        // 7  four parallel sine     (none)                carriers 0,1,2,3
        { { 0u, 0u,      0u,      0u      }, 0b1111u },
    };

    void prepare (double sampleRate) noexcept { fs = (float) sampleRate; reset(); }

    void reset (float startPhase = 0.0f) noexcept
    {
        for (int i = 0; i < kOps; ++i) phase[i] = startPhase;
        fbLast = fbPrev = 0.0f;
    }

    void setAlgo (int a) noexcept { algo = a < 0 ? 0 : (a >= kNumAlgos ? kNumAlgos - 1 : a); }
    void setFeedback (float fb) noexcept { feedback = fb < 0.0f ? 0.0f : (fb > 1.0f ? 1.0f : fb); }

    void setOp (int i, float ratio, float level) noexcept
    {
        if ((unsigned) i < (unsigned) kOps) { opRatio[i] = ratio; opLevel[i] = level; }
    }

    // baseFreqHz = the (already pitched) note frequency; operator i runs at
    // ratio[i] * baseFreqHz. Returns one mono sample.
    float tick (float baseFreqHz) noexcept
    {
        const Algo& al = kAlgos[algo];
        const float baseInc = baseFreqHz / fs;
        float out[kOps];

        for (int i = 0; i < kOps; ++i)
        {
            float m = 0.0f;
            if (const uint8_t mask = al.mod[i])
            {
                for (int j = 0; j < i; ++j)              // modulators are lower-indexed
                    if (mask & (1u << j)) m += out[j];
                m *= kFmDepth;
            }
            if (i == 0 && feedback > 0.0f)
                m += feedback * kFbDepth * 0.5f * (fbLast + fbPrev);

            const float o = FastMath::sinCycle (phase[i] + m) * opLevel[i];
            out[i] = o;

            float inc = opRatio[i] * baseInc;
            if (inc > 0.49f) inc = 0.49f;
            phase[i] += inc;
            if (phase[i] >= 1.0f) phase[i] -= 1.0f;

            if (i == 0) { fbPrev = fbLast; fbLast = o; }
        }

        float sum = 0.0f; int nc = 0;
        for (int i = 0; i < kOps; ++i)
            if (al.carriers & (1u << i)) { sum += out[i]; ++nc; }
        return nc > 0 ? sum / (float) nc : 0.0f;
    }

private:
    // a modulator's -1..1 output becomes a phase offset of up to +/- kFmDepth
    // cycles (~+/-2*pi rad at level 1 => a strong but musical max index)
    static constexpr float kFmDepth = 1.0f;
    static constexpr float kFbDepth = 0.5f;   // operator-1 feedback scaling

    float fs = 44100.0f;
    float phase[kOps] {};
    float opRatio[kOps] { 1.0f, 1.0f, 1.0f, 1.0f };
    float opLevel[kOps] { 1.0f, 0.0f, 0.0f, 0.0f };
    int   algo = 0;
    float feedback = 0.0f;
    float fbLast = 0.0f, fbPrev = 0.0f;
};
