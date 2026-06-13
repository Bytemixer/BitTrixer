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
//  LofiStage — the master output "format" stage: reduce a signal to a lower
//  effective sample rate and/or bit depth.
//   1) anti-alias / fidelity lowpass (2-pole) at the target Nyquist, so a
//      lower rate audibly loses its highs (the obvious "fidelity" change)
//   2) sample & hold decimation at the target rate (the staircase / grit)
//   3) bit-depth quantization (8-bit crunch)
//  No JUCE dependency, so it can be unit-tested on its own.
// ============================================================================

class LofiStage
{
public:
    void prepare (float hostSampleRate) noexcept
    {
        fsHost = hostSampleRate > 1.0f ? hostSampleRate : 44100.0f;
        reset();
    }

    void reset() noexcept
    {
        decimCount = 0.0f;
        holdL = holdR = 0.0f;
        lpL1 = lpL2 = lpR1 = lpR2 = 0.0f;
    }

    // targetRate >= host disables rate reduction (we can't upsample)
    void setRate (float targetRate) noexcept
    {
        if (targetRate > 0.0f && targetRate < fsHost - 1.0f)
        {
            decimStep = fsHost / targetRate;
            doDecim = true;
            const float fc = targetRate * 0.5f;        // target Nyquist
            lpCoef = 1.0f - std::exp (-2.0f * 3.14159265f * fc / fsHost);
        }
        else
        {
            doDecim = false;
        }
    }

    void setBits8 (bool on) noexcept
    {
        doBits = on;
        bitLevels = on ? 128.0f : 32768.0f;
    }

    bool active() const noexcept { return doDecim || doBits; }

    void process (float& l, float& r) noexcept
    {
        if (doDecim)
        {
            // 2-pole lowpass removes the content that would otherwise alias,
            // giving a clean loss of highs as the rate drops
            lpL1 += lpCoef * (l - lpL1);  lpL2 += lpCoef * (lpL1 - lpL2);  l = lpL2;
            lpR1 += lpCoef * (r - lpR1);  lpR2 += lpCoef * (lpR1 - lpR2);  r = lpR2;

            if (decimCount <= 0.0f)
            {
                decimCount += decimStep;
                holdL = l;
                holdR = r;
            }
            decimCount -= 1.0f;
            l = holdL;
            r = holdR;
        }

        if (doBits)
        {
            l = std::round (l * bitLevels) / bitLevels;
            r = std::round (r * bitLevels) / bitLevels;
        }
    }

private:
    float fsHost = 44100.0f;
    float decimStep = 1.0f;
    float decimCount = 0.0f;
    float holdL = 0.0f, holdR = 0.0f;
    float lpCoef = 1.0f;
    float lpL1 = 0.0f, lpL2 = 0.0f, lpR1 = 0.0f, lpR2 = 0.0f;
    float bitLevels = 32768.0f;
    bool  doDecim = false;
    bool  doBits = false;
};
