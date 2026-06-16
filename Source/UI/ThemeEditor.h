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
#include "Theme.h"

// ============================================================================
//  ThemeEditor — the "Custom..." theme overlay: eight color swatches, each
//  opening a JUCE ColourSelector in a callout. Edits apply live and persist
//  through the ThemeManager.
// ============================================================================

class ThemeEditor : public juce::Component
{
public:
    explicit ThemeEditor (ThemeManager& tm) : themes (tm)
    {
        for (int i = 0; i < ThemeManager::kNumBase; ++i)
        {
            auto* b = swatches.add (new Swatch (themes, i));
            addAndMakeVisible (b);
        }
        closeButton.setButtonText ("CLOSE");
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (RetroColors::background.withAlpha (0.85f));

        auto panel = panelBounds().toFloat();
        g.setColour (RetroColors::panel);
        g.fillRoundedRectangle (panel, 8.0f);
        g.setColour (RetroColors::accent);
        g.drawRoundedRectangle (panel, 8.0f, 1.4f);

        g.setColour (RetroColors::text);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("CUSTOM THEME", panelBounds().withHeight (38),
                    juce::Justification::centred);
    }

    void resized() override
    {
        auto b = panelBounds().reduced (18).withTrimmedTop (36);
        closeButton.setBounds (b.removeFromBottom (26).withSizeKeepingCentre (110, 24));
        b.removeFromBottom (8);

        const int rows = 4, cols = 2;
        const int cellH = b.getHeight() / rows;
        const int cellW = b.getWidth() / cols;
        for (int i = 0; i < swatches.size(); ++i)
        {
            const int r = i / cols, c = i % cols;
            swatches[i]->setBounds (juce::Rectangle<int> (b.getX() + c * cellW,
                                                          b.getY() + r * cellH,
                                                          cellW, cellH).reduced (6, 5));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panelBounds().contains (e.getPosition()))
            setVisible (false);
    }

private:
    juce::Rectangle<int> panelBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre (360, 320);
    }

    struct Swatch : public juce::Component, private juce::ChangeListener
    {
        Swatch (ThemeManager& tm, int baseIndex) : themes (tm), index (baseIndex) {}

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            auto chip = b.removeFromLeft (44.0f).reduced (2.0f);
            g.setColour (themes.getBase (index));
            g.fillRoundedRectangle (chip, 4.0f);
            g.setColour (RetroColors::panelEdge);
            g.drawRoundedRectangle (chip, 4.0f, 1.0f);

            g.setColour (RetroColors::text);
            g.setFont (juce::Font (juce::FontOptions (12.5f)));
            g.drawText (ThemeManager::baseName (index),
                        getLocalBounds().withTrimmedLeft (52),
                        juce::Justification::centredLeft);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            auto selector = std::make_unique<juce::ColourSelector> (
                juce::ColourSelector::showColourAtTop
              | juce::ColourSelector::showSliders
              | juce::ColourSelector::showColourspace);
            selector->setCurrentColour (themes.getBase (index));
            selector->setSize (240, 220);
            selector->addChangeListener (this);
            juce::CallOutBox::launchAsynchronously (std::move (selector),
                                                    getScreenBounds(), nullptr);
        }

        void changeListenerCallback (juce::ChangeBroadcaster* src) override
        {
            if (auto* sel = dynamic_cast<juce::ColourSelector*> (src))
            {
                themes.setBase (index, sel->getCurrentColour());
                if (auto* parent = getParentComponent())
                    parent->repaint();
            }
        }

        ThemeManager& themes;
        int index;
    };

    ThemeManager& themes;
    juce::OwnedArray<Swatch> swatches;
    juce::TextButton closeButton;
};
