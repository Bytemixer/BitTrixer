/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "RetroLookAndFeel.h"

using namespace RetroColors;

RetroLookAndFeel::RetroLookAndFeel()
{
    applyThemeColours();
}

void RetroLookAndFeel::applyThemeColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, track);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDark);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, switchOff);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, track);
    setColour (juce::TextEditor::textColourId, text);
}

void RetroLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h)
                      .reduced (2.0f);
    // cap the pot diameter: smaller, uniform knobs across all panels
    const float size = juce::jmin (juce::jmin (bounds.getWidth(), bounds.getHeight()), 42.0f);
    auto area = bounds.withSizeKeepingCentre (size, size);
    const auto centre = area.getCentre();
    const float radius = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // value arc
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f,
                           startAngle, endAngle, true);
        g.setColour (track);
        g.strokePath (arc, juce::PathStrokeType (2.4f));

        juce::Path val;
        val.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f,
                           startAngle, angle, true);
        g.setColour (slider.isEnabled() ? accent : textDim);
        g.strokePath (val, juce::PathStrokeType (2.4f));
    }

    // knob body
    const float knobR = radius * 0.72f;
    auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    g.setColour (knobRim);
    g.fillEllipse (knob.expanded (1.5f));

    juce::ColourGradient grad (knobFace.brighter (0.25f),
                               centre.x - knobR * 0.5f, centre.y - knobR * 0.7f,
                               knobFace.darker (0.45f),
                               centre.x + knobR * 0.5f, centre.y + knobR * 0.9f, false);
    g.setGradientFill (grad);
    g.fillEllipse (knob);

    // pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -knobR + 2.0f, 3.2f, knobR * 0.55f, 1.4f);
    g.setColour (slider.isEnabled() ? accent.brighter (0.2f) : textDim);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void RetroLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float sliderPos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = (float) x + (float) w * 0.5f;

        // slot
        g.setColour (track);
        g.fillRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f, 1.0f);

        // filled portion below the cap
        g.setColour (accentDark.withAlpha (0.7f));
        g.fillRoundedRectangle (cx - 2.0f, sliderPos, 4.0f,
                                (float) y + (float) h - sliderPos, 2.0f);

        // fader cap
        const float capW = juce::jmin ((float) w, 24.0f);
        const float capH = 13.0f;
        auto cap = juce::Rectangle<float> (capW, capH)
                       .withCentre ({ cx, sliderPos });
        g.setColour (knobRim);
        g.fillRoundedRectangle (cap.expanded (1.0f), 3.0f);
        juce::ColourGradient grad (knobFace.brighter (0.3f), cap.getX(), cap.getY(),
                                   knobFace.darker (0.4f), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (slider.isEnabled() ? accent : textDim);
        g.fillRect (cap.reduced (3.0f, 0.0f).withHeight (2.0f)
                        .withCentre ({ cx, sliderPos }).toFloat());
        return;
    }

    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
}

void RetroLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                         bool highlighted, bool)
{
    const bool on = b.getToggleState();
    auto bounds = b.getLocalBounds().toFloat();

    // slide switch on the left, label on the right
    const float swW = 30.0f, swH = 15.0f;
    auto sw = juce::Rectangle<float> (swW, swH)
                  .withCentre ({ bounds.getX() + swW * 0.5f + 2.0f, bounds.getCentreY() });

    g.setColour (on ? accentDark : switchOff);
    g.fillRoundedRectangle (sw, swH * 0.5f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (sw, swH * 0.5f, 1.0f);

    const float thumbR = swH * 0.5f - 1.5f;
    const float thumbX = on ? sw.getRight() - thumbR - 2.5f : sw.getX() + thumbR + 2.5f;
    g.setColour (on ? accent.brighter (0.3f) : juce::Colour (0xff7a818b));
    g.fillEllipse (thumbX - thumbR, sw.getCentreY() - thumbR, thumbR * 2.0f, thumbR * 2.0f);

    g.setColour (highlighted ? text.brighter() : text);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (b.getButtonText(),
                bounds.withTrimmedLeft (swW + 8.0f).toNearestInt(),
                juce::Justification::centredLeft);
}

void RetroLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                             const juce::Colour& bg,
                                             bool highlighted, bool down)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = bg;
    if (down)             c = c.darker (0.3f);
    else if (highlighted) c = c.brighter (0.15f);

    juce::ColourGradient grad (c.brighter (0.12f), 0.0f, bounds.getY(),
                               c.darker (0.18f), 0.0f, bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

void RetroLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (track);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (false) ? accentDark : panelEdge);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 13.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (accent);
    g.fillPath (arrow);
}

juce::Font RetroLookAndFeel::getLabelFont (juce::Label& l)
{
    juce::ignoreUnused (l);
    return juce::Font (juce::FontOptions (11.5f));
}

juce::Font RetroLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

juce::Font RetroLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (13.0f));
}
