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

#include <vector>
#include <juce_core/juce_core.h>
#include "Params.h"

// ============================================================================
//  Variation — the VARIATE nudge set: which parameters move, how far, and the
//  triangular nudge applied to each. Shared by the live VARIATE / MUTATE
//  buttons and the offline batch export. Values are normalised 0..1.
// ============================================================================

namespace Variation
{
    struct Nudge { juce::String pid; float scale; };

    inline constexpr float kVariateScale = 0.12f;
    inline constexpr float kMutateScale  = 0.4f;

    inline std::vector<Nudge> targets (bool includeLoudnessAndLength)
    {
        using namespace Params;
        std::vector<Nudge> t;
        for (int i = 1; i <= kNumOscs; ++i)
        {
            t.push_back ({ oscId (i, "fine"), 1.0f });
            t.push_back ({ oscId (i, "pwm"),  1.0f });
            t.push_back ({ oscId (i, "fold"), 0.7f });
        }
        t.push_back ({ id::baseFreq,   0.6f });
        t.push_back ({ id::noiseColor, 1.0f });
        for (int j = 1; j <= kNumLfos; ++j)
            t.push_back ({ lfoId (j, "rate"), 0.8f });
        for (int k = 1; k <= kNumModSlots; ++k)
            t.push_back ({ modId (k, "depth"), 0.5f });
        t.push_back ({ id::lpfCutoff, 0.6f });
        t.push_back ({ id::lpfRes,    0.6f });
        t.push_back ({ id::lpfEnv,    0.5f });
        t.push_back ({ id::envFDecay, 0.5f });
        t.push_back ({ id::envADecay, 0.5f });
        t.push_back ({ id::uniDetune, 0.8f });

        // FM voice (3rd generator slot): operator ratios/levels + feedback
        for (int i = 1; i <= 4; ++i)
        {
            t.push_back ({ fmOpId (i, "ratio"), 0.5f });
            t.push_back ({ fmOpId (i, "level"), 0.6f });
        }
        t.push_back ({ id::fmFeedback, 0.6f });

        if (includeLoudnessAndLength)
        {
            for (int i = 1; i <= kNumOscs; ++i)
                t.push_back ({ oscId (i, "level"), 0.5f });
            t.push_back ({ id::noiseLevel,  0.5f });
            t.push_back ({ id::vcaDrive,    0.5f });
            t.push_back ({ id::envFAttack,  0.5f });
            t.push_back ({ id::envARelease, 0.5f });
            t.push_back ({ id::pj1Amt,      0.4f });
            t.push_back ({ id::pj2Amt,      0.4f });
        }
        return t;
    }

    // triangular distribution on -1..1: extremes are rare, the centre is likely
    inline float triangular (juce::Random& rng)
    {
        return 0.5f * ((rng.nextFloat() * 2.0f - 1.0f) + (rng.nextFloat() * 2.0f - 1.0f));
    }

    // getBase (pid, float& out) -> bool; setValue (pid, float normalised).
    // A target whose base is missing is skipped and draws nothing from rng.
    template <typename GetBase, typename SetValue>
    void apply (const std::vector<Nudge>& targets, float baseScale, float amount,
                juce::Random& rng, GetBase&& getBase, SetValue&& setValue)
    {
        for (const auto& t : targets)
        {
            float base = 0.0f;
            if (! getBase (t.pid, base))
                continue;
            const float r = triangular (rng);
            setValue (t.pid, juce::jlimit (0.0f, 1.0f, base + r * amount * baseScale * t.scale));
        }
    }
}
