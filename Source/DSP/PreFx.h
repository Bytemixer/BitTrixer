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

#include "Bitcrusher.h"
#include "RingMod.h"
#include "Tremolo.h"
#include "Phaser.h"
#include "Formant.h"

// ============================================================================
//  PreFx — the "pre-filter" subgroup, run PER VOICE on the mono signal before
//  the filter (so the filter's resonance/drive reshape the effected signal).
//  Holds the five buffer-free effects (Crush, RingMod, Tremolo, Phaser,
//  Formant) processed in a user sub-order. The buffer effects (Flanger/Delay)
//  can't live here -- they stay post-VCA in FxChain. Only active when the FX
//  chain is "split"; otherwise these five run post-VCA in FxChain instead.
// ============================================================================

class PreFx
{
public:
    static constexpr int kCount = 5;
    enum class Effect { Crush = 0, RingMod, Tremolo, Phaser, Formant };

    // plain-data config the engine fills from the patch (params already
    // matrix-modulated); order[] maps chain position -> Effect.
    struct Config
    {
        bool  on = false;
        bool  crushOn = false, ringOn = false, tremOn = false, phaseOn = false, formOn = false;
        float crushBits = 8.0f, crushDown = 1.0f;
        float ringFreq = 200.0f, ringMix = 1.0f;   int ringWave = 0;
        float tremRate = 5.0f, tremDepth = 0.5f;    int tremWave = 0;
        float phaseRate = 1.0f, phaseDepth = 0.5f, phaseFb = 0.3f;
        float formVowel = 0.5f, formReso = 0.5f, formMix = 1.0f;
        int   order[kCount] { 0, 1, 2, 3, 4 };
    };

    void prepare (double sr) noexcept
    {
        crush.reset();
        ring.prepare (sr); trem.prepare (sr); phaser.prepare (sr); formant.prepare (sr);
    }

    void retrigger() noexcept
    {
        crush.reset();
        ring.retrigger(); trem.retrigger(); phaser.retrigger(); formant.retrigger();
    }

    void configure (const Config& c) noexcept
    {
        crushOn = c.crushOn; crush.setParams (c.crushBits, c.crushDown);
        ringOn  = c.ringOn;  ring.setWave (c.ringWave); ring.setParams (c.ringFreq, c.ringMix);
        tremOn  = c.tremOn;  trem.setWave (c.tremWave); trem.setParams (c.tremRate, c.tremDepth);
        phaseOn = c.phaseOn; phaser.setParams (c.phaseRate, c.phaseDepth, c.phaseFb);
        formOn  = c.formOn;  formant.setParams (c.formVowel, c.formReso, c.formMix);
        for (int i = 0; i < kCount; ++i)
            ord[i] = (c.order[i] >= 0 && c.order[i] < kCount) ? c.order[i] : i;
    }

    void processMono (float* buf, int n) noexcept
    {
        for (int i = 0; i < kCount; ++i)
            runOne (ord[i], buf, n);
    }

private:
    void runOne (int e, float* buf, int n) noexcept
    {
        switch ((Effect) e)
        {
            case Effect::Crush:
                if (crushOn) for (int s = 0; s < n; ++s) buf[s] = crush.tick (buf[s]);
                break;
            case Effect::RingMod: if (ringOn)  ring.processMono    (buf, n); break;
            case Effect::Tremolo: if (tremOn)  trem.processMono    (buf, n); break;
            case Effect::Phaser:  if (phaseOn) phaser.processMono  (buf, n); break;
            case Effect::Formant: if (formOn)  formant.processMono (buf, n); break;
        }
    }

    Bitcrusher crush;
    RingMod    ring;
    Tremolo    trem;
    Phaser     phaser;
    Formant    formant;

    bool crushOn = false, ringOn = false, tremOn = false, phaseOn = false, formOn = false;
    int  ord[kCount] { 0, 1, 2, 3, 4 };
};
