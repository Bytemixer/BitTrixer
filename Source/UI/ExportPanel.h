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
#include "../BatchExporter.h"
#include "../ExportLocations.h"

// ============================================================================
//  ExportPanel — the EXPORT WAV overlay: file name, directory (the export
//  locations from SETTINGS, or a one-off browse), an optional folder named
//  after the file, count, variation amount, seed and level match. "Export to"
//  lists every file to be written. Renders on a BatchExport::Job and reports
//  each file's LUFS, true peak and gain. Choices persist in the settings file.
// ============================================================================

class ExportPanel : public OverlayPanel, private juce::Timer
{
public:
    ExportPanel (juce::AudioProcessorValueTreeState& state, juce::PropertiesFile& settingsFile)
        : OverlayPanel ("EXPORT WAV", 640, 690),
          apvts (state), props (settingsFile), locations (settingsFile)
    {
        auto label = [this] (juce::Label& l, const char* text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (l);
        };
        label (nameLabel, "File name");
        label (folderLabel, "Directory");
        label (exportToLabel, "Export to");
        label (countLabel, "Count");
        label (amountLabel, "Amount");
        label (seedLabel, "Seed");
        label (levelLabel, "Level match");
        label (targetLabel, "Target");

        nameEditor.onTextChange = [this] { updateSummary(); };
        addAndMakeVisible (nameEditor);

        folderBox.onChange = [this] { folderChosen(); };
        addAndMakeVisible (folderBox);
        browseButton.setButtonText ("BROWSE...");
        browseButton.onClick = [this] { browse(); };
        addAndMakeVisible (browseButton);

        subfolderToggle.setButtonText ("Create a folder named after the file");
        subfolderToggle.onClick = [this] { updateSummary(); };
        addAndMakeVisible (subfolderToggle);

        countSlider.setSliderStyle (juce::Slider::IncDecButtons);
        countSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 24);
        countSlider.setRange (1.0, (double) BatchExport::kMaxCount, 1.0);
        countSlider.onValueChange = [this] { updateEnablement(); updateSummary(); };
        addAndMakeVisible (countSlider);

        amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
        amountSlider.setRange (0.0, 1.0, 0.001);
        amountSlider.setTooltip ("Variation width, the same scale as the VARIATE amount");
        addAndMakeVisible (amountSlider);

        keepOriginalToggle.setButtonText ("_0 is the unchanged sound");
        addAndMakeVisible (keepOriginalToggle);

        seedEditor.setInputRestrictions (9, "0123456789");
        seedEditor.setTooltip ("The same seed re-renders the same batch");
        addAndMakeVisible (seedEditor);
        diceButton.setButtonText ("NEW SEED");
        diceButton.onClick = [this] { rollSeed(); };
        addAndMakeVisible (diceButton);

        levelBox.addItemList ({ "Off", "Match peak", "Match loudness", "Loudness target" }, 1);
        levelBox.onChange = [this] { updateEnablement(); };
        addAndMakeVisible (levelBox);

        targetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        targetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 22);
        targetSlider.setRange (-36.0, -6.0, 0.5);
        targetSlider.setTextValueSuffix (" LUFS");
        addAndMakeVisible (targetSlider);

        infoLabel.setText ("Loudness = LUFS (ungated, loudest 100 ms).  Ceiling "
                           + juce::String (BatchExport::kCeilingDbTP, 1) + " dBTP, never clipped.",
                           juce::dontSendNotification);
        infoLabel.setColour (juce::Label::textColourId, RetroColors::textDim);
        addAndMakeVisible (infoLabel);
        exportToBox.setMultiLine (true, true);
        exportToBox.setReadOnly (true);
        exportToBox.setScrollbarsShown (true);
        exportToBox.setCaretVisible (false);
        addAndMakeVisible (exportToBox);

        addAndMakeVisible (progressBar);

        report.setMultiLine (true);
        report.setReadOnly (true);
        report.setScrollbarsShown (true);
        report.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                       12.5f, juce::Font::plain)));
        addAndMakeVisible (report);

        exportButton.setButtonText ("EXPORT");
        exportButton.setColour (juce::TextButton::buttonColourId, RetroColors::accentDark);
        exportButton.onClick = [this] { requestExport(); };
        addAndMakeVisible (exportButton);
        closeButton.setButtonText ("CLOSE");
        closeButton.onClick = [this] { closeOrCancel(); };
        addAndMakeVisible (closeButton);

        loadStoredSettings();
        rollSeed();
    }

    ~ExportPanel() override { job.reset(); }

    // show the panel for the current sound
    void open (const juce::String& soundName, double sampleRate)
    {
        renderRate = sampleRate > 8000.0 ? sampleRate : 44100.0;
        nameEditor.setText (legalName (soundName), false);
        amountSlider.setValue (apvts.getRawParameterValue (Params::id::autoVarAmt)->load(),
                               juce::dontSendNotification);
        folder = locations.defaultLocation();
        rebuildFolderBox();
        updateEnablement();
        updateSummary();
        show();
    }

    void resized() override
    {
        auto b = contentBounds();
        static constexpr int rowH = 26, gap = 6, labelW = 92;

        auto row = [&b] { auto r = b.removeFromTop (rowH); b.removeFromTop (gap); return r; };

        auto r = row();
        nameLabel.setBounds (r.removeFromLeft (labelW));
        nameEditor.setBounds (r);

        r = row();
        folderLabel.setBounds (r.removeFromLeft (labelW));
        browseButton.setBounds (r.removeFromRight (96));
        r.removeFromRight (6);
        folderBox.setBounds (r);

        r = row();
        subfolderToggle.setBounds (r.withTrimmedLeft (labelW));

        r = row();
        countLabel.setBounds (r.removeFromLeft (labelW));
        countSlider.setBounds (r.removeFromLeft (130));
        r.removeFromLeft (24);
        seedLabel.setBounds (r.removeFromLeft (40));
        diceButton.setBounds (r.removeFromRight (88));
        r.removeFromRight (6);
        seedEditor.setBounds (r);

        r = row();
        amountLabel.setBounds (r.removeFromLeft (labelW));
        amountSlider.setBounds (r);

        r = row();
        keepOriginalToggle.setBounds (r.withTrimmedLeft (labelW));

        r = row();
        levelLabel.setBounds (r.removeFromLeft (labelW));
        levelBox.setBounds (r.removeFromLeft (150));
        r.removeFromLeft (18);
        targetLabel.setBounds (r.removeFromLeft (48));
        targetSlider.setBounds (r);

        infoLabel.setBounds (row());
        r = b.removeFromTop (78);
        b.removeFromTop (gap);
        exportToLabel.setBounds (r.removeFromLeft (labelW).removeFromTop (rowH));
        exportToBox.setBounds (r);
        progressBar.setBounds (b.removeFromTop (18));
        b.removeFromTop (gap);

        auto buttons = b.removeFromBottom (28);
        b.removeFromBottom (gap);
        report.setBounds (b);
        closeButton.setBounds (buttons.removeFromRight (110));
        buttons.removeFromRight (8);
        exportButton.setBounds (buttons.removeFromRight (110));
    }

private:
    bool canDismiss() const override { return job == nullptr; }

    static juce::String legalName (const juce::String& s)
    {
        const auto n = juce::File::createLegalFileName (s.trim());
        return n.isNotEmpty() ? n : juce::String ("BitTrixer");
    }

    BatchExport::Settings currentSettings() const
    {
        BatchExport::Settings s;
        s.name         = legalName (nameEditor.getText());
        s.folder       = folder;
        s.subfolder    = subfolderToggle.getToggleState();
        s.count        = (int) countSlider.getValue();
        s.amount       = (float) amountSlider.getValue();
        s.keepOriginal = keepOriginalToggle.getToggleState();
        s.seed         = seedEditor.getText().getLargeIntValue();
        s.level        = (BatchExport::LevelMode) juce::jmax (0, levelBox.getSelectedId() - 1);
        s.targetLufs   = (float) targetSlider.getValue();
        return s;
    }

    // ---- directory: the export locations, plus a browsed one-off ----
    void rebuildFolderBox()
    {
        const auto saved = locations.list();
        const auto defaultDir = locations.defaultLocation();
        folderChoices = saved;
        if (! folderChoices.contains (folder))
            folderChoices.insert (0, folder);

        folderBox.clear (juce::dontSendNotification);
        for (int i = 0; i < folderChoices.size(); ++i)
        {
            const auto& f = folderChoices.getReference (i);
            const bool exists = f.isDirectory();
            juce::String text = f.getFullPathName();
            if (! saved.isEmpty() && f == defaultDir) text << "   (default)";
            if (! exists)                             text << "   (missing)";
            folderBox.addItem (text, i + 1);
            folderBox.setItemEnabled (i + 1, exists || f == folder);
        }
        folderBox.setSelectedId (folderChoices.indexOf (folder) + 1, juce::dontSendNotification);
        updateSummary();
    }

    void folderChosen()
    {
        const int idx = folderBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (idx, folderChoices.size()) && folderChoices[idx] != folder)
        {
            folder = folderChoices[idx];
            rebuildFolderBox();
        }
    }

    void browse()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export folder", folder);
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
            [safe = juce::Component::SafePointer<ExportPanel> (this)] (const juce::FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (safe == nullptr || dir == juce::File())
                    return;
                safe->folder = dir;
                safe->rebuildFolderBox();
            });
    }

    // ---- state ----
    void rollSeed()
    {
        seedEditor.setText (juce::String (juce::Random::getSystemRandom().nextInt (1000000000)), false);
    }

    void loadStoredSettings()
    {
        subfolderToggle.setToggleState (props.getBoolValue ("exportMakeFolder", true), juce::dontSendNotification);
        countSlider.setValue (props.getIntValue ("exportCount", 1), juce::dontSendNotification);
        keepOriginalToggle.setToggleState (props.getBoolValue ("exportKeepOriginal", true), juce::dontSendNotification);
        levelBox.setSelectedId (props.getIntValue ("exportLevelMode", 0) + 1, juce::dontSendNotification);
        targetSlider.setValue (props.getDoubleValue ("exportTargetLufs", -16.0), juce::dontSendNotification);
    }

    void storeSettings (const BatchExport::Settings& s)
    {
        props.setValue ("exportMakeFolder", s.subfolder);
        props.setValue ("exportCount", s.count);
        props.setValue ("exportKeepOriginal", s.keepOriginal);
        props.setValue ("exportLevelMode", (int) s.level);
        props.setValue ("exportTargetLufs", (double) s.targetLufs);
    }

    void updateEnablement()
    {
        const bool idle  = job == nullptr;
        const bool batch = countSlider.getValue() > 1.5;
        for (juce::Component* c : { (juce::Component*) &nameEditor, (juce::Component*) &folderBox,
                                    (juce::Component*) &browseButton,
                                    (juce::Component*) &subfolderToggle, (juce::Component*) &countSlider,
                                    (juce::Component*) &levelBox, (juce::Component*) &exportButton })
            c->setEnabled (idle);
        for (juce::Component* c : { (juce::Component*) &amountSlider, (juce::Component*) &keepOriginalToggle,
                                    (juce::Component*) &seedEditor, (juce::Component*) &diceButton })
            c->setEnabled (idle && batch);
        targetSlider.setEnabled (idle && levelBox.getSelectedId() - 1 == (int) BatchExport::LevelMode::Target);
        closeButton.setButtonText (idle ? "CLOSE" : "CANCEL");
    }

    // the directory on the first line, then every file name
    void updateSummary()
    {
        const auto s = currentSettings();
        juce::StringArray names;
        for (const auto& f : BatchExport::outputFiles (s))
            names.add (f.getFileName());
        exportToBox.setText (BatchExport::outputDir (s).getFullPathName()
                               + juce::File::getSeparatorString() + "\n"
                               + names.joinIntoString (",  "), false);
    }

    // ---- export ----
    void requestExport()
    {
        const auto s = currentSettings();
        if (! s.folder.isDirectory())
        {
            report.setText ("The folder does not exist:\n" + s.folder.getFullPathName());
            return;
        }

        juce::StringArray existing;
        for (const auto& f : BatchExport::outputFiles (s))
            if (f.existsAsFile())
                existing.add (f.getFileName());

        if (existing.isEmpty())
        {
            start (s);
            return;
        }

        const int shown = juce::jmin (10, existing.size());
        juce::String list = existing.joinIntoString ("\n", 0, shown);
        if (existing.size() > shown)
            list << "\n... and " << (existing.size() - shown) << " more";

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
            "Overwrite files?", "These files already exist:\n\n" + list,
            "Overwrite", "Cancel", this,
            juce::ModalCallbackFunction::create (
                [safe = juce::Component::SafePointer<ExportPanel> (this), s] (int result)
                {
                    if (safe != nullptr && result == 1)
                        safe->start (s);
                }));
    }

    void start (const BatchExport::Settings& s)
    {
        storeSettings (s);
        report.clear();
        progressValue = 0.0;
        job = std::make_unique<BatchExport::Job> (s, std::make_unique<ShadowParams> (apvts), renderRate);
        job->startThread();
        updateEnablement();
        startTimerHz (15);
    }

    void closeOrCancel()
    {
        if (job != nullptr) job->signalThreadShouldExit();
        else                setVisible (false);
    }

    void timerCallback() override
    {
        if (job == nullptr)
        {
            stopTimer();
            return;
        }
        progressValue = (double) job->progress();
        if (! job->finished())
            return;

        stopTimer();
        showReport (*job);
        job.reset();
        updateEnablement();
    }

    void showReport (const BatchExport::Job& j)
    {
        auto num = [] (float v, int width)
        {
            const auto s = v <= Loudness::kSilenceDb ? juce::String ("-inf")
                                                     : juce::String (v, 1);
            return s.paddedLeft (' ', width);
        };

        juce::String t;
        if (j.folderFailed())
            t << "Could not create the folder.\n";
        t << "Unchanged sound: " << num (j.reference().lufs, 0) << " LUFS, "
          << num (j.reference().truePeakDb, 0) << " dBTP\n\n";
        t << juce::String ("File").paddedRight (' ', 28) << "   LUFS    dBTP    Gain\n";

        int written = 0;
        for (const auto& r : j.reports())
        {
            t << r.file.getFileName().paddedRight (' ', 28)
              << num (r.lufs, 7) << num (r.truePeakDb, 8)
              << (r.gainDb >= 0.0f ? "   +" : "   ") << juce::String (r.gainDb, 1)
              << (r.limited ? "  held at ceiling" : "")
              << (r.written ? "" : "  NOT WRITTEN") << "\n";
            written += r.written ? 1 : 0;
        }
        const int asked = currentSettings().count;
        t << "\n" << written << " of " << asked << " files written"
          << (j.reports().size() < (size_t) asked ? " (cancelled)" : "") << ".";
        report.setText (t);
        progressValue = 1.0;
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::PropertiesFile& props;
    ExportLocations locations;

    juce::File folder;
    juce::Array<juce::File> folderChoices;
    double renderRate = 44100.0;
    double progressValue = 0.0;
    std::unique_ptr<BatchExport::Job> job;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label nameLabel, folderLabel, countLabel, amountLabel, seedLabel, levelLabel, targetLabel;
    juce::Label infoLabel, exportToLabel;
    juce::TextEditor nameEditor, seedEditor, exportToBox, report;
    juce::ComboBox folderBox, levelBox;
    juce::TextButton browseButton, diceButton, exportButton, closeButton;
    juce::ToggleButton subfolderToggle, keepOriginalToggle;
    juce::Slider countSlider, amountSlider, targetSlider;
    juce::ProgressBar progressBar { progressValue };
};
