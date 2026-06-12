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

// ============================================================================
//  HeaderBar — title, current preset name, and the file actions:
//  Save / Load preset, Export WAV, About (AGPL notice).
// ============================================================================

class HeaderBar : public juce::Component
{
public:
    HeaderBar()
    {
        presetLabel.setJustificationType (juce::Justification::centred);
        presetLabel.setColour (juce::Label::textColourId, RetroColors::text);
        presetLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::italic)));
        addAndMakeVisible (presetLabel);

        auto initButton = [this] (juce::TextButton& b, const char* text,
                                  std::function<void()>& target)
        {
            b.setButtonText (text);
            b.onClick = [&target] { if (target) target(); };
            addAndMakeVisible (b);
        };
        initButton (saveButton,   "SAVE",       onSave);
        initButton (loadButton,   "LOAD",       onLoad);
        initButton (exportButton, "EXPORT WAV", onExport);
        initButton (aboutButton,  "ABOUT",      onAbout);
        exportButton.setColour (juce::TextButton::buttonColourId, RetroColors::accentDark);

        themeBox.onChange = [this]
        {
            if (onThemeSelected && themeBox.getSelectedId() > 0)
                onThemeSelected (themeBox.getSelectedId() - 1);
        };
        addAndMakeVisible (themeBox);
    }

    void setThemeNames (const juce::StringArray& names, int selectedIndex)
    {
        themeBox.clear (juce::dontSendNotification);
        themeBox.addItemList (names, 1);
        themeBox.setSelectedId (selectedIndex + 1, juce::dontSendNotification);
    }

    void setPresetName (const juce::String& name)
    {
        presetLabel.setText (name, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        juce::ColourGradient grad (RetroColors::panel.brighter (0.06f), 0, 0,
                                   RetroColors::background, 0, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (b);
        g.setColour (RetroColors::panelEdge);
        g.fillRect (b.removeFromBottom (1.0f));

        g.setColour (RetroColors::accent);
        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.drawText ("RETROFORGE", 14, 0, 220, getHeight(), juce::Justification::centredLeft);

        g.setColour (RetroColors::textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("RETRO GAME SFX SYNTHESIZER", 16, getHeight() - 16, 260, 12,
                    juce::Justification::topLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8, 9);
        aboutButton.setBounds (b.removeFromRight (64));
        b.removeFromRight (6);
        exportButton.setBounds (b.removeFromRight (100));
        b.removeFromRight (6);
        loadButton.setBounds (b.removeFromRight (64));
        b.removeFromRight (6);
        saveButton.setBounds (b.removeFromRight (64));
        b.removeFromRight (6);
        themeBox.setBounds (b.removeFromRight (92));
        presetLabel.setBounds (b.withTrimmedLeft (240));
    }

    std::function<void()> onSave, onLoad, onExport, onAbout;
    std::function<void (int)> onThemeSelected;

private:
    juce::Label presetLabel { {}, "Init" };
    juce::TextButton saveButton, loadButton, exportButton, aboutButton;
    juce::ComboBox themeBox;
};
