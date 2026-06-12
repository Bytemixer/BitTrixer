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
#include "Oscillator.h"
#include "NoiseGen.h"
#include "LadderFilter.h"
#include "Drive.h"
#include "../Params.h"

// ============================================================================
//  Voice — ONE unison voice of a trigger instance.
//  Composes 3 Oscillators + NoiseGen + LadderFilter(+HPF) + Drive.
//  Signal path per voice:  MIX -> tanh -> LPF/HPF -> VCA (amp) -> Drive.
//  Carries its own detune offset, pan position and slow "analog drift"
//  random walk so stacked voices never phase-lock sterile.
// ============================================================================

class Voice
{
public:
    // Everything the voice needs for one sub-block, precomputed by the
    // instance (mod matrix + envelopes already applied).
    struct SubBlockCtx
    {
        bool  oscOn[Params::kNumOscs] {};
        Oscillator::Wave oscWave[Params::kNumOscs] {};
        float oscFreqHz[Params::kNumOscs] {};   // pre-detune frequency
        float oscPwm[Params::kNumOscs] {};
        float oscFold[Params::kNumOscs] {};
        float oscLevel[Params::kNumOscs] {};

        Params::SyncMode syncMode = Params::SyncMode::Off;

        bool  noiseOn = false;
        float noiseLevel = 0.0f;

        float cutoffHz = 20000.0f;
        float res01 = 0.0f;
        bool  fourPole = true;
        bool  hpfOn = false;
        float hpfHz = 20.0f;

        float driveAmt = 0.0f;
        const float* amp = nullptr;             // per-sample VCA gain
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        for (auto& o : oscs) o.prepare (sampleRate);
        noise.prepare (sampleRate);
        filter.prepare (sampleRate);
    }

    // Called at trigger time. detuneCents/pan position this voice within the
    // unison stack; the seed decorrelates phases, noise and drift.
    void start (float detuneCents, float pan, uint32_t seed) noexcept
    {
        rng = seed != 0 ? seed : 0xB16B00B5u;
        detuneRatio = std::pow (2.0f, detuneCents / 1200.0f);

        // equal-power pan
        const float a = (pan + 1.0f) * 0.25f * 3.14159265f;
        panL = std::cos (a);
        panR = std::sin (a);

        for (auto& o : oscs)
            o.reset (nextRand01());
        noise.seed (nextRandU32());
        // keep filter state (anti-click on voice steal), drift keeps walking
    }

    void hardReset() noexcept
    {
        filter.reset();
        noise.reset();
        driftLp = 0.0f;
    }

    void renderAdd (float* left, float* right, int n, const SubBlockCtx& ctx) noexcept
    {
        // ---- slow analog drift: one random-walk step per sub-block ----
        driftLp += 0.002f * (nextBipolar() - driftLp);
        const float driftRatio  = std::pow (2.0f, driftLp * 3.0f / 1200.0f); // ±3 cents
        const float driftCutoff = 1.0f + driftLp * 0.02f;

        float inc[Params::kNumOscs] {};
        for (int i = 0; i < Params::kNumOscs; ++i)
        {
            if (! ctx.oscOn[i])
                continue;
            oscs[(size_t) i].setWave (ctx.oscWave[i]);
            oscs[(size_t) i].setPwm  (ctx.oscPwm[i]);
            oscs[(size_t) i].setFold (ctx.oscFold[i]);
            inc[i] = ctx.oscFreqHz[i] * detuneRatio * driftRatio;
        }

        filter.setFourPole (ctx.fourPole);
        filter.setCutoff (ctx.cutoffHz * driftCutoff);
        filter.setResonance (ctx.res01);
        filter.setHpf (ctx.hpfOn, ctx.hpfHz);
        noise.setColor (noiseColor);
        drive.setAmount (ctx.driveAmt);

        using Sync = Params::SyncMode;
        const Sync sync = ctx.syncMode;
        const bool sync2 = sync == Sync::S2to1 || sync == Sync::S23to1 || sync == Sync::S2to1_3to2;
        const bool sync3from1 = sync == Sync::S3to1 || sync == Sync::S23to1;
        const bool sync3from2 = sync == Sync::S3to2 || sync == Sync::S2to1_3to2;

        for (int s = 0; s < n; ++s)
        {
            float mix = 0.0f;
            bool w1 = false, w2 = false;

            if (ctx.oscOn[0])
            {
                mix += oscs[0].tick (inc[0]) * ctx.oscLevel[0];
                w1 = oscs[0].wrapped();
            }
            if (ctx.oscOn[1])
            {
                if (sync2 && w1)
                    oscs[1].hardSync();
                mix += oscs[1].tick (inc[1]) * ctx.oscLevel[1];
                w2 = oscs[1].wrapped();
            }
            if (ctx.oscOn[2])
            {
                if ((sync3from1 && w1) || (sync3from2 && w2))
                    oscs[2].hardSync();
                mix += oscs[2].tick (inc[2]) * ctx.oscLevel[2];
            }

            if (ctx.noiseOn)
                mix += noise.tick() * ctx.noiseLevel;

            mix = std::tanh (mix);                    // mixer bus warmth

            float y = filter.tick (mix);
            y *= ctx.amp[s];                          // VCA
            y = drive.tick (y);                       // amp "preamp push"

            left[s]  += y * panL;
            right[s] += y * panR;
        }
    }

    void setNoiseColor (float c) noexcept { noiseColor = c; }

private:
    uint32_t nextRandU32() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return rng;
    }
    float nextRand01() noexcept  { return (float) (nextRandU32() >> 8) * (1.0f / 16777216.0f); }
    float nextBipolar() noexcept { return nextRand01() * 2.0f - 1.0f; }

    float fs = 44100.0f;
    Oscillator   oscs[Params::kNumOscs];
    NoiseGen     noise;
    LadderFilter filter;
    Drive        drive;

    float detuneRatio = 1.0f;
    float panL = 0.7071f, panR = 0.7071f;
    float noiseColor = 0.0f;
    float driftLp = 0.0f;
    uint32_t rng = 0xB16B00B5u;
};
