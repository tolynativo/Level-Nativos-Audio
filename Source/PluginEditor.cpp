#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr const char* names[] = { "CLEAN", "LOWEND", "GATE", "DYNAMICS", "BREATH", "SIBILANCE", "MOUTH NOISE" };
constexpr const char* enableIds[] = { "cleanOn", "lowendOn", "gateOn", "dynamicsOn", "breathOn", "sibilanceOn", "mouthOn" };
constexpr const char* ids[][3] = {
    { "clean", "cleanReverb", "cleanHighs" }, { "cut", "body", "" }, { "threshold", "reduction", "" },
    { "target", "speed", "" }, { "breath", "", "" }, { "deess", "", "" }, { "click", "", "" }
};
constexpr const char* titles[][3] = {
    { "NOISE REDUCTION", "REVERB REDUCTION", "RECOVER HIGHS" }, { "LOW CUT", "BODY", "" }, { "THRESHOLD", "REDUCTION", "" },
    { "TARGET LEVEL", "SPEED", "" }, { "BREATH LEVEL", "", "" },
    { "DE-ESS AMOUNT", "", "" }, { "CLICK REDUCTION", "", "" }
};
const juce::Colour bg(0xff0b111a), panel(0xff131e2a), outline(0xff2c3b4a);
const juce::Colour yellow(0xffddc466), text(0xffeef3f9), muted(0xff8295aa);
}

void LevelEditor::configureSlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 105, 21);
    s.setColour(juce::Slider::rotarySliderFillColourId, yellow);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, outline);
    s.setColour(juce::Slider::thumbColourId, yellow);
    s.setColour(juce::Slider::textBoxTextColourId, text);
    s.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    s.setColour(juce::Slider::textBoxOutlineColourId, outline);
    addAndMakeVisible(s);
}

LevelEditor::LevelEditor(LevelProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    formatManager.registerBasicFormats();
    setResizable(true, true);
    setResizeLimits(1120, 650, 1900, 1120);
    setSize(1440, 800);

    for (int i = 0; i < 7; ++i)
    {
        auto& tab = tabs[(size_t)i];
        tab.setButtonText(juce::String(names[i]).toLowerCase());
        tab.onClick = [this, i] { select(i); };
        tab.setColour(juce::TextButton::textColourOffId, text);
        addAndMakeVisible(tab);

        auto& sw = switches[(size_t)i];
        sw.setButtonText("⏻");
        sw.setClickingTogglesState(true);
        sw.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff467861));
        sw.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff293442));
        sw.setColour(juce::TextButton::textColourOnId, text);
        addAndMakeVisible(sw);
        switchAttachments[(size_t)i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.state, enableIds[i], sw);
        // These require a real event detector. Keep them visible as planned modules,
        // but do not pretend a noisy amplitude heuristic is a working vocal tool.
        if (i == 4 || i == 6)
        {
            sw.setButtonText("—");
            sw.setEnabled(false);
            tab.setTooltip("Event detector not yet available; audio is passed through safely.");
        }
    }
    for (size_t i = 0; i < moduleControls.size(); ++i)
    {
        configureSlider(moduleControls[i]);
        moduleLabels[i].setJustificationType(juce::Justification::centred);
        moduleLabels[i].setColour(juce::Label::textColourId, muted);
        addAndMakeVisible(moduleLabels[i]);
    }
    configureSlider(inputGain);
    configureSlider(outputGain);
    for (auto* label : { &inputLabel, &outputLabel })
    {
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, muted);
        addAndMakeVisible(*label);
    }
    inputLabel.setText("INPUT dB", juce::dontSendNotification);
    outputLabel.setText("OUTPUT dB", juce::dontSendNotification);
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "input", inputGain);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "output", outputGain);

    for (auto* button : { &bypass, &settings, &save, &load, &reset, &importButton, &exportButton })
    {
        button->setColour(juce::TextButton::buttonColourId, panel);
        button->setColour(juce::TextButton::textColourOffId, text);
        addAndMakeVisible(*button);
    }
    bypass.setClickingTogglesState(true);
    bypass.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff884c48));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.state, "bypass", bypass);

    importButton.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import vocal audio", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<LevelEditor> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, chooser](const juce::FileChooser& selectedFile) { if (safe) safe->importFile(selectedFile.getResult()); });
    };
    exportButton.onClick = [this] {
        if (!hasImportedFile || !importedAudio.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "LEVEL Export",
                "Import a WAV, AIFF or FLAC recording before exporting.");
            return;
        }
        if (exporter.currentStatus() == LevelOfflineExport::Status::Working) return;
        juce::PopupMenu bits;
        bits.addItem(16, "WAV 16-bit PCM");
        bits.addItem(24, "WAV 24-bit PCM");
        bits.addItem(32, "WAV 32-bit float");
        juce::Component::SafePointer<LevelEditor> safe(this);
        bits.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),
            [safe](int bitDepth) {
                if (!safe || (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)) return;
                auto chooser = std::make_shared<juce::FileChooser>("Export processed WAV",
                    safe->importedAudio.getParentDirectory().getChildFile(
                        safe->importedAudio.getFileNameWithoutExtension() + "-LEVEL.wav"), "*.wav");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [safe, chooser, bitDepth](const juce::FileChooser& selected) {
                        if (!safe) return;
                        auto output = selected.getResult();
                        if (output == juce::File()) return;
                        if (!output.hasFileExtension(".wav")) output = output.withFileExtension(".wav");
                        juce::MemoryBlock stateSnapshot;
                        safe->processor.getStateInformation(stateSnapshot);
                        auto regionSnapshot = safe->regions.all();
                        if (!safe->exporter.begin(safe->importedAudio, output, stateSnapshot,
                                                 std::move(regionSnapshot), bitDepth))
                            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                "LEVEL Export", "Unable to start export. Check the source, destination and active jobs.");
                    });
            });
    };
    reset.onClick = [this] {
        for (auto* param : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
                ranged->setValueNotifyingHost(ranged->getDefaultValue());
    };
    save.onClick = [this] {
        juce::Component::SafePointer<LevelEditor> safe(this);
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save LEVEL preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.levelpreset");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, chooser](const juce::FileChooser& fileChooser) {
                if (!safe) return;
                auto file = fileChooser.getResult();
                if (file != juce::File())
                {
                    juce::MemoryBlock data;
                    safe->processor.getStateInformation(data);
                    file.replaceWithData(data.getData(), data.getSize());
                }
            });
    };
    load.onClick = [this] {
        juce::Component::SafePointer<LevelEditor> safe(this);
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load LEVEL preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.levelpreset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, chooser](const juce::FileChooser& fileChooser) {
                if (!safe) return;
                juce::MemoryBlock data;
                if (fileChooser.getResult().loadFileAsData(data))
                    safe->processor.setStateInformation(data.getData(), (int)data.getSize());
            });
    };
    settings.onClick = [this] {
        juce::PopupMenu menu;
        menu.addItem(1, "Open Manual");
        menu.addItem(2, "Update status");
        menu.addSeparator();
        menu.addItem(3, "LEVEL v0.4.1 - DSP safety build", false, false);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settings), [](int selection) {
            if (selection == 1)
            {
#if JUCE_MAC
                auto folder = juce::File("/Library/Application Support/Nativos Audio/Level");
#elif JUCE_WINDOWS
                auto folder = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                    .getChildFile("Nativos Audio").getChildFile("Level");
#else
                auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
#endif
                const auto manual = folder.getChildFile("Level-Manual.html");
                if (manual.existsAsFile()) manual.startAsProcess();
                else juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "LEVEL", "Manual was not installed.");
            }
            if (selection == 2)
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "LEVEL Updates",
                    "Automatic installer downloads are not enabled until a signed, validated release is available.");
        });
    };
    // Real toolbar actions. Host playback is intentionally not faked inside a VST.
    for (auto* control : { &zoomIn, &zoomOut, &waveView, &spectralView,
                            &slotToggle, &prevPreset, &nextPreset, &undo, &redo,
                            &play, &help })
    {
        control->setColour(juce::TextButton::buttonColourId, panel);
        control->setColour(juce::TextButton::textColourOffId, text);
        addAndMakeVisible(*control);
    }
    undo.setEnabled(false);
    redo.setEnabled(false);
    undo.setTooltip("Edit history is not implemented yet.");
    redo.setTooltip("Edit history is not implemented yet.");
    play.setEnabled(false);
    play.setTooltip("Play the timeline from your DAW; imported-file playback is not yet implemented.");
    zoomIn.onClick = [this] { zoom = juce::jmin(12.f, zoom * 1.35f); viewStart = juce::jlimit(0.f, 1.f-1.f/zoom, viewStart); repaint(); };
    zoomOut.onClick = [this] { zoom = juce::jmax(1.f, zoom / 1.35f); viewStart = juce::jlimit(0.f, 1.f-1.f/zoom, viewStart); repaint(); };
    waveView.onClick = [this] { spectrogramView = false; repaint(); };
    spectralView.onClick = [this] { spectrogramView = true; repaint(); };
    prevPreset.onClick = [this] { choosePreset(-1); };
    nextPreset.onClick = [this] { choosePreset(1); };
    slotToggle.onClick = [this] {
        if (!activeSlotB) {
            processor.getStateInformation(slotA);
            if (slotB.isEmpty()) slotB = slotA;
            processor.setStateInformation(slotB.getData(), (int)slotB.getSize());
        } else {
            processor.getStateInformation(slotB);
            processor.setStateInformation(slotA.getData(), (int)slotA.getSize());
        }
        activeSlotB = !activeSlotB;
        slotToggle.setButtonText(activeSlotB ? "B | A" : "A | B");
    };
    presetName.setText("Default", juce::dontSendNotification);
    presetName.setJustificationType(juce::Justification::centred);
    presetName.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(presetName);
    cleanMode.addItemList({"Gentle", "Balanced", "Strong"}, 1);
    cleanMode.setSelectedId(2, juce::dontSendNotification);
    cleanMode.onChange = [this] {
        if (selected == 0) {
            const float amounts[] = { 15.f, 35.f, 65.f };
            const auto idx = juce::jlimit(0,2,cleanMode.getSelectedId()-1);
            if (auto* p = processor.state.getParameter("clean"))
                p->setValueNotifyingHost(p->convertTo0to1(amounts[idx]));
        }
    };
    addAndMakeVisible(cleanMode);
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state, "delta", delta);
    addAndMakeVisible(delta);
    help.onClick = [this] { settings.triggerClick(); };
    // The import/export functions remain real and are accessible through the
    // toolbar file menu. No decorative placeholder controls.
    load.setButtonText("▱");
    save.setButtonText("▣");
    settings.setButtonText("⚙");
    reset.setVisible(false);
    importButton.setVisible(false);
    exportButton.setVisible(false);
    settings.onClick = [this] {
        juce::PopupMenu menu;
        menu.addItem(1, "Import vocal file");
        menu.addItem(2, "Export processed WAV");
        menu.addItem(3, "Reset processing");
        menu.addItem(4, "Manual / Settings");
        menu.addItem(5, "Version: LEVEL v0.5 UI build", false, false);
        juce::Component::SafePointer<LevelEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settings), [safe](int id) {
            if (!safe) return;
            if (id == 1) safe->importButton.triggerClick();
            if (id == 2) safe->exportButton.triggerClick();
            if (id == 3) safe->reset.triggerClick();
            if (id == 4) juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "LEVEL",
                "LEVEL development build. Save/load presets and import/export are available. "
                "AI vocal event models, automatic dereverb and restore-highs are not installed.");
        });
    };
    select(0);
    startTimerHz(20);
}

void LevelEditor::select(int newSelection)
{
    selected = juce::jlimit(0, 6, newSelection);
    for (size_t i = 0; i < moduleControls.size(); ++i)
    {
        moduleAttachments[i].reset();
        const auto id = juce::String(ids[selected][i]);
        const bool active = id.isNotEmpty();
        moduleControls[i].setVisible(active);
        moduleLabels[i].setVisible(active);
        moduleControls[i].setEnabled(active && selected != 4 && selected != 6 && !(selected == 0 && i > 0));
        if (selected == 0 && i > 0)
            moduleControls[i].setTooltip("Requires validated spectral restoration engine; no DSP is connected yet.");
        if (selected == 4 || selected == 6)
            moduleControls[i].setTooltip("Pending validated breath / mouth-click detector.");
        if (active)
        {
            moduleLabels[i].setText(titles[selected][i], juce::dontSendNotification);
            moduleAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state, id, moduleControls[i]);
        }
    }
    for (int i = 0; i < 7; ++i)
        tabs[(size_t)i].setColour(juce::TextButton::buttonColourId,
            i == selected ? juce::Colour(0xff373027) : panel);
    cleanMode.setVisible(selected == 0);
    delta.setVisible(selected == 0);
    repaint();
}

void LevelEditor::choosePreset(int change)
{
    presetIndex = (presetIndex + change + presetNames.size()) % presetNames.size();
    presetName.setText(presetNames[presetIndex], juce::dontSendNotification);
    reset.triggerClick();
    auto assign = [this](const char* id, float value) {
        if (auto* p = processor.state.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    if (presetIndex == 1) { assign("cleanOn", 1.f); assign("clean", 20.f); }
    if (presetIndex == 2) { assign("lowendOn", 1.f); assign("cut", 100.f); }
    if (presetIndex == 3) { assign("dynamicsOn", 1.f); assign("target", -18.f); assign("speed", 650.f); }
    if (presetIndex == 4) { assign("sibilanceOn", 1.f); assign("deess", 23.f); }
}

void LevelEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    for (int i = 0; i < 7; ++i)
    {
        const int y = 98 + i * 65;
        tabs[(size_t)i].setBounds(16, y, 125, 43);
        switches[(size_t)i].setBounds(146, y + 5, 37, 32);
    }
    const int controlTop = 100;
    for (int i = 0; i < 2; ++i)
    {
        moduleControls[(size_t)i].setBounds(215 + 148 * i, controlTop, 130, 102);
        moduleLabels[(size_t)i].setBounds(215 + 148 * i, 202, 130, 20);
    }
    inputGain.setBounds(w - 335, controlTop, 125, 101);
    inputLabel.setBounds(w - 335, 202, 125, 20);
    outputGain.setBounds(w - 205, controlTop, 125, 101);
    outputLabel.setBounds(w - 205, 202, 125, 20);
    importButton.setBounds(16, h - 44, 79, 30);
    exportButton.setBounds(101, h - 44, 81, 30);
    const int y = 25;
    reset.setBounds(w - 482, y, 75, 34);
    load.setBounds(w - 400, y, 67, 34);
    save.setBounds(w - 325, y, 67, 34);
    settings.setBounds(w - 251, y, 109, 34);
    bypass.setBounds(w - 135, y, 113, 34);
    (void)h;
}

void LevelEditor::paint(juce::Graphics& g)
{
    const float w = (float)getWidth(), h = (float)getHeight();
    g.fillAll(bg);
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(26.f, juce::Font::bold)));
    g.drawText("LEVEL", 207, 12, 240, 40, juce::Justification::left);
    g.setColour(yellow);
    g.setFont(juce::Font(juce::FontOptions(14.f, juce::Font::bold)));
    g.drawText("NATIVOS AUDIO", 16, 12, 172, 23, juce::Justification::left);
    g.setFont(juce::Font(juce::FontOptions(12.f)));
    g.drawText("by Yai y Toly", 16, 35, 164, 25, juce::Justification::left);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    g.drawText("VOCAL PROCESSING", 208, 50, 240, 20, juce::Justification::left);

    g.setColour(outline);
    g.drawLine(194.f, 78.f, 194.f, h - 26.f);
    g.setColour(panel);
    g.fillRoundedRectangle(202.f, 88.f, w - 220.f, 150.f, 10.f);
    g.setColour(yellow);
    g.setFont(juce::Font(juce::FontOptions(12.f, juce::Font::bold)));
    g.drawText(juce::String(names[selected]) + "  •  CONTROLS", 216, 86, 380, 22, juce::Justification::left);

    const float x = 202.f, y = 249.f, graphWidth = w - 308.f, graphHeight = h - 307.f;
    g.setColour(panel);
    g.fillRoundedRectangle(x, y, graphWidth, graphHeight, 10.f);
    g.setColour(outline);
    g.drawRoundedRectangle(x, y, graphWidth, graphHeight, 10.f, 1.0f);
    g.setColour(juce::Colour(0xff263746));
    for (int row = 1; row < 6; ++row)
    {
        float gy = y + graphHeight * (float)row / 6.f;
        g.drawLine(x + 4.f, gy, x + graphWidth - 4.f, gy, 0.8f);
    }
    for (int col = 1; col < 12; ++col)
    {
        float gx = x + graphWidth * (float)col / 12.f;
        g.drawLine(gx, y + 3.f, gx, y + graphHeight - 3.f, 0.7f);
    }

    // Imported file waveform is generated by JUCE AudioThumbnail asynchronously.
    if (hasImportedFile && thumbnail.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colour(0xffe5dec7)); // Approved cream waveform colour.
        thumbnail.drawChannels(g,
            waveformBounds().toNearestInt(),
            0.0, thumbnail.getTotalLength(),
            0.86f);
    }
    else
    {
    // Live input is still available when no recording has been imported.
    juce::Path upper, lower;
    const auto total = LevelProcessor::waveLength;
    const auto current = processor.wavePos.load(std::memory_order_relaxed);
    int points = juce::jmax(1, (int)graphWidth - 24);
    for (int i = 0; i < points; ++i)
    {
        const unsigned offset = (unsigned)((points - i) * (int)total / points);
        const auto index = (current + (unsigned)total - 1u - offset) % (unsigned)total;
        const float sample = processor.wave[index].load(std::memory_order_relaxed);
        const float amp = juce::jlimit(0.f, graphHeight * .43f, std::abs(sample) * graphHeight * 2.f);
        const float xx = x + 12.f + (float)i;
        if (i == 0) { upper.startNewSubPath(xx, y + graphHeight * .5f - amp); lower.startNewSubPath(xx, y + graphHeight * .5f + amp); }
        else { upper.lineTo(xx, y + graphHeight * .5f - amp); lower.lineTo(xx, y + graphHeight * .5f + amp); }
    }
    g.setColour(yellow.withAlpha(.88f));
    g.strokePath(upper, juce::PathStrokeType(1.3f));
    g.strokePath(lower, juce::PathStrokeType(1.3f));
    }
    if (hasImportedFile && thumbnail.getTotalLength() > 0.0)
    {
        const float spectTop = waveformBounds().getBottom() + 8.f;
        const float spectBottom = h - 66.f;
        if (spectBottom > spectTop)
        {
            const juce::Rectangle<float> spectArea(206.f, spectTop, w - 316.f, spectBottom - spectTop);
            if (auto rendered = spectrogram.getImage(); rendered.isValid())
                g.drawImage(rendered, spectArea, juce::RectanglePlacement::stretchToFit);
            else {
                g.setColour(muted);
                g.drawText("SPECTROGRAM - ANALYZING", spectArea.toNearestInt(), juce::Justification::centred);
            }
            g.setColour(muted);
            g.setFont(juce::Font(juce::FontOptions(10.f)));
            g.drawText(juce::String(spectrogram.upperFrequencyHz() / 1000.f, 1) + " kHz", (int)spectArea.getX() + 5, (int)spectArea.getY() + 3, 75, 16, juce::Justification::left);
            g.drawText("200 Hz", (int)spectArea.getX() + 5, (int)spectArea.getBottom() - 19, 75, 16, juce::Justification::left);
        }
        const auto regionArea = waveformBounds();
        const auto duration = thumbnail.getTotalLength();
        for (const auto& region : regions.all())
        {
            const float left = regionArea.getX() + (float)(region.startSeconds / duration) * regionArea.getWidth();
            const float right = regionArea.getX() + (float)(region.endSeconds / duration) * regionArea.getWidth();
            if (right <= left) continue;
            const auto colour = region.enabled ? juce::Colour(0xffe9a65f) : juce::Colour(0xff66717c);
            g.setColour(colour.withAlpha(0.25f));
            g.fillRect(juce::Rectangle<float>(left, regionArea.getY(), right - left, regionArea.getHeight()));
            g.setColour(colour);
            g.drawVerticalLine((int)left, regionArea.getY(), regionArea.getBottom());
            g.drawVerticalLine((int)right, regionArea.getY(), regionArea.getBottom());
        }
    }
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    g.drawText(hasImportedFile ? "IMPORTED  /  " + importedFilename : juce::String(names[selected]) + "  /  LIVE AUDIO", (int)x + 16, (int)y + 12, (int)graphWidth - 30, 20, juce::Justification::left);

    const float meterStart = w - 91.f;
    g.setColour(panel);
    g.fillRoundedRectangle(meterStart, y, 73.f, graphHeight, 8.f);
    auto drawMeter = [&](float positionX, float peak, const char* name) {
        g.setColour(muted);
        g.drawText(name, (int)positionX - 3, (int)y + 16, 34, 20, juce::Justification::centred);
        g.setColour(outline);
        g.fillRoundedRectangle(positionX + 8.f, y + 52.f, 13.f, graphHeight - 83.f, 3.f);
        const float level = juce::jlimit(0.f, 1.f, std::sqrt(juce::jmax(0.f, peak)));
        if (level > .0001f)
        {
            g.setColour(level > .9f ? juce::Colour(0xffed956e) : juce::Colour(0xff8fd0a6));
            const float available = graphHeight - 87.f;
            g.fillRoundedRectangle(positionX + 10.f, y + 54.f + available * (1.f - level), 9.f, available * level, 2.f);
        }
    };
    drawMeter(w - 86.f, processor.meterIn.load(std::memory_order_relaxed), "IN");
    drawMeter(w - 51.f, processor.meterOut.load(std::memory_order_relaxed), "OUT");
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(12.f)));
    const auto exportStatus = exporter.currentStatus();
    const char* statusText = exportStatus == LevelOfflineExport::Status::Working ? "  |  EXPORTING WAV..." :
                             exportStatus == LevelOfflineExport::Status::Complete ? "  |  WAV SAVED" :
                             exportStatus == LevelOfflineExport::Status::Failed ? "  |  EXPORT ERROR" : "";
    g.drawText(juce::String("LEVEL v0.4.1  •  NATIVOS AUDIO  •  DSP SAFETY BUILD") + statusText, 210, (int)h - 41, (int)w - 225, 22, juce::Justification::left);
}

juce::AudioProcessorEditor* LevelProcessor::createEditor()
{
    return new LevelEditor(*this);
}

juce::Rectangle<float> LevelEditor::waveformBounds() const
{
    return { 206.f, 276.f, (float)getWidth() - 316.f, ((float)getHeight() - 347.f) * 0.52f };
}

double LevelEditor::waveformTimeAt(float x) const
{
    const auto bounds = waveformBounds();
    const auto duration = thumbnail.getTotalLength();
    if (duration <= 0.0 || bounds.getWidth() <= 0.0f) return 0.0;
    return duration * (double)juce::jlimit(0.f, 1.f, (x - bounds.getX()) / bounds.getWidth());
}

bool LevelEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (juce::StringArray{ ".wav", ".aif", ".aiff", ".flac" }.contains(
                juce::File(path).getFileExtension().toLowerCase()))
            return true;
    return false;
}

void LevelEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (!files.isEmpty()) importFile(juce::File(files[0]));
}

void LevelEditor::importFile(const juce::File& file)
{
    if (!file.existsAsFile()) return;
    const auto ext = file.getFileExtension().toLowerCase();
    if (ext != ".wav" && ext != ".aif" && ext != ".aiff" && ext != ".flac")
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "LEVEL Import", "Supported formats: WAV, AIFF and FLAC.");
        return;
    }
    hasImportedFile = thumbnail.setSource(new juce::FileInputSource(file));
    if (hasImportedFile) {
        spectrogram.analyse(file);
        importedFilename = file.getFileName();
        importedAudio = file;
        regions.clear();
    } else {
        importedFilename.clear();
        importedAudio = juce::File();
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "LEVEL Import", "Unable to read the selected recording.");
    }
    repaint();
}

void LevelEditor::mouseDown(const juce::MouseEvent& e)
{
    mouseDownX = -1.f;
    if (!hasImportedFile || !waveformBounds().contains(e.position)) return;
    if (e.mods.isPopupMenu())
    {
        const double seconds = waveformTimeAt(e.position.x);
        for (const auto& region : regions.all())
            if (seconds >= region.startSeconds && seconds <= region.endSeconds)
            {
                regions.erase(region.id);
                repaint();
                break;
            }
    }
    else mouseDownX = e.position.x;
}

void LevelEditor::mouseUp(const juce::MouseEvent& e)
{
    if (mouseDownX < 0.f || !hasImportedFile) return;
    const auto left = juce::jmin(mouseDownX, e.position.x);
    const auto right = juce::jmax(mouseDownX, e.position.x);
    mouseDownX = -1.f;
    if (right - left < 5.f) return;
    // Only categories with explicit time regions can be manually selected.
    level::RegionType type;
    switch (selected)
    {
        case 1: type = level::RegionType::Plosive; break;
        case 2: type = level::RegionType::Pause; break;
        case 4: type = level::RegionType::Breath; break;
        case 5: type = level::RegionType::Sibilance; break;
        case 6: type = level::RegionType::MouthNoise; break;
        default: return;
    }
    regions.add(waveformTimeAt(left), waveformTimeAt(right), type);
    repaint();
}
