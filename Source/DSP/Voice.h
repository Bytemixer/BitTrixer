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
#include "Bitcrusher.h"
#include "FastMath.h"
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

        bool  oscSync[Params::kNumOscs] {};   // sync this osc to OSC 1 master

        bool  noiseOn = false;
        Params::NoiseType noiseType = Params::NoiseType::Analog;
        float noiseLevel = 0.0f;

        float cutoffHz = 20000.0f;
        float res01 = 0.0f;
        bool  fourPole = true;
        bool  hpfOn = false;
        float hpfHz = 20.0f;

        float driveAmt = 0.0f;
        const float* amp = nullptr;             // per-sample VCA gain

        bool  crushOn = false;                  // bitcrusher: post-mix, pre-VCF
        float crushBits = 8.0f;
        float crushDown = 1.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        for (auto& o : oscs) o.prepare (sampleRate);
        noise.prepare (sampleRate);
        filter.prepare (sampleRate);
    }

    // Called at trigger time. detuneCents/pan position this voice within the
    // unison stack; phaseOffset spreads the unison voices; waves let each
    // oscillator start at its rising zero crossing (no onset pop).
    void start (float detuneCents, float pan, uint32_t seed, float phaseOffset,
                const Oscillator::Wave* waves) noexcept
    {
        rng = seed != 0 ? seed : 0xB16B00B5u;
        detuneRatio = std::pow (2.0f, detuneCents / 1200.0f);

        // equal-power pan
        const float a = (pan + 1.0f) * 0.25f * 3.14159265f;
        panL = std::cos (a);
        panR = std::sin (a);

        for (int i = 0; i < Params::kNumOscs; ++i)
        {
            oscs[(size_t) i].setWave (waves[i]);
            float p = Oscillator::zeroCrossingPhase (waves[i]) + phaseOffset;
            p -= std::floor (p);
            oscs[(size_t) i].reset (p);
        }
        noise.seed (nextRandU32());
        // keep filter state (anti-click on voice steal), drift keeps walking
    }

    void hardReset() noexcept
    {
        filter.reset();
        noise.reset();
        crusher.reset();
        driftLp = 0.0f;
    }

    // retrigger: restart the oscillators (the amp env hard-restarts from 0,
    // so the phase reset is masked — no click)
    void retrigger() noexcept
    {
        for (auto& o : oscs)
            o.reset (0.0f);
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
        noise.setType ((NoiseGen::Type) (int) ctx.noiseType);
        noise.setColor (noiseColor);
        drive.setAmount (ctx.driveAmt);
        if (ctx.crushOn)
            crusher.setParams (ctx.crushBits, ctx.crushDown);

        // The sync master is the first ENABLED oscillator (normally OSC 1, but
        // if it is off the role falls through to OSC 2, then OSC 3). Any other
        // enabled oscillator with its sync switch on hard-syncs to the master.
        int master = -1;
        for (int i = 0; i < Params::kNumOscs; ++i)
            if (ctx.oscOn[i]) { master = i; break; }

        for (int s = 0; s < n; ++s)
        {
            float mix = 0.0f;
            bool wMaster = false;

            for (int i = 0; i < Params::kNumOscs; ++i)
            {
                if (! ctx.oscOn[i])
                    continue;
                if (i != master && ctx.oscSync[i] && wMaster)
                    oscs[(size_t) i].hardSync();
                mix += oscs[(size_t) i].tick (inc[i]) * ctx.oscLevel[i];
                if (i == master)
                    wMaster = oscs[(size_t) i].wrapped();
            }

            if (ctx.noiseOn)
                mix += noise.tick() * ctx.noiseLevel;

            mix = FastMath::tanh (mix);               // mixer bus warmth

            if (ctx.crushOn)
                mix = crusher.tick (mix);             // lo-fi grit, smoothed by the VCF

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
    Bitcrusher   crusher;

    float detuneRatio = 1.0f;
    float panL = 0.7071f, panR = 0.7071f;
    float noiseColor = 0.0f;
    float driftLp = 0.0f;
    uint32_t rng = 0xB16B00B5u;
};
