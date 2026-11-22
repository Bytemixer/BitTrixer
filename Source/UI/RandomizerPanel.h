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
//  RandomizerPanel — RANDOM, MUTATE and one button per sfxr-style category
//  generator, in four rows grouped by family. Each button's width follows its
//  label. The owner wires onRandom / onMutate / onCategory.
// ============================================================================

class RandomizerPanel : public SectionPanel
{
public:
    static constexpr float kButtonFontHeight = 14.0f;

    RandomizerPanel() : SectionPanel ("Generate")
    {
        randomButton.getProperties().set (RetroLookAndFeel::kFontHeightProperty, kButtonFontHeight);
        mutateButton.getProperties().set (RetroLookAndFeel::kFontHeightProperty, kButtonFontHeight);
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
            juce::Colour (0xff86d4f5),   // shimmer - icy cyan
            juce::Colour (0xffeda07a),   // voice   - warm peach
            juce::Colour (0xffe0782f),   // crackle - ember
            juce::Colour (0xffc7c46a),   // twang   - bowstring tan
            juce::Colour (0xffa98a6e)    // impact  - earthy brown
        };

        for (int i = 0; i < Randomizer::kNumCategories; ++i)
        {
            auto* b = categoryButtons.add (new juce::TextButton());
            b->setButtonText (Randomizer::categoryName ((Randomizer::Category) i));
            b->getProperties().set (RetroLookAndFeel::kFontHeightProperty, kButtonFontHeight);
            b->setColour (juce::TextButton::buttonColourId, catColors[i].withAlpha (0.85f));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff10151c));
            const int index = i;
            b->onClick = [this, index] { if (onCategory) onCategory (index); };
            addAndMakeVisible (b);
        }
    }

    void resized() override
    {
        using C = Randomizer::Category;
        auto cat = [this] (C c) -> juce::TextButton* { return categoryButtons[(int) c]; };
        const std::vector<std::vector<juce::TextButton*>> rows {
            { &randomButton, &mutateButton, cat (C::Pickup), cat (C::Powerup), cat (C::OneUp), cat (C::Lose) },
            { cat (C::Blip), cat (C::Jump), cat (C::Laser), cat (C::Explosion), cat (C::Voice) },
            { cat (C::Hit), cat (C::Impact), cat (C::Slash), cat (C::Clang), cat (C::Twang) },
            { cat (C::Gust), cat (C::Flame), cat (C::Crackle), cat (C::Spark), cat (C::Shimmer) } };

        auto b = content();
        const int rowH = b.getHeight() / (int) rows.size();
        for (size_t r = 0; r < rows.size(); ++r)
        {
            auto area = (r + 1 == rows.size() ? b : b.removeFromTop (rowH)).reduced (0, 2);
            layoutRow (rows[r], area);
        }
    }

    std::function<void()> onRandom, onMutate;
    std::function<void (int)> onCategory;

private:
    // label width plus padding, then the spare width shared equally
    void layoutRow (const std::vector<juce::TextButton*>& row, juce::Rectangle<int> area)
    {
        constexpr float kPad = 16.0f;   // text insets plus the gap between buttons
        std::vector<float> w;
        float total = 0.0f;
        for (auto* btn : row)
        {
            const auto font = getLookAndFeel().getTextButtonFont (*btn, area.getHeight());
            w.push_back (juce::GlyphArrangement::getStringWidth (font, btn->getButtonText()) + kPad);
            total += w.back();
        }
        const float spare = ((float) area.getWidth() - total) / (float) row.size();
        float x = (float) area.getX();
        for (size_t i = 0; i < row.size(); ++i)
        {
            const float right = i + 1 == row.size() ? (float) area.getRight() : x + w[i] + spare;
            row[i]->setBounds (juce::Rectangle<int> ((int) x, area.getY(), (int) right - (int) x,
                                                     area.getHeight()).reduced (2, 0));
            x = right;
        }
    }

    juce::TextButton randomButton, mutateButton;
    juce::OwnedArray<juce::TextButton> categoryButtons;
};
