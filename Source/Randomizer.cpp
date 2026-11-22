/*  This file is part of the BitTrixer audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "Randomizer.h"
#include "DSP/Formant.h"
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

void Randomizer::fmFormantVoice (float baseHz, float vowel01, float outLevel)
{
    // the classic FM voice: operator 1 at the fundamental modulates three
    // carriers, one on the fundamental and two on the harmonics nearest the
    // vowel's first two formants (same vowel table as the Formant effect)
    auto harmonicNear = [baseHz] (float hz)
    { return juce::jlimit (1.0f, 16.0f, std::round (hz / juce::jmax (1.0f, baseHz))); };
    const float ratios[4] = { 1.0f, harmonicNear (Formant::formantHz (vowel01, 0)),
                              harmonicNear (Formant::formantHz (vowel01, 1)), 1.0f };
    const float levels[4] = { rnd (0.15f, 0.35f), rnd (0.5f, 0.7f), rnd (0.25f, 0.45f), rnd (0.7f, 0.9f) };
    setFmVoice (5, rnd (0.0f, 0.15f), ratios, levels, outLevel);   // algo 5: op 1 -> ops 2, 3, 4
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
    setChoice (id::noiseType, rndInt (0, 4));
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
    setChoice (modId (slot, "src"), srcs [rndInt (0, (int) (sizeof (srcs) / sizeof (srcs[0])) - 1)]);
    // mostly an expressive destination, but occasionally ANY destination so the
    // matrix can reach everything
    const int dest = chance (0.75f) ? dests[rndInt (0, (int) (sizeof (dests) / sizeof (dests[0])) - 1)]
                                    : rndInt (1, (int) ModDest::Count - 1);
    setChoice (modId (slot, "dest"), dest);
    set (modId (slot, "depth"), rnd (-0.5f, 0.5f));
}

void Randomizer::wildcardSprinkle()
{
    // every category may, rarely, reach for something off-recipe. Each roll is
    // low-probability, so the category's own (much higher) odds keep it on-theme;
    // this just removes the hard "never" from anything. Only adds, never clobbers
    // an effect the category already enabled.
    auto isOn = [this] (const juce::String& pid)
    { return apvts.getRawParameterValue (pid)->load() > 0.5f; };

    if (! isOn (id::crushOn) && chance (0.07f))
    {
        setBool (id::crushOn, true);
        set (id::crushBits, rnd (4.0f, 12.0f)); set (id::crushDown, rndLog (1.0f, 16.0f));
    }
    if (! isOn (id::phaseOn) && chance (0.06f))
    {
        setBool (id::phaseOn, true);
        set (id::phaseRate, rndLog (0.2f, 5.0f)); set (id::phaseDepth, rnd (0.3f, 0.9f)); set (id::phaseFb, rnd (0.1f, 0.7f));
    }
    if (! isOn (id::flangeOn) && chance (0.06f))
    {
        setBool (id::flangeOn, true);
        set (id::flangeRate, rndLog (0.1f, 2.0f)); set (id::flangeDepth, rnd (0.3f, 0.9f)); set (id::flangeFb, rnd (0.2f, 0.8f));
    }
    if (! isOn (id::ringOn) && chance (0.06f))
    {
        setBool (id::ringOn, true);
        set (id::ringFreq, rndLog (40.0f, 2000.0f)); set (id::ringMix, rnd (0.3f, 0.8f)); setChoice (id::ringWave, rndInt (0, 3));
    }
    if (! isOn (id::tremOn) && chance (0.06f))
    {
        setBool (id::tremOn, true);
        set (id::tremRate, rndLog (3.0f, 30.0f)); set (id::tremDepth, rnd (0.3f, 0.8f)); setChoice (id::tremWave, rndInt (0, 3));
    }
    if (! isOn (id::formOn) && chance (0.05f))
    {
        setBool (id::formOn, true);
        set (id::formVowel, rnd (0.0f, 1.0f)); set (id::formReso, rnd (0.3f, 0.7f)); set (id::formMix, rnd (0.5f, 1.0f));
    }
    if (! isOn (id::delayOn) && chance (0.06f))
    {
        setBool (id::delayOn, true);
        set (id::delayTime, rndLog (0.03f, 0.35f)); set (id::delayFb, rnd (0.2f, 0.6f)); set (id::delayMix, rnd (0.2f, 0.5f));
    }
    if (! isOn (oscId (2, "on")) && chance (0.08f))                 // a stray OSC 2 layer
    {
        setBool (oscId (2, "on"), true);
        setChoice (oscId (2, "wave"), rndInt (0, 7));
        set (oscId (2, "pitch"), (float) rndInt (-12, 12));
        set (oscId (2, "level"), rnd (0.3f, 0.6f));
    }
    if (! isOn (oscId (3, "on")) && chance (0.08f))                 // a stray FM voice
    {
        setBool (oscId (3, "on"), true);
        setChoice (id::fmAlgo, rndInt (0, 11));
        set (id::fmFeedback, rnd (0.0f, 0.6f));
        for (int i = 1; i <= 4; ++i)
        {
            set (fmOpId (i, "ratio"), chance (0.6f) ? (float) rndInt (1, 8) : (float) rndInt (2, 14) * 0.5f);
            set (fmOpId (i, "level"), rnd (0.3f, 0.9f));
        }
    }
}

void Randomizer::compactModMatrix()
{
    // Pull every populated routing (src AND dest both set) up to the top slots
    // in slot order, then clear the rest -- so randomize/category always fills
    // the matrix from slot 1 downward with no empty gaps between routings.
    struct Route { int src; int dest; float depth; };
    std::vector<Route> active;
    for (int k = 1; k <= kNumModSlots; ++k)
    {
        const int src  = (int) apvts.getRawParameterValue (modId (k, "src"))->load();
        const int dest = (int) apvts.getRawParameterValue (modId (k, "dest"))->load();
        if (src != 0 && dest != 0)
            active.push_back ({ src, dest,
                                apvts.getRawParameterValue (modId (k, "depth"))->load() });
    }
    for (int k = 1; k <= kNumModSlots; ++k)
    {
        if (k - 1 < (int) active.size())
        {
            const auto& r = active[(size_t) (k - 1)];
            setChoice (modId (k, "src"),  r.src);
            setChoice (modId (k, "dest"), r.dest);
            set       (modId (k, "depth"), r.depth);
        }
        else
        {
            setChoice (modId (k, "src"),  0);     // Off
            setChoice (modId (k, "dest"), 0);     // Off
            set       (modId (k, "depth"), 0.0f);
        }
    }
}

// ----------------------------------------------------------------------------
//  variate — perturb the current patch (editable, undo-able variation)
// ----------------------------------------------------------------------------

void Randomizer::refreshAnchorIfNeeded (const std::vector<Variation::Nudge>& targets)
{
    if (! anchorDirty.exchange (false) && ! anchor.empty())
        return;

    anchor.clear();
    for (const auto& t : targets)
        if (auto* p = apvts.getParameter (t.pid))
            anchor[t.pid] = p->getValue();
}

void Randomizer::applyNudges (const std::vector<Variation::Nudge>& targets, float baseScale,
                              bool fromAnchor)
{
    const float amt = apvts.getRawParameterValue (id::autoVarAmt)->load();
    const juce::ScopedValueSetter<bool> guard (selfChanging, true);

    Variation::apply (targets, baseScale, amt, random,
        [this, fromAnchor] (const juce::String& pid, float& base)
        {
            auto* p = apvts.getParameter (pid);
            if (p == nullptr)
                return false;
            base = p->getValue();
            if (fromAnchor)
            {
                const auto it = anchor.find (pid);
                if (it != anchor.end())
                    base = it->second;
            }
            return true;
        },
        [this] (const juce::String& pid, float v)
        {
            if (auto* p = apvts.getParameter (pid))
                p->setValueNotifyingHost (v);
        });
}

void Randomizer::variate()
{
    // anchored: every press is a fresh sibling of the SAME sound, so it can
    // never random-walk out of its family
    const auto targets = Variation::targets (true);
    refreshAnchorIfNeeded (targets);
    snapshotForUndo();
    applyNudges (targets, Variation::kVariateScale, true);
    anchorDirty.store (false);   // our own writes must not invalidate the anchor
}

void Randomizer::mutate()
{
    // compounding explorer: walks from wherever the sound currently is
    snapshotForUndo();
    applyNudges (Variation::targets (true), Variation::kMutateScale, false);
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
    setChoice (id::noiseType, rndInt (0, 4));
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

    compactModMatrix();    // no gaps: routings fill from slot 1 down
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
            set (id::envADecay, sweepLen * rnd (0.9f, 1.3f));      // amp holds through the sweep
            set (id::envACurve, rnd (-0.3f, 0.1f));
            const bool invertedDive = chance (0.5f);
            bool osc2Used = false, fmUsed = false;
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
                // B: inverted filter env: dive from base and hold the bottom
                set (id::baseFreq, rndLog (1200.0f, 2600.0f));
                setBool (id::envFInvert, true);
                set (modId (1, "depth"), rnd (-0.85f, -0.5f));
                set (id::envFRelease, 2.0f);                       // return drifts inaudibly
            }
            set (id::lpfCutoff, rndLog (4000.0f, 14000.0f));
            set (id::lpfRes, rnd (0.35f, 0.7f));                   // the "pew" ring
            set (id::vcaDrive, rnd (0.3f, 0.7f));                  // body and bite
            set (id::comp, rnd (0.3f, 0.7f));
            if (chance (0.4f))                                     // a wider beam
            {
                set (id::uniVoices, (float) rndInt (2, 4));
                set (id::uniDetune, rnd (10.0f, 25.0f));
            }
            if (chance (0.4f))                                     // sync sweep zap
            {
                osc2Used = true;
                setBool (oscId (2, "sync"), true);                 // OSC 2 -> master
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Saw);
                set (oscId (2, "pitch"), rnd (4.0f, 14.0f));
                set (oscId (2, "level"), rnd (0.6f, 0.9f));
                set (oscId (1, "level"), rnd (0.45f, 0.65f));
                setChoice (modId (2, "src"), (int) ModSrc::FilterEnv);
                setChoice (modId (2, "dest"), (int) ModDest::Osc2Pitch);
                set (modId (2, "depth"), invertedDive ? rnd (-0.7f, -0.35f)
                                                      : rnd (0.35f, 0.7f));
            }
            if (chance (0.5f))                                     // sub layer: fattens the beam
            {
                if (! osc2Used)                                    // OSC 2 an octave down
                {
                    setBool (oscId (2, "on"), true);
                    setChoice (oscId (2, "wave"), chance (0.5f) ? (int) OscWave::Sine
                                                                : (int) OscWave::Triangle);
                    set (oscId (2, "pitch"), -12.0f);
                    set (oscId (2, "level"), rnd (0.4f, 0.7f));
                }
                else                                               // FM voice as a pure sine sub
                {
                    fmUsed = true;
                    const float ratios[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                    const float levels[4] = { 0.0f, 0.0f, 0.0f, 0.9f };
                    setFmVoice (0, 0.0f, ratios, levels, rnd (0.4f, 0.7f));
                    set (oscId (3, "pitch"), -12.0f);
                }
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
            if (! fmUsed && chance (0.4f))                         // metallic FM beam (rides the pitch dive)
            {
                fmClang();
                set (oscId (1, "level"), rnd (0.5f, 0.7f));        // FM leads
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
            // a rolling explosion, or (blast) a sudden short bang with a
            // concussive low thump behind the crack
            const bool blast = chance (0.4f);
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
            if (blast)                                             // bright crack over a heavy thump
            {
                if (chance (0.5f))
                    set (id::noiseColor, rnd (0.0f, 0.4f));        // whiter = sharper
                set (id::noiseLevel, rnd (0.6f, 0.85f));           // the thump carries the weight
                setBool (oscId (1, "on"), true);                   // concussive body, always
                set (id::baseFreq, rndLog (40.0f, 70.0f));
                set (oscId (1, "level"), rnd (0.7f, 0.9f));
                set (id::lpfCutoff, rndLog (400.0f, 1200.0f));
                set (id::lpfEnv, rnd (0.5f, 0.85f));
                set (id::envFDecay, rnd (0.08f, 0.25f));
                set (id::envADecay, rnd (0.25f, 0.6f));
                set (id::envACurve, rnd (-0.6f, -0.3f));
                set (id::envARelease, rnd (0.08f, 0.15f));
                set (id::vcaDrive, rnd (0.5f, 0.9f));
                set (id::comp, rnd (0.6f, 0.9f));
                setChoice (modId (1, "src"),  (int) ModSrc::FilterEnv);   // the body dives with the crack
                setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
                set (modId (1, "depth"), rnd (0.2f, 0.4f));
            }
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
            if (! blast && chance (0.35f))                         // sfxr-style whoosh
            {
                setBool (id::flangeOn, true);
                set (id::flangeRate, rnd (0.1f, 0.4f));
                set (id::flangeDepth, rnd (0.5f, 1.0f));
                set (id::flangeFb, rnd (0.5f, 0.8f));
            }
            if (chance (blast ? 0.6f : 0.4f))                      // low sub layer (OSC 2)
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Sine);
                set (oscId (2, "pitch"), -12.0f);
                set (oscId (2, "level"), rnd (0.3f, 0.5f));
            }
            if (! blast && chance (0.25f))                         // rumble flutter (tremolo)
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (8.0f, 25.0f));
                set (id::tremDepth, rnd (0.3f, 0.6f));
            }
            if (chance (0.3f))                                     // FM-noise debris burst
                fmNoise();
            set (id::gateTime, blast ? rnd (0.12f, 0.2f) : 0.4f);
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

        case Category::Clang:
        {
            // struck metal: an inharmonic FM clang leads, with a bright ringing
            // tail and an optional ring-mod edge -- bells, anvils, pipes, blades
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (25.0f, 60.0f));
            set (id::baseFreq, rndLog (220.0f, 900.0f));
            set (oscId (1, "level"), rnd (0.25f, 0.45f));          // FM clang leads
            fmClang();                                             // the metallic core
            set (id::envADecay, rnd (0.3f, 0.8f));                 // ringing tail
            set (id::envACurve, rnd (-0.7f, -0.3f));               // exp ring-out
            set (id::envARelease, rnd (0.1f, 0.3f));
            set (id::lpfCutoff, rndLog (4000.0f, 14000.0f));
            set (id::lpfRes, rnd (0.15f, 0.45f));
            set (id::vcaDrive, rnd (0.2f, 0.55f));                 // bite
            if (chance (0.5f))                                     // doubled ring-mod edge
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (200.0f, 1800.0f));
                set (id::ringMix, rnd (0.3f, 0.6f));
                if (chance (0.4f)) setBool (id::ringPre, true);
            }
            if (chance (0.4f))                                     // strike transient
            {
                setBool (id::noiseOn, true);
                setChoice (id::noiseType, (int) NoiseType::LfsrHiss);
                set (id::noiseColor, rnd (0.0f, 0.4f));
                set (id::noiseLevel, rnd (0.2f, 0.4f));
                set (id::envFDecay, rnd (0.02f, 0.06f));           // very short click
            }
            if (chance (0.35f))                                    // long metallic shimmer
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.06f, 0.2f));
                set (id::delayFb, rnd (0.25f, 0.55f));
                set (id::delayMix, rnd (0.2f, 0.4f));
            }
            set (id::gateTime, rnd (0.3f, 0.5f));
            break;
        }

        case Category::Slash:
        {
            // sword swipe: a filtered-noise cut with real body and a metallic
            // blade edge -- not a thin swoosh; sometimes it lands with an impact
            setBool (oscId (1, "on"), chance (0.5f));            // tonal body, but noise-led
            setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Saw
                                                        : (int) OscWave::Square);
            set (oscId (1, "level"), rnd (0.22f, 0.42f));        // a subtle body, not the lead
            set (id::baseFreq, rndLog (300.0f, 1100.0f));        // a touch lower = meatier
            setBool (id::noiseOn, true);
            setChoice (id::noiseType, chance (0.5f) ? (int) NoiseType::Analog
                                                    : (int) NoiseType::LfsrHiss);
            set (id::noiseColor, rnd (0.1f, 0.5f));              // less thin, a little grit
            set (id::noiseLevel, rnd (0.65f, 0.95f));            // noise leads = airier, less tonal
            if (chance (0.2f))                                   // serrated edge: dense Dust grit
            {
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, rnd (0.75f, 0.95f));        // 350-1500 clicks per second
            }
            // the swoosh: a resonant filter sweep is the heart of the swing.
            // Pick a gesture -- a bright slicing cut, an inverted rising whoosh,
            // or a full up-then-down arc (the classic sword swoosh)
            set (id::lpfRes, rnd (0.4f, 0.66f));                // resonant swoosh, but not whistly
            set (id::envACurve, rnd (-0.4f, -0.1f));
            set (id::envARelease, rnd (0.04f, 0.1f));
            const int swoosh = rndInt (0, 2);
            if (swoosh == 0)                                    // bright cut, sweeps down
            {
                set (id::lpfCutoff, rndLog (1600.0f, 4500.0f));
                set (id::lpfEnv, rnd (0.5f, 0.85f));
                set (id::envFDecay, rnd (0.08f, 0.2f));
                set (id::envADecay, rnd (0.1f, 0.22f));
            }
            else if (swoosh == 1)                              // inverted: dark -> bright rise
            {
                setBool (id::envFInvert, true);
                set (id::lpfCutoff, rndLog (500.0f, 1300.0f)); // muffled start
                set (id::lpfEnv, rnd (0.6f, 0.9f));            // inverted env opens it up
                set (id::envFDecay, rnd (0.1f, 0.22f));        // the rise time
                set (id::envFRelease, 0.4f);
                set (id::envASustain, rnd (0.2f, 0.4f));       // hold so the rise is heard
                set (id::envADecay, rnd (0.12f, 0.25f));
            }
            else                                               // full arc: up to a peak then down
            {
                set (id::lpfCutoff, rndLog (700.0f, 1600.0f));
                set (id::lpfEnv, rnd (0.6f, 0.95f));
                set (id::envFAttack, rnd (0.04f, 0.1f));       // sweep UP...
                set (id::envFDecay, rnd (0.08f, 0.18f));       // ...then back DOWN
                set (id::envFCurve, rnd (-0.2f, 0.3f));
                set (id::envASustain, rnd (0.2f, 0.4f));       // sustain through the arc
                set (id::envADecay, rnd (0.15f, 0.3f));
            }
            if (chance (0.45f))                                 // thin/airy variant (HPF)
            {
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, rndLog (150.0f, 500.0f));   // lighter cut, keeps body
            }
            if (chance (0.45f))                                 // it LANDS: attack impact
            {
                set (id::vcaDrive, rnd (0.4f, 0.75f));          // punch
                set (id::comp, rnd (0.4f, 0.7f));
                set (id::pj1Amt, (float) -rndInt (3, 8));       // quick downward chunk
                set (id::pj1Time, rnd (0.02f, 0.05f));
                if (chance (0.5f))                              // low body thud
                {
                    setBool (oscId (2, "on"), true);
                    setChoice (oscId (2, "wave"), (int) OscWave::Sine);
                    set (oscId (2, "pitch"), -12.0f);
                    set (oscId (2, "level"), rnd (0.35f, 0.55f));
                }
            }
            if (chance (0.55f))                                 // the whoosh swirl
            {
                setBool (id::flangeOn, true);
                set (id::flangeRate, rnd (0.5f, 2.0f));
                set (id::flangeDepth, rnd (0.5f, 0.9f));
                set (id::flangeFb, rnd (0.1f, 0.35f));           // low feedback: the swirl stops with the cut
            }
            if (chance (0.45f))                                  // blade contact ring
                fmClang();
            if (chance (0.3f))                                   // metallic edge
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (400.0f, 2000.0f));
                set (id::ringMix, rnd (0.25f, 0.5f));
            }
            set (id::gateTime, rnd (0.14f, 0.26f));            // a quick swish
            break;
        }

        case Category::Gust:
        {
            // wind: pure filtered-noise air with a slow swelling sweep -- gusts,
            // breezes, whooshing drafts. No real pitch.
            setBool (oscId (1, "on"), false);                    // noise only
            setBool (id::noiseOn, true);
            setChoice (id::noiseType, (int) NoiseType::Analog);
            set (id::noiseColor, rnd (0.6f, 1.0f));              // pink-ish, soft
            set (id::noiseLevel, rnd (0.8f, 1.0f));
            if (chance (0.25f))                                  // sand and debris: Dust grit over FM-noise air
            {
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, rnd (0.8f, 1.0f));          // 500-2000 clicks per second
                fmNoise();
                set (oscId (3, "level"), rnd (0.3f, 0.5f));
            }
            setBool (id::hpfOn, true);
            set (id::hpfCutoff, rndLog (200.0f, 700.0f));        // remove low rumble
            set (id::lpfCutoff, rndLog (700.0f, 2500.0f));
            set (id::lpfRes, rnd (0.3f, 0.6f));                  // a vocal-ish band
            set (id::envAAttack, rnd (0.15f, 0.4f));             // swell in
            set (id::envADecay, rnd (0.4f, 0.9f));
            set (id::envASustain, rnd (0.3f, 0.6f));
            set (id::envARelease, rnd (0.3f, 0.6f));
            setChoice (modId (1, "src"),  (int) ModSrc::Lfo1);   // gusting filter motion
            setChoice (modId (1, "dest"), (int) ModDest::Cutoff);
            set (modId (1, "depth"), rnd (0.3f, 0.6f));
            setChoice (lfoId (1, "wave"), chance (0.5f) ? (int) LfoWave::Sine
                                                        : (int) LfoWave::Triangle);
            set (lfoId (1, "rate"), rndLog (0.4f, 2.0f));        // slow gusting
            if (chance (0.4f))                                   // howling formant
            {
                setBool (id::formOn, true);
                set (id::formVowel, rnd (0.0f, 1.0f));
                set (id::formReso, rnd (0.4f, 0.7f));
                set (id::formMix, rnd (0.5f, 0.9f));
                if (chance (0.5f))                               // vowel drifts -> howl
                {
                    setChoice (modId (2, "src"),  (int) ModSrc::Lfo1);
                    setChoice (modId (2, "dest"), (int) ModDest::FormVowel);
                    set (modId (2, "depth"), rnd (0.3f, 0.6f));
                }
            }
            if (chance (0.3f))                                   // turbulent flutter
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (3.0f, 9.0f));
                set (id::tremDepth, rnd (0.2f, 0.5f));
            }
            set (id::gateTime, rnd (0.6f, 1.0f));                // wind lingers
            break;
        }

        case Category::Flame:
        {
            // fire, from big to small: a flamethrower or rocket roar, a firewall
            // bursting out along the ground, an ignition "fwoomp", a torch hiss.
            // Noise carries the flame; an S&G LFO on the cutoff is the turbulence.
            enum { Thrower, Firewall, Ignition, Torch };
            const int take = rndInt (0, 3);
            setChoice (oscId (1, "wave"), (int) OscWave::Sine);
            setBool (oscId (1, "on"), false);
            setBool (id::noiseOn, true);
            setChoice (id::noiseType, chance (0.7f) ? (int) NoiseType::Analog
                                                    : (int) NoiseType::Rasp);
            setBool (id::hpfOn, true);

            setChoice (lfoId (1, "wave"), (int) LfoWave::SampleGlide);   // turbulence
            setChoice (modId (1, "src"),  (int) ModSrc::Lfo1);
            setChoice (modId (1, "dest"), (int) ModDest::Cutoff);

            auto crackleOverRoar = [this] (float clickColourLo, float clickColourHi, float roarLevel)
            {
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, rnd (clickColourLo, clickColourHi));
                fmNoise();
                set (oscId (3, "level"), roarLevel);
            };

            switch (take)
            {
                case Thrower:                                    // sustained turbulent roar
                {
                    set (id::noiseColor, rnd (0.2f, 0.6f));
                    set (id::noiseLevel, rnd (0.8f, 1.0f));
                    set (id::hpfCutoff, rndLog (80.0f, 200.0f));
                    set (id::lpfCutoff, rndLog (1500.0f, 5000.0f));
                    set (id::lpfRes, rnd (0.1f, 0.35f));
                    set (lfoId (1, "rate"), rndLog (6.0f, 15.0f));
                    set (modId (1, "depth"), rnd (0.15f, 0.3f));
                    if (chance (0.6f))                           // low rumble
                    {
                        setBool (oscId (1, "on"), true);
                        set (id::baseFreq, rndLog (40.0f, 80.0f));
                        set (oscId (1, "level"), rnd (0.3f, 0.5f));
                    }
                    if (chance (0.4f))                           // rocket: the roar opens up
                    {
                        set (id::envFAttack, rnd (0.3f, 0.8f));
                        set (id::envFSustain, 1.0f);
                        set (id::lpfEnv, rnd (0.3f, 0.6f));
                    }
                    if (chance (0.4f))                           // flutter
                    {
                        setBool (id::tremOn, true);
                        set (id::tremRate, rndLog (8.0f, 16.0f));
                        set (id::tremDepth, rnd (0.15f, 0.3f));
                    }
                    if (chance (0.3f))                           // jet whoosh
                    {
                        setBool (id::phaseOn, true);
                        set (id::phaseRate, rndLog (0.2f, 1.0f));
                        set (id::phaseDepth, rnd (0.5f, 0.9f));
                        set (id::phaseFb, rnd (0.3f, 0.6f));
                    }
                    set (id::vcaDrive, rnd (0.4f, 0.7f));
                    set (id::comp, rnd (0.3f, 0.6f));
                    set (id::envAAttack, rnd (0.05f, 0.2f));
                    set (id::envADecay, 0.5f);
                    set (id::envASustain, rnd (0.8f, 1.0f));
                    set (id::envARelease, rnd (0.25f, 0.5f));
                    set (id::gateTime, rnd (0.8f, 1.5f));
                    break;
                }

                case Firewall:                                   // bursts out, then burns
                {
                    set (id::noiseColor, rnd (0.3f, 0.7f));
                    set (id::noiseLevel, rnd (0.8f, 1.0f));
                    set (id::hpfCutoff, rndLog (100.0f, 300.0f));
                    set (id::lpfCutoff, rndLog (600.0f, 1500.0f));
                    set (id::lpfRes, rnd (0.15f, 0.4f));
                    set (id::lpfEnv, rnd (0.5f, 0.8f));          // the arc opens and settles
                    set (id::envFAttack, rnd (0.1f, 0.3f));
                    set (id::envFDecay, rnd (0.4f, 0.8f));
                    set (id::envFSustain, rnd (0.4f, 0.6f));
                    set (lfoId (1, "rate"), rndLog (5.0f, 12.0f));
                    set (modId (1, "depth"), rnd (0.1f, 0.25f));
                    if (chance (0.5f))
                        crackleOverRoar (0.45f, 0.7f, rnd (0.25f, 0.45f));
                    if (chance (0.4f))                           // moving along the ground
                    {
                        setBool (id::flangeOn, true);
                        set (id::flangeRate, rndLog (0.2f, 0.8f));
                        set (id::flangeDepth, rnd (0.4f, 0.8f));
                        set (id::flangeFb, rnd (0.2f, 0.5f));
                    }
                    set (id::vcaDrive, rnd (0.3f, 0.6f));
                    set (id::envAAttack, rnd (0.05f, 0.15f));
                    set (id::envADecay, rnd (0.4f, 0.8f));
                    set (id::envASustain, rnd (0.5f, 0.75f));
                    set (id::envARelease, rnd (0.3f, 0.6f));
                    set (id::gateTime, rnd (0.8f, 1.3f));
                    break;
                }

                case Ignition:                                   // fwoomp, then a small burn
                {
                    set (id::noiseColor, rnd (0.4f, 0.8f));
                    set (id::noiseLevel, rnd (0.7f, 0.9f));
                    set (id::hpfCutoff, rndLog (60.0f, 150.0f));
                    set (id::lpfCutoff, rndLog (300.0f, 800.0f));
                    set (id::lpfRes, rnd (0.1f, 0.3f));
                    set (id::lpfEnv, rnd (0.6f, 0.9f));          // the bloom
                    set (id::envFAttack, rnd (0.02f, 0.06f));
                    set (id::envFDecay, rnd (0.2f, 0.4f));
                    set (lfoId (1, "rate"), rndLog (4.0f, 10.0f));
                    set (modId (1, "depth"), rnd (0.08f, 0.18f));
                    setBool (oscId (1, "on"), true);             // low swell under the bloom
                    set (id::baseFreq, rndLog (50.0f, 90.0f));
                    set (oscId (1, "level"), rnd (0.3f, 0.5f));
                    setChoice (modId (2, "src"),  (int) ModSrc::FilterEnv);
                    setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
                    set (modId (2, "depth"), rnd (0.05f, 0.12f));
                    set (id::envAAttack, rnd (0.01f, 0.04f));
                    set (id::envADecay, rnd (0.3f, 0.5f));
                    set (id::envASustain, rnd (0.15f, 0.3f));
                    set (id::envARelease, rnd (0.2f, 0.4f));
                    set (id::gateTime, rnd (0.4f, 0.7f));
                    break;
                }

                case Torch:                                      // quiet steady hiss
                {
                    set (id::noiseColor, rnd (0.5f, 0.9f));
                    set (id::noiseLevel, rnd (0.55f, 0.75f));
                    set (id::hpfCutoff, rndLog (150.0f, 400.0f));
                    set (id::lpfCutoff, rndLog (800.0f, 2500.0f));
                    set (id::lpfRes, rnd (0.3f, 0.5f));          // a hint of whistle
                    set (lfoId (1, "rate"), rndLog (2.0f, 6.0f));
                    set (modId (1, "depth"), rnd (0.05f, 0.12f));
                    if (chance (0.4f))
                        crackleOverRoar (0.25f, 0.45f, rnd (0.2f, 0.35f));
                    set (id::envAAttack, rnd (0.05f, 0.15f));
                    set (id::envADecay, 0.5f);
                    set (id::envASustain, rnd (0.7f, 0.9f));
                    set (id::envARelease, rnd (0.2f, 0.4f));
                    set (id::gateTime, rnd (1.0f, 1.6f));
                    break;
                }
            }
            if (take != Torch && chance (0.25f))                 // charring grit
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (5.0f, 10.0f));
                set (id::crushDown, rnd (2.0f, 8.0f));
                if (chance (0.5f)) setBool (id::crushPre, true);
            }
            break;
        }

        case Category::Spark:
        {
            // electric: a harsh SUSTAINED buzz with a ring-mod edge and a fast
            // random crackle -- arcs, tasers, bug-zappers, shorting wires
            setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Square
                                                        : (int) OscWave::Saw);
            set (oscId (1, "pwm"), rnd (18.0f, 45.0f));          // thin, buzzy pulse
            set (id::baseFreq, rndLog (700.0f, 2400.0f));
            set (oscId (1, "level"), rnd (0.45f, 0.75f));
            setBool (id::noiseOn, true);
            setChoice (id::noiseType, chance (0.6f) ? (int) NoiseType::LfsrBuzz
                                                    : (int) NoiseType::LfsrHiss);
            set (id::noiseColor, rnd (0.0f, 0.4f));              // bright
            set (id::noiseLevel, rnd (0.4f, 0.75f));
            if (chance (0.3f))                                   // dense Dust crackle instead
            {
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, rnd (0.6f, 0.85f));         // 125-700 clicks per second
                set (id::noiseLevel, rnd (0.6f, 0.9f));
            }
            set (id::lpfCutoff, rndLog (4000.0f, 16000.0f));
            set (id::lpfRes, rnd (0.25f, 0.55f));
            // the buzz core: an audio-rate ring mod throws gritty electric
            // sidebands -- a lower carrier makes a denser, nastier AM buzz
            setBool (id::ringOn, true);
            set (id::ringFreq, rndLog (120.0f, 1200.0f));
            set (id::ringMix, rnd (0.55f, 0.95f));               // prominent
            setChoice (id::ringWave, rndInt (0, 3));
            if (chance (0.65f))                                  // amplitude "bzzzt"
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (25.0f, 80.0f));       // fast buzzing rate
                set (id::tremDepth, rnd (0.4f, 0.85f));
                setChoice (id::tremWave, 2);                     // square = hard chop
                if (chance (0.4f)) setBool (id::tremPre, true);  // pre-filter: buzz then shaped
            }
            // fast S&H jitter = the random crackle riding on top
            setChoice (modId (1, "src"),  (int) ModSrc::Lfo1);
            setChoice (modId (1, "dest"), chance (0.5f) ? (int) ModDest::AllPitch
                                                        : (int) ModDest::Cutoff);
            set (modId (1, "depth"), rnd (0.2f, 0.5f));
            setChoice (lfoId (1, "wave"), (int) LfoWave::SampleHold);
            set (lfoId (1, "rate"), rndLog (20.0f, 60.0f));
            // sustain so the buzz holds, then a snappy tail
            set (id::envASustain, rnd (0.3f, 0.6f));
            set (id::envADecay, rnd (0.15f, 0.4f));
            set (id::envACurve, rnd (-0.6f, -0.2f));
            set (id::envARelease, rnd (0.06f, 0.14f));
            if (chance (0.5f))                                   // digital bite
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (3.0f, 8.0f));
                set (id::crushDown, rnd (2.0f, 10.0f));
            }
            if (chance (0.35f))                                  // FM feedback grit = electric hiss
                fmNoise();
            if (chance (0.35f))                                  // quick discharge zap down
            {
                set (id::pj1Amt, (float) -rndInt (5, 12));
                set (id::pj1Time, rnd (0.03f, 0.08f));
            }
            set (id::gateTime, rnd (0.3f, 0.55f));               // longer: the buzz sustains
            break;
        }

        case Category::Shimmer:
        {
            // ice / crystal: high glassy FM bells with a fast twinkle and a long
            // shimmering tail -- magic sparkles, freezing, chimes of frost
            setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Sine
                                                        : (int) OscWave::Triangle);
            set (id::baseFreq, rndLog (1200.0f, 3000.0f));       // high & bright
            set (oscId (1, "level"), rnd (0.3f, 0.5f));
            fmChime (3);                                         // glassy bell core
            set (id::lpfCutoff, rndLog (9000.0f, 18000.0f));     // crystalline top
            set (id::lpfRes, rnd (0.1f, 0.3f));
            set (id::envADecay, rnd (0.4f, 0.8f));               // bell ring-out
            set (id::envACurve, rnd (-0.6f, -0.3f));
            set (id::envARelease, rnd (0.15f, 0.35f));
            if (chance (0.6f))                                   // step-sequenced sparkle
            {
                setChoice (modId (2, "src"),  (int) ModSrc::Lfo2);
                setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
                set (modId (2, "depth"), rnd (0.06f, 0.16f));
                set (lfoId (2, "rate"), rnd (10.0f, 22.0f));     // fast twinkle
                set (id::stepCount, (float) rndInt (4, 8));
                set (id::stepGlide, chance (0.3f) ? rnd (0.1f, 0.4f) : 0.0f);
                for (int k = 1; k <= kMaxSteps; ++k)
                    set (stepValId (k), rnd (0.2f, 1.0f));       // bright, upward-biased
            }
            else                                                // S&H glint
            {
                setChoice (modId (2, "src"),  (int) ModSrc::Lfo1);
                setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
                set (modId (2, "depth"), rnd (0.05f, 0.12f));
                setChoice (lfoId (1, "wave"), (int) LfoWave::SampleHold);
                set (lfoId (1, "rate"), rndLog (10.0f, 28.0f));
            }
            if (chance (0.7f))                                   // shimmering tail (delay)
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.06f, 0.22f));
                set (id::delayFb, rnd (0.3f, 0.6f));
                set (id::delayMix, rnd (0.25f, 0.5f));
            }
            if (chance (0.4f))                                   // glassy ring partials
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (800.0f, 4000.0f));
                set (id::ringMix, rnd (0.2f, 0.45f));
            }
            if (chance (0.3f))                                   // octave crystal layer
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Sine);
                set (oscId (2, "pitch"), 12.0f);
                set (oscId (2, "level"), rnd (0.2f, 0.4f));
            }
            if (chance (0.25f))                                  // glitter: sparse Dust ticks
            {
                setBool (id::noiseOn, true);
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, rnd (0.3f, 0.55f));         // 15-90 clicks per second
                set (id::noiseLevel, rnd (0.25f, 0.45f));
            }
            if (chance (0.25f))                                  // 8-bit crystalline shimmer
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (8.0f, 12.0f));
                set (id::crushDown, rnd (1.0f, 4.0f));
            }
            set (id::gateTime, rnd (0.3f, 0.5f));
            break;
        }

        case Category::Voice:
        {
            // human and creature vocalisations: a buzzy glottal source (OSC 1,
            // often with the FM formant voice) through the formant filter. The
            // filter env shapes pitch and vowel; an LFO keeps the pitch alive.
            // Takes: shout/alarm, bark, growl, roar, death cry, screech, yelp, moan.
            enum { Shout, Bark, Growl, Roar, Death, Screech, Yelp, Moan };
            struct Take { float hzLo, hzHi, vowelLo, vowelHi, fmChance; };
            static const Take takes[8] = {
                { 220.0f,  420.0f, 0.30f, 0.50f, 0.50f },   // shout / alarm
                { 250.0f,  600.0f, 0.35f, 0.55f, 0.35f },   // bark
                {  60.0f,  120.0f, 0.60f, 0.90f, 0.25f },   // growl
                {  70.0f,  160.0f, 0.40f, 0.70f, 0.30f },   // roar
                { 160.0f,  320.0f, 0.30f, 0.50f, 0.45f },   // death cry
                { 900.0f, 2200.0f, 0.00f, 0.25f, 0.60f },   // screech
                { 500.0f, 1100.0f, 0.40f, 0.70f, 0.45f },   // yelp / whimper
                {  90.0f,  200.0f, 0.55f, 0.95f, 0.50f } }; // moan
            const int take = rndInt (0, 7);
            const auto& tk = takes[take];
            const float hz = rndLog (tk.hzLo, tk.hzHi);
            const float vowel = rnd (tk.vowelLo, tk.vowelHi);

            set (id::baseFreq, hz);
            setChoice (oscId (1, "wave"), chance (0.6f) ? (int) OscWave::Saw
                                                        : (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (25.0f, 45.0f));            // nasal pulse
            setBool (id::formOn, true);
            set (id::formVowel, vowel);
            set (id::formReso, rnd (0.55f, 0.85f));
            set (id::formMix, rnd (0.85f, 1.0f));
            setBool (id::noiseOn, true);                           // breath
            setChoice (id::noiseType, (int) NoiseType::Analog);
            set (id::noiseColor, rnd (0.3f, 0.7f));
            set (id::noiseLevel, take == Roar  ? rnd (0.3f, 0.5f)
                               : take == Growl ? rnd (0.2f, 0.35f)
                               : take == Death ? rnd (0.15f, 0.3f)
                                               : rnd (0.06f, 0.18f));
            if (chance (take == Growl || take == Roar ? 0.35f : 0.2f))   // fry and rattle: Dust in the throat
            {
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, rnd (0.4f, 0.75f));         // 30-350 clicks per second
                set (id::noiseLevel, rnd (0.15f, 0.35f));
            }
            const bool high = take == Screech || take == Yelp;
            set (id::lpfCutoff, high ? rndLog (6000.0f, 12000.0f) : rndLog (2500.0f, 6000.0f));
            set (id::lpfRes, rnd (0.1f, 0.3f));
            if (take != Growl && take != Roar && chance (0.5f))
            {
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, high ? rndLog (400.0f, 900.0f) : rndLog (100.0f, 200.0f));
            }
            if (chance (tk.fmChance))                              // FM formant voice
            {
                fmFormantVoice (hz, vowel, rnd (0.4f, 0.7f));
                set (oscId (1, "level"), rnd (0.5f, 0.75f));
            }
            set (id::envAAttack, rnd (0.008f, 0.03f));
            setChoice (modId (1, "src"),  (int) ModSrc::FilterEnv);   // pitch contour
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            setChoice (modId (2, "src"),  (int) ModSrc::FilterEnv);   // vowel contour
            setChoice (modId (2, "dest"), (int) ModDest::FormVowel);
            setChoice (modId (3, "src"),  (int) ModSrc::Lfo1);        // pitch jitter or vibrato
            setChoice (modId (3, "dest"), (int) ModDest::AllPitch);
            setChoice (lfoId (1, "wave"), (int) LfoWave::SampleGlide);
            set (lfoId (1, "rate"), rnd (8.0f, 20.0f));
            set (modId (3, "depth"), rnd (0.003f, 0.008f));

            auto vibrato = [this] (float rateLo, float rateHi, float depthLo, float depthHi, float delay)
            {
                setChoice (lfoId (1, "wave"), (int) LfoWave::Sine);
                set (lfoId (1, "rate"), rnd (rateLo, rateHi));
                set (lfoId (1, "delay"), delay);
                set (modId (3, "depth"), rnd (depthLo, depthHi));
            };
            auto thickLayer = [this] (bool sub)                    // OSC 2 detuned or an octave down
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Saw);
                set (oscId (2, "pitch"), sub ? -12.0f : 0.0f);
                set (oscId (2, "fine"), rnd (6.0f, 20.0f));
                set (oscId (2, "level"), rnd (0.45f, 0.7f));
            };
            auto syncSweep = [this]                                // hard-sync "yeow" resonance
            {
                setBool (oscId (2, "on"), true);
                setBool (oscId (2, "sync"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Saw);
                set (oscId (2, "pitch"), rnd (5.0f, 12.0f));
                set (oscId (2, "level"), rnd (0.4f, 0.6f));
                setChoice (modId (4, "src"),  (int) ModSrc::FilterEnv);
                setChoice (modId (4, "dest"), (int) ModDest::Osc2Pitch);
                set (modId (4, "depth"), rnd (0.1f, 0.25f));
            };
            auto growlRing = [this] (float mixLo, float mixHi)
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (25.0f, 60.0f));
                set (id::ringMix, rnd (mixLo, mixHi));
            };

            switch (take)
            {
                case Shout:                                        // up, then falls
                    set (modId (1, "depth"), rnd (0.04f, 0.1f));
                    set (modId (2, "depth"), rnd (-0.25f, -0.1f));
                    set (id::envFDecay, rnd (0.15f, 0.35f));
                    set (id::envADecay, rnd (0.25f, 0.4f));
                    set (id::envASustain, rnd (0.55f, 0.75f));
                    set (id::envARelease, rnd (0.12f, 0.25f));
                    set (id::vcaDrive, rnd (0.4f, 0.8f));
                    if (chance (0.4f))                             // alarm: the rising "hey!"
                    {
                        set (id::pj1Amt, (float) rndInt (3, 5));
                        set (id::pj1Time, rnd (0.06f, 0.12f));
                    }
                    if (chance (0.4f)) syncSweep();
                    set (id::gateTime, rnd (0.3f, 0.5f));
                    break;

                case Bark:                                         // short, quick drop
                    set (modId (1, "depth"), rnd (0.08f, 0.18f));
                    set (modId (2, "depth"), rnd (-0.2f, -0.05f));
                    set (id::envFDecay, rnd (0.06f, 0.12f));
                    set (id::noiseLevel, rnd (0.15f, 0.3f));
                    set (id::vcaDrive, rnd (0.3f, 0.6f));
                    if (chance (0.45f))                            // woof-woof: Step LFO opens twice
                    {
                        set (id::stepCount, 2.0f);
                        set (id::stepGlide, 0.0f);
                        set (stepValId (1), 0.0f);
                        set (stepValId (2), -1.0f);
                        set (lfoId (2, "rate"), rnd (5.0f, 8.0f));
                        setChoice (modId (4, "src"),  (int) ModSrc::Lfo2);
                        setChoice (modId (4, "dest"), (int) ModDest::VcaLevel);
                        set (modId (4, "depth"), 1.0f);
                        set (id::envADecay, rnd (0.3f, 0.45f));
                        set (id::envASustain, rnd (0.5f, 0.7f));
                        set (id::envARelease, 0.05f);
                        set (id::gateTime, rnd (0.3f, 0.4f));
                    }
                    else
                    {
                        set (id::envADecay, rnd (0.1f, 0.2f));
                        set (id::envARelease, 0.05f);
                        set (id::gateTime, 0.12f);
                    }
                    break;

                case Growl:                                        // low, rough, sustained
                    set (modId (1, "depth"), rnd (-0.02f, 0.02f));
                    set (modId (2, "depth"), rnd (-0.15f, 0.0f));
                    set (id::envFDecay, rnd (0.2f, 0.4f));
                    set (id::envAAttack, rnd (0.03f, 0.08f));
                    set (id::envADecay, rnd (0.3f, 0.5f));
                    set (id::envASustain, rnd (0.5f, 0.7f));
                    set (id::envARelease, rnd (0.15f, 0.3f));
                    set (id::vcaDrive, rnd (0.5f, 0.9f));
                    setChoice (lfoId (1, "wave"), (int) LfoWave::SampleHold);   // rattling throat
                    set (lfoId (1, "rate"), rnd (20.0f, 40.0f));
                    set (modId (3, "depth"), rnd (0.004f, 0.01f));
                    growlRing (0.3f, 0.55f);
                    if (chance (0.6f))
                    {
                        setBool (id::tremOn, true);
                        set (id::tremRate, rndLog (20.0f, 40.0f));
                        set (id::tremDepth, rnd (0.2f, 0.45f));
                    }
                    if (chance (0.7f)) thickLayer (chance (0.5f));
                    set (id::gateTime, rnd (0.5f, 0.9f));
                    break;

                case Roar:                                         // swells up, then falls away
                    set (id::envFAttack, rnd (0.1f, 0.25f));
                    set (id::envFDecay, rnd (0.4f, 0.8f));
                    set (modId (1, "depth"), rnd (0.06f, 0.14f));
                    set (modId (2, "depth"), rnd (-0.3f, -0.15f));
                    set (id::envAAttack, rnd (0.05f, 0.12f));
                    set (id::envADecay, rnd (0.6f, 1.0f));
                    set (id::envASustain, rnd (0.3f, 0.5f));
                    set (id::envARelease, 0.3f);
                    set (id::vcaDrive, rnd (0.6f, 0.9f));
                    set (id::comp, rnd (0.4f, 0.7f));
                    if (chance (0.6f)) growlRing (0.2f, 0.4f);
                    if (chance (0.6f)) thickLayer (true);
                    if (chance (0.25f)) fmNoise();                 // throat grit
                    if (chance (0.3f))
                    {
                        setBool (id::crushOn, true);
                        set (id::crushBits, rnd (6.0f, 10.0f));
                        set (id::crushDown, rnd (1.0f, 4.0f));
                    }
                    set (id::gateTime, rnd (0.6f, 1.0f));
                    break;

                case Death:                                        // long slide down
                    setBool (id::envFInvert, true);                // slide from base and hold
                    set (modId (1, "depth"), rnd (-0.35f, -0.2f));
                    set (modId (2, "depth"), rnd (0.3f, 0.5f));    // vowel closes toward "oo"
                    set (id::envFDecay, rnd (0.5f, 1.0f));
                    set (id::envFRelease, 2.0f);
                    vibrato (4.0f, 7.0f, 0.006f, 0.015f, rnd (0.15f, 0.3f));
                    set (id::envAAttack, 0.02f);
                    set (id::envADecay, rnd (0.7f, 1.2f));
                    set (id::envACurve, rnd (-0.3f, 0.0f));
                    set (id::envARelease, 0.25f);
                    if (chance (0.5f)) thickLayer (false);
                    set (id::gateTime, rnd (0.6f, 0.9f));
                    break;

                case Screech:                                      // high, shrill, wavering
                    set (modId (1, "depth"), chance (0.5f) ? rnd (0.08f, 0.2f) : -rnd (0.08f, 0.2f));
                    set (modId (2, "depth"), rnd (0.0f, 0.15f));
                    set (id::envFDecay, rnd (0.15f, 0.4f));
                    vibrato (8.0f, 14.0f, 0.01f, 0.025f, 0.0f);
                    set (id::envADecay, rnd (0.25f, 0.5f));
                    set (id::envASustain, rnd (0.3f, 0.5f));
                    set (id::envARelease, 0.1f);
                    if (chance (0.3f))
                    {
                        setBool (id::ringOn, true);
                        set (id::ringFreq, rndLog (800.0f, 2000.0f));
                        set (id::ringMix, rnd (0.2f, 0.4f));
                    }
                    if (chance (0.4f)) syncSweep();
                    set (id::gateTime, rnd (0.25f, 0.45f));
                    break;

                case Yelp:                                         // a jump up, then a fall
                    set (id::pj1Amt, (float) rndInt (3, 7));
                    set (id::pj1Time, rnd (0.02f, 0.05f));
                    set (modId (1, "depth"), rnd (0.1f, 0.2f));
                    set (modId (2, "depth"), rnd (-0.4f, -0.25f)); // "ee" opening out
                    set (id::envFDecay, rnd (0.08f, 0.2f));
                    if (chance (0.4f))                             // whimper: longer, wavering
                    {
                        vibrato (5.0f, 8.0f, 0.006f, 0.012f, 0.05f);
                        set (id::envADecay, rnd (0.4f, 0.7f));
                        set (id::gateTime, rnd (0.3f, 0.5f));
                    }
                    else
                    {
                        set (id::envADecay, rnd (0.12f, 0.3f));
                        set (id::gateTime, 0.15f);
                    }
                    set (id::envARelease, 0.08f);
                    break;

                case Moan:                                         // slow swell, wandering vowel
                    set (modId (1, "depth"), rnd (-0.04f, 0.0f));  // the pitch sags
                    set (modId (2, "depth"), rnd (-0.1f, 0.1f));
                    set (id::envFDecay, rnd (0.6f, 1.0f));
                    vibrato (4.0f, 6.0f, 0.004f, 0.01f, rnd (0.2f, 0.4f));
                    set (id::stepCount, 4.0f);                     // Step LFO wanders the vowel
                    set (id::stepGlide, rnd (0.8f, 1.0f));
                    for (int k = 1; k <= 4; ++k)
                        set (stepValId (k), rnd (-1.0f, 1.0f));
                    set (lfoId (2, "rate"), rnd (1.0f, 3.0f));
                    setChoice (modId (4, "src"),  (int) ModSrc::Lfo2);
                    setChoice (modId (4, "dest"), (int) ModDest::FormVowel);
                    set (modId (4, "depth"), rnd (0.25f, 0.45f));
                    set (id::envAAttack, rnd (0.15f, 0.35f));
                    set (id::envADecay, rnd (0.6f, 1.0f));
                    set (id::envASustain, rnd (0.5f, 0.7f));
                    set (id::envARelease, rnd (0.3f, 0.5f));
                    if (chance (0.5f)) thickLayer (false);
                    set (id::gateTime, rnd (0.8f, 1.2f));
                    break;
            }
            if ((take == Shout || take == Death || take == Moan) && chance (0.15f))   // gravel
            {
                setBool (id::tremOn, true);
                set (id::tremRate, rndLog (25.0f, 45.0f));
                set (id::tremDepth, rnd (0.2f, 0.4f));
            }

            // optional colour on top of the formant: speech-chip grit, robot,
            // alien or ghostly sweeps. Each skips an effect the take already set.
            auto isOn = [this] (const juce::String& pid)
            { return apvts.getRawParameterValue (pid)->load() > 0.5f; };
            if (! isOn (id::crushOn) && chance (0.25f))
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (5.0f, 10.0f));
                set (id::crushDown, rnd (1.0f, 6.0f));
            }
            if (! isOn (id::ringOn) && chance (0.15f))
            {
                setBool (id::ringOn, true);
                set (id::ringFreq, rndLog (80.0f, 400.0f));
                set (id::ringMix, rnd (0.3f, 0.6f));
            }
            if (chance (0.15f))
            {
                setBool (id::phaseOn, true);
                set (id::phaseRate, rndLog (0.3f, 3.0f));
                set (id::phaseDepth, rnd (0.4f, 0.8f));
                set (id::phaseFb, rnd (0.3f, 0.6f));
            }
            if (chance (0.15f))
            {
                setBool (id::flangeOn, true);
                set (id::flangeRate, rndLog (0.1f, 1.0f));
                set (id::flangeDepth, rnd (0.3f, 0.7f));
                set (id::flangeFb, rnd (0.3f, 0.7f));
            }
            break;
        }

        case Category::Crackle:
        {
            // crackle. Campfire, embers, vinyl and crinkling paper use Dust noise
            // (sparse millisecond clicks, COLOR = clicks per second) over an
            // optional soft FM-noise bed, with an LFO flaring the click rate.
            // Static/electric gates noise with the Step LFO on the VCA.
            enum { Campfire, Embers, Vinyl, Paper, Static };
            const int take = rndInt (0, 4);
            setBool (oscId (1, "on"), false);
            setBool (id::noiseOn, true);
            set (id::noiseLevel, rnd (0.85f, 1.0f));

            if (take != Static)
            {
                // Dust colour c gives 2 * 1000^c clicks per second
                setChoice (id::noiseType, (int) NoiseType::Dust);
                set (id::noiseColor, take == Embers ? rnd (0.08f, 0.25f)    // 3-10 /s
                                   : take == Vinyl  ? rnd (0.15f, 0.4f)     // 5-30 /s
                                   : take == Paper  ? rnd (0.6f, 0.8f)      // 125-500 /s
                                                    : rnd (0.3f, 0.53f));   // 15-80 /s
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, take == Vinyl ? rndLog (1500.0f, 4000.0f)
                                  : take == Paper ? rndLog (1500.0f, 5000.0f)
                                                  : rndLog (150.0f, 500.0f));   // keep the wood's body
                set (id::lpfCutoff, take == Embers ? rndLog (1500.0f, 4000.0f)
                                  : take == Vinyl  ? rndLog (6000.0f, 10000.0f)
                                  : take == Paper  ? rndLog (8000.0f, 16000.0f)
                                                   : rndLog (2500.0f, 7000.0f));
                set (id::lpfRes, rnd (0.0f, 0.2f));

                const float bedChance = take == Campfire ? 0.6f : take == Vinyl ? 0.6f
                                      : take == Embers ? 0.4f : 0.0f;
                if (chance (bedChance))                            // roar or hiss under the clicks
                {
                    fmNoise();
                    set (oscId (3, "level"), take == Vinyl ? rnd (0.04f, 0.1f) : rnd (0.08f, 0.18f));
                }

                setChoice (lfoId (1, "wave"), take == Paper ? (int) LfoWave::SampleHold
                                                            : (int) LfoWave::SampleGlide);
                set (lfoId (1, "rate"), take == Paper ? rndLog (4.0f, 12.0f) : rndLog (0.4f, 2.0f));
                if (take != Vinyl)                                 // the fire flares and settles
                {
                    setChoice (modId (1, "src"),  (int) ModSrc::Lfo1);
                    setChoice (modId (1, "dest"), (int) ModDest::NoiseColor);
                    set (modId (1, "depth"), take == Paper ? rnd (0.15f, 0.3f) : rnd (0.08f, 0.2f));
                }
                set (id::comp, rnd (0.4f, 0.7f));                  // lifts the quiet clicks
                if (take == Campfire || take == Embers)
                    set (id::vcaDrive, rnd (0.3f, 0.6f));          // weight on the pops

                if (take == Paper)                                 // a hand crumpling: swells in
                {
                    set (id::envAAttack, rnd (0.05f, 0.25f));
                    set (id::envADecay, rnd (0.6f, 1.2f));
                    set (id::envASustain, rnd (0.4f, 0.7f));
                    set (id::envARelease, rnd (0.2f, 0.4f));
                    set (id::gateTime, rnd (0.8f, 1.4f));
                }
                else
                {
                    set (id::envAAttack, rnd (0.02f, 0.1f));
                    set (id::envADecay, 0.5f);
                    set (id::envASustain, rnd (0.8f, 1.0f));
                    set (id::envARelease, rnd (0.3f, 0.6f));
                    set (id::gateTime, rnd (1.5f, 2.5f));
                }
                if (take == Campfire && chance (0.2f))             // a little room
                {
                    setBool (id::delayOn, true);
                    set (id::delayTime, rndLog (0.02f, 0.06f));
                    set (id::delayFb, rnd (0.1f, 0.3f));
                    set (id::delayMix, rnd (0.15f, 0.3f));
                }
                break;
            }

            // static / electric: the Step LFO gates the VCA (most steps closed,
            // a few open); an S&H LFO jitters the step rate and the pop colour.
            // Closed steps may leave a quiet hiss bed.
            const bool hissBed = chance (0.5f);
            if (chance (0.5f))
            {
                setChoice (id::noiseType, (int) NoiseType::LfsrHiss);
                set (id::noiseColor, rnd (0.0f, 0.2f));
            }
            else
            {
                setChoice (id::noiseType, chance (0.5f) ? (int) NoiseType::Rasp
                                                        : (int) NoiseType::Analog);
                set (id::noiseColor, rnd (0.0f, 0.3f));
            }
            setBool (id::hpfOn, true);
            set (id::hpfCutoff, rndLog (300.0f, 3000.0f));
            if (chance (0.4f))                                     // radio band
            {
                set (id::lpfCutoff, rndLog (2500.0f, 4500.0f));
                set (id::lpfRes, rnd (0.2f, 0.4f));
            }
            else
            {
                set (id::lpfCutoff, rndLog (4000.0f, 14000.0f));
                set (id::lpfRes, rnd (0.0f, 0.3f));
            }

            const int steps = rndInt (6, 8);
            set (id::stepCount, (float) steps);
            set (id::stepGlide, 0.0f);
            set (id::stepSkew, rnd (-0.6f, 0.6f));
            for (int k = 1; k <= kMaxSteps; ++k)
                set (stepValId (k), hissBed ? -rnd (0.82f, 0.92f) : -1.0f);
            for (int n = rndInt (1, 3); n > 0; --n)
                set (stepValId (rndInt (1, steps)), rnd (-0.1f, 0.7f));
            set (lfoId (2, "rate"), rndLog (35.0f, 60.0f));
            setChoice (modId (1, "src"),  (int) ModSrc::Lfo2);
            setChoice (modId (1, "dest"), (int) ModDest::VcaLevel);
            set (modId (1, "depth"), 1.0f);

            setChoice (lfoId (1, "wave"), (int) LfoWave::SampleHold);
            set (lfoId (1, "rate"), rndLog (3.0f, 9.0f));
            setChoice (modId (2, "src"),  (int) ModSrc::Lfo1);     // irregular timing
            setChoice (modId (2, "dest"), (int) ModDest::Lfo2Rate);
            set (modId (2, "depth"), rnd (0.15f, 0.35f));
            setChoice (modId (3, "src"),  (int) ModSrc::Lfo1);     // pop colour
            setChoice (modId (3, "dest"), (int) ModDest::Cutoff);
            set (modId (3, "depth"), rnd (0.1f, 0.25f));

            set (id::envAAttack, 0.005f);
            set (id::envADecay, rnd (0.8f, 1.5f));
            set (id::envASustain, rnd (0.5f, 0.8f));
            set (id::envARelease, rnd (0.2f, 0.4f));
            if (chance (0.5f))                                     // digital grit
            {
                setBool (id::crushOn, true);
                set (id::crushBits, rnd (5.0f, 10.0f));
                set (id::crushDown, rnd (1.0f, 6.0f));
            }
            set (id::gateTime, rnd (0.8f, 1.4f));
            break;
        }

        case Category::Twang:
        {
            // twang: a bright plucked tone with a resonant filter snapping shut.
            // Three takes: bow string, sproing (pitch wobble), wire pluck (a
            // short delay tuned to the note acts as a comb, like a string).
            const int take = rndInt (0, 2);             // 0 bow, 1 sproing, 2 wire
            float hz = 0.0f;
            if (take == 0)
            {
                setChoice (oscId (1, "wave"), (int) OscWave::Saw);
                hz = rndLog (70.0f, 180.0f);
                set (id::envADecay, rnd (0.35f, 0.7f));
            }
            else if (take == 1)
            {
                setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Triangle
                                                            : (int) OscWave::Sine);
                hz = rndLog (180.0f, 500.0f);
                set (id::envADecay, rnd (0.4f, 0.8f));
            }
            else
            {
                setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Square
                                                            : (int) OscWave::Saw);
                set (oscId (1, "pwm"), rnd (20.0f, 50.0f));
                hz = rndLog (110.0f, 400.0f);
                set (id::envADecay, rnd (0.25f, 0.6f));
            }
            set (id::baseFreq, hz);
            set (id::envACurve, rnd (0.0f, 0.4f));                 // hard pluck: no exponential sag
            set (id::envARelease, rnd (0.1f, 0.2f));

            set (id::lpfCutoff, take == 1 ? rndLog (1200.0f, 3500.0f) : rndLog (600.0f, 1800.0f));
            set (id::lpfRes, rnd (0.55f, 0.8f));                   // the twang ring
            set (id::lpfEnv, rnd (0.5f, 0.8f));                    // bright pluck, closes fast
            set (id::envFDecay, rnd (0.06f, 0.18f));
            set (id::envFCurve, -0.4f);

            setChoice (modId (1, "src"),  (int) ModSrc::FilterEnv);   // starts sharp, settles
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), take == 1 ? rnd (0.06f, 0.12f) : rnd (0.02f, 0.05f));
            if (take == 1)                                         // the boing wobble
            {
                setChoice (lfoId (1, "wave"), (int) LfoWave::Sine);
                set (lfoId (1, "rate"), rnd (9.0f, 18.0f));
                setChoice (modId (2, "src"),  (int) ModSrc::Lfo1);
                setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
                set (modId (2, "depth"), rnd (0.04f, 0.1f));
            }
            if (take == 2 || (take == 0 && chance (0.4f)))         // string comb
            {
                setBool (id::delayOn, true);
                set (id::delayTime, juce::jlimit (0.0025f, 0.012f, (chance (0.5f) ? 1.0f : 2.0f) / hz));
                set (id::delayFb, rnd (0.6f, 0.85f));
                set (id::delayMix, rnd (0.35f, 0.55f));
            }
            if (chance (0.25f))                                    // shimmering tail
            {
                setBool (id::flangeOn, true);
                set (id::flangeRate, rnd (0.3f, 1.5f));
                set (id::flangeDepth, rnd (0.3f, 0.6f));
                set (id::flangeFb, rnd (0.3f, 0.6f));
            }
            if (chance (0.3f))                                     // a second, detuned string
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Saw);
                set (oscId (2, "fine"), rnd (5.0f, 15.0f));
                set (oscId (2, "level"), rnd (0.4f, 0.6f));
            }
            set (id::gateTime, rnd (0.12f, 0.2f));
            break;
        }

        case Category::Impact:
        {
            // impact: thump, thud, body fall, heavy door. A sub sine with a fast
            // pitch dive and a short dull noise burst riding the same filter env.
            // Low-passed throughout: HIT keeps the bright punch and pierce roles.
            const int take = rndInt (0, 2);             // 0 thump, 1 thud / body fall, 2 heavy door
            const bool light = chance (0.55f);          // high-passed: less sub, more knock
            setChoice (oscId (1, "wave"), chance (0.7f) ? (int) OscWave::Sine
                                                        : (int) OscWave::Triangle);
            set (id::baseFreq, light ? rndLog (40.0f, 120.0f) : rndLog (40.0f, 90.0f));
            set (oscId (1, "level"), rnd (0.7f, 0.9f));
            setChoice (modId (1, "src"),  (int) ModSrc::FilterEnv);   // pitch dive
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (0.15f, 0.4f));
            set (id::envFDecay, rnd (0.025f, 0.08f));
            set (id::envFCurve, -0.6f);

            setBool (id::noiseOn, true);                           // dull burst on the env
            setChoice (id::noiseType, (int) NoiseType::Analog);
            set (id::noiseColor, rnd (0.5f, 1.0f));
            set (id::noiseLevel, 0.0f);
            setChoice (modId (2, "src"),  (int) ModSrc::FilterEnv);
            setChoice (modId (2, "dest"), (int) ModDest::NoiseLevel);
            set (modId (2, "depth"), rnd (0.35f, 0.7f));

            if (light)                                             // the LPF stays above the HPF
            {
                const float hpf = rndLog (80.0f, 2000.0f);
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, hpf);
                set (id::lpfCutoff, juce::jmax (rndLog (300.0f, 1200.0f), hpf * rnd (1.5f, 3.0f)));
            }
            else
            {
                set (id::lpfCutoff, rndLog (180.0f, 700.0f));
            }
            set (id::lpfRes, rnd (0.0f, 0.2f));
            set (id::lpfEnv, rnd (0.1f, 0.35f));                   // a brief knock
            set (id::envADecay, take == 0 ? rnd (0.12f, 0.3f)
                              : take == 1 ? rnd (0.2f, 0.45f)
                                          : rnd (0.4f, 0.8f));
            set (id::envACurve, rnd (-0.7f, -0.4f));
            set (id::envARelease, 0.08f);
            set (id::vcaDrive, rnd (0.3f, 0.7f));
            set (id::comp, rnd (0.5f, 0.85f));
            if (! light && chance (0.4f))                          // sub octave weight
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Sine);
                set (oscId (2, "pitch"), -12.0f);
                set (oscId (2, "level"), rnd (0.4f, 0.7f));
            }
            if (take == 2)                                         // a room behind the door
            {
                setBool (id::delayOn, true);
                set (id::delayTime, rndLog (0.03f, 0.07f));
                set (id::delayFb, rnd (0.2f, 0.4f));
                set (id::delayMix, rnd (0.2f, 0.35f));
            }
            set (id::gateTime, 0.1f);
            break;
        }
    }

    // shared spice for every category: a little wavefolder grit, extra mod-matrix
    // routes (source/destination variety), an occasional chain reorder and an
    // occasional pre-filter route
    maybeFold (0.35f);
    maybeNoise (0.22f);
    wildcardSprinkle();
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

    if (c == Category::Slash)
        setBool (id::delayOn, false);   // a slash is a quick swish: no echo tail

    compactModMatrix();    // no gaps: routings fill from slot 1 down
}
