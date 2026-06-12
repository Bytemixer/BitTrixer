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

#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
//  RetroLookAndFeel — the hardware front-panel aesthetic.
//  Gray/blue scheme: charcoal panels, brushed-metal rotary pots with a blue
//  pointer, slotted vertical faders, slide-style toggle switches, and
//  colorful category buttons (each button keeps its own buttonColourId).
// ============================================================================

namespace RetroColors
{
    const juce::Colour background   { 0xff23262b };
    const juce::Colour panel        { 0xff2f333a };
    const juce::Colour panelEdge    { 0xff1a1c20 };
    const juce::Colour panelTitle   { 0xff9fb4cc };
    const juce::Colour text         { 0xffd8dde5 };
    const juce::Colour textDim      { 0xff8b939f };
    const juce::Colour accent       { 0xff4da3ff };
    const juce::Colour accentDark   { 0xff2f6fb5 };
    const juce::Colour knobFace     { 0xff52575f };
    const juce::Colour knobRim      { 0xff15171a };
    const juce::Colour track        { 0xff1b1d21 };
    const juce::Colour switchOff    { 0xff3c4148 };
    const juce::Colour ledOn        { 0xff63d471 };
}

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};
