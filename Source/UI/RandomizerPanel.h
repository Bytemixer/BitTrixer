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
#include "../Randomizer.h"

// ============================================================================
//  RandomizerPanel — the fun row: full RANDOM plus the seven colorful
//  sfxr-style category generators. The owner wires onRandom / onCategory.
// ============================================================================

class RandomizerPanel : public SectionPanel
{
public:
    RandomizerPanel() : SectionPanel ("Generate")
    {
        randomButton.setButtonText ("RANDOM");
        randomButton.setColour (juce::TextButton::buttonColourId, RetroColors::accent);
        randomButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff10151c));
        randomButton.onClick = [this] { if (onRandom) onRandom(); };
        addAndMakeVisible (randomButton);

        static const juce::Colour catColors[Randomizer::kNumCategories] = {
            juce::Colour (0xffe8b339),   // pickup  - coin gold
            juce::Colour (0xffff5e57),   // laser   - red
            juce::Colour (0xffff8c42),   // explode - orange
            juce::Colour (0xff63d471),   // powerup - green
            juce::Colour (0xffc95efb),   // hit     - purple
            juce::Colour (0xff4da3ff),   // jump    - blue
            juce::Colour (0xff4dd6d2)    // blip    - teal
        };

        for (int i = 0; i < Randomizer::kNumCategories; ++i)
        {
            auto* b = categoryButtons.add (new juce::TextButton());
            b->setButtonText (Randomizer::categoryName ((Randomizer::Category) i));
            b->setColour (juce::TextButton::buttonColourId, catColors[i].withAlpha (0.85f));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff10151c));
            const int index = i;
            b->onClick = [this, index] { if (onCategory) onCategory (index); };
            addAndMakeVisible (b);
        }
    }

    void resized() override
    {
        auto b = content();
        const int rowH = b.getHeight() / 2;
        auto top = b.removeFromTop (rowH).reduced (0, 2);
        auto bottom = b.reduced (0, 2);

        // top row: RANDOM + first 3 categories; bottom row: last 4
        const int cw = top.getWidth() / 4;
        randomButton.setBounds (top.removeFromLeft (cw).reduced (2, 0));
        for (int i = 0; i < 3; ++i)
            categoryButtons[i]->setBounds (top.removeFromLeft (cw).reduced (2, 0));
        const int cw2 = bottom.getWidth() / 4;
        for (int i = 3; i < Randomizer::kNumCategories; ++i)
            categoryButtons[i]->setBounds (bottom.removeFromLeft (cw2).reduced (2, 0));
    }

    std::function<void()> onRandom;
    std::function<void (int)> onCategory;

private:
    juce::TextButton randomButton;
    juce::OwnedArray<juce::TextButton> categoryButtons;
};
