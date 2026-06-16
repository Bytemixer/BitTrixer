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

#include "PluginProcessor.h"
#include "Randomizer.h"
#include "PresetManager.h"
#include "WavExporter.h"
#include "UI/RetroLookAndFeel.h"
#include "UI/Theme.h"
#include "UI/ThemeEditor.h"
#include "UI/HeaderBar.h"
#include "UI/GeneratorsPanel.h"
#include "UI/PitchPanel.h"
#include "UI/FilterPanel.h"
#include "UI/EnvelopesPanel.h"
#include "UI/VcaPanel.h"
#include "UI/LfoPanel.h"
#include "UI/StepLfoPanel.h"
#include "UI/ModMatrixPanel.h"
#include "UI/TriggerPanel.h"
#include "UI/RandomizerPanel.h"
#include "UI/ScopePanel.h"
#include "UI/FxPanel.h"

// ============================================================================
//  BitTrixerEditor
//  The front panel. Arranges the per-section panel components; all controls
//  live inside the panels (Source/UI/*), never directly in here.
// ============================================================================

class BitTrixerEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit BitTrixerEditor (BitTrixerProcessor&);
    ~BitTrixerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void previewSound();
    void showPresetMenu();           // browse-menu popup from the preset folder
    void paintContent (juce::Graphics&);
    void drawSignalTraces (juce::Graphics&);
    void layoutPanels (juce::Rectangle<int> bounds);
    void timerCallback() override;   // repaint the traces when a pre-filter flag toggles
    bool anyPreFilter() const;       // any mono effect routed before the filter
    void updateMidiStatus();         // reflect MIDI-learn state in the header

    BitTrixerProcessor& proc;
    std::atomic<float>* prePresent[5] {};   // the 5 per-effect pre-filter flags
    bool lastSplit = false;
    int  lastLearnGen = 0;
    int  midiStatusTicks = 0;

    RetroLookAndFeel lookAndFeel;
    ThemeManager themeManager;
    Randomizer randomizer;
    PresetManager presetManager;
    WavExporter wavExporter;

    // the whole UI lives at a fixed "design" resolution inside this holder,
    // which is scaled by a transform so everything resizes proportionally
    struct Content : juce::Component
    {
        explicit Content (BitTrixerEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        BitTrixerEditor& owner;
    };
    Content content { *this };

    // hover hints for knobs / switches / combos / visual windows (600ms delay)
    juce::TooltipWindow tooltipWindow { this, 600 };

    HeaderBar header;
    GeneratorsPanel generatorsPanel;
    FilterPanel filterPanel;
    EnvelopesPanel envelopesPanel;
    VcaPanel vcaPanel;
    PitchPanel pitchPanel;
    LfoPanel lfo1Panel;
    StepLfoPanel stepLfoPanel;
    ModMatrixPanel modMatrixPanel;
    TriggerPanel triggerPanel;
    RandomizerPanel randomizerPanel;
    ScopePanel scopePanel;
    FxPanel fxPanel;
    ThemeEditor themeEditor { themeManager };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitTrixerEditor)
};
