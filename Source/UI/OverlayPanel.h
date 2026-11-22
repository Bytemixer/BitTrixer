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

// ============================================================================
//  OverlayPanel — a full-editor overlay holding one titled, centred dialog
//  card. Uses a LookAndFeel with larger text than the front panel. A click
//  outside the card closes it while canDismiss() is true.
// ============================================================================

class OverlayPanel : public juce::Component
{
public:
    OverlayPanel (juce::String titleText, int cardWidth, int cardHeight)
        : title (std::move (titleText)), cardW (cardWidth), cardH (cardHeight)
    {
        setLookAndFeel (&dialogLookAndFeel);
    }

    ~OverlayPanel() override { setLookAndFeel (nullptr); }

    void show()
    {
        dialogLookAndFeel.applyThemeColours();
        sendLookAndFeelChange();
        setVisible (true);
        toFront (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (RetroColors::background.withAlpha (0.85f));
        auto card = cardBounds().toFloat();
        g.setColour (RetroColors::panel);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (RetroColors::accent);
        g.drawRoundedRectangle (card, 8.0f, 1.4f);

        g.setColour (RetroColors::text);
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.drawText (title, cardBounds().withHeight (40), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (canDismiss() && ! cardBounds().contains (e.getPosition()))
            setVisible (false);
    }

protected:
    // a card size of 0 or less fills the overlay, less a margin
    juce::Rectangle<int> cardBounds() const
    {
        constexpr int kMargin = 40;
        return getLocalBounds().withSizeKeepingCentre (cardW > 0 ? cardW : getWidth()  - 2 * kMargin,
                                                       cardH > 0 ? cardH : getHeight() - 2 * kMargin);
    }

    // the area under the title, inside the card margin
    juce::Rectangle<int> contentBounds() const
    {
        return cardBounds().reduced (18).withTrimmedTop (30);
    }

    virtual bool canDismiss() const { return true; }

private:
    struct DialogLookAndFeel : RetroLookAndFeel
    {
        float fontScale() const override { return 1.25f; }
    };

    // a base member, so it outlives every child component of the derived panel
    DialogLookAndFeel dialogLookAndFeel;
    juce::String title;
    int cardW, cardH;
};
