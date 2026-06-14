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
#include <vector>
#include <memory>
#include <cstring>
#include "PanelCommon.h"
#include "WaveGlyph.h"
#include "../Params.h"

// ============================================================================
//  FxPanel — the FX chain: seven named effect strips you can drag (by the grip
//  header) to reorder the signal path. Each strip is one effect: a grip/name
//  header, an enable switch (+ wave selector for RingMod/Tremolo), and its
//  knobs. The chain order is stored in the fxOrder params; dragging rewrites
//  them, and a timer keeps the layout in sync with external changes (presets).
// ============================================================================

struct EffectStrip : public juce::Component
{
    struct KnobDef { juce::String id, label; };
    static constexpr int kHandleH = 16;

    EffectStrip (juce::AudioProcessorValueTreeState& s, int effectId_, juce::String displayName,
                 const juce::String& switchId, const juce::String& switchName,
                 std::initializer_list<KnobDef> knobDefs, const juce::String& comboId = {})
        : effectId (effectId_), name (std::move (displayName)),
          enable (s, switchId, switchName)
    {
        addAndMakeVisible (enable);
        for (auto& kd : knobDefs)
        {
            knobs.push_back (std::make_unique<LabeledKnob> (s, kd.id, kd.label, false));
            addAndMakeVisible (*knobs.back());
        }
        if (comboId.isNotEmpty())
        {
            combo = std::make_unique<ChoiceCombo> (s, comboId);
            addAndMakeVisible (*combo);
            glyph = std::make_unique<WaveGlyph> (s, comboId, WaveGlyph::Set::Fx);
            addAndMakeVisible (*glyph);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto h = getLocalBounds().removeFromTop (kHandleH).toFloat().reduced (1.0f, 1.0f);
        g.setColour (RetroColors::panelEdge.withAlpha (0.45f));
        g.fillRoundedRectangle (h, 3.0f);

        // grip dots
        g.setColour (RetroColors::textDim.withAlpha (0.7f));
        for (int c = 0; c < 2; ++c)
            for (int row = 0; row < 3; ++row)
                g.fillRect (h.getX() + 4.0f + (float) c * 3.0f, h.getY() + 4.0f + (float) row * 3.0f, 1.6f, 1.6f);

        g.setColour (RetroColors::panelTitle);
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (name, h.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (kHandleH);                              // name header (painted)

        // on/off switch: centred in the strip
        enable.setBounds (b.removeFromTop (22).withSizeKeepingCentre (40, 18));
        b.removeFromTop (2);

        // three fixed-size knob slots so every knob is the same size and the
        // rows line up across strips. Knobs are bottom-aligned; the freed top
        // slot holds the wave selector + glyph (RingMod/Tremolo) or stays empty.
        constexpr int nSlots = 3;
        const int slotH = b.getHeight() / nSlots;
        juce::Rectangle<int> slot[nSlots];
        for (int i = 0; i < nSlots; ++i)
            slot[i] = b.removeFromTop (i == nSlots - 1 ? b.getHeight() : slotH);

        if (combo)
        {
            auto waveRow = slot[0].withSizeKeepingCentre (slot[0].getWidth() - 10, 22);
            glyph->setBounds (waveRow.removeFromRight (24));
            waveRow.removeFromRight (4);
            combo->setBounds (waveRow);
            if (knobs.size() > 0) knobs[0]->setBounds (slot[1].reduced (3, 1));
            if (knobs.size() > 1) knobs[1]->setBounds (slot[2].reduced (3, 1));
        }
        else
        {
            const int n = (int) knobs.size();
            const int first = juce::jmax (0, nSlots - n);
            for (int i = 0; i < n; ++i)
                knobs[(size_t) i]->setBounds (slot[first + i].reduced (3, 1));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { if (e.y < kHandleH && onDragStart) onDragStart (this, e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (onDrag)    onDrag    (this, e); }
    void mouseUp   (const juce::MouseEvent& e) override { if (onDragEnd) onDragEnd (this, e); }

    int effectId;
    juce::String name;
    SwitchToggle enable;
    std::vector<std::unique_ptr<LabeledKnob>> knobs;
    std::unique_ptr<ChoiceCombo> combo;
    std::unique_ptr<WaveGlyph> glyph;
    std::function<void (EffectStrip*, const juce::MouseEvent&)> onDragStart, onDrag, onDragEnd;
};

// ---------------------------------------------------------------------------

class FxPanel : public SectionPanel, private juce::Timer
{
public:
    explicit FxPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("FX chain  (drag a panel by its grip to reorder)"),
          splitToggle (s, Params::id::fxSplit, "PRE-FILTER SPLIT")
    {
        addAndMakeVisible (splitToggle);
        using K = EffectStrip::KnobDef;
        namespace id = Params::id;
        strips[0] = std::make_unique<EffectStrip> (s, 0, "CRUSH",   id::crushOn,  "",
                        std::initializer_list<K> { { id::crushBits, "BITS" }, { id::crushDown, "DIV" } });
        strips[1] = std::make_unique<EffectStrip> (s, 1, "PHASER",  id::phaseOn,  "",
                        std::initializer_list<K> { { id::phaseRate, "RATE" }, { id::phaseDepth, "DEPTH" }, { id::phaseFb, "FDBK" } });
        strips[2] = std::make_unique<EffectStrip> (s, 2, "FLANGER", id::flangeOn, "",
                        std::initializer_list<K> { { id::flangeRate, "RATE" }, { id::flangeDepth, "DEPTH" }, { id::flangeFb, "FDBK" } });
        strips[3] = std::make_unique<EffectStrip> (s, 3, "RING MOD", id::ringOn,  "",
                        std::initializer_list<K> { { id::ringFreq, "FREQ" }, { id::ringMix, "WET" } }, id::ringWave);
        strips[4] = std::make_unique<EffectStrip> (s, 4, "TREMOLO", id::tremOn,   "",
                        std::initializer_list<K> { { id::tremRate, "SPEED" }, { id::tremDepth, "DEPTH" } }, id::tremWave);
        strips[5] = std::make_unique<EffectStrip> (s, 5, "FORMANT", id::formOn,   "",
                        std::initializer_list<K> { { id::formVowel, "VOWEL" }, { id::formReso, "RESO" }, { id::formMix, "WET" } });
        strips[6] = std::make_unique<EffectStrip> (s, 6, "DELAY",   id::delayOn,  "",
                        std::initializer_list<K> { { id::delayTime, "TIME" }, { id::delayFb, "FDBK" }, { id::delayMix, "WET" } });

        for (auto& st : strips)
        {
            addAndMakeVisible (*st);
            st->onDragStart = [this] (EffectStrip* e, const juce::MouseEvent& ev) { beginDrag (e, ev); };
            st->onDrag      = [this] (EffectStrip* e, const juce::MouseEvent& ev) { doDrag (e, ev); };
            st->onDragEnd   = [this] (EffectStrip* e, const juce::MouseEvent& ev) { endDrag (e, ev); };
        }

        for (int i = 0; i < Params::kFxChainLen; ++i)
            orderP[i] = dynamic_cast<juce::AudioParameterInt*> (s.getParameter (Params::fxOrderId (i)));

        startTimerHz (8);
    }

    ~FxPanel() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);
        auto b = content();
        const int colW = b.getWidth() / Params::kFxChainLen;

        g.setColour (RetroColors::panelEdge);
        for (int i = 1; i < Params::kFxChainLen; ++i)
            g.drawLine ((float) (b.getX() + i * colW), (float) b.getY() + 2.0f,
                        (float) (b.getX() + i * colW), (float) b.getBottom() - 2.0f, 1.0f);

        if (dragged != nullptr && dragTargetCol >= 0)
        {
            const float x = (float) (b.getX() + dragTargetCol * colW);
            g.setColour (RetroColors::accent);
            g.fillRect (x - 1.5f, (float) b.getY(), 3.0f, (float) b.getHeight());
        }
    }

    void resized() override
    {
        splitToggle.setBounds (getLocalBounds().removeFromTop (18).removeFromRight (158).reduced (4, 1));
        layoutStrips();
    }

private:
    void readOrder (int* out) const
    {
        for (int i = 0; i < Params::kFxChainLen; ++i)
        {
            const int e = orderP[i] ? orderP[i]->get() : i;
            out[i] = (e >= 0 && e < Params::kFxChainLen) ? e : i;
        }
    }

    int colOf (int effectId) const
    {
        int ord[Params::kFxChainLen];
        readOrder (ord);
        for (int i = 0; i < Params::kFxChainLen; ++i)
            if (ord[i] == effectId) return i;
        return 0;
    }

    void layoutStrips()
    {
        auto b = content();
        colWidth = b.getWidth() / Params::kFxChainLen;
        stripY = b.getY();
        stripH = b.getHeight();
        contentX = b.getX();

        int ord[Params::kFxChainLen];
        readOrder (ord);
        std::memcpy (lastOrder, ord, sizeof (ord));

        for (int i = 0; i < Params::kFxChainLen; ++i)
        {
            int eff = ord[i];
            if (eff < 0 || eff >= Params::kFxChainLen) eff = i;
            if (strips[(size_t) eff] && strips[(size_t) eff].get() != dragged)
                strips[(size_t) eff]->setBounds (b.getX() + i * colWidth, b.getY(), colWidth, b.getHeight());
        }
    }

    void writeOrder (const int* ord)
    {
        for (int i = 0; i < Params::kFxChainLen; ++i)
            if (orderP[i] != nullptr)
                orderP[i]->setValueNotifyingHost (orderP[i]->convertTo0to1 ((float) ord[i]));
    }

    void beginDrag (EffectStrip* st, const juce::MouseEvent& e)
    {
        dragged = st;
        grabDX = e.x;
        dragTargetCol = colOf (st->effectId);
        st->toFront (false);
        repaint();
    }

    void doDrag (EffectStrip* st, const juce::MouseEvent& e)
    {
        auto pe = e.getEventRelativeTo (this);
        st->setBounds (pe.x - grabDX, stripY, colWidth, stripH);
        const int rel = pe.x - contentX;
        dragTargetCol = juce::jlimit (0, Params::kFxChainLen - 1, colWidth > 0 ? rel / colWidth : 0);
        repaint();
    }

    void endDrag (EffectStrip* st, const juce::MouseEvent&)
    {
        if (dragged == st && dragTargetCol >= 0)
        {
            int ord[Params::kFxChainLen];
            readOrder (ord);

            // remove this effect, re-insert at the target column
            int rebuilt[Params::kFxChainLen];
            int w = 0;
            for (int i = 0; i < Params::kFxChainLen; ++i)
                if (ord[i] != st->effectId) rebuilt[w++] = ord[i];   // w ends at kFxChainLen-1

            const int to = juce::jlimit (0, w, dragTargetCol);
            int finalOrd[Params::kFxChainLen];
            for (int i = 0, r = 0; i < Params::kFxChainLen; ++i)
                finalOrd[i] = (i == to) ? st->effectId : rebuilt[r++];

            writeOrder (finalOrd);
        }

        dragged = nullptr;
        dragTargetCol = -1;
        layoutStrips();
        repaint();
    }

    void timerCallback() override
    {
        if (dragged != nullptr) return;
        int ord[Params::kFxChainLen];
        readOrder (ord);
        if (std::memcmp (ord, lastOrder, sizeof (ord)) != 0)
            layoutStrips();
    }

    SwitchToggle splitToggle;
    std::array<std::unique_ptr<EffectStrip>, Params::kFxChainLen> strips;
    juce::AudioParameterInt* orderP[Params::kFxChainLen] {};

    EffectStrip* dragged = nullptr;
    int dragTargetCol = -1;
    int grabDX = 0;
    int colWidth = 0, stripY = 0, stripH = 0, contentX = 0;
    int lastOrder[Params::kFxChainLen] { 0, 1, 2, 3, 4, 5, 6 };
};
