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

#include <juce_audio_processors/juce_audio_processors.h>
#include "RetroLookAndFeel.h"

// ============================================================================
//  PanelCommon — shared building blocks for the front-panel sections:
//  SectionPanel (engraved rounded panel with a title strip) and the
//  attachment-wired control widgets (LabeledKnob, VFader, SwitchToggle,
//  ChoiceCombo).
// ============================================================================

class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel (juce::String titleText,
                           juce::Colour accentColour = RetroColors::accent)
        : title (std::move (titleText)), accent (accentColour) {}

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (RetroColors::panel);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (RetroColors::panelEdge);
        g.drawRoundedRectangle (b, 6.0f, 1.2f);

        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (b.getX() + 8.0f, b.getY() + 6.0f, 3.0f, 10.0f, 1.5f);

        g.setColour (RetroColors::panelTitle);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (title.toUpperCase(), getLocalBounds().reduced (16, 4).removeFromTop (14),
                    juce::Justification::centredLeft);
    }

    juce::Rectangle<int> content() const
    {
        return getLocalBounds().reduced (8).withTrimmedTop (14);
    }

protected:
    juce::String title;
    juce::Colour accent;
};

// ---------------------------------------------------------------------------

struct LabeledKnob : public juce::Component
{
    LabeledKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId,
                 const juce::String& name, bool showValue = true)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (showValue ? juce::Slider::TextBoxBelow
                                          : juce::Slider::NoTextBox,
                                false, 58, 13);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, RetroColors::textDim);
        label.setFont (juce::Font (juce::FontOptions (10.5f)));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            s, paramId, slider);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (12));
        slider.setBounds (b);
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ---------------------------------------------------------------------------

struct VFader : public juce::Component
{
    VFader (juce::AudioProcessorValueTreeState& s, const juce::String& paramId,
            const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, RetroColors::textDim);
        label.setFont (juce::Font (juce::FontOptions (10.5f)));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            s, paramId, slider);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromBottom (12));
        slider.setBounds (b);
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ---------------------------------------------------------------------------

struct SwitchToggle : public juce::Component
{
    SwitchToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramId,
                  const juce::String& name)
    {
        button.setButtonText (name);
        addAndMakeVisible (button);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            s, paramId, button);
    }

    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// ---------------------------------------------------------------------------

struct ChoiceCombo : public juce::Component
{
    ChoiceCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramId)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (paramId)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            s, paramId, box);
    }

    void resized() override { box.setBounds (getLocalBounds()); }

    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
