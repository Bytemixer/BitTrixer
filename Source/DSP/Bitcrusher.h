/*  This file is part of the BitTrixer audio plugin.
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
//  Bitcrusher — bit-depth quantization + sample-rate reduction (sample &
//  hold). Sits per-voice between the mixer and the VCF (jfxr ordering), so
//  the filter smooths the grit into the sound instead of icing it on top.
// ============================================================================

class Bitcrusher
{
public:
    void reset() noexcept
    {
        held = 0.0f;
        counter = 1.0e9f;   // force a fresh sample immediately
    }

    // bits 2..16, downsample factor 1..64 (1 = no rate reduction)
    void setParams (float bitDepth, float downsampleFactor) noexcept
    {
        levels = std::exp2 (bitDepth < 2.0f ? 2.0f : (bitDepth > 16.0f ? 16.0f : bitDepth)) * 0.5f;
        step = downsampleFactor < 1.0f ? 1.0f : downsampleFactor;
    }

    float tick (float x) noexcept
    {
        counter += 1.0f;
        if (counter >= step)
        {
            counter -= step;
            if (counter >= step) counter = 0.0f;   // after reset()
            held = std::round (x * levels) / levels;
        }
        return held;
    }

private:
    float levels = 128.0f;
    float step = 1.0f;
    float counter = 1.0e9f;
    float held = 0.0f;
};
