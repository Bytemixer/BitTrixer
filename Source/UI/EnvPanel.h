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

#include "PanelCommon.h"
#include "../Params.h"

// ============================================================================
//  EnvCurveDisplay — paints the actual ADSR output shape (curve + invert
//  included, release always falling to silence like the DSP does). Polls
//  the parameters on a slow timer.
// ============================================================================

class EnvCurveDisplay : public juce::Component, private juce::Timer
{
public:
    EnvCurveDisplay (juce::AudioProcessorValueTreeState& s,
                     const char* aId, const char* dId, const char* sId,
                     const char* rId, const char* cId, const char* invId)
    {
        const char* ids[6] = { aId, dId, sId, rId, cId, invId };
        for (int i = 0; i < 6; ++i)
            raw[i] = s.getRawParameterValue (ids[i]);
        setInterceptsMouseClicks (false, false);
        startTimerHz (8);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (RetroColors::track);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (RetroColors::panelEdge);
        g.drawRoundedRectangle (b, 4.0f, 1.0f);

        const auto r = b.reduced (4.0f, 4.0f);
        const float A = v (0), D = v (1), S = v (2), R = v (3), curve = v (4);
        const bool inv = v (5) > 0.5f;

        // segment widths: log-ish time scale, fixed sustain plateau. The small
        // floor keeps a 0-time segment a steep-but-not-vertical edge (so an
        // instant attack reads as near-vertical, matching the DSP).
        auto seg = [] (float t) { return 0.015f + 0.27f * std::pow (t / 8.0f, 0.3f); };
        const float wA = seg (A), wD = seg (D), wR = seg (R);
        const float wS = 1.0f - juce::jmin (0.82f, wA + wD + wR);
        const float total = wA + wD + wS + wR;

        const float k = std::pow (4.0f, -curve);
        auto shape = [k] (float t) { return std::pow (t, k); };
        auto out = [inv] (float lvl) { return inv ? 1.0f - lvl : lvl; };
        auto px = [&r, total] (float x) { return r.getX() + (x / total) * r.getWidth(); };
        auto py = [&r] (float y) { return r.getBottom() - y * r.getHeight(); };

        juce::Path p;
        p.startNewSubPath (px (0.0f), py (out (0.0f)));
        constexpr int N = 14;
        for (int i = 1; i <= N; ++i)   // attack 0 -> 1
        {
            const float t = (float) i / N;
            p.lineTo (px (t * wA), py (out (shape (t))));
        }
        for (int i = 1; i <= N; ++i)   // decay 1 -> S
        {
            const float t = (float) i / N;
            p.lineTo (px (wA + t * wD), py (out (1.0f - (1.0f - S) * shape (t))));
        }
        p.lineTo (px (wA + wD + wS), py (out (S)));         // sustain plateau
        const float relStart = out (S);                     // release: OUTPUT falls to 0
        for (int i = 1; i <= N; ++i)
        {
            const float t = (float) i / N;
            p.lineTo (px (wA + wD + wS + t * wR), py (relStart * (1.0f - shape (t))));
        }

        g.setColour (RetroColors::accent.withAlpha (0.25f));
        juce::Path fill (p);
        fill.lineTo (px (total), py (0.0f));
        fill.lineTo (px (0.0f), py (0.0f));
        fill.closeSubPath();
        g.fillPath (fill);

        g.setColour (RetroColors::accent);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

        // faint stage boundaries (attack->decay, decay->sustain) so the four
        // ADSR sections read at a glance even when a stage is very short
        g.setColour (RetroColors::textDim.withAlpha (0.18f));
        g.drawLine (px (wA),      r.getY(), px (wA),      r.getBottom(), 1.0f);
        g.drawLine (px (wA + wD), r.getY(), px (wA + wD), r.getBottom(), 1.0f);

        // gate-off marker (release start) — the key one, drawn brighter
        g.setColour (RetroColors::textDim.withAlpha (0.5f));
        const float gx = px (wA + wD + wS);
        g.drawLine (gx, r.getY(), gx, r.getBottom(), 1.0f);
    }

private:
    float v (int i) const { return raw[i] != nullptr ? raw[i]->load() : 0.0f; }

    void timerCallback() override
    {
        bool changed = false;
        for (int i = 0; i < 6; ++i)
        {
            const float n = v (i);
            if (n != cached[i]) { cached[i] = n; changed = true; }
        }
        if (changed)
            repaint();
    }

    std::atomic<float>* raw[6] {};
    float cached[6] { -1e9f, -1e9f, -1e9f, -1e9f, -1e9f, -1e9f };
};

// ============================================================================
//  EnvPanel — one ADSR section as DeepMind-style vertical faders, plus the
//  live curve display, the curve-shape pot and the invert switch.
//  Used twice (filter env, amp env).
// ============================================================================

class EnvPanel : public SectionPanel
{
public:
    EnvPanel (juce::AudioProcessorValueTreeState& s, const juce::String& titleText,
              const char* attackId, const char* decayId, const char* sustainId,
              const char* releaseId, const char* curveId, const char* invertId)
        : SectionPanel (titleText),
          a (s, attackId,  "A"),
          d (s, decayId,   "D"),
          su (s, sustainId, "S"),
          r (s, releaseId, "R"),
          curve  (s, curveId, "CURVE"),
          invert (s, invertId, "INV"),
          display (s, attackId, decayId, sustainId, releaseId, curveId, invertId)
    {
        addAndMakeVisible (a);
        addAndMakeVisible (d);
        addAndMakeVisible (su);
        addAndMakeVisible (r);
        addAndMakeVisible (curve);
        addAndMakeVisible (invert);
        addAndMakeVisible (display);
    }

    void resized() override
    {
        auto b = content();
        auto right = b.removeFromRight (124);
        display.setBounds (right.removeFromTop (48));
        right.removeFromTop (2);
        invert.setBounds (right.removeFromBottom (20).withTrimmedLeft (26));
        curve.setBounds (right.withSizeKeepingCentre (64, right.getHeight()));

        const int fw = b.getWidth() / 4;
        a.setBounds  (b.removeFromLeft (fw).reduced (4, 0));
        d.setBounds  (b.removeFromLeft (fw).reduced (4, 0));
        su.setBounds (b.removeFromLeft (fw).reduced (4, 0));
        r.setBounds  (b.reduced (4, 0));
    }

private:
    VFader a, d, su, r;
    LabeledKnob  curve;
    SwitchToggle invert;
    EnvCurveDisplay display;
};
