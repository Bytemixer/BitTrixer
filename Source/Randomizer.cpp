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

using namespace Params;

namespace
{
    // parameters the randomizer must never touch
    bool isProtected (const juce::String& pid)
    {
        return pid == id::masterVol || pid == id::loopOn || pid == id::loopRate
            || pid == id::gateTime  || pid == id::autoVarOn || pid == id::autoVarAmt
            || pid == id::midiTrack;
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
//  variate — perturb the current patch (editable, undo-able variation)
// ----------------------------------------------------------------------------

void Randomizer::variate()
{
    snapshotForUndo();

    const float amt = apvts.getRawParameterValue (id::autoVarAmt)->load();

    // continuous params that benefit from gentle perturbation
    juce::StringArray targets;
    for (int i = 1; i <= kNumOscs; ++i)
    {
        targets.add (oscId (i, "fine"));
        targets.add (oscId (i, "pwm"));
        targets.add (oscId (i, "fold"));
        targets.add (oscId (i, "level"));
    }
    targets.add (id::baseFreq);
    targets.add (id::noiseColor);
    targets.add (id::noiseLevel);
    for (int j = 1; j <= kNumLfos; ++j)
        targets.add (lfoId (j, "rate"));
    for (int k = 1; k <= kNumModSlots; ++k)
        targets.add (modId (k, "depth"));
    targets.add (id::lpfCutoff);
    targets.add (id::lpfRes);
    targets.add (id::lpfEnv);
    targets.add (id::envFAttack); targets.add (id::envFDecay);
    targets.add (id::envADecay);  targets.add (id::envARelease);
    targets.add (id::vcaDrive);
    targets.add (id::uniDetune);

    for (const auto& pid : targets)
    {
        if (auto* p = apvts.getParameter (pid))
        {
            const float v = p->getValue();   // normalized 0..1
            const float nudge = (random.nextFloat() * 2.0f - 1.0f) * amt * 0.25f;
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v + nudge));
        }
    }
}

// ----------------------------------------------------------------------------
//  full random
// ----------------------------------------------------------------------------

void Randomizer::fullRandom()
{
    snapshotForUndo();
    resetToNeutral();

    // oscillators
    bool anySource = false;
    const float oscProb[kNumOscs] = { 0.85f, 0.45f, 0.25f };
    for (int i = 1; i <= kNumOscs; ++i)
    {
        const bool on = chance (oscProb[i - 1]);
        anySource |= on;
        setBool (oscId (i, "on"), on);
        setChoice (oscId (i, "wave"), rndInt (0, 5));
        set (oscId (i, "pitch"), chance (0.4f) ? (float) rndInt (-12, 12) : 0.0f);
        set (oscId (i, "fine"),  rnd (-20.0f, 20.0f));
        set (oscId (i, "pwm"),   rnd (10.0f, 90.0f));
        set (oscId (i, "fold"),  chance (0.3f) ? rnd (0.0f, 0.8f) : 0.0f);
        set (oscId (i, "level"), rnd (0.5f, 1.0f));
    }

    const bool noise = chance (0.35f) || ! anySource;
    setBool (id::noiseOn, noise);
    set (id::noiseColor, rnd (0.0f, 1.0f));
    set (id::noiseLevel, rnd (0.3f, 1.0f));

    set (id::baseFreq, rndLog (60.0f, 2500.0f));

    // LFOs
    for (int j = 1; j <= kNumLfos; ++j)
    {
        setChoice (lfoId (j, "wave"), rndInt (0, 6));
        set (lfoId (j, "rate"), rndLog (0.2f, 30.0f));
        set (lfoId (j, "delay"), chance (0.25f) ? rnd (0.0f, 0.5f) : 0.0f);
    }

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
        setChoice (modId (k, "dest"), rndInt (1, 12));
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
    setBool (oscId (1, "on"), true);

    switch (c)
    {
        case Category::Pickup:
        {
            // coin: bright square, quick upward step via slow-ish filter env -> pitch
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (35.0f, 65.0f));
            set (id::baseFreq, rndLog (900.0f, 1800.0f));
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (0.10f, 0.18f));          // ~5..9 st up
            set (id::envFAttack, rnd (0.03f, 0.07f));              // the "jump"
            set (id::envFCurve, -1.0f);                            // late jump feel
            set (id::envFSustain, 1.0f);
            set (id::envFDecay, 0.2f);
            set (id::envADecay, rnd (0.15f, 0.4f));
            set (id::lpfCutoff, rndLog (6000.0f, 16000.0f));
            break;
        }

        case Category::Laser:
        {
            setChoice (oscId (1, "wave"), chance (0.5f) ? (int) OscWave::Saw
                                                        : (int) OscWave::Square);
            set (oscId (1, "pwm"), rnd (15.0f, 60.0f));
            set (id::baseFreq, rndLog (700.0f, 2200.0f));
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (-0.75f, -0.35f));        // fast dive
            set (id::envFDecay, rnd (0.08f, 0.3f));
            set (id::envFCurve, rnd (-0.6f, 0.0f));
            set (id::envADecay, rnd (0.12f, 0.35f));
            set (id::lpfCutoff, rndLog (2500.0f, 12000.0f));
            set (id::lpfRes, rnd (0.15f, 0.55f));
            if (chance (0.35f))
            {
                setBool (oscId (2, "on"), true);
                setChoice (oscId (2, "wave"), (int) OscWave::Saw);
                set (oscId (2, "fine"), rnd (-30.0f, 30.0f));
                set (oscId (2, "level"), rnd (0.3f, 0.7f));
            }
            if (chance (0.4f))
            {
                set (id::uniVoices, (float) rndInt (2, 4));
                set (id::uniDetune, rnd (8.0f, 25.0f));
            }
            break;
        }

        case Category::Explosion:
        {
            setBool (oscId (1, "on"), chance (0.5f));              // optional rumble
            setChoice (oscId (1, "wave"), (int) OscWave::Sine);
            set (oscId (1, "level"), 0.6f);
            set (id::baseFreq, rndLog (40.0f, 90.0f));
            setBool (id::noiseOn, true);
            set (id::noiseColor, rnd (0.3f, 1.0f));
            set (id::noiseLevel, rnd (0.8f, 1.0f));
            set (id::lpfCutoff, rndLog (800.0f, 3000.0f));
            set (id::lpfRes, rnd (0.05f, 0.4f));
            set (id::lpfEnv, rnd (-0.6f, -0.25f));                 // darkening sweep
            set (id::envFDecay, rnd (0.5f, 1.5f));
            set (id::envFSustain, 0.0f);
            set (id::envADecay, rnd (0.8f, 2.2f));
            set (id::envACurve, rnd (-0.8f, -0.3f));               // exp die-away
            set (id::envARelease, rnd (0.2f, 0.5f));
            set (id::vcaDrive, rnd (0.3f, 0.8f));
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
            setChoice (oscId (1, "wave"), chance (0.6f) ? (int) OscWave::Square
                                                        : (int) OscWave::Saw);
            set (id::baseFreq, rndLog (300.0f, 800.0f));
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (0.2f, 0.45f));           // long rise
            set (id::envFAttack, rnd (0.25f, 0.6f));
            set (id::envFSustain, 1.0f);
            set (id::envFDecay, 0.3f);
            setChoice (modId (2, "src"), (int) ModSrc::Lfo1);      // warble
            setChoice (modId (2, "dest"), (int) ModDest::AllPitch);
            set (modId (2, "depth"), rnd (0.02f, 0.08f));
            setChoice (lfoId (1, "wave"), (int) LfoWave::Triangle);
            set (lfoId (1, "rate"), rnd (5.0f, 11.0f));
            set (id::envAAttack, 0.005f);
            set (id::envADecay, rnd (0.5f, 0.9f));
            set (id::envASustain, rnd (0.3f, 0.6f));
            set (id::envARelease, rnd (0.15f, 0.35f));
            set (id::lpfCutoff, rndLog (4000.0f, 14000.0f));
            break;
        }

        case Category::Hit:
        {
            setChoice (oscId (1, "wave"), (int) OscWave::Square);
            set (id::baseFreq, rndLog (100.0f, 320.0f));
            setBool (id::noiseOn, true);
            set (id::noiseColor, rnd (0.0f, 0.6f));
            set (id::noiseLevel, rnd (0.4f, 0.8f));
            setChoice (modId (1, "src"), (int) ModSrc::FilterEnv);
            setChoice (modId (1, "dest"), (int) ModDest::AllPitch);
            set (modId (1, "depth"), rnd (-0.3f, -0.1f));
            set (id::envFDecay, rnd (0.06f, 0.18f));
            set (id::envADecay, rnd (0.08f, 0.22f));
            set (id::lpfCutoff, rndLog (1200.0f, 5000.0f));
            set (id::vcaDrive, rnd (0.2f, 0.5f));
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
            if (chance (0.3f))
            {
                setBool (id::hpfOn, true);
                set (id::hpfCutoff, rndLog (80.0f, 250.0f));
            }
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
            break;
        }
    }
}
