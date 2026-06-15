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
#include "Phaser.h"
#include "Flanger.h"
#include "RingMod.h"
#include "Tremolo.h"
#include "Formant.h"
#include "Delay.h"

// ============================================================================
//  FxChain — the per-instance effect pedalboard that runs after the voice sum
//  (retriggered with the sound). Seven effects, each with its own enable, run
//  in a user-defined order (see setOrder). Crush now lives here too (post-VCA),
//  so it reorders like the rest. Disabled effects are skipped.
// ============================================================================

class FxChain
{
public:
    static constexpr int kCount = 7;
    enum class Effect { Crush = 0, Phaser, Flanger, RingMod, Tremolo, Formant, Delay };

    void prepare (double sampleRate) noexcept
    {
        crushL.reset(); crushR.reset();
        phaser.prepare (sampleRate); flanger.prepare (sampleRate); ring.prepare (sampleRate);
        trem.prepare (sampleRate);   formant.prepare (sampleRate); delay.prepare (sampleRate);
    }

    void retrigger() noexcept
    {
        crushL.reset(); crushR.reset();
        phaser.retrigger(); flanger.retrigger(); ring.retrigger();
        trem.retrigger();   formant.retrigger(); delay.retrigger();
    }

    // per-effect pre-filter flags: a mono effect flagged pre ran per-voice in
    // PreFx, so it is skipped here. Flanger/Delay have no pre flag (always post).
    void setPreFlags (bool crushF, bool phaseF, bool ringF, bool tremF, bool formF) noexcept
    {
        crushPre = crushF; phasePre = phaseF; ringPre = ringF; tremPre = tremF; formPre = formF;
    }

    // order is a permutation of 0..kCount-1 (chain position -> Effect)
    void setOrder (const int* order) noexcept
    {
        for (int i = 0; i < kCount; ++i)
        {
            const int e = order[i];
            ord[i] = (e >= 0 && e < kCount) ? e : i;
        }
    }

    void setCrush   (bool on, float bits, float down) noexcept           { crushOn = on; crushL.setParams (bits, down); crushR.setParams (bits, down); }
    void setPhaser  (bool on, float rate, float depth, float fb) noexcept { phaseOn = on;  phaser.setParams (rate, depth, fb); }
    void setFlanger (bool on, float rate, float depth, float fb) noexcept { flangeOn = on; flanger.setParams (rate, depth, fb); }
    void setRing    (bool on, float freq, float mix, int wave) noexcept   { ringOn = on;   ring.setWave (wave); ring.setParams (freq, mix); }
    void setTrem    (bool on, float rate, float depth, int wave) noexcept { tremOn = on;   trem.setWave (wave); trem.setParams (rate, depth); }
    void setFormant (bool on, float vowel, float reso, float mix) noexcept{ formOn = on;   formant.setParams (vowel, reso, mix); }
    void setDelay   (bool on, float timeMs, float fb, float mix) noexcept { delayOn = on;  delay.setParams (timeMs, fb, mix); }

    void process (float* left, float* right, int n) noexcept
    {
        for (int i = 0; i < kCount; ++i)
            runEffect (ord[i], left, right, n);
    }

private:
    void runEffect (int e, float* l, float* r, int n) noexcept
    {
        // a mono effect flagged pre-filter already ran per-voice in PreFx -> skip
        switch ((Effect) e)
        {
            case Effect::Crush:
                if (crushOn && ! crushPre)
                    for (int s = 0; s < n; ++s) { l[s] = crushL.tick (l[s]); r[s] = crushR.tick (r[s]); }
                break;
            case Effect::Phaser:  if (phaseOn  && ! phasePre) phaser.process  (l, r, n); break;
            case Effect::Flanger: if (flangeOn)               flanger.process (l, r, n); break;
            case Effect::RingMod: if (ringOn   && ! ringPre)  ring.process    (l, r, n); break;
            case Effect::Tremolo: if (tremOn   && ! tremPre)  trem.process    (l, r, n); break;
            case Effect::Formant: if (formOn   && ! formPre)  formant.process (l, r, n); break;
            case Effect::Delay:   if (delayOn)                delay.process   (l, r, n); break;
            default: break;
        }
    }

    Bitcrusher crushL, crushR;
    Phaser  phaser;
    Flanger flanger;
    RingMod ring;
    Tremolo trem;
    Formant formant;
    Delay   delay;

    bool crushOn = false, phaseOn = false, flangeOn = false, ringOn = false,
         tremOn = false, formOn = false, delayOn = false;
    bool crushPre = false, phasePre = false, ringPre = false, tremPre = false, formPre = false;

    int ord[kCount] { 0, 1, 2, 3, 4, 5, 6 };
};
