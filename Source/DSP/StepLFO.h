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
#include <algorithm>

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
        elapsed = 0.0f;
        step = 0;
        age = 0.0f;
        glided = stepVals[0];
        lastOut = 0.0f;
    }

    void setRate  (float hz) noexcept       { rateHz = hz < 0.01f ? 0.01f : hz; }
    void setDelay (float seconds) noexcept  { delaySec = seconds < 0.0f ? 0.0f : seconds; }
    void setSteps (int n) noexcept          { numSteps = n < 2 ? 2 : (n > kMaxSteps ? kMaxSteps : n); }
    void setGlide (float g) noexcept        { glide = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g); }
    void setSkew  (float s) noexcept        { skew = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s); }
    void setStepValue (int i, float v) noexcept
    {
        if (i >= 0 && i < kMaxSteps)
            stepVals[(size_t) i] = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
    }

    float value() const noexcept { return lastOut; }
    int   currentStep() const noexcept { return step; }

    float tick() noexcept
    {
        // SKEW staggers durations: even steps shorter / odd longer (or vice
        // versa). A two-step pattern with skew makes step 1 short, step 2 long.
        const float dur = stepDuration (step);
        elapsed += 1.0f / fs;
        if (elapsed >= dur)
        {
            elapsed -= dur;
            step = (step + 1) % numSteps;
        }

        const float target = stepVals[(size_t) step];
        float out;
        if (glide > 0.001f)
        {
            // glide over up to half the step duration as the knob opens
            const float tau = glide * stepDuration (step) * 0.5f;
            const float a = 1.0f - std::exp (-1.0f / (std::max (0.0001f, tau) * fs));
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
    float stepDuration (int s) const noexcept
    {
        const float base = 1.0f / rateHz;        // average step length (seconds)
        const float k = (s & 1) ? (1.0f + skew * 0.85f) : (1.0f - skew * 0.85f);
        return base * k;
    }

    float fs = 44100.0f;
    float rateHz = 8.0f;
    float delaySec = 0.0f;
    int   numSteps = 4;
    int   step = 0;
    float elapsed = 0.0f;
    float age = 0.0f;
    float glided = 0.0f;
    float lastOut = 0.0f;
    float glide = 0.0f;
    float skew = 0.0f;
    std::array<float, kMaxSteps> stepVals {};
};
