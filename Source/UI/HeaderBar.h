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
//  HeaderBar — title, current preset name, and the file actions:
//  Save / Load preset, Export WAV, About (AGPL notice).
// ============================================================================

class HeaderBar : public juce::Component
{
public:
    // a text button that also reports right-clicks (used to clear MIDI maps)
    struct ClickButton : juce::TextButton
    {
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); }
            else juce::TextButton::mouseDown (e);
        }
    };

    explicit HeaderBar (juce::AudioProcessorValueTreeState& s)
    {
        // global output sample rate lives in the top bar (it is a global
        // format setting, not a per-section knob)
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (Params::id::outRate)))
            rateBox.addItemList (choice->choices, 1);
        rateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            s, Params::id::outRate, rateBox);
        addAndMakeVisible (rateBox);
        rateLabel.setText ("RATE", juce::dontSendNotification);
        rateLabel.setColour (juce::Label::textColourId, RetroColors::textDim);
        rateLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
        rateLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (rateLabel);

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

        midiButton.setButtonText ("MIDI");
        midiButton.setTooltip ("MIDI Learn: arm, touch a control, then move a hardware knob."
                               "  Right-click to clear all mappings.");
        midiButton.onClick      = [this] { if (onMidiLearn) onMidiLearn(); };
        midiButton.onRightClick = [this] { if (onMidiClear) onMidiClear(); };
        addAndMakeVisible (midiButton);

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

    void setMidiArmed (bool armed)
    {
        midiButton.setButtonText (armed ? "LEARN..." : "MIDI");
        if (armed)
        {
            midiButton.setColour (juce::TextButton::buttonColourId, RetroColors::accent);
            midiButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff10151c));
        }
        else
        {
            midiButton.removeColour (juce::TextButton::buttonColourId);
            midiButton.removeColour (juce::TextButton::textColourOffId);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        juce::ColourGradient grad (RetroColors::panel.brighter (0.06f), 0, 0,
                                   RetroColors::background, 0, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (b);

        if (RetroColors::prideMode)
        {
            // the flag flies across the bottom of the header
            auto band = b.removeFromBottom (6.0f);
            const float stripeH = band.getHeight() / 6.0f;
            for (int i = 0; i < 6; ++i)
                g.setColour (RetroColors::kPrideFlag[i]),
                g.fillRect (band.getX(), band.getY() + (float) i * stripeH,
                            band.getWidth(), stripeH + 0.5f);
        }
        else
        {
            g.setColour (RetroColors::panelEdge);
            g.fillRect (b.removeFromBottom (1.0f));
        }

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
        b.removeFromRight (8);
        midiButton.setBounds (b.removeFromRight (58));
        b.removeFromRight (10);
        rateBox.setBounds (b.removeFromRight (78));
        rateLabel.setBounds (b.removeFromRight (38));
        b.removeFromRight (10);
        themeBox.setBounds (b.removeFromRight (92));
        presetLabel.setBounds (b.withTrimmedLeft (240));
    }

    std::function<void()> onSave, onLoad, onExport, onAbout, onMidiLearn, onMidiClear;
    std::function<void (int)> onThemeSelected;

private:
    juce::Label presetLabel { {}, "Init" };
    juce::Label rateLabel;
    juce::ComboBox rateBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAtt;
    juce::TextButton saveButton, loadButton, exportButton, aboutButton;
    ClickButton midiButton;
    juce::ComboBox themeBox;
};
