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
#include <cstdint>
#include "Voice.h"
#include "Envelope.h"
#include "LFO.h"
#include "ModMatrix.h"
#include "../Params.h"

// ============================================================================
//  SynthEngine — composition root of the DSP.
//  Owns up to kMaxInstances "trigger instances"; each instance is one fired
//  sound: a stack of unison Voices sharing two envelopes and two LFOs.
//  Also runs the loop-trigger clock and generates per-trigger variate
//  offsets (ephemeral — never written back to the parameter tree).
//  All methods are called from the audio thread (or an offline renderer).
// ============================================================================

class SynthEngine
{
public:
    static constexpr int kMaxInstances = 4;
    static constexpr int kSubBlock = 16;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setPatch (const Params::Patch& p) noexcept { patch = p; }
    const Params::Patch& getPatch() const noexcept  { return patch; }

    // ---- triggering (audio thread) ----
    void noteOn (int midiNote);
    void noteOff (int midiNote);
    void manualGateOn();          // TRIGGER button pressed
    void manualGateOff();         // TRIGGER button released
    void oneShot();               // timed gate of patch.gateTime (loop/preview)

    // Overwrites the two channel buffers with the rendered output.
    void render (float* left, float* right, int numSamples);

    bool anyActive() const noexcept;

private:
    struct VariateOffsets
    {
        float pitchSemis = 0.0f;
        float cutoffOct  = 0.0f;
        float decayMul   = 1.0f;
        float pwm        = 0.0f;
    };

    struct Instance
    {
        bool active = false;
        int  note   = kNoteNone;        // >= 0 MIDI; see tags below
        uint64_t startClock = 0;
        int  gateRemaining = -1;        // samples until auto gate-off; -1 = held

        Envelope envF, envA;
        LFO lfo1, lfo2;
        std::array<Voice, Params::kMaxUnison> voices;
        int numVoices = 1;

        float freqOverrideHz = 0.0f;    // > 0 when MIDI pitch-track set it
        VariateOffsets var;

        void prepare (double fs);
        void start (const Params::Patch& p, int noteTag, float overrideHz,
                    int gateSamples, const VariateOffsets& v, uint32_t seed,
                    uint64_t clock);
        void gateOff();
        // renders one sub-block additively into the bus
        void renderAdd (float* left, float* right, int n,
                        const Params::Patch& p, float fs);
    };

    static constexpr int kNoteNone   = -1;
    static constexpr int kNoteManual = -2;
    static constexpr int kNoteTimed  = -3;

    Instance* findFreeInstance();
    void fire (int noteTag, float overrideHz, int gateSamples);
    VariateOffsets makeVariate();
    uint32_t nextRand() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return rng;
    }

    Params::Patch patch;
    std::array<Instance, kMaxInstances> instances;

    double fs = 44100.0;
    uint64_t clock = 0;

    // loop trigger scheduling
    bool prevLoopOn = false;
    uint64_t nextLoopTrigger = 0;

    // smoothed master gain
    float masterGain = 0.5f;
    float masterTarget = 0.5f;

    uint32_t rng = 0x5EEDF00Du;
};
