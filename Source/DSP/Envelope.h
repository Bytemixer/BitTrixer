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
//  Envelope — ADSR with a continuously variable curve shape and an invert
//  switch, behaving like an analog gate-driven envelope:
//   * gate-on re-attacks from the CURRENT level (no click-y hard reset)
//   * curve  -1 -> exponential-feel, 0 -> linear, +1 -> logarithmic-feel
//   * invert flips the output; the release always fades the OUTPUT to
//     silence so an inverted envelope can never leave a voice stuck loud.
// ============================================================================

class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        level = 0.0f;
        out   = 0.0f;
        t     = 0.0f;
    }

    void setParams (float attackSec, float decaySec, float sustain01,
                    float releaseSec, float curveAmt, bool inverted) noexcept
    {
        aTime = attackSec  < kMinTime ? kMinTime : attackSec;
        dTime = decaySec   < kMinTime ? kMinTime : decaySec;
        rTime = releaseSec < kMinTime ? kMinTime : releaseSec;
        sus   = sustain01;
        curve = curveAmt;
        invert = inverted;
    }

    void gateOn() noexcept
    {
        attackStart = level;          // analog behaviour: continue from here
        t = 0.0f;
        stage = Stage::Attack;
    }

    void gateOff() noexcept
    {
        if (stage == Stage::Idle)
            return;
        releaseStartLevel = level;
        releaseStartOut   = out;
        t = 0.0f;
        stage = Stage::Release;
    }

    bool  isActive() const noexcept { return stage != Stage::Idle; }
    bool  isReleasing() const noexcept { return stage == Stage::Release; }
    float value() const noexcept { return out; }

    float tick() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                level = 0.0f;
                out = 0.0f;
                break;

            case Stage::Attack:
            {
                t += 1.0f / (aTime * fs);
                if (t >= 1.0f)
                {
                    level = 1.0f;
                    t = 0.0f;
                    stage = Stage::Decay;
                }
                else
                {
                    level = attackStart + (1.0f - attackStart) * shape (t);
                }
                out = invert ? 1.0f - level : level;
                break;
            }

            case Stage::Decay:
            {
                t += 1.0f / (dTime * fs);
                if (t >= 1.0f)
                {
                    level = sus;
                    stage = Stage::Sustain;
                }
                else
                {
                    level = 1.0f - (1.0f - sus) * shape (t);
                }
                out = invert ? 1.0f - level : level;
                break;
            }

            case Stage::Sustain:
                level = sus;
                out = invert ? 1.0f - level : level;
                break;

            case Stage::Release:
            {
                t += 1.0f / (rTime * fs);
                if (t >= 1.0f)
                {
                    level = 0.0f;
                    out   = 0.0f;
                    stage = Stage::Idle;
                }
                else
                {
                    const float s = shape (t);
                    level = releaseStartLevel * (1.0f - s);
                    // output domain: always fades to zero, even when inverted
                    out = releaseStartOut * (1.0f - s);
                }
                break;
            }
        }
        return out;
    }

private:
    static constexpr float kMinTime = 0.0001f;

    // curve -1 .. +1 mapped to a power shape; 0 = linear.
    float shape (float x) const noexcept
    {
        if (curve > -0.001f && curve < 0.001f)
            return x;
        const float k = std::pow (4.0f, -curve);   // +1 -> 0.25 (log feel), -1 -> 4 (exp feel)
        return std::pow (x, k);
    }

    float fs = 44100.0f;
    Stage stage = Stage::Idle;

    float aTime = 0.001f, dTime = 0.3f, rTime = 0.2f;
    float sus = 0.7f, curve = 0.0f;
    bool  invert = false;

    float level = 0.0f;     // internal (non-inverted) level
    float out   = 0.0f;     // published output
    float t     = 0.0f;     // normalized stage time
    float attackStart = 0.0f;
    float releaseStartLevel = 0.0f;
    float releaseStartOut   = 0.0f;
};
