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
#include "EnvPanel.h"          // reuse EnvCurveDisplay
#include "../Params.h"

// ============================================================================
//  EnvelopesPanel — both ADSR envelopes in one panel: FILTER ENV and AMP ENV
//  as labelled bands separated by a divider (mirrors the Sound Generators
//  panel). Each band: A/D/S/R faders, curve pot, invert switch, live curve.
// ============================================================================

class EnvelopesPanel : public SectionPanel
{
public:
    explicit EnvelopesPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("Envelopes"),
          filterEnv (s, "FILTER ENV", RetroColors::accent,
                     Params::id::envFAttack, Params::id::envFDecay,
                     Params::id::envFSustain, Params::id::envFRelease,
                     Params::id::envFCurve, Params::id::envFInvert),
          ampEnv (s, "AMP ENV", juce::Colour (0xffe0a13a),
                  Params::id::envAAttack, Params::id::envADecay,
                  Params::id::envASustain, Params::id::envARelease,
                  Params::id::envACurve, Params::id::envAInvert)
    {
        addAndMakeVisible (filterEnv);
        addAndMakeVisible (ampEnv);
    }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);
        g.setColour (RetroColors::panelEdge.withAlpha (0.7f));
        g.drawLine (getLocalBounds().toFloat().getX() + 10.0f, dividerY,
                    getLocalBounds().toFloat().getRight() - 10.0f, dividerY, 1.0f);
    }

    void resized() override
    {
        auto b = content();
        const int half = b.getHeight() / 2;
        filterEnv.setBounds (b.removeFromTop (half));
        dividerY = (float) b.getY();
        ampEnv.setBounds (b);
    }

    // band bounds in the editor's coordinates (for the routing traces)
    juce::Rectangle<int> filterBandBounds() const { return filterEnv.getBounds() + getPosition(); }
    juce::Rectangle<int> ampBandBounds()    const { return ampEnv.getBounds()    + getPosition(); }

private:
    // ---- one envelope as a band (no SectionPanel chrome of its own) ----
    struct EnvBand : juce::Component
    {
        EnvBand (juce::AudioProcessorValueTreeState& s, juce::String tagText,
                 juce::Colour tagColour,
                 const char* aId, const char* dId, const char* sId,
                 const char* rId, const char* cId, const char* invId)
            : tag (std::move (tagText)), tagCol (tagColour),
              a (s, aId, "A"), d (s, dId, "D"), su (s, sId, "S"), r (s, rId, "R"),
              curve (s, cId, "CURVE"), invert (s, invId, "INV"),
              display (s, aId, dId, sId, rId, cId, invId)
        {
            for (auto* c : std::initializer_list<juce::Component*> {
                     &a, &d, &su, &r, &curve, &invert, &display })
                addAndMakeVisible (c);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (tagCol);
            g.fillRoundedRectangle (1.0f, 2.0f, 3.0f, 10.0f, 1.5f);
            g.setColour (RetroColors::panelTitle);
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (tag, getLocalBounds().withTrimmedLeft (8).withHeight (14),
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (2, 2);
            b.removeFromTop (14);                    // tag strip

            // CURVE pot + INV switch get the side column, spread vertically
            auto right = b.removeFromRight (76);
            invert.setBounds (right.removeFromBottom (24).withTrimmedLeft (16));
            right.removeFromBottom (6);
            curve.setBounds (right);                 // label + knob, room to breathe
            b.removeFromRight (6);

            // envelope shape window sits ABOVE the (now shorter) faders
            display.setBounds (b.removeFromTop (52));
            b.removeFromTop (4);

            const int fw = b.getWidth() / 4;
            a.setBounds  (b.removeFromLeft (fw).reduced (4, 0));
            d.setBounds  (b.removeFromLeft (fw).reduced (4, 0));
            su.setBounds (b.removeFromLeft (fw).reduced (4, 0));
            r.setBounds  (b.reduced (4, 0));
        }

        juce::String tag;
        juce::Colour tagCol;
        VFader a, d, su, r;
        LabeledKnob  curve;
        SwitchToggle invert;
        EnvCurveDisplay display;
    };

    EnvBand filterEnv, ampEnv;
    float dividerY = 0.0f;
};
