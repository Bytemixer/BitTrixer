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
#include "FastMath.h"

// ============================================================================
//  Drive — the VCA "preamp push". A dry/wet tanh waveshaper: transparent at
//  amount 0, musical overdrive as the amount rises (input gain increases
//  with the amount, output level is roughly compensated).
// ============================================================================

class Drive
{
public:
    void setAmount (float amount01) noexcept
    {
        amt = amount01 < 0.0f ? 0.0f : (amount01 > 1.0f ? 1.0f : amount01);
        preGain = 1.0f + amt * 6.0f;
        makeup  = 1.0f / (0.6f + 0.4f * preGain);   // tame the loudness jump
    }

    float tick (float x) const noexcept
    {
        if (amt < 0.0001f)
            return x;
        const float wet = FastMath::tanh (x * preGain) * makeup * 1.6f;
        return x + amt * (wet - x);
    }

private:
    float amt = 0.0f, preGain = 1.0f, makeup = 1.0f;
};
