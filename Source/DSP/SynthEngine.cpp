/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "SynthEngine.h"
#include <algorithm>
#include <cmath>

namespace
{
    inline float clampf (float v, float lo, float hi) noexcept
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    inline float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db * 0.05f);
    }

    inline float midiToHz (int note) noexcept
    {
        return 440.0f * std::exp2 ((float) (note - 69) / 12.0f);
    }

    // transparent below the threshold, tanh knee above — safety only
    inline float softClip (float x) noexcept
    {
        constexpr float th = 0.85f;
        const float ax = std::fabs (x);
        if (ax <= th)
            return x;
        const float knee = th + FastMath::tanh ((ax - th) / (1.0f - th)) * (1.0f - th);
        return x < 0.0f ? -knee : knee;
    }
}

// ============================================================================
//  Instance
// ============================================================================

void SynthEngine::Instance::prepare (double sampleRate)
{
    fs = sampleRate;
    envF.prepare (sampleRate);
    envA.prepare (sampleRate);
    lfo1.prepare (sampleRate);
    stepLfo.prepare (sampleRate);
    fxChain.prepare (sampleRate);
    for (auto& v : voices)
        v.prepare (sampleRate);
}

void SynthEngine::Instance::start (const Params::Patch& p, int noteTag,
                                   float overrideHz, int gateSamples,
                                   const VariateOffsets& v, uint32_t seed,
                                   uint64_t clockNow)
{
    active = true;
    note = noteTag;
    startClock = clockNow;
    ageSamples = 0;
    gateRemaining = gateSamples;
    retrigCounter = p.retrigHz >= 0.25f ? (int) (fs / p.retrigHz) : 1000000000;

    // a quick TRIGGER click must still play at least one full waveform
    // cycle (30 ms floor so very short taps stay audible)
    const float baseHz = p.baseFreqHz > 20.0f ? p.baseFreqHz : 20.0f;
    minGateSamples = (int) std::max (fs / (double) baseHz + 1.0, 0.03 * fs);
    freqOverrideHz = overrideHz;
    var = v;

    numVoices = p.uniVoices < 1 ? 1
              : (p.uniVoices > Params::kMaxUnison ? Params::kMaxUnison : p.uniVoices);

    Oscillator::Wave waves[Params::kNumOscs];
    for (int j = 0; j < Params::kNumOscs; ++j)
        waves[j] = (Oscillator::Wave) (int) p.osc[(size_t) j].wave;

    uint32_t s = seed;
    for (int i = 0; i < numVoices; ++i)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        const float pos = numVoices == 1 ? 0.0f
                        : 2.0f * (float) i / (float) (numVoices - 1) - 1.0f;
        // voice 0 (and any single voice) starts exactly at the zero crossing;
        // unison voices fan out by a deterministic phase spread
        const float phaseOffset = numVoices == 1 ? 0.0f
                                : (float) i / (float) numVoices;
        voices[(size_t) i].start (pos * p.uniDetuneCents,
                                  pos * p.uniSpread, s, phaseOffset, waves);
    }

    lfo1.retrigger();
    stepLfo.retrigger();
    fxChain.retrigger();
    envF.gateOn();   // analog semantics: continues from current level on steal
    envA.gateOn();
}

void SynthEngine::Instance::gateOff()
{
    envF.gateOff();
    envA.gateOff();
    gateRemaining = -1;
}

void SynthEngine::Instance::renderAdd (float* left, float* right, int n,
                                       const Params::Patch& p, float fsf)
{
    // live-follow the panel: envelope + LFO settings refresh every sub-block
    envF.setParams (p.envF.attack, p.envF.decay * var.decayMul, p.envF.sustain,
                    p.envF.release, p.envF.curve, p.envF.invert);
    envA.setParams (p.envA.attack, p.envA.decay * var.decayMul, p.envA.sustain,
                    p.envA.release, p.envA.curve, p.envA.invert);

    // matrix sources sampled at sub-block start ("CV" snapshot)
    const ModValues mv = ModMatrix::compute (p, lfo1.value(), stepLfo.value(),
                                             envF.value(), envA.value());

    lfo1.setWave ((LFO::Wave) (int) p.lfo[0].wave);
    lfo1.setRate (p.lfo[0].rateHz * std::exp2 (mv.lfoRateOct[0]));
    lfo1.setDelay (p.lfo[0].delaySec);

    stepLfo.setRate (p.lfo[1].rateHz * std::exp2 (mv.lfoRateOct[1]));
    stepLfo.setDelay (p.lfo[1].delaySec);
    stepLfo.setSteps (p.stepCount);
    stepLfo.setGlide (p.stepGlide);
    stepLfo.setSkew (p.stepSkew);
    for (int k = 0; k < Params::kMaxSteps; ++k)
        stepLfo.setStepValue (k, p.stepVals[(size_t) k]);

    // per-sample envelopes + LFOs; build the VCA buffer
    float amp[kSubBlock];
    const float vcaGain = clampf (1.0f + mv.vca, 0.0f, 2.0f);
    const float norm = 1.0f / std::sqrt ((float) numVoices);
    for (int s = 0; s < n; ++s)
    {
        lfo1.tick();
        stepLfo.tick();
        envF.tick();
        // squared: linear fader motion maps to perceived loudness
        const float a = envA.tick();
        amp[s] = a * a * vcaGain * norm;
    }

    // auto gate-off for timed (loop / one-shot) triggers
    if (gateRemaining >= 0)
    {
        gateRemaining -= n;
        if (gateRemaining < 0)
            gateOff();
    }

    // retrigger: re-strike the sound at the retrigger rate while it is gated
    // (a stutter/arpeggio texture; the release tail still plays out at the end)
    if (p.retrigHz >= 0.25f && ! envA.isReleasing())
    {
        retrigCounter -= n;
        if (retrigCounter <= 0)
        {
            envF.retrigger();
            envA.retrigger();
            for (int i = 0; i < numVoices; ++i)
                voices[(size_t) i].retrigger();
            ageSamples = 0;
            retrigCounter += (int) (fsf / p.retrigHz);
            if (retrigCounter <= 0)
                retrigCounter = (int) (fsf / p.retrigHz);
        }
    }

    const float base = freqOverrideHz > 0.0f ? freqOverrideHz : p.baseFreqHz;

    // discrete pitch jumps (sfxr-style arpeggio steps)
    const float ageSec = (float) ageSamples / fsf;
    float jumpSemis = 0.0f;
    if (p.pj1AmtSemis != 0.0f && ageSec >= p.pj1TimeSec) jumpSemis += p.pj1AmtSemis;
    if (p.pj2AmtSemis != 0.0f && ageSec >= p.pj2TimeSec) jumpSemis += p.pj2AmtSemis;
    ageSamples += (uint64_t) n;

    Voice::SubBlockCtx ctx;
    for (int i = 0; i < Params::kNumOscs; ++i)
    {
        const auto& o = p.osc[(size_t) i];
        ctx.oscOn[i]   = o.on;
        ctx.oscWave[i] = (Oscillator::Wave) (int) o.wave;
        const float semis = o.pitchSemis + o.fineCents * 0.01f
                          + mv.allPitchSemis + mv.oscPitchSemis[i]
                          + var.pitchSemis + jumpSemis;
        ctx.oscFreqHz[i] = clampf (base * std::exp2 (semis / 12.0f),
                                   0.05f, fsf * 0.45f);
        ctx.oscPwm[i]   = clampf (o.pwm + mv.pwm + var.pwm, 0.05f, 0.95f);
        ctx.oscFold[i]  = clampf (o.fold + mv.fold, 0.0f, 1.0f);
        ctx.oscLevel[i] = o.level;
    }

    for (int i = 0; i < Params::kNumOscs; ++i)
        ctx.oscSync[i] = p.osc[(size_t) i].sync;
    ctx.noiseOn    = p.noiseOn;
    ctx.noiseType  = p.noiseType;
    ctx.noiseLevel = clampf (p.noiseLevel + mv.noiseLevel, 0.0f, 1.0f);

    const float envFv = envF.value();
    ctx.cutoffHz = clampf (p.lpfCutoff
                       * std::exp2 (mv.cutoffOct
                                    + envFv * p.lpfEnvAmt * ModMatrix::kCutoffRangeOct
                                    + var.cutoffOct),
                       20.0f, fsf * 0.45f);
    ctx.res01    = clampf (p.lpfRes + mv.resonance, 0.0f, 1.0f);
    ctx.fourPole = p.lpf4Pole;
    ctx.hpfOn    = p.hpfOn;
    ctx.hpfHz    = p.hpfCutoff;
    ctx.driveAmt = p.vcaDrive;
    ctx.amp      = amp;

    // FX params modulated by the matrix (Form Vowel / Ring Freq / Trem Depth /
    // Delay Time) -- e.g. Step LFO -> Form Vowel makes the sound "talk".
    const float ringFreqM  = clampf (p.ringFreq  * std::exp2 (mv.ringFreqOct),  20.0f, 4000.0f);
    const float tremDepthM = clampf (p.tremDepth + mv.tremDepth,                0.0f,  1.0f);
    const float formVowelM = clampf (p.formVowel + mv.formVowel,                0.0f,  1.0f);
    const float delayTimeM = clampf (p.delayTime * std::exp2 (mv.delayTimeOct), 0.001f, 0.4f);

    // when split, the mono subgroup runs per-voice before the filter. Build its
    // config (params already modulated) and the pre sub-order from fxOrder.
    PreFx::Config pre;
    pre.on      = p.fxSplit;
    pre.crushOn = p.crushOn; pre.crushBits  = p.crushBits;  pre.crushDown = p.crushDown;
    pre.ringOn  = p.ringOn;  pre.ringFreq   = ringFreqM;     pre.ringMix   = p.ringMix;   pre.ringWave = p.ringWave;
    pre.tremOn  = p.tremOn;  pre.tremRate   = p.tremRate;    pre.tremDepth = tremDepthM;  pre.tremWave = p.tremWave;
    pre.phaseOn = p.phaseOn; pre.phaseRate  = p.phaseRate;   pre.phaseDepth = p.phaseDepth; pre.phaseFb = p.phaseFb;
    pre.formOn  = p.formOn;  pre.formVowel  = formVowelM;    pre.formReso  = p.formReso;  pre.formMix  = p.formMix;
    {
        int w = 0;
        for (int i = 0; i < Params::kFxChainLen; ++i)
            switch (p.fxOrder[(size_t) i])     // FxChain::Effect -> PreFx::Effect
            {
                case 0: pre.order[w++] = 0; break;   // Crush
                case 3: pre.order[w++] = 1; break;   // RingMod
                case 4: pre.order[w++] = 2; break;   // Tremolo
                case 1: pre.order[w++] = 3; break;   // Phaser
                case 5: pre.order[w++] = 4; break;   // Formant
                default: break;                      // Flanger(2)/Delay(6): post-only
            }
    }
    ctx.preFx = &pre;
    fxChain.setSplit (p.fxSplit);

    // voices render into a local scratch so the per-instance FX get applied
    // to this sound only (not to other overlapping triggers)
    float scratchL[kSubBlock] {};
    float scratchR[kSubBlock] {};

    for (int i = 0; i < numVoices; ++i)
    {
        voices[(size_t) i].setNoiseColor (p.noiseColor);
        voices[(size_t) i].renderAdd (scratchL, scratchR, n, ctx);
    }

    // post-VCA effect chain, processed in the user's order. When split, the
    // mono effects above were already applied per-voice, so setSplit() makes
    // these calls run only Flanger/Delay here.
    fxChain.setOrder   (p.fxOrder.data());
    fxChain.setCrush   (p.crushOn,  p.crushBits,  p.crushDown);
    fxChain.setPhaser  (p.phaseOn,  p.phaseRate,  p.phaseDepth,  p.phaseFb);
    fxChain.setFlanger (p.flangeOn, p.flangeRate, p.flangeDepth, p.flangeFb);
    fxChain.setRing    (p.ringOn,   ringFreqM,    p.ringMix,     p.ringWave);
    fxChain.setTrem    (p.tremOn,   p.tremRate,   tremDepthM,    p.tremWave);
    fxChain.setFormant (p.formOn,   formVowelM,   p.formReso,    p.formMix);
    fxChain.setDelay   (p.delayOn,  delayTimeM * 1000.0f, p.delayFb, p.delayMix);
    fxChain.process (scratchL, scratchR, n);

    for (int s = 0; s < n; ++s)
    {
        left[s]  += scratchL[s];
        right[s] += scratchR[s];
    }

    if (! envA.isActive())
        active = false;
}

// ============================================================================
//  SynthEngine
// ============================================================================

void SynthEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    fs = sampleRate;
    lofi.prepare ((float) sampleRate);
    for (auto& inst : instances)
        inst.prepare (sampleRate);
    reset();
}

void SynthEngine::reset()
{
    for (auto& inst : instances)
    {
        inst.active = false;
        inst.note = kNoteNone;
        inst.envF.reset();
        inst.envA.reset();
        for (auto& v : inst.voices)
            v.hardReset();
    }
    clock = 0;
    prevLoopOn = false;
    nextLoopTrigger = 0;
    lofi.reset();
    masterGain = masterTarget = dbToGain (patch.masterVolDb);
}

SynthEngine::Instance* SynthEngine::findFreeInstance()
{
    for (auto& inst : instances)
        if (! inst.active)
            return &inst;

    Instance* oldest = &instances[0];
    for (auto& inst : instances)
        if (inst.startClock < oldest->startClock)
            oldest = &inst;
    return oldest;
}

void SynthEngine::fire (int noteTag, float overrideHz, int gateSamples)
{
    findFreeInstance()->start (patch, noteTag, overrideHz, gateSamples,
                               makeVariate(), nextRand(), clock);
}

SynthEngine::VariateOffsets SynthEngine::makeVariate()
{
    VariateOffsets v;
    if (! patch.autoVarOn)
        return v;

    auto bip = [this] { return (float) (nextRand() >> 8) * (2.0f / 16777216.0f) - 1.0f; };
    const float amt = patch.autoVarAmt;
    v.pitchSemis = bip() * amt * 7.0f;
    v.cutoffOct  = bip() * amt * 1.5f;
    v.decayMul   = std::exp2 (bip() * amt * 0.8f);
    v.pwm        = bip() * amt * 0.2f;
    return v;
}

void SynthEngine::noteOn (int midiNote)
{
    const float hz = patch.midiTrack ? midiToHz (midiNote) : 0.0f;
    fire (midiNote, hz, -1);
}

void SynthEngine::noteOff (int midiNote)
{
    for (auto& inst : instances)
        if (inst.active && inst.note == midiNote && inst.gateRemaining < 0)
            inst.gateOff();
}

void SynthEngine::manualGateOn()
{
    fire (kNoteManual, 0.0f, -1);
}

void SynthEngine::manualGateOff()
{
    for (auto& inst : instances)
    {
        if (! inst.active || inst.note != kNoteManual)
            continue;

        if (inst.ageSamples >= (uint64_t) inst.minGateSamples)
            inst.gateOff();
        else if (inst.gateRemaining < 0)   // too quick: defer via the timed path
            inst.gateRemaining = inst.minGateSamples - (int) inst.ageSamples;
    }
}

void SynthEngine::oneShot()
{
    const int gateSamples = (int) (patch.gateTime * fs);
    fire (kNoteTimed, 0.0f, gateSamples > 1 ? gateSamples : 1);
}

bool SynthEngine::anyActive() const noexcept
{
    for (const auto& inst : instances)
        if (inst.active)
            return true;
    return false;
}

void SynthEngine::render (float* left, float* right, int numSamples)
{
    masterTarget = dbToGain (patch.masterVolDb);

    // loop edge: first trigger fires immediately on enable
    if (patch.loopOn && ! prevLoopOn)
        nextLoopTrigger = clock;
    prevLoopOn = patch.loopOn;

    int pos = 0;
    while (pos < numSamples)
    {
        if (patch.loopOn && clock >= nextLoopTrigger)
        {
            oneShot();
            auto interval = (uint64_t) (patch.loopRate * fs);
            if (interval < (uint64_t) (0.05 * fs))
                interval = (uint64_t) (0.05 * fs);
            nextLoopTrigger = clock + interval;
        }

        const int n = numSamples - pos < kSubBlock ? numSamples - pos : kSubBlock;
        float* l = left + pos;
        float* r = right + pos;

        for (int s = 0; s < n; ++s)
            l[s] = r[s] = 0.0f;

        for (auto& inst : instances)
            if (inst.active)
                inst.renderAdd (l, r, n, patch, (float) fs);

        // last-resort NaN/inf guard: if anything blew up, silence the
        // sub-block and hard-reset every voice path
        if (! std::isfinite (l[0] + r[0] + l[n - 1] + r[n - 1]))
        {
            for (auto& inst : instances)
            {
                inst.active = false;
                inst.envF.reset();
                inst.envA.reset();
                for (auto& v : inst.voices)
                    v.hardReset();
            }
            for (int s = 0; s < n; ++s)
                l[s] = r[s] = 0.0f;
        }

        // master output stage: gain -> compression -> safety clip -> lo-fi
        //   compression: bfxr-style power-law density/punch (boosts quiet parts)
        //   lo-fi:       anti-aliased rate reduction + bit-depth quantization
        const float compExp = 1.0f - 0.7f * clampf (patch.compAmount, 0.0f, 1.0f);
        const bool  doComp  = patch.compAmount > 0.001f;

        lofi.setRate (patch.outRateHz);
        lofi.setBits8 (patch.out8bit);

        for (int s = 0; s < n; ++s)
        {
            masterGain += 0.005f * (masterTarget - masterGain);
            float lv = softClip (l[s] * masterGain);
            float rv = softClip (r[s] * masterGain);

            if (doComp)
            {
                lv = (lv < 0.0f ? -1.0f : 1.0f) * std::pow (std::fabs (lv), compExp);
                rv = (rv < 0.0f ? -1.0f : 1.0f) * std::pow (std::fabs (rv), compExp);
                lv = clampf (lv, -1.0f, 1.0f);
                rv = clampf (rv, -1.0f, 1.0f);
            }

            lofi.process (lv, rv);

            l[s] = lv;
            r[s] = rv;
        }

        pos += n;
        clock += (uint64_t) n;
    }
}
