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

#include "OverlayPanel.h"
#include "../ExportLocations.h"

// ============================================================================
//  SettingsPanel — the SETTINGS overlay: the export locations list (add,
//  remove, choose the default) and the About notice.
// ============================================================================

class SettingsPanel : public OverlayPanel, private juce::ListBoxModel
{
public:
    explicit SettingsPanel (juce::PropertiesFile& settingsFile)
        : OverlayPanel ("SETTINGS", 600, 420), locations (settingsFile)
    {
        sectionLabel.setText ("Export locations", juce::dontSendNotification);
        addAndMakeVisible (sectionLabel);

        hintLabel.setText ("EXPORT WAV opens on the default location. Double-click a row to make it the default.",
                           juce::dontSendNotification);
        hintLabel.setColour (juce::Label::textColourId, RetroColors::textDim);
        addAndMakeVisible (hintLabel);

        listBox.setModel (this);
        listBox.setRowHeight (28);
        listBox.setColour (juce::ListBox::backgroundColourId, RetroColors::track);
        listBox.setColour (juce::ListBox::outlineColourId, RetroColors::panelEdge);
        listBox.setOutlineThickness (1);
        addAndMakeVisible (listBox);

        addButton.setButtonText ("ADD...");
        addButton.onClick = [this] { browseToAdd(); };
        removeButton.setButtonText ("REMOVE");
        removeButton.onClick = [this] { locations.remove (listBox.getSelectedRow()); refresh(); };
        defaultButton.setButtonText ("SET DEFAULT");
        defaultButton.onClick = [this] { locations.setDefault (listBox.getSelectedRow()); refresh(); };
        aboutButton.setButtonText ("ABOUT");
        aboutButton.onClick = [this] { if (onAbout) onAbout(); };
        closeButton.setButtonText ("CLOSE");
        closeButton.onClick = [this] { setVisible (false); };
        for (auto* b : { &addButton, &removeButton, &defaultButton, &aboutButton, &closeButton })
            addAndMakeVisible (b);
    }

    void open()
    {
        refresh();
        show();
    }

    void resized() override
    {
        auto b = contentBounds();
        sectionLabel.setBounds (b.removeFromTop (24));
        hintLabel.setBounds (b.removeFromTop (24));
        b.removeFromTop (6);

        auto bottom = b.removeFromBottom (28);
        closeButton.setBounds (bottom.removeFromRight (110));
        bottom.removeFromRight (8);
        aboutButton.setBounds (bottom.removeFromLeft (110));
        b.removeFromBottom (14);

        auto rowButtons = b.removeFromBottom (28);
        addButton.setBounds (rowButtons.removeFromLeft (110));
        rowButtons.removeFromLeft (8);
        removeButton.setBounds (rowButtons.removeFromLeft (110));
        rowButtons.removeFromLeft (8);
        defaultButton.setBounds (rowButtons.removeFromLeft (130));
        b.removeFromBottom (8);
        listBox.setBounds (b);
    }

    std::function<void()> onAbout;

private:
    void refresh()
    {
        listBox.updateContent();
        listBox.repaint();
        const bool any = ! locations.list().isEmpty();
        removeButton.setEnabled (any);
        defaultButton.setEnabled (any);
    }

    void browseToAdd()
    {
        chooser = std::make_unique<juce::FileChooser> ("Add an export location", locations.defaultLocation());
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
            [safe = juce::Component::SafePointer<SettingsPanel> (this)] (const juce::FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (safe == nullptr || dir == juce::File())
                    return;
                safe->locations.add (dir);
                safe->refresh();
                safe->listBox.selectRow (safe->locations.list().indexOf (dir));
            });
    }

    // ---- ListBoxModel ----
    int getNumRows() override { return locations.list().size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        const auto dirs = locations.list();
        if (! juce::isPositiveAndBelow (row, dirs.size()))
            return;
        if (selected)
            g.fillAll (RetroColors::accentDark.withAlpha (0.6f));

        const auto& dir = dirs.getReference (row);
        const bool isDefault = row == locations.defaultIndex();
        const bool exists = dir.isDirectory();

        auto r = juce::Rectangle<int> (0, 0, width, height).reduced (10, 0);
        g.setFont (juce::Font (juce::FontOptions (14.0f, isDefault ? juce::Font::bold : juce::Font::plain)));
        if (isDefault || ! exists)
        {
            g.setColour (exists ? RetroColors::accent : RetroColors::textDim);
            g.drawText (exists ? "DEFAULT" : "MISSING", r.removeFromRight (80), juce::Justification::centredRight);
        }
        g.setColour (exists ? RetroColors::text : RetroColors::textDim);
        g.drawText (dir.getFullPathName(), r, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        locations.setDefault (row);
        refresh();
    }

    ExportLocations locations;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label sectionLabel, hintLabel;
    juce::ListBox listBox;
    juce::TextButton addButton, removeButton, defaultButton, aboutButton, closeButton;
};
