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
        auto initTinyLabel = [this] (juce::Label& lbl, const char* txt)
        {
            lbl.setText (txt, juce::dontSendNotification);
            lbl.setColour (juce::Label::textColourId, RetroColors::textDim);
            lbl.setFont (juce::Font (juce::FontOptions (10.5f)));
            lbl.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (lbl);
        };
        initTinyLabel (themeLabel, "THEME");
        initTinyLabel (rateLabel,  "RATE");
        initTinyLabel (chanLabel,  "CH");

        presetLabel.setJustificationType (juce::Justification::centred);
        presetLabel.setColour (juce::Label::textColourId, RetroColors::text);
        presetLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::italic)));
        presetLabel.setInterceptsMouseClicks (false, false);   // clicks fall through -> browse menu
        addAndMakeVisible (presetLabel);

        prevButton.setButtonText ("<");
        prevButton.setTooltip ("Previous preset");
        prevButton.onClick = [this] { if (onPresetPrev) onPresetPrev(); };
        addAndMakeVisible (prevButton);
        nextButton.setButtonText (">");
        nextButton.setTooltip ("Next preset");
        nextButton.onClick = [this] { if (onPresetNext) onPresetNext(); };
        addAndMakeVisible (nextButton);

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

        midiChanBox.addItem ("All", 1);
        for (int ch = 1; ch <= 16; ++ch)
            midiChanBox.addItem ("Ch " + juce::String (ch), ch + 1);
        midiChanBox.setSelectedId (1, juce::dontSendNotification);
        midiChanBox.setTooltip ("MIDI channel to listen on (All = every channel)");
        midiChanBox.onChange = [this] { if (onMidiChannel) onMidiChannel (midiChanBox.getSelectedId() - 1); };
        addAndMakeVisible (midiChanBox);

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

    void setMidiChannel (int ch)   // 0 = Omni, 1-16
    {
        midiChanBox.setSelectedId (ch + 1, juce::dontSendNotification);
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

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (presetBounds.contains (e.getPosition()) && onPresetMenu)
            onPresetMenu();                 // click the preset display -> browse menu
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

        // preset display: a themed recess that follows the active colour scheme
        if (! presetBounds.isEmpty())
        {
            auto pr = presetBounds.toFloat();
            g.setColour (RetroColors::track);
            g.fillRoundedRectangle (pr, 4.0f);
            g.setColour (RetroColors::panelEdge);
            g.drawRoundedRectangle (pr, 4.0f, 1.0f);
            g.setColour (RetroColors::accent.withAlpha (0.85f));   // little LED tick
            g.fillRoundedRectangle (pr.getX() + 6.0f, pr.getCentreY() - 5.0f, 3.0f, 10.0f, 1.5f);
        }

        // wordmark + tagline (left)
        g.setColour (RetroColors::accent);
        g.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold)));
        g.drawText ("BIT-TRIXER", 14, 6, 240, 21, juce::Justification::centredLeft);

        g.setColour (RetroColors::textDim);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("RETRO GAME SFX SYNTHESIZER", 14, 28, 290, 13,
                    juce::Justification::topLeft);
    }

    void resized() override
    {
        // one row: [wordmark] ... [preset display | Save Load] ... [globals]
        auto b = getLocalBounds().reduced (8, 0);
        const int ctrlH = 26;
        auto row = b.withSizeKeepingCentre (b.getWidth(), ctrlH);

        // right cluster: global settings, right-aligned, each combo labelled
        aboutButton.setBounds (row.removeFromRight (58));
        row.removeFromRight (6);
        exportButton.setBounds (row.removeFromRight (90));
        row.removeFromRight (10);
        midiButton.setBounds (row.removeFromRight (50));
        row.removeFromRight (6);
        midiChanBox.setBounds (row.removeFromRight (54));
        chanLabel.setBounds (row.removeFromRight (22));
        row.removeFromRight (8);
        rateBox.setBounds (row.removeFromRight (70));
        rateLabel.setBounds (row.removeFromRight (32));
        row.removeFromRight (8);
        themeBox.setBounds (row.removeFromRight (84));
        themeLabel.setBounds (row.removeFromRight (44));
        row.removeFromRight (16);

        // reserve the wordmark area on the left
        row.removeFromLeft (250);

        // Save / Load grouped at the right edge of the preset cluster
        loadButton.setBounds (row.removeFromRight (54));
        row.removeFromRight (4);
        saveButton.setBounds (row.removeFromRight (54));
        row.removeFromRight (10);

        // browse arrows flank the preset display
        nextButton.setBounds (row.removeFromRight (24));
        row.removeFromRight (4);
        prevButton.setBounds (row.removeFromLeft (24));
        row.removeFromLeft (4);

        // the clickable preset recess fills the remaining middle, a touch taller
        // than the buttons so it reads as a small display
        presetBounds = juce::Rectangle<int> (row.getX(), 0, row.getWidth(), getHeight())
                           .withSizeKeepingCentre (row.getWidth(), 32);
        presetLabel.setBounds (presetBounds.reduced (12, 3));
    }

    std::function<void()> onSave, onLoad, onExport, onAbout, onMidiLearn, onMidiClear,
                          onPresetPrev, onPresetNext, onPresetMenu;
    std::function<void (int)> onThemeSelected, onMidiChannel;

private:
    juce::Rectangle<int> presetBounds;
    juce::Label presetLabel { {}, "Init" };
    juce::Label themeLabel, rateLabel, chanLabel;
    juce::ComboBox rateBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAtt;
    juce::TextButton saveButton, loadButton, exportButton, aboutButton;
    juce::TextButton prevButton, nextButton;
    ClickButton midiButton;
    juce::ComboBox midiChanBox;
    juce::ComboBox themeBox;
};
