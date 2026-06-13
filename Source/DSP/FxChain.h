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

#include "Phaser.h"
#include "Flanger.h"
#include "RingMod.h"
#include "Tremolo.h"
#include "Formant.h"
#include "Delay.h"

// ============================================================================
//  FxChain — the per-instance effect chain that runs after the voice sum
//  (retriggered with the sound). A pedalboard of named effects, each with its
//  own enable; disabled effects are skipped. Crush is NOT here yet -- it still
//  sits per-voice, pre-filter -- it joins the chain when drag-reorder lands.
//
//  Fixed processing order for now (Phaser -> Flanger -> RingMod -> Tremolo ->
//  Formant -> Delay); a user-defined order arrives with the drag-reorder UI.
// ============================================================================

class FxChain
{
public:
    void prepare (double sampleRate) noexcept
    {
        phaser.prepare (sampleRate); flanger.prepare (sampleRate); ring.prepare (sampleRate);
        trem.prepare (sampleRate);   formant.prepare (sampleRate); delay.prepare (sampleRate);
    }

    void retrigger() noexcept
    {
        phaser.retrigger(); flanger.retrigger(); ring.retrigger();
        trem.retrigger();   formant.retrigger(); delay.retrigger();
    }

    void setPhaser  (bool on, float rate, float depth, float fb) noexcept { phaseOn = on;  phaser.setParams (rate, depth, fb); }
    void setFlanger (bool on, float rate, float depth, float fb) noexcept { flangeOn = on; flanger.setParams (rate, depth, fb); }
    void setRing    (bool on, float freq, float mix, int wave) noexcept   { ringOn = on;   ring.setWave (wave); ring.setParams (freq, mix); }
    void setTrem    (bool on, float rate, float depth, int wave) noexcept { tremOn = on;   trem.setWave (wave); trem.setParams (rate, depth); }
    void setFormant (bool on, float vowel, float reso, float mix) noexcept{ formOn = on;   formant.setParams (vowel, reso, mix); }
    void setDelay   (bool on, float timeMs, float fb, float mix) noexcept { delayOn = on;  delay.setParams (timeMs, fb, mix); }

    void process (float* left, float* right, int n) noexcept
    {
        if (phaseOn)  phaser.process  (left, right, n);
        if (flangeOn) flanger.process (left, right, n);
        if (ringOn)   ring.process    (left, right, n);
        if (tremOn)   trem.process    (left, right, n);
        if (formOn)   formant.process (left, right, n);
        if (delayOn)  delay.process   (left, right, n);
    }

private:
    Phaser  phaser;
    Flanger flanger;
    RingMod ring;
    Tremolo trem;
    Formant formant;
    Delay   delay;

    bool phaseOn = false, flangeOn = false, ringOn = false,
         tremOn = false, formOn = false, delayOn = false;
};
