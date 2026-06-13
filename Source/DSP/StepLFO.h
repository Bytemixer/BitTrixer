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
//  StepLFO — a step-sequencer LFO (the unique RetroForge twist). Advances
//  through 2..8 user-drawn steps at a settable rate and outputs the current
//  step level, bipolar (-1..+1). Routed through the mod matrix like LFO 1,
//  so mapping it to pitch gives an arpeggio/sequence, to cutoff a rhythmic
//  filter pattern, etc. Optional glide smooths the transitions; a delay
//  fades the whole thing in after the trigger.
// ============================================================================

class StepLFO
{
public:
    static constexpr int kMaxSteps = 8;

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        retrigger();
    }

    void retrigger() noexcept
    {
        phase = 0.0f;
        step = 0;
        age = 0.0f;
        glided = stepVals[0];
        lastOut = 0.0f;
    }

    void setRate  (float hz) noexcept       { rateHz = hz < 0.0f ? 0.0f : hz; }
    void setDelay (float seconds) noexcept  { delaySec = seconds < 0.0f ? 0.0f : seconds; }
    void setSteps (int n) noexcept          { numSteps = n < 2 ? 2 : (n > kMaxSteps ? kMaxSteps : n); }
    void setSmooth (bool s) noexcept        { smooth = s; }
    void setStepValue (int i, float v) noexcept
    {
        if (i >= 0 && i < kMaxSteps)
            stepVals[(size_t) i] = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
    }

    float value() const noexcept { return lastOut; }
    int   currentStep() const noexcept { return step; }

    float tick() noexcept
    {
        const float inc = rateHz / fs;        // rateHz = steps per second
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            step = (step + 1) % numSteps;
        }

        const float target = stepVals[(size_t) step];
        float out;
        if (smooth)
        {
            // glide over a quarter of a step's duration
            const float tau = 0.25f / (rateHz > 0.1f ? rateHz : 0.1f);
            const float a = 1.0f - std::exp (-1.0f / (tau * fs));
            glided += a * (target - glided);
            out = glided;
        }
        else
        {
            out = target;
        }

        age += 1.0f / fs;
        float amp = 1.0f;
        if (delaySec > 0.001f)
        {
            amp = age / delaySec;
            if (amp > 1.0f) amp = 1.0f;
        }

        lastOut = out * amp;
        return lastOut;
    }

private:
    float fs = 44100.0f;
    float rateHz = 8.0f;
    float delaySec = 0.0f;
    int   numSteps = 4;
    int   step = 0;
    float phase = 0.0f;
    float age = 0.0f;
    float glided = 0.0f;
    float lastOut = 0.0f;
    bool  smooth = false;
    std::array<float, kMaxSteps> stepVals {};
};
