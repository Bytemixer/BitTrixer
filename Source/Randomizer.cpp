/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "Randomizer.h"
#include <vector>

using namespace Params;

namespace
{
    // parameters the randomizer must never touch (gate time is NOT protected:
    // it is part of a sound's design, so recipes set it per category)
    bool isProtected (const juce::String& pid)
    {
        // output format + performance controls stay where the user set them
        return pid == id::masterVol || pid == id::loopOn || pid == id::loopRate
            || pid == id::autoVarOn || pid == id::autoVarAmt
            || pid == id::midiTrack  || pid == id::retrigRate
            || pid == id::outRate    || pid == id::outBits;
    }
}

// ----------------------------------------------------------------------------
//  plumbing
// ----------------------------------------------------------------------------

void Randomizer::snapshotForUndo()
{
    undoState = apvts.copyState().createCopy();
    hasUndo = true;
}

void Randomizer::undo()
{
    if (! hasUndo)
        return;
    apvts.replaceState (undoState.createCopy());
    hasUndo = false;
}

void Randomizer::set (const juce::String& paramId, float realValue)
{
    if (auto* p = apvts.getParameter (paramId))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    else
        jassertfalse;
}

void Randomizer::setChoice (const juce::String& paramId, int index)
{
    set (paramId, (float) index);
}

void Randomizer::setBool (const juce::String& paramId, bool on)
{
    set (paramId, on ? 1.0f : 0.0f);
}

void Randomizer::resetToNeutral()
{
    // walk every parameter and restore its default, except protected ones
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isProtected (rp->paramID))
                rp->setValueNotifyingHost (rp->getDefaultValue());
}

// ----------------------------------------------------------------------------
//  FM voice presets — the 4-op engine in OSC 3's slot. FM excels at bells,
//  chimes and metallic/inharmonic timbres, so the recipes layer it in for those
//  flavours. Movement comes from the mod matrix (no per-operator envelopes).
// ----------------------------------------------------------------------------

void Randomizer::setFmVoice (int algo, float feedback, const float* ratios,
                             const float* levels, float outLevel)
{
    setBool (oscId (3, "on"), true);        // enable the FM voice (3rd slot)
    set (oscId (3, "pitch"), 0.0f);
    set (oscId (3, "level"), outLevel);     // FM output into the mix
    setChoice (id::fmAlgo, algo);
    set (id::fmFeedback, feedback);
    for (int i = 0; i < 4; ++i)
    {
        set (fmOpId (i + 1, "ratio"), ratios[i]);
        set (fmOpId (i + 1, "level"), levels[i]);
    }
}

void Randomizer::fmChime (int modSlot)
{
    // bell / coin / chime: serial or stacked cascade with a high partial; the
    // amp env lifts the top operator so it is bright on attack and purifies as
    // it rings out -- the classic FM bell decay
    static const float bell[4][4] = {
        { 3.5f, 1.0f, 2.0f, 1.0f }, { 7.0f, 1.0f, 3.5f, 1.0f },
        { 2.0f, 3.0f, 1.0f, 1.0f }, { 1.5f, 3.0f, 1.0f, 1.0f } };
    const float* r = bell[rndInt (0, 3)];
    const float levels[4] = { 0.25f, rnd (0.3f, 0.5f), rnd (0.35f, 0.55f), rnd (0.65f, 0.85f) };
    setFmVoice (chance (0.5f) ? 0 : 1, rnd (0.0f, 0.15f), r, levels, rnd (0.5f, 0.8f));
    setChoice (modId (modSlot, "src"),  (int) ModSrc::AmpEnv);
    setChoice (modId (modSlot, "dest"), (int) ModDest::FmOp1Level);
    set (modId (modSlot, "depth"), rnd (0.4f, 0.7f));     // bright attack -> pure ring
}

void Randomizer::fmClang()
{
    // metallic / inharmonic clang: branched or dual modulators at fractional
    // ratios with operator-1 feedback for bite
    static const float clang[4][4] = {
        { 1.5f, 3.5f, 5.5f, 1.0f }, { 2.5f, 1.5f, 1.0f, 1.0f },
        { 7.0f, 5.0f, 3.0f, 1.0f }, { 1.5f, 1.0f, 2.5f, 1.0f } };
    const float* r = clang[rndInt (0, 3)];
    const float levels[4] = { rnd (0.5f, 0.8f), rnd (0.4f, 0.7f), rnd (0.4f, 0.7f), rnd (0.7f, 0.9f) };
    static const int algos[4] = { 2, 3, 5, 8 };          // Dual Mod A/B, Branch, Triple
    setFmVoice (algos[rndInt (0, 3)], rnd (0.25f, 0.6f), r, levels, rnd (0.5f, 0.8f));
}

void Randomizer::fmNoise()
{
    // crank operator-1 feedback on a serial stack: the sine collapses into a
    // digital noise source, ideal for explosions through the VCF
    const float ratios[4] = { 1.0f, 1.5f, 1.0f, 1.0f };
    const float levels[4] = { 0.8f, 0.6f, 0.5f, 0.85f };
    setFmVoice (0, rnd (0.85f, 1.0f), ratios, levels, rnd (0.4f, 0.7f));
}

void Randomizer::setFxOrder (const int* order)
{
    for (int k = 0; k < kFxChainLen; ++k)
        set (fxOrderId (k), (float) order[k]);
}

void Randomizer::shuffleFxOrder()
{
    int order[kFxChainLen];
    for (int k = 0; k < kFxChainLen; ++k) order[k] = k;
    for (int k = kFxChainLen - 1; k > 0; --k)        // Fisher-Yates
    {
        const int j = rndInt (0, k);
        const int tmp = order[k]; order[k] = order[j]; order[j] = tmp;
    }
    setFxOrder (order);
}

void Randomizer::maybeFold (float prob)
{
    // a touch of wavefolder adds harmonic grit; OSC 2 only if it's enabled
    if (chance (prob)) set (oscId (1, "fold"), rnd (0.05f, 0.5f));
    if (apvts.getRawParameterValue (oscId (2, "on"))->load() > 0.5f && chance (prob))
        set (oscId (2, "fold"), rnd (0.05f, 0.5f));
}

void Randomizer::maybeNoise (float prob)
{
    // any category may occasionally mix in a little noise for a harsher tone --
    // but don't disturb a category that already built its own (Explosion/Hit/...)
    if (apvts.getRawParameterValue (id::noiseOn)->load() > 0.5f)
        return;
    if (! chance (prob))
        return;
    setBool (id::noiseOn, true);
    setChoice (id::noiseType, rndInt (0, 3));
    set (id::noiseColor, rnd (0.0f, 1.0f));
    set (id::noiseLevel, rnd (0.1f, 0.4f));   // light: blends under the main tone
}

void Randomizer::addModSpice (int slot)
{
    // one tasteful extra modulation: a varied source into an expressive
    // destination (timbre/filter/FX rather than just pitch)
    static const int srcs[]  = { (int) ModSrc::Lfo1, (int) ModSrc::Lfo2,
                                 (int) ModSrc::FilterEnv, (int) ModSrc::AmpEnv };
    static const int dests[] = { (int) ModDest::Cutoff,    (int) ModDest::Resonance,
                                 (int) ModDest::Pwm,       (int) ModDest::Fold,
                                 (int) ModDest::Osc1Pwm,   (int) ModDest::Osc2Pitch,
                                 (int) ModDest::FmOp2Level, (int) ModDest::RingFreq,
                                 (int) ModDest::TremDepth, (int) ModDest::FormVowel };
    setChoice (modId (slot, "src"),  srcs [rndInt (0, (int) (sizeof (srcs)  / sizeof (srcs[0]))  - 1)]);
    setChoice (modId (slot, "dest"), dests[rndInt (0, (int) (sizeof (dests) / sizeof (dests[0])) - 1)]);
    set (modId (slot, "depth"), rnd (-0.5f, 0.5f));
}

// ----------------------------------------------------------------------------
//  variate — perturb the current patch (editable, undo-able variation)
// ----------------------------------------------------------------------------

std::vector<Randomizer::Nudge> Randomizer::perturbTargets (bool includeLoudnessAndLength) const
{
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

void Randomizer::refreshAnchorIfNeeded (const std::vector<Nudge>& targets)
{
    if (! anchorDirty.exchange (false) && ! anchor.empty())
        return;

    anchor.clear();
    for (const auto& t : targets)
        if (auto* p = apvts.getParameter (t.pid))
            anchor[t.pid] = p->getValue();
}

void Randomizer::applyNudges (const std::vector<Nudge>& targets, float baseScale,
                              bool fromAnchor)
{
    const float amt = apvts.getRawParameterValue (id::autoVarAmt)->load();
    const juce::ScopedValueSetter<bool> guard (selfChanging, true);

    for (const auto& t : targets)
    {
        if (auto* p = apvts.getParameter (t.pid))
        {
            float base = p->getValue();                  // normalized 0..1
            if (fromAnchor)
            {
                const auto it = anchor.find (t.pid);
                if (it != anchor.end())
                    base = it->second;
            }

            // triangular distribution: extremes are rare, centre is likely
            const float r = 0.5f * ((random.nextFloat() * 2.0f - 1.0f)
                                  + (random.nextFloat() * 2.0f - 1.0f));
            const float nv = juce::jlimit (0.0f, 1.0f,
                                           base + r * amt * baseScale * t.scale);
            p->setValueNotifyingHost (nv);
        }
    }
}

void Randomizer::variate()
{
    // anchored: every press is a fresh sibling of the SAME sound, so it can
    // never random-walk out of its family
    const auto targets = perturbTargets (true);
    refreshAnchorIfNeeded (targets);
    snapshotForUndo();
    applyNudges (targets, 0.12f, true);
    anchorDirty.store (false);   // our own writes must not invalidate the anchor
}

void Randomizer::mutate()
{
    // compounding explorer: walks from wherever the sound currently is
    snapshotForUndo();
    applyNudges (perturbTargets (true), 0.4f, false);
    anchorDirty.store (true);    // mutate moves the family itself
}

// ----------------------------------------------------------------------------
//  full random
// ----------------------------------------------------------------------------

void Randomizer::fullRandom()
{
    snapshotForUndo();
    resetToNeutral();

    // oscillators (OSC 3's slot is the FM voice, handled separately below)
    bool anySource = false;
    const float oscProb[2] = { 0.85f, 0.45f };
    for (int i = 1; i <= kNumOscs - 1; ++i)
    {
        const bool on = chance (oscProb[i - 1]);
        anySource |= on;
        setBool (oscId (i, "on"), on);
        setChoice (oscId (i, "wave"), rndInt (0, 7));
        set (oscId (i, "pitch"), chance (0.4f) ? (float) rndInt (-12, 12) : 0.0f);
        set (oscId (i, "fine"),  rnd (-20.0f, 20.0f));
        set (oscId (i, "pwm"),   rnd (10.0f, 90.0f));
        set (oscId (i, "fold"),  chance (0.3f) ? rnd (0.0f, 0.8f) : 0.0f);
        set (oscId (i, "level"), rnd (0.5f, 1.0f));
    }

    const bool noise = chance (0.35f) || ! anySource;
    setBool (id::noiseOn, noise);
    setChoice (id::noiseType, rndInt (0, 3));
    set (id::noiseColor, rnd (0.0f, 1.0f));
    set (id::noiseLevel, rnd (0.3f, 1.0f));

    set (id::baseFreq, rndLog (60.0f, 2500.0f));

    // LFO 1 (classic)
    setChoice (lfoId (1, "wave"), rndInt (0, 6));
    set (lfoId (1, "rate"), rndLog (0.2f, 30.0f));
    set (lfoId (1, "delay"), chance (0.25f) ? rnd (0.0f, 0.5f) : 0.0f);

    // LFO 2 = the Step LFO: a fresh random sequence
    set (lfoId (2, "rate"), rndLog (2.0f, 24.0f));     // step advance rate
    set (id::stepCount, (float) rndInt (3, 8));
    set (id::stepGlide, chance (0.3f) ? rnd (0.2f, 0.9f) : 0.0f);
    set (id::stepSkew,  chance (0.35f) ? rnd (-0.7f, 0.7f) : 0.0f);
    for (int k = 1; k <= kMaxSteps; ++k)
        set (stepValId (k), rnd (-1.0f, 1.0f));

    // mod matrix: slot 1 is the classic pitch sweep most of the time
    if (chance (0.75f))
    {
        setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
        setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
        set (modId (1, "depth"), rnd (-0.6f, 0.6f));
    }
    const int extraRoutes = rndInt (0, 2);
    for (int k = 2; k <= 2 + extraRoutes && k <= kNumModSlots; ++k)
    {
        setChoice (modId (k, "src"), rndInt (1, 4));
        setChoice (modId (k, "dest"), rndInt (1, (int) ModDest::Count - 1));
        set (modId (k, "depth"), rnd (-0.5f, 0.5f));
    }

    // filter
    set (id::lpfCutoff, rndLog (300.0f, 16000.0f));
    set (id::lpfRes, rnd (0.0f, 0.75f));
    setChoice (id::lpfPoles, rndInt (0, 1));
    set (id::lpfEnv, chance (0.5f) ? rnd (-0.7f, 0.7f) : 0.0f);
    if (chance (0.25f))
    {
        setBool (id::hpfOn, true);
        set (id::hpfCutoff, rndLog (40.0f, 800.0f));
    }

    // envelopes — one-shot shapes, sustain biased low
    set (id::envFAttack, chance (0.3f) ? rndLog (0.001f, 0.4f) : 0.0001f);
    set (id::envFDecay,  rndLog (0.05f, 1.5f));
    set (id::envFSustain, chance (0.4f) ? rnd (0.0f, 1.0f) : 0.0f);
    set (id::envFRelease, rndLog (0.02f, 0.6f));
    set (id::envFCurve, rnd (-1.0f, 1.0f));
    setBool (id::envFInvert, chance (0.15f));

    set (id::envAAttack, chance (0.2f) ? rndLog (0.001f, 0.15f) : 0.0001f);
    set (id::envADecay,  rndLog (0.08f, 1.2f));
    set (id::envASustain, chance (0.3f) ? rnd (0.1f, 0.7f) : 0.0f);
    set (id::envARelease, rndLog (0.02f, 0.5f));
    set (id::envACurve, rnd (-1.0f, 0.5f));

    set (id::vcaDrive, chance (0.4f) ? rnd (0.1f, 0.7f) : 0.0f);
    set (id::comp, chance (0.35f) ? rnd (0.2f, 0.7f) : 0.0f);   // density/punch

    set (id::gateTime, rndLog (0.1f, 0.8f));
    setBool (oscId (2, "sync"), chance (0.18f));

    if (chance (0.3f))                                     // arpeggio-style jumps
    {
        set (id::pj1Amt, (float) rndInt (-7, 12));
        set (id::pj1Time, rnd (0.05f, 0.25f));
        if (chance (0.5f))
        {
            set (id::pj2Amt, (float) rndInt (-9, 12));
            set (id::pj2Time, rnd (0.2f, 0.5f));
        }
    }

    // integrated FX
    if (chance (0.3f))
    {
        setBool (id::crushOn, true);
        set (id::crushBits, rnd (4.0f, 12.0f));
        set (id::crushDown, rndLog (1.0f, 20.0f));
    }
    if (chance (0.12f))
    {
        setBool (id::phaseOn, true);
        set (id::phaseRate, rndLog (0.2f, 5.0f));
        set (id::phaseDepth, rnd (0.3f, 0.9f));
        set (id::phaseFb, rnd (0.1f, 0.7f));
    }
    if (chance (0.18f))
    {
        setBool (id::flangeOn, true);
        set (id::flangeRate, rndLog (0.1f, 2.0f));
        set (id::flangeDepth, rnd (0.3f, 0.9f));
        set (id::flangeFb, rnd (0.2f, 0.8f));
    }
    if (chance (0.12f))                                   // ring mod (metallic/robotic)
    {
        setBool (id::ringOn, true);
        set (id::ringFreq, rndLog (40.0f, 1200.0f));
        set (id::ringMix, rnd (0.4f, 1.0f));
        setChoice (id::ringWave, rndInt (0, 3));
    }
    if (chance (0.12f))                                   // tremolo
    {
        setBool (id::tremOn, true);
        set (id::tremRate, rndLog (3.0f, 30.0f));
        set (id::tremDepth, rnd (0.3f, 0.9f));
        setChoice (id::tremWave, rndInt (0, 3));
    }
    if (chance (0.1f))                                    // formant (vocal)
    {
        setBool (id::formOn, true);
        set (id::formVowel, rnd (0.0f, 1.0f));
        set (id::formReso, rnd (0.3f, 0.7f));
        set (id::formMix, rnd (0.5f, 1.0f));
    }
    if (chance (0.15f))                                   // delay
    {
        setBool (id::delayOn, true);
        set (id::delayTime, rndLog (0.03f, 0.35f));
        set (id::delayFb, rnd (0.2f, 0.6f));
        set (id::delayMix, rnd (0.2f, 0.5f));
    }

    // FM voice (4-op): occasionally layer a random algorithm into OSC 3's slot
    if (chance (0.3f))
    {
        setBool (oscId (3, "on"), true);
        setChoice (id::fmAlgo, rndInt (0, 11));
        set (id::fmFeedback, chance (0.4f) ? rnd (0.2f, 0.9f) : rnd (0.0f, 0.2f));
        for (int i = 1; i <= 4; ++i)
        {
            const float ratio = chance (0.6f) ? (float) rndInt (1, 8)
                                              : (float) rndInt (2, 14) * 0.5f;
            set (fmOpId (i, "ratio"), ratio);
            set (fmOpId (i, "level"), rnd (0.3f, 0.9f));
        }
    }

    // signal-path topology: occasionally push some mono effects pre-filter,
    // and/or reorder the effects chain
    if (chance (0.25f))
    {
        setBool (id::crushPre, chance (0.5f));
        setBool (id::ringPre,  chance (0.4f));
        setBool (id::phasePre, chance (0.3f));
        setBool (id::tremPre,  chance (0.3f));
        setBool (id::formPre,  chance (0.3f));
    }
    if (chance (0.25f))
        shuffleFxOrder();

    // unison
    static const int voiceChoices[] = { 1, 1, 1, 2, 3, 4, 6, 8 };
    set (id::uniVoices, (float) voiceChoices[rndInt (0, 7)]);
    set (id::uniDetune, rnd (4.0f, 40.0f));
    set (id::uniSpread, rnd (0.2f, 1.0f));
}

// ----------------------------------------------------------------------------
//  category recipes
// ----------------------------------------------------------------------------

void Randomizer::applyCategory (Category c)
{
    snapshotForUndo();
    resetToNeutral();

    // shared one-shot baseline: instant attack, no sustain, short release
    set (id::envAAttack, 0.0001f);
    set (id::envASustain, 0.0f);
    set (id::envARelease, 0.08f);
    set (id::envFAttack, 0.0001f);
    set (id::envFSustain, 0.0f);
    set (id::gateTime, 0.25f);
    setBool (oscId (1, "on"), true);

    switch (c)
    {
        case Category::Pickup:
        {
            // coin: bright square + octave sparkle layer, a late upward pitch
            // step, and a ringing decay tail — richer than a plain Blip
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (35.0f, 65.0f));
            set (id::baseFreq, rndLog (900.0f, 1700.0f));
            if (chance (0.6f))                                     // octave layer
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), chance (0.5f) ? (int) OscWave::Sine
                                                            : (int) OscWave::Triangle);
                set (oscId (2, "pitch"), 12.0f);
                set (oscId (2, "level"), rnd (0.3f, 0.55f));
            }
            if (chance (0.4f))
                set (oscId (1, "fold"), rnd (0.05f, 0.25f));       // glassy sparkle
            // the classic coin: a REAL pitch step up a 4th/5th
            set (id::pj1Amt, (float) rndInt (5, 7));
            set (id::pj1Time, rnd (0.05f, 0.09f));
            if (chance (0.35f))                                    // double-coin variant
            {
                set (id::pj2Amt, (float) rndInt (4, 6));
                set (id::pj2Time, rnd (0.12f, 0.17f));
            }
            set (id::envADecay, rnd (0.3f, 0.55f));                // ring-out tail
            set (id::envACurve, rnd (-0.6f, -0.3f));
            set (id::lpfCutoff, rndLog (8000.0f, 16000.0f));
            set (id::lpfRes, rnd (0.1f, 0.3f));
            if (chance (0.2f))                                     // gentle 8-bit shimmer
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (8.0f, 12.0f));
                set (id::crushDown, rnd (1.0f, 4.0f));
            }
            if (chance (0.45f))                                    // FM bell/chime sparkle
                fmChime (3);
            if (chance (0.2f))                                     // sci-fi echoing coin
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.04f, 0.12f));
                set (id::delayFb, rnd (0.15f, 0.4f));
                set (id::delayMix, rnd (0.2f, 0.4f));
            }
            if (chance (0.2f))                                     // robotic coin (ring mod)
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (300.0f, 1800.0f));
                set (id::ringMix, rnd (0.3f, 0.6f));
            }
            set (id::gateTime, 0.2f);
            break;
        }

        case Category::Laser:
        {
            // pew: a genuine DOWNWARD sweep, two mechanisms:
            //  A) start high, dive to base (env decays to neutral - no tail artifact)
            //  B) inverted filter env: start at base, dive low and HOLD the bottom
            setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Saw
                                                        : (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (15.0f, 50.0f));
            const float sweepLen = rnd (0.15f, 0.4f);
            set (id::envFDecay, sweepLen);
            set (id::envFCurve, rnd (-0.4f, 0.0f));
            set (id::envADecay, sweepLen * rnd (0.7f, 1.0f));      // amp dies with the sweep
            const bool invertedDive = chance (0.5f);
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            if (! invertedDive)
            {
                // A: start-high dive (env decays to neutral - artifact-free tail)
                set (id::baseFreq, rndLog (400.0f, 1000.0f));
                set (modId (1, "depth"), rnd (0.5f, 0.85f));
            }
            else
            {
                // B: inverted dive from base (user-suggested invert flavor)
                set (id::baseFreq, rndLog (1200.0f, 2600.0f));
                setBool (id::envFInvert, true);
                set (modId (1, "depth"), rnd (-0.85f, -0.5f));
                set (id::envFRelease, 2.0f);                       // return drifts inaudibly
            }
            set (id::lpfCutoff, rndLog (4000.0f, 14000.0f));
            set (id::lpfRes, rnd (0.35f, 0.7f));                   // the "pew" ring
            if (chance (0.4f))                                     // sync sweep zap
            {
                setBool (oscId (2, "sync"), true);                 // OSC 2 -> master
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Saw);
                set (oscId (2, "pitch"), rnd (4.0f, 14.0f));
                set (oscId (2, "level"), rnd (0.6f, 0.9f));
                set (oscId (1, "level"), 0.25f);
                setChoice (modId (2, "src"), (int) ModSrc::FilterEnv);
                setChoice (modId (2, "dest"), (int) ModDest::Osc2Pitch);
                set (modId (2, "depth"), invertedDive ? rnd (-0.7f, -0.35f)
                                                      : rnd (0.35f, 0.7f));
            }
            if (chance (0.35f))                                    // sub layer: fattens the beam
            {
                setBool (oscId (3, "on"), true);
                setChoice (oscId (3, "wave"), chance (0.5f) ? (int) OscWave::Sine
                                                            : (int) OscWave::Triangle);
                set (oscId (3, "pitch"), -12.0f);
                set (oscId (3, "level"), rnd (0.4f, 0.7f));
            }
            if (chance (0.3f))                                     // discharge sizzle
            {
                setBool (id::noiseOn, true);
                setChoice (id::noiseType, (int) NoiseType::Analog);
                set (id::noiseColor, rnd (0.0f, 0.3f));
                set (id::noiseLevel, rnd (0.15f, 0.35f));
            }
            if (chance (0.3f))
            {
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, rndLog (200.0f, 600.0f));
            }
            if (chance (0.25f))                                    // swooshy zap
            {
                setBool (id::phaseOn, true);
                set (id::phaseRate, rndLog (1.0f, 5.0f));
                set (id::phaseDepth, rnd (0.4f, 0.9f));
                set (id::phaseFb, rnd (0.3f, 0.7f));
            }
            if (chance (0.4f))                                     // metallic FM beam (rides the pitch dive)
            {
                fmClang();
                set (oscId (1, "level"), rnd (0.3f, 0.55f));       // FM leads
            }
            if (chance (0.25f))                                    // biting ring-mod edge
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (200.0f, 1600.0f));
                set (id::ringMix, rnd (0.3f, 0.7f));
                if (chance (0.5f))                                 // ring pre-filter: filtered sidebands
                    setBool (id::ringPre, true);
            }
            if (chance (0.25f))                                    // echoing space zap (delay)
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.05f, 0.18f));
                set (id::delayFb, rnd (0.2f, 0.5f));
                set (id::delayMix, rnd (0.2f, 0.4f));
            }
            set (id::gateTime, 0.2f);
            break;
        }

        case Category::Explosion:
        {
            setBool (oscId (1, "on"), chance (0.5f));              // optional rumble
            setChoice (oscId (1, "wave"), (int) OscWave::Sine);
            set (oscId (1, "level"), 0.6f);
            set (id::baseFreq, rndLog (40.0f, 90.0f));
            setBool (id::noiseOn, true);
            if (chance (0.3f))                                     // console-chip boom
            {
                setChoice (id::noiseType, (int) NoiseType::LfsrHiss);
                set (id::noiseColor, rnd (0.3f, 0.7f));            // mid clock = crunchy
            }
            else
            {
                set (id::noiseColor, rnd (0.3f, 1.0f));            // analog, pink-ish
            }
            set (id::noiseLevel, rnd (0.8f, 1.0f));
            set (id::lpfCutoff, rndLog (250.0f, 800.0f));          // dark resting point
            set (id::lpfRes, rnd (0.05f, 0.4f));
            set (id::lpfEnv, rnd (0.35f, 0.7f));                   // bright impact, darkens away
            set (id::envFDecay, rnd (0.5f, 1.5f));
            set (id::envFSustain, 0.0f);
            set (id::envADecay, rnd (0.8f, 2.2f));
            set (id::envACurve, rnd (-0.8f, -0.3f));               // exp die-away
            set (id::envARelease, rnd (0.2f, 0.5f));
            set (id::vcaDrive, rnd (0.3f, 0.8f));
            set (id::comp, rnd (0.3f, 0.7f));                      // body/punch
            if (chance (0.45f))                                    // debris crunch
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (4.0f, 8.0f));
                set (id::crushDown, rnd (4.0f, 16.0f));
                if (chance (0.55f))                                // crush pre-filter: the sweep shapes the grit
                    setBool (id::crushPre, true);
                else if (chance (0.4f))                            // or bitcrush the whole FX tail
                {
                    static const int crushLast[kFxChainLen] = { 1, 3, 4, 5, 2, 6, 0 };
                    setFxOrder (crushLast);
                }
            }
            if (chance (0.35f))                                    // sfxr-style whoosh
            {
                setBool (id::flangeOn, true);
                set (id::flangeRate, rnd (0.1f, 0.4f));
                set (id::flangeDepth, rnd (0.5f, 1.0f));
                set (id::flangeFb, rnd (0.5f, 0.8f));
            }
            if (chance (0.4f))                                     // low sub layer (OSC 2)
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Sine);
                set (oscId (2, "pitch"), -12.0f);
                set (oscId (2, "level"), rnd (0.3f, 0.5f));
            }
            if (chance (0.25f))                                    // rumble flutter (tremolo)
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (8.0f, 25.0f));
                set (id::tremDepth, rnd (0.3f, 0.6f));
            }
            if (chance (0.3f))                                     // FM-noise debris burst
                fmNoise();
            set (id::gateTime, 0.4f);
            if (chance (0.4f))                                     // crackle
            {
                setChoice (modId (2, "src"), (int) ModSrc::Lfo1);
                setChoice (modId (2, "dest"), (int) ModDest::Cutoff);
                set (modId (2, "depth"), rnd (0.1f, 0.3f));
                setChoice (lfoId (1, "wave"), (int) LfoWave::SampleHold);
                set (lfoId (1, "rate"), rndLog (15.0f, 50.0f));
            }
            break;
        }

        case Category::Powerup:
        {
            // long bubbly STEPPED rise — much slower and longer than a laser,
            // with a square-LFO trill riding the climb and a sustained body
            setChoice (oscId (1, "wave"), chance (0.6f) ? (int) OscWave::Square
                                                        : (int) OscWave::Triangle);
            set (id::baseFreq, rndLog (250.0f, 600.0f));
            if (chance (0.5f))
            {
                // variant A: smooth climb + square-LFO trill riding it
                setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
                setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
                set (modId (1, "depth"), rnd (0.25f, 0.5f));       // big climb
                set (id::envFAttack, rnd (0.35f, 0.8f));           // slow rise
                set (id::envFCurve, rnd (0.2f, 0.7f));
                set (id::envFSustain, 1.0f);
                set (id::envFDecay, 0.3f);
                setChoice (modId (2, "src"), (int) ModSrc::Lfo1);  // stepped trill
                setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
                set (modId (2, "depth"), rnd (0.04f, 0.10f));
                setChoice (lfoId (1, "wave"), (int) LfoWave::Square);
                set (lfoId (1, "rate"), rnd (7.0f, 14.0f));
            }
            else
            {
                // variant B: arpeggio ladder of real pitch steps
                set (id::pj1Amt, (float) rndInt (3, 5));
                set (id::pj1Time, rnd (0.12f, 0.2f));
                set (id::pj2Amt, (float) rndInt (5, 9));
                set (id::pj2Time, rnd (0.3f, 0.45f));
                setChoice (modId (2, "src"), (int) ModSrc::Lfo1);  // light trill
                setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
                set (modId (2, "depth"), rnd (0.02f, 0.05f));
                setChoice (lfoId (1, "wave"), (int) LfoWave::Square);
                set (lfoId (1, "rate"), rnd (8.0f, 14.0f));
            }
            set (id::envAAttack, 0.005f);
            set (id::envADecay, rnd (0.7f, 1.2f));
            set (id::envASustain, rnd (0.4f, 0.7f));
            set (id::envARelease, rnd (0.2f, 0.4f));
            set (id::lpfCutoff, rndLog (3000.0f, 9000.0f));
            set (id::lpfRes, rnd (0.15f, 0.4f));
            if (chance (0.3f))                                     // stepped arpeggio rise (Step LFO -> pitch)
            {
                setChoice (modId (4, "src"),  (int) ModSrc::Lfo2);
                setChoice (modId (4, "dest"), (int) ModDest::AllPitch);
                set (modId (4, "depth"), rnd (0.06f, 0.14f));
                set (lfoId (2, "rate"), rnd (8.0f, 16.0f));
                set (id::stepCount, (float) rndInt (3, 6));
                set (id::stepGlide, chance (0.4f) ? rnd (0.2f, 0.6f) : 0.0f);
                for (int k = 1; k <= kMaxSteps; ++k)
                    set (stepValId (k), juce::jmin (1.0f, (float) (k - 1) * 0.3f));   // ascending
            }
            if (chance (0.5f))
            {
                set (id::uniVoices, (float) rndInt (2, 3));
                set (id::uniDetune, rnd (8.0f, 18.0f));
            }
            if (chance (0.25f))                                    // sparkle swirl
            {
                setBool (id::flangeOn, true);
                set (id::flangeRate, rnd (0.3f, 1.0f));
                set (id::flangeDepth, rnd (0.3f, 0.6f));
                set (id::flangeFb, rnd (0.2f, 0.5f));
            }
            if (chance (0.3f))                                     // pulsing power throb (tremolo)
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (6.0f, 16.0f));
                set (id::tremDepth, rnd (0.3f, 0.7f));
                setChoice (id::tremWave, chance (0.5f) ? 0 : 1);   // sine / triangle
            }
            if (chance (0.25f))                                    // talking power-up (formant)
            {
                setBool (id::formOn, true);
                set (id::formVowel, rnd (0.0f, 1.0f));
                set (id::formReso, rnd (0.3f, 0.6f));
                set (id::formMix, rnd (0.5f, 0.9f));
            }
            if (chance (0.4f))                                     // glassy FM sparkle on the climb
                fmChime (3);
            set (id::gateTime, rnd (0.9f, 1.3f));                  // let the rise finish
            break;
        }

        case Category::Hit:
        {
            // impact family: low punches/kicks/smashes, or bright
            // pierces/slashes/chops — both noise-led and percussive
            const bool sharp = chance (0.5f);
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (id::baseFreq, sharp ? rndLog (350.0f, 1600.0f)   // pierce/slash
                                     : rndLog (80.0f, 220.0f));   // punch/kick
            set (oscId (1, "level"), rnd (0.5f, 0.8f));
            setBool (id::noiseOn, true);
            if (chance (0.35f))                                    // digital smack
            {
                setChoice (id::noiseType, chance (0.5f) ? (int) NoiseType::Rasp
                                                        : (int) NoiseType::LfsrHiss);
                set (id::noiseColor, rnd (0.1f, 0.5f));
            }
            else
            {
                set (id::noiseColor, rnd (0.0f, 0.5f));
            }
            set (id::noiseLevel, rnd (0.7f, 1.0f));                // noise leads
            setBool (id::envFInvert, true);                        // dive and HOLD the bottom
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (-0.65f, -0.35f));        // steep thunk
            set (id::envFDecay, rnd (0.04f, 0.12f));               // very fast
            set (id::envFCurve, -0.5f);
            set (id::envFRelease, 1.5f);                           // return drifts inaudibly
            set (id::envADecay, sharp ? rnd (0.05f, 0.12f)         // slashes snap shut
                                      : rnd (0.07f, 0.16f));
            set (id::envACurve, rnd (-0.8f, -0.5f));
            set (id::envARelease, 0.05f);
            set (id::lpfCutoff, sharp ? rndLog (3000.0f, 10000.0f) // bright edge
                                      : rndLog (1500.0f, 5000.0f));
            set (id::lpfEnv, rnd (-0.5f, -0.2f));                  // darkening snap
            set (id::lpfRes, rnd (0.0f, 0.25f));
            if (sharp && chance (0.5f))                            // thin out the body: slash/chop
            {
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, rndLog (150.0f, 600.0f));
            }
            set (id::vcaDrive, rnd (0.4f, 0.8f));                  // crunch
            set (id::comp, rnd (0.4f, 0.8f));                      // punch
            if (chance (0.4f))                                     // smashed-speaker grit
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (4.0f, 9.0f));
                set (id::crushDown, rnd (2.0f, 10.0f));
                if (chance (0.5f))                                 // crush pre-filter: filtered grit
                    setBool (id::crushPre, true);
            }
            if (! sharp && chance (0.45f))                         // low body layer (OSC 2)
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), chance (0.5f) ? (int) OscWave::Triangle
                                                            : (int) OscWave::Sine);
                set (oscId (2, "pitch"), -12.0f);
                set (oscId (2, "level"), rnd (0.3f, 0.5f));
            }
            if (chance (0.3f))                                     // metallic ring edge
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (150.0f, 1400.0f));
                set (id::ringMix, rnd (0.3f, 0.6f));
            }
            if (chance (0.4f))                                     // metallic FM clang
                fmClang();
            set (id::gateTime, 0.15f);
            break;
        }

        case Category::Jump:
        {
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (30.0f, 70.0f));
            set (id::baseFreq, rndLog (160.0f, 420.0f));
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (0.12f, 0.3f));           // rise
            set (id::envFAttack, rnd (0.08f, 0.2f));
            set (id::envFSustain, 1.0f);
            set (id::envFDecay, 0.25f);
            set (id::envADecay, rnd (0.2f, 0.45f));
            set (id::lpfCutoff, rndLog (2500.0f, 9000.0f));
            if (chance (0.4f))                                     // octave / sub body (OSC 2)
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), chance (0.5f) ? (int) OscWave::Triangle
                                                            : (int) OscWave::Sine);
                set (oscId (2, "pitch"), chance (0.5f) ? -12.0f : 12.0f);
                set (oscId (2, "level"), rnd (0.3f, 0.55f));
            }
            if (chance (0.3f))                                     // launch "thwip" of noise
            {
                setBool (id::noiseOn, true);
                set (id::noiseColor, rnd (0.0f, 0.4f));
                set (id::noiseLevel, rnd (0.1f, 0.3f));
            }
            if (chance (0.3f))                                     // metallic FM boop
                fmClang();
            if (chance (0.35f))                                    // 8-bit voice
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (6.0f, 11.0f));
                set (id::crushDown, rnd (1.0f, 6.0f));
            }
            else if (chance (0.25f))                               // springy swirl
            {
                setBool (id::phaseOn, true);
                set (id::phaseRate, rndLog (1.0f, 4.0f));
                set (id::phaseDepth, rnd (0.3f, 0.7f));
                set (id::phaseFb, rnd (0.2f, 0.6f));
            }
            if (chance (0.3f))
            {
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, rndLog (80.0f, 250.0f));
            }
            set (id::gateTime, 0.3f);
            break;
        }

        case Category::Blip:
        {
            setChoice (oscId (1, "wave"), chance (0.6f) ? (int) OscWave::Square
                                                        : (int) OscWave::Sine);
            set (oscId (1, "pwm"), rnd (20.0f, 80.0f));
            set (id::baseFreq, rndLog (700.0f, 2600.0f));
            set (id::envADecay, rnd (0.04f, 0.12f));
            set (id::envARelease, 0.03f);
            set (id::lpfCutoff, rndLog (5000.0f, 18000.0f));
            if (chance (0.35f))                                    // 8-bit voice character
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (6.0f, 10.0f));
                set (id::crushDown, rnd (2.0f, 8.0f));
            }
            if (chance (0.3f))                                     // robotic ring blip
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (200.0f, 2000.0f));
                set (id::ringMix, rnd (0.4f, 0.8f));
            }
            if (chance (0.2f))                                     // slapback echo
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.03f, 0.1f));
                set (id::delayFb, rnd (0.1f, 0.3f));
                set (id::delayMix, rnd (0.15f, 0.3f));
            }
            if (chance (0.3f))                                     // FM blip timbre
                fmChime (3);
            set (id::gateTime, 0.08f);
            break;
        }

        case Category::OneUp:
        {
            // extra life: bright 3-note up-arpeggio with a ringing tail
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (40.0f, 60.0f));
            set (id::baseFreq, rndLog (500.0f, 900.0f));
            set (id::pj1Amt, (float) rndInt (4, 5));               // major 3rd / 4th
            set (id::pj1Time, rnd (0.07f, 0.1f));
            set (id::pj2Amt, (float) rndInt (5, 8));               // up to the 5th/octave
            set (id::pj2Time, rnd (0.15f, 0.2f));
            if (chance (0.5f))                                     // octave shimmer layer
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Triangle);
                set (oscId (2, "pitch"), 12.0f);
                set (oscId (2, "level"), rnd (0.25f, 0.45f));
            }
            set (id::envADecay, rnd (0.35f, 0.6f));
            set (id::envACurve, rnd (-0.5f, -0.2f));
            set (id::envARelease, 0.12f);
            set (id::lpfCutoff, rndLog (7000.0f, 16000.0f));
            if (chance (0.25f))
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (8.0f, 12.0f));
                set (id::crushDown, rnd (1.0f, 4.0f));
            }
            if (chance (0.3f))                                     // echoing arpeggio (delay)
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.08f, 0.2f));
                set (id::delayFb, rnd (0.2f, 0.45f));
                set (id::delayMix, rnd (0.2f, 0.4f));
            }
            if (chance (0.2f))                                     // singing 1-up (formant)
            {
                setBool (id::formOn, true);
                set (id::formVowel, rnd (0.0f, 1.0f));
                set (id::formReso, rnd (0.3f, 0.6f));
                set (id::formMix, rnd (0.4f, 0.8f));
            }
            if (chance (0.45f))                                    // bell arpeggio FM layer
                fmChime (3);
            set (id::gateTime, 0.3f);
            break;
        }

        case Category::Lose:
        {
            // defeat: slow descending slide with sad downward steps
            setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Triangle
                                                        : (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (35.0f, 65.0f));
            set (id::baseFreq, rndLog (400.0f, 700.0f));
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv); // gradual sag
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (-0.28f, -0.12f));
            set (id::envFAttack, rnd (0.4f, 0.8f));
            set (id::envFCurve, rnd (0.0f, 0.5f));
            set (id::envFSustain, 1.0f);
            set (id::envFDecay, 0.3f);
            set (id::pj1Amt, (float) -rndInt (3, 5));              // sad steps down
            set (id::pj1Time, rnd (0.18f, 0.28f));
            set (id::pj2Amt, (float) -rndInt (5, 9));
            set (id::pj2Time, rnd (0.45f, 0.6f));
            set (id::envAAttack, 0.005f);
            set (id::envADecay, rnd (0.8f, 1.2f));
            set (id::envASustain, rnd (0.25f, 0.45f));
            set (id::envARelease, rnd (0.25f, 0.4f));
            set (id::lpfCutoff, rndLog (2000.0f, 6000.0f));
            set (id::lpfRes, rnd (0.05f, 0.25f));
            if (chance (0.35f))                                    // sad harmony layer (OSC 2)
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Triangle);
                set (oscId (2, "pitch"), chance (0.5f) ? -3.0f : -5.0f);   // minor-ish below
                set (oscId (2, "level"), rnd (0.3f, 0.5f));
            }
            if (chance (0.3f))                                     // wavering, dying tremolo
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (3.0f, 9.0f));
                set (id::tremDepth, rnd (0.3f, 0.6f));
            }
            if (chance (0.25f))                                    // fading echo (delay)
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.1f, 0.3f));
                set (id::delayFb, rnd (0.25f, 0.5f));
                set (id::delayMix, rnd (0.2f, 0.4f));
            }
            if (chance (0.3f))                                     // melancholy FM bell
                fmChime (3);
            set (id::gateTime, rnd (0.8f, 1.1f));
            break;
        }
    }

    // shared spice for every category: a little wavefolder grit, extra mod-matrix
    // routes (source/destination variety), an occasional chain reorder and an
    // occasional pre-filter route
    maybeFold (0.35f);
    maybeNoise (0.22f);
    if (chance (0.4f)) addModSpice (5);
    if (chance (0.2f)) addModSpice (6);
    if (chance (0.18f)) shuffleFxOrder();
    if (chance (0.15f))
    {
        setBool (id::crushPre, chance (0.5f));
        setBool (id::ringPre,  chance (0.45f));
        setBool (id::phasePre, chance (0.35f));
        setBool (id::tremPre,  chance (0.35f));
        setBool (id::formPre,  chance (0.35f));
    }
}
