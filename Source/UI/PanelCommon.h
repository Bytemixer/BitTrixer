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

#include <juce_audio_processors/juce_audio_processors.h>
#include "RetroLookAndFeel.h"
#include "ParamTooltips.h"

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

        g.setColour (tickColour().withAlpha (0.9f));
        g.fillRoundedRectangle (b.getX() + 8.0f, b.getY() + 6.0f, 3.0f, 10.0f, 1.5f);

        g.setColour (RetroColors::panelTitle);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (title.toUpperCase(), getLocalBounds().reduced (16, 4).removeFromTop (14),
                    juce::Justification::centredLeft);

        // uniform title underline across every section
        if (RetroColors::prideMode)
        {
            // six-stripe flag underline
            const float x0 = b.getX() + 8.0f, x1 = b.getRight() - 8.0f;
            const float seg = (x1 - x0) / 6.0f;
            for (int i = 0; i < 6; ++i)
            {
                g.setColour (RetroColors::kPrideFlag[i].withAlpha (0.85f));
                g.fillRect (x0 + (float) i * seg, b.getY() + 18.0f, seg, 1.6f);
            }
        }
        else
        {
            g.setColour (RetroColors::panelEdge.withAlpha (0.8f));
            g.drawLine (b.getX() + 8.0f, b.getY() + 19.0f, b.getRight() - 8.0f, b.getY() + 19.0f, 1.0f);
        }
    }

    juce::Rectangle<int> content() const
    {
        return getLocalBounds().reduced (8).withTrimmedTop (14);
    }

protected:
    juce::Colour tickColour() const
    {
        if (! RetroColors::prideMode)
            return RetroColors::accent;

        // pride theme: each section gets its own flag stripe color
        return RetroColors::kPrideFlag[(size_t) ((title.hashCode() & 0x7fffffff) % 6)];
    }

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
        slider.setTooltip (ParamTooltips::lookup (paramId));
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
        b.removeFromTop (4);                      // breathing room under the label

        // A fixed knob region so the SAME knob is the SAME size in every
        // panel, regardless of how tall its cell is (a short cell no longer
        // shrinks the rotary). Extra cell height becomes bottom padding.
        constexpr int knobRegion = 52;            // ~37px rotary + value box
        if (b.getHeight() > knobRegion)
            b = b.removeFromTop (knobRegion);
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
        slider.setTooltip (ParamTooltips::lookup (paramId));
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
        button.setTooltip (ParamTooltips::lookup (paramId));
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
        box.setTooltip (ParamTooltips::lookup (paramId));
        addAndMakeVisible (box);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            s, paramId, box);
    }

    void resized() override { box.setBounds (getLocalBounds()); }

    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
