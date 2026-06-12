/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "PluginEditor.h"

RetroForgeEditor::RetroForgeEditor (RetroForgeProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      randomizer (p.apvts),
      presetManager (p.apvts),
      osc1 (p.apvts, 1), osc2 (p.apvts, 2), osc3 (p.apvts, 3),
      noisePanel (p.apvts),
      filterPanel (p.apvts),
      envFPanel (p.apvts, "Filter Envelope",
                 Params::id::envFAttack, Params::id::envFDecay,
                 Params::id::envFSustain, Params::id::envFRelease,
                 Params::id::envFCurve, Params::id::envFInvert),
      envAPanel (p.apvts, "Amp Envelope",
                 Params::id::envAAttack, Params::id::envADecay,
                 Params::id::envASustain, Params::id::envARelease,
                 Params::id::envACurve, Params::id::envAInvert),
      vcaPanel (p.apvts),
      pitchPanel (p.apvts),
      lfo1Panel (p.apvts, 1), lfo2Panel (p.apvts, 2),
      modMatrixPanel (p.apvts),
      triggerPanel (p.apvts),
      scopePanel (p.apvts, [&p] { return p.snapshotPatch(); }),
      fxPanel (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* c : std::initializer_list<juce::Component*> {
             &header, &osc1, &osc2, &osc3, &noisePanel, &filterPanel,
             &envFPanel, &envAPanel, &vcaPanel, &pitchPanel,
             &lfo1Panel, &lfo2Panel, &modMatrixPanel,
             &triggerPanel, &randomizerPanel, &scopePanel, &fxPanel })
        addAndMakeVisible (c);

    // ---- wiring ----
    triggerPanel.onTrigger = [this] { proc.uiOneShot(); };
    triggerPanel.onVariate = [this] { randomizer.variate(); previewSound(); };
    triggerPanel.onUndo    = [this] { randomizer.undo(); previewSound(); };

    randomizerPanel.onMutate = [this] { randomizer.mutate(); previewSound(); };

    randomizerPanel.onRandom = [this]
    {
        randomizer.fullRandom();
        presetManager.setCurrentName ("Random");
        header.setPresetName ("Random");
        previewSound();
    };
    randomizerPanel.onCategory = [this] (int index)
    {
        const auto cat = (Randomizer::Category) index;
        randomizer.applyCategory (cat);
        const juce::String name = Randomizer::categoryName (cat);
        presetManager.setCurrentName (name);
        header.setPresetName (name);
        previewSound();
    };

    header.onSave = [this] { presetManager.saveAsync(); };
    header.onLoad = [this] { presetManager.loadAsync(); };
    presetManager.onPresetChanged = [this]
    {
        header.setPresetName (presetManager.getCurrentName());
    };

    header.onExport = [this]
    {
        const double sr = proc.getSampleRate();
        wavExporter.setSampleRate (sr > 8000.0 ? sr : 44100.0);
        wavExporter.exportAsync (proc.snapshotPatch());
    };

    header.onAbout = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "RetroForge",
            "RetroForge - Retro Game SFX Synthesizer\n"
            "Copyright (C) 2026 Bytemixer\n\n"
            "Licensed under the GNU Affero General Public License v3.0 or later.\n"
            "This program comes with ABSOLUTELY NO WARRANTY.\n"
            "See the LICENSE file for details.",
            "OK", this);
    };

    header.setPresetName (presetManager.getCurrentName());
    setSize (1180, 850);
}

RetroForgeEditor::~RetroForgeEditor()
{
    setLookAndFeel (nullptr);
}

void RetroForgeEditor::previewSound()
{
    proc.uiOneShot();
}

void RetroForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (RetroColors::background);
}

void RetroForgeEditor::resized()
{
    auto b = getLocalBounds();
    header.setBounds (b.removeFromTop (46));
    b.reduce (8, 8);
    constexpr int gap = 6;

    fxPanel.setBounds (b.removeFromBottom (104));
    b.removeFromBottom (gap);

    // ---- left column: oscillators + noise ----
    auto left = b.removeFromLeft (330);
    osc1.setBounds (left.removeFromTop (168));
    left.removeFromTop (gap);
    osc2.setBounds (left.removeFromTop (168));
    left.removeFromTop (gap);
    osc3.setBounds (left.removeFromTop (168));
    left.removeFromTop (gap);
    noisePanel.setBounds (left);

    b.removeFromLeft (gap);

    // ---- middle column: filter, envelopes, vca ----
    auto mid = b.removeFromLeft (430);
    filterPanel.setBounds (mid.removeFromTop (180));
    mid.removeFromTop (gap);
    envFPanel.setBounds (mid.removeFromTop (164));
    mid.removeFromTop (gap);
    envAPanel.setBounds (mid.removeFromTop (164));
    mid.removeFromTop (gap);
    vcaPanel.setBounds (mid.removeFromLeft (180));
    mid.removeFromLeft (gap);
    scopePanel.setBounds (mid);

    b.removeFromLeft (gap);

    // ---- right column: pitch/voices, LFOs, matrix, trigger, generate ----
    auto right = b;
    pitchPanel.setBounds (right.removeFromTop (172));
    right.removeFromTop (gap);
    auto lfoRow = right.removeFromTop (116);
    lfo1Panel.setBounds (lfoRow.removeFromLeft ((lfoRow.getWidth() - gap) / 2));
    lfoRow.removeFromLeft (gap);
    lfo2Panel.setBounds (lfoRow);
    right.removeFromTop (gap);
    modMatrixPanel.setBounds (right.removeFromTop (160));
    right.removeFromTop (gap);
    triggerPanel.setBounds (right.removeFromTop (120));
    right.removeFromTop (gap);
    randomizerPanel.setBounds (right);
}
