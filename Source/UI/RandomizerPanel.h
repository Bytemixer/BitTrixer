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

        mutateButton.setButtonText ("MUTATE");
        mutateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8a63d4));
        mutateButton.onClick = [this] { if (onMutate) onMutate(); };
        addAndMakeVisible (mutateButton);

        static const juce::Colour catColors[Randomizer::kNumCategories] = {
            juce::Colour (0xffe8b339),   // pickup  - coin gold
            juce::Colour (0xffff5e57),   // laser   - red
            juce::Colour (0xffff8c42),   // explode - orange
            juce::Colour (0xff63d471),   // powerup - green
            juce::Colour (0xffc95efb),   // hit     - purple
            juce::Colour (0xff4da3ff),   // jump    - blue
            juce::Colour (0xff4dd6d2),   // blip    - teal
            juce::Colour (0xffb8e84f),   // 1-up    - lime
            juce::Colour (0xffd46a8a),   // lose    - dusky rose
            juce::Colour (0xff8fa3b8),   // clang   - steel
            juce::Colour (0xffc4dbe0),   // slash   - silver-cyan
            juce::Colour (0xff9fd9c4),   // gust    - airy mint
            juce::Colour (0xffff5a2e),   // flame   - fiery red-orange
            juce::Colour (0xfff5d028),   // spark   - electric yellow
            juce::Colour (0xff86d4f5)    // shimmer - icy cyan
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
        // three equal-height rows so the new category row matches the others
        auto b = content();
        const int rowH = b.getHeight() / 3;
        auto row1 = b.removeFromTop (rowH).reduced (0, 2);
        auto row2 = b.removeFromTop (rowH).reduced (0, 2);
        auto row3 = b.reduced (0, 2);

        // row 1: RANDOM + MUTATE + first 3 categories (Pickup / Laser / Explode)
        const int cw = row1.getWidth() / 5;
        randomButton.setBounds (row1.removeFromLeft (cw).reduced (2, 0));
        mutateButton.setBounds (row1.removeFromLeft (cw).reduced (2, 0));
        for (int i = 0; i < 3; ++i)
            categoryButtons[i]->setBounds (row1.removeFromLeft (cw).reduced (2, 0));

        // row 2: categories 3..8 — variable widths so "Power-Up" fits (Hit
        // narrower). Order: powerup, hit, jump, blip, 1-up, lose.
        const float wts[6] = { 1.42f, 0.80f, 0.95f, 0.90f, 0.95f, 0.98f };
        float wsum = 0.0f; for (float wgt : wts) wsum += wgt;
        const int bw = row2.getWidth();
        for (int i = 3; i < 9; ++i)
        {
            const int colW = (i == 8) ? row2.getWidth()
                                      : (int) ((float) bw * wts[i - 3] / wsum);
            categoryButtons[i]->setBounds (row2.removeFromLeft (colW).reduced (2, 0));
        }

        // row 3: the six new categories (Clang / Slash / Gust / Flame / Spark /
        // Shimmer). "Shimmer" is the long one, so it gets a wider cell.
        const float w3[6] = { 0.93f, 0.93f, 0.86f, 0.93f, 0.93f, 1.42f };
        float w3sum = 0.0f; for (float wgt : w3) w3sum += wgt;
        const int bw3 = row3.getWidth();
        for (int i = 9; i < Randomizer::kNumCategories; ++i)
        {
            const int w = (i == Randomizer::kNumCategories - 1)
                            ? row3.getWidth()
                            : (int) ((float) bw3 * w3[i - 9] / w3sum);
            categoryButtons[i]->setBounds (row3.removeFromLeft (w).reduced (2, 0));
        }
    }

    std::function<void()> onRandom, onMutate;
    std::function<void (int)> onCategory;

private:
    juce::TextButton randomButton, mutateButton;
    juce::OwnedArray<juce::TextButton> categoryButtons;
};
