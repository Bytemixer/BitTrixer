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
//  Brushed-metal rotary pots, slotted vertical faders, slide-style toggle
//  switches, colorful category buttons, and PCB-style signal traces drawn
//  on the editor background.
//
//  RetroColors are MUTABLE globals (single instance each): the active Theme
//  writes into them and the whole UI repaints — every paint() routine reads
//  them live, so theme switching is instant.
// ============================================================================

namespace RetroColors
{
    inline juce::Colour background   { 0xff23262b };
    inline juce::Colour panel        { 0xff2f333a };
    inline juce::Colour panelEdge    { 0xff1a1c20 };
    inline juce::Colour panelTitle   { 0xff9fb4cc };
    inline juce::Colour text         { 0xffd8dde5 };
    inline juce::Colour textDim     { 0xff8b939f };
    inline juce::Colour accent       { 0xff4da3ff };
    inline juce::Colour accentDark   { 0xff2f6fb5 };
    inline juce::Colour knobFace     { 0xff52575f };
    inline juce::Colour knobRim      { 0xff15171a };
    inline juce::Colour track        { 0xff1b1d21 };
    inline juce::Colour switchOff    { 0xff3c4148 };
    inline juce::Colour ledOn        { 0xff63d471 };
    inline juce::Colour trace        { 0xff5580ab };   // PCB signal traces
    inline bool prideMode = false;                     // pride theme: flag stripes everywhere

    // the six-stripe LGBTQ+ flag, used by the pride theme
    inline const juce::Colour kPrideFlag[6] = {
        juce::Colour (0xffe40303), juce::Colour (0xffff8c00),
        juce::Colour (0xffffed00), juce::Colour (0xff008026),
        juce::Colour (0xff24408e), juce::Colour (0xff732982) };
}

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();

    // re-reads the RetroColors globals into the LookAndFeel colour ids;
    // call after a theme switch, then sendLookAndFeelChange()/repaint
    void applyThemeColours();

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
