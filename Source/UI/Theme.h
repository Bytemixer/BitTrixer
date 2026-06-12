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

#include <juce_data_structures/juce_data_structures.h>
#include "RetroLookAndFeel.h"

// ============================================================================
//  Theme + ThemeManager — color-scheme presets and a user-customizable
//  theme. The active theme writes into the RetroColors globals; preferences
//  persist in a per-user settings file shared by Standalone and VST3.
// ============================================================================

struct Theme
{
    juce::String name;
    juce::Colour background, panel, panelEdge, panelTitle, text, textDim,
                 accent, accentDark, knobFace, knobRim, track, switchOff,
                 ledOn, trace;
    bool rainbowTicks = false;

    void apply() const
    {
        RetroColors::background = background;
        RetroColors::panel      = panel;
        RetroColors::panelEdge  = panelEdge;
        RetroColors::panelTitle = panelTitle;
        RetroColors::text       = text;
        RetroColors::textDim    = textDim;
        RetroColors::accent     = accent;
        RetroColors::accentDark = accentDark;
        RetroColors::knobFace   = knobFace;
        RetroColors::knobRim    = knobRim;
        RetroColors::track      = track;
        RetroColors::switchOff  = switchOff;
        RetroColors::ledOn      = ledOn;
        RetroColors::trace      = trace;
        RetroColors::rainbowTicks = rainbowTicks;
    }

    // build the full palette from the 8 user-editable base colors
    static Theme fromBase (const juce::String& name, juce::Colour background,
                           juce::Colour panel, juce::Colour text, juce::Colour accent,
                           juce::Colour knob, juce::Colour track, juce::Colour trace,
                           juce::Colour led)
    {
        Theme t;
        t.name = name;
        t.background = background;
        t.panel      = panel;
        t.panelEdge  = panel.darker (0.55f);
        t.panelTitle = text.interpolatedWith (accent, 0.45f);
        t.text       = text;
        t.textDim    = text.interpolatedWith (panel, 0.45f);
        t.accent     = accent;
        t.accentDark = accent.darker (0.35f);
        t.knobFace   = knob;
        t.knobRim    = knob.darker (0.8f);
        t.track      = track;
        t.switchOff  = panel.brighter (0.12f);
        t.ledOn      = led;
        t.trace      = trace;
        return t;
    }
};

namespace ThemePresets
{
    inline Theme slate()
    {
        Theme t;
        t.name = "Slate";
        t.background = juce::Colour (0xff23262b);
        t.panel      = juce::Colour (0xff2f333a);
        t.panelEdge  = juce::Colour (0xff1a1c20);
        t.panelTitle = juce::Colour (0xff9fb4cc);
        t.text       = juce::Colour (0xffd8dde5);
        t.textDim    = juce::Colour (0xff8b939f);
        t.accent     = juce::Colour (0xff4da3ff);
        t.accentDark = juce::Colour (0xff2f6fb5);
        t.knobFace   = juce::Colour (0xff52575f);
        t.knobRim    = juce::Colour (0xff15171a);
        t.track      = juce::Colour (0xff1b1d21);
        t.switchOff  = juce::Colour (0xff3c4148);
        t.ledOn      = juce::Colour (0xff63d471);
        t.trace      = juce::Colour (0xff5580ab);
        return t;
    }

    inline Theme light()
    {
        Theme t;
        t.name = "Light";
        t.background = juce::Colour (0xffd6dae1);
        t.panel      = juce::Colour (0xffe9ebef);
        t.panelEdge  = juce::Colour (0xffaeb5c0);
        t.panelTitle = juce::Colour (0xff4a5d75);
        t.text       = juce::Colour (0xff262b33);
        t.textDim    = juce::Colour (0xff6b7280);
        t.accent     = juce::Colour (0xff2f7fd6);
        t.accentDark = juce::Colour (0xff1f5fa6);
        t.knobFace   = juce::Colour (0xffc4c9d2);
        t.knobRim    = juce::Colour (0xff878e99);
        t.track      = juce::Colour (0xffbfc5cd);
        t.switchOff  = juce::Colour (0xffb4bbc5);
        t.ledOn      = juce::Colour (0xff2fa84f);
        t.trace      = juce::Colour (0xff9aa6b6);
        return t;
    }

    inline Theme cute()
    {
        Theme t;
        t.name = "Cute";
        t.background = juce::Colour (0xfff4e3ee);
        t.panel      = juce::Colour (0xfffdf2f8);
        t.panelEdge  = juce::Colour (0xffdcb8cd);
        t.panelTitle = juce::Colour (0xffa05a86);
        t.text       = juce::Colour (0xff5a3a50);
        t.textDim    = juce::Colour (0xffae849c);
        t.accent     = juce::Colour (0xffff7eb6);
        t.accentDark = juce::Colour (0xffe05a96);
        t.knobFace   = juce::Colour (0xffefd3e3);
        t.knobRim    = juce::Colour (0xffc298b1);
        t.track      = juce::Colour (0xffecd5e2);
        t.switchOff  = juce::Colour (0xffe4c5d6);
        t.ledOn      = juce::Colour (0xff8ad48a);
        t.trace      = juce::Colour (0xffd9aac6);
        return t;
    }

    inline Theme pride()
    {
        Theme t;
        t.name = "Pride";
        t.background = juce::Colour (0xff1d1d24);
        t.panel      = juce::Colour (0xff2a2a33);
        t.panelEdge  = juce::Colour (0xff141419);
        t.panelTitle = juce::Colour (0xffc9a8e8);
        t.text       = juce::Colour (0xffe9e5f1);
        t.textDim    = juce::Colour (0xff9a92ab);
        t.accent     = juce::Colour (0xffe85aa0);
        t.accentDark = juce::Colour (0xff8a4ad4);
        t.knobFace   = juce::Colour (0xff4a4655);
        t.knobRim    = juce::Colour (0xff131118);
        t.track      = juce::Colour (0xff19191f);
        t.switchOff  = juce::Colour (0xff3c3848);
        t.ledOn      = juce::Colour (0xff63d471);
        t.trace      = juce::Colour (0xff7a5fd0);
        t.rainbowTicks = true;
        return t;
    }
}

// ----------------------------------------------------------------------------

class ThemeManager
{
public:
    static constexpr int kCustomIndex = 4;

    ThemeManager()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "RetroForge";
        o.filenameSuffix = "settings";
        o.folderName = "RetroForge";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);

        presets = { ThemePresets::slate(), ThemePresets::light(),
                    ThemePresets::cute(), ThemePresets::pride() };
        loadCustom();
        applyIndex (props->getIntValue ("themeIndex", 0));
    }

    juce::StringArray names() const
    {
        juce::StringArray n;
        for (const auto& t : presets)
            n.add (t.name);
        n.add ("Custom...");
        return n;
    }

    int currentIndex() const noexcept { return index; }

    void applyIndex (int newIndex)
    {
        index = juce::jlimit (0, kCustomIndex, newIndex);
        (index == kCustomIndex ? custom : presets[(size_t) index]).apply();
        props->setValue ("themeIndex", index);
        props->saveIfNeeded();
        if (onThemeChanged)
            onThemeChanged();
    }

    // ---- custom theme: 8 user-editable base colors ----
    static constexpr int kNumBase = 8;
    static const char* baseName (int i)
    {
        static const char* names[kNumBase] = { "Background", "Panel", "Text", "Accent",
                                               "Knob", "Track", "Trace", "LED" };
        return names[juce::jlimit (0, kNumBase - 1, i)];
    }

    juce::Colour getBase (int i) const { return base[(size_t) juce::jlimit (0, kNumBase - 1, i)]; }

    void setBase (int i, juce::Colour c)
    {
        base[(size_t) juce::jlimit (0, kNumBase - 1, i)] = c;
        custom = Theme::fromBase ("Custom", base[0], base[1], base[2], base[3],
                                  base[4], base[5], base[6], base[7]);
        for (int k = 0; k < kNumBase; ++k)
            props->setValue ("custom_" + juce::String (k), base[(size_t) k].toString());
        props->saveIfNeeded();
        if (index == kCustomIndex)
            applyIndex (kCustomIndex);
    }

    std::function<void()> onThemeChanged;

private:
    void loadCustom()
    {
        const auto s = ThemePresets::slate();
        const juce::Colour defaults[kNumBase] = { s.background, s.panel, s.text, s.accent,
                                                  s.knobFace, s.track, s.trace, s.ledOn };
        for (int i = 0; i < kNumBase; ++i)
        {
            const auto stored = props->getValue ("custom_" + juce::String (i));
            base[(size_t) i] = stored.isNotEmpty() ? juce::Colour::fromString (stored)
                                                   : defaults[i];
        }
        custom = Theme::fromBase ("Custom", base[0], base[1], base[2], base[3],
                                  base[4], base[5], base[6], base[7]);
    }

    std::unique_ptr<juce::PropertiesFile> props;
    std::vector<Theme> presets;
    Theme custom;
    std::array<juce::Colour, kNumBase> base;
    int index = 0;
};
