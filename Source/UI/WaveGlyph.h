/*  This file is part of the BitTrixer audio plugin.
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
//  WaveGlyph — a small panel-printed picture of the currently selected
//  oscillator or LFO waveform, drawn next to the wave selector. Polls the
//  parameter on a slow timer (safe regardless of which thread changed it).
// ============================================================================

class WaveGlyph : public juce::Component, private juce::Timer
{
public:
    enum class Set { Osc, Lfo, Fx };

    WaveGlyph (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, Set glyphSet)
        : set (glyphSet), raw (s.getRawParameterValue (paramId))
    {
        jassert (raw != nullptr);
        setInterceptsMouseClicks (false, false);
        startTimerHz (6);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (RetroColors::track);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (RetroColors::panelEdge);
        g.drawRoundedRectangle (b, 4.0f, 1.0f);

        const auto r = b.reduced (5.0f, 5.0f);
        g.setColour (RetroColors::accent);
        g.strokePath (makeWavePath (r), juce::PathStrokeType (1.6f,
                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    void timerCallback() override
    {
        const int v = raw != nullptr ? (int) raw->load() : 0;
        if (v != cached)
        {
            cached = v;
            repaint();
        }
    }

    juce::Path makeWavePath (juce::Rectangle<float> r) const
    {
        juce::Path p;
        const float x0 = r.getX(), w = r.getWidth();
        const float yMid = r.getCentreY(), a = r.getHeight() * 0.5f;
        auto pt = [&] (float fx, float fy)   // fx 0..1, fy -1..+1 (up positive)
        {
            return juce::Point<float> (x0 + fx * w, yMid - fy * a);
        };

        enum { Sine, Triangle, SquareOrSaw, D3, D4, D5, D6 };
        const int v = cached;

        const bool isOsc = set == Set::Osc;
        using OW = Params::OscWave;
        using LW = Params::LfoWave;

        auto sine = [&]
        {
            p.startNewSubPath (pt (0.0f, 0.0f));
            for (int i = 1; i <= 32; ++i)
            {
                const float fx = (float) i / 32.0f;
                p.lineTo (pt (fx, std::sin (fx * juce::MathConstants<float>::twoPi)));
            }
        };
        auto triangle = [&]
        {
            p.startNewSubPath (pt (0.0f, 0.0f));
            p.lineTo (pt (0.25f, 1.0f));
            p.lineTo (pt (0.75f, -1.0f));
            p.lineTo (pt (1.0f, 0.0f));
        };
        auto square = [&]
        {
            p.startNewSubPath (pt (0.0f, -1.0f));
            p.lineTo (pt (0.0f, 1.0f));
            p.lineTo (pt (0.5f, 1.0f));
            p.lineTo (pt (0.5f, -1.0f));
            p.lineTo (pt (1.0f, -1.0f));
        };
        auto saw = [&] (bool reverse, float yOff = 0.0f, float scale = 1.0f)
        {
            // two cycles for clarity
            for (int c = 0; c < 2; ++c)
            {
                const float xa = c * 0.5f, xb = xa + 0.5f;
                if (! reverse)
                {
                    p.startNewSubPath (pt (xa, -scale + yOff));
                    p.lineTo (pt (xb, scale + yOff));
                    p.lineTo (pt (xb, -scale + yOff));
                }
                else
                {
                    p.startNewSubPath (pt (xa, scale + yOff));
                    p.lineTo (pt (xb, -scale + yOff));
                    p.lineTo (pt (xb, scale + yOff));
                }
            }
        };
        auto steps = [&]
        {
            static constexpr float lv[] = { 0.4f, -0.8f, 0.9f, -0.2f, 0.6f, -0.6f };
            const float seg = 1.0f / (float) std::size (lv);
            p.startNewSubPath (pt (0.0f, lv[0]));
            for (size_t i = 0; i < std::size (lv); ++i)
            {
                p.lineTo (pt ((float) i * seg, lv[i]));
                p.lineTo (pt ((float) (i + 1) * seg, lv[i]));
            }
        };
        auto glide = [&]
        {
            static constexpr float lv[] = { 0.4f, -0.8f, 0.9f, -0.2f, 0.6f, -0.6f };
            p.startNewSubPath (pt (0.0f, lv[0]));
            const float seg = 1.0f / (float) (std::size (lv) - 1);
            for (size_t i = 1; i < std::size (lv); ++i)
                p.quadraticTo (pt (((float) i - 0.5f) * seg, lv[i - 1]),
                               pt ((float) i * seg, lv[i]));
        };
        auto tanShape = [&]
        {
            // rising curve to the asymptote, then re-enter from below
            p.startNewSubPath (pt (0.0f, 0.0f));
            for (int i = 1; i <= 12; ++i)
            {
                const float fx = 0.46f * (float) i / 12.0f;
                p.lineTo (pt (fx, std::pow ((float) i / 12.0f, 2.2f)));
            }
            p.startNewSubPath (pt (0.54f, -1.0f));
            for (int i = 1; i <= 12; ++i)
            {
                const float fx = 0.54f + 0.46f * (float) i / 12.0f;
                p.lineTo (pt (fx, -std::pow (1.0f - (float) i / 12.0f, 2.2f)));
            }
        };
        auto breakerShape = [&]
        {
            p.startNewSubPath (pt (0.0f, 0.8f));
            for (int i = 1; i <= 24; ++i)
            {
                const float fp = (float) i / 24.0f;
                p.lineTo (pt (fp, std::fabs (1.0f - 2.0f * fp * fp) * 1.8f - 1.0f));
            }
        };

        if (isOsc)
        {
            switch ((OW) v)
            {
                case OW::Sine:     sine(); break;
                case OW::Triangle: triangle(); break;
                case OW::Square:   square(); break;
                case OW::Saw:      saw (false); break;
                case OW::RevSaw:   saw (true); break;
                case OW::SuperSaw: saw (false, 0.25f, 0.6f); saw (false, -0.25f, 0.6f); break;
                case OW::Tan:      tanShape(); break;
                case OW::Breaker:  breakerShape(); break;
                default: break;
            }
        }
        else if (set == Set::Lfo)
        {
            switch ((LW) v)
            {
                case LW::Sine:        sine(); break;
                case LW::Triangle:    triangle(); break;
                case LW::Saw:         saw (false); break;
                case LW::RevSaw:      saw (true); break;
                case LW::Square:      square(); break;
                case LW::SampleHold:  steps(); break;
                case LW::SampleGlide: glide(); break;
                default: break;
            }
        }
        else   // Set::Fx -- fxWaveNames order: Sine, Tri, Square, Saw
        {
            switch (v)
            {
                case 0: sine(); break;
                case 1: triangle(); break;
                case 2: square(); break;
                case 3: saw (false); break;
                default: break;
            }
        }
        return p;
    }

    Set set;
    std::atomic<float>* raw = nullptr;
    int cached = -1;
};
