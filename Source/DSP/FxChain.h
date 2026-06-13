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

#include <array>
#include "Bitcrusher.h"
#include "Phaser.h"
#include "Flanger.h"
#include "RingMod.h"
#include "Tremolo.h"
#include "AutoWah.h"
#include "Delay.h"

// ============================================================================
//  FxChain — an ordered stack of kSlots effect slots that runs after the voice
//  sum, per trigger-instance (retriggered with the sound). Each slot holds one
//  effect type (or Off) plus three generic params A/B/C whose meaning depends
//  on the type (see configure()). Slots process top-to-bottom; the UI reorders
//  the chain simply by reordering each slot's config. Per-slot state means you
//  can run, say, two Delays with different settings.
//
//  Knob map (A, B, C in 0..1 unless noted):
//    Crush   : Bits(2..16)      Downsample(1..32)   Mix
//    Phaser  : Rate(0.05..8Hz)  Depth               Feedback
//    Flanger : Rate(0.05..8Hz)  Depth               Feedback
//    RingMod : Freq(20..4000Hz) Mix                 (unused)
//    Tremolo : Rate(0.1..30Hz)  Depth               Shape(sine..square)
//    Wah     : Freq(100..1500)  Reso                Sense
//    Delay   : Time(1..400ms)   Feedback            Mix
// ============================================================================

class FxChain
{
public:
    static constexpr int kSlots = 6;

    enum class Type
    {
        Off = 0, Crush, Phaser, Flanger, RingMod, Tremolo, Wah, Delay, kNumTypes
    };

    struct SlotConfig
    {
        int   type = 0;            // cast to Type
        float a = 0.5f, b = 0.5f, c = 0.5f;
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        for (auto& s : slots) s.prepare (sampleRate);
    }

    void retrigger() noexcept
    {
        for (auto& s : slots) s.retrigger();
    }

    bool anyActive() const noexcept
    {
        for (const auto& s : slots)
            if (s.type != Type::Off) return true;
        return false;
    }

    // slotIdx is the processing position (0 = first). The UI supplies configs
    // already in chain order, so reordering is just a reassignment of configs.
    void setConfig (int slotIdx, const SlotConfig& cfg) noexcept
    {
        if (slotIdx < 0 || slotIdx >= kSlots) return;
        slots[(size_t) slotIdx].configure ((Type) cfg.type, cfg.a, cfg.b, cfg.c);
    }

    void process (float* left, float* right, int n) noexcept
    {
        for (auto& s : slots) s.process (left, right, n);
    }

private:
    struct Slot
    {
        Type type = Type::Off;
        float crushMix = 1.0f;

        Bitcrusher crushL, crushR;
        Phaser   phaser;
        Flanger  flanger;
        RingMod  ring;
        Tremolo  trem;
        AutoWah  wah;
        Delay    delay;

        void prepare (double sr) noexcept
        {
            crushL.reset(); crushR.reset();
            phaser.prepare (sr); flanger.prepare (sr); ring.prepare (sr);
            trem.prepare (sr);   wah.prepare (sr);     delay.prepare (sr);
        }

        void retrigger() noexcept
        {
            crushL.reset(); crushR.reset();
            phaser.retrigger(); flanger.retrigger(); ring.retrigger();
            trem.retrigger();   wah.retrigger();     delay.retrigger();
        }

        void configure (Type t, float a, float b, float c) noexcept
        {
            type = t;
            switch (t)
            {
                case Type::Crush:
                    crushL.setParams (2.0f + a * 14.0f, 1.0f + b * 31.0f);
                    crushR.setParams (2.0f + a * 14.0f, 1.0f + b * 31.0f);
                    crushMix = c;
                    break;
                case Type::Phaser:  phaser.setParams  (0.05f + a * 7.95f, b, c); break;
                case Type::Flanger: flanger.setParams (0.05f + a * 7.95f, b, c); break;
                case Type::RingMod: ring.setParams    (20.0f + a * 3980.0f, b);  break;
                case Type::Tremolo: trem.setParams    (0.1f + a * 29.9f, b, c);  break;
                case Type::Wah:     wah.setParams     (100.0f + a * 1400.0f, b, c); break;
                case Type::Delay:   delay.setParams   (1.0f + a * 399.0f, b, c); break;
                default: break;
            }
        }

        void process (float* l, float* r, int n) noexcept
        {
            switch (type)
            {
                case Type::Crush:
                    for (int s = 0; s < n; ++s)
                    {
                        const float cl = crushL.tick (l[s]);
                        const float cr = crushR.tick (r[s]);
                        l[s] += crushMix * (cl - l[s]);
                        r[s] += crushMix * (cr - r[s]);
                    }
                    break;
                case Type::Phaser:  phaser.process  (l, r, n); break;
                case Type::Flanger: flanger.process (l, r, n); break;
                case Type::RingMod: ring.process    (l, r, n); break;
                case Type::Tremolo: trem.process    (l, r, n); break;
                case Type::Wah:     wah.process      (l, r, n); break;
                case Type::Delay:   delay.process   (l, r, n); break;
                default: break;   // Off: passthrough
            }
        }
    };

    std::array<Slot, kSlots> slots;
    float fs = 44100.0f;
};
