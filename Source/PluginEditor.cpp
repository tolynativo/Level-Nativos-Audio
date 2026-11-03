#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr const char* names[] = { "CLEAN", "LOWEND", "GATE", "DYNAMICS", "BREATH", "SIBILANCE", "MOUTH NOISE" };
constexpr const char* enableIds[] = { "cleanOn", "lowendOn", "gateOn", "dynamicsOn", "breathOn", "sibilanceOn", "mouthOn" };
constexpr const char* ids[][2] = {
    { "clean", "" }, { "cut", "body" }, { "threshold", "reduction" },
    { "target", "speed" }, { "breath", "" }, { "deess", "" }, { "click", "" }
};
constexpr const char* titles[][2] = {
    { "CLEAN AMOUNT", "" }, { "LOW CUT", "BODY" }, { "THRESHOLD", "REDUCTION" },
    { "TARGET LEVEL", "SPEED" }, { "BREATH LEVEL", "" },
    { "DE-ESS AMOUNT", "" }, { "CLICK REDUCTION", "" }
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
    setResizable(true, true);
    setResizeLimits(960, 590, 1700, 1100);
    setSize(1100, 670);

    for (int i = 0; i < 7; ++i)
    {
        auto& tab = tabs[(size_t)i];
        tab.setButtonText(names[i]);
        tab.onClick = [this, i] { select(i); };
        tab.setColour(juce::TextButton::textColourOffId, text);
        addAndMakeVisible(tab);

        auto& sw = switches[(size_t)i];
        sw.setButtonText("ON");
        sw.setClickingTogglesState(true);
        sw.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff467861));
        sw.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff293442));
        sw.setColour(juce::TextButton::textColourOnId, text);
        addAndMakeVisible(sw);
        switchAttachments[(size_t)i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.state, enableIds[i], sw);
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

    for (auto* button : { &bypass, &settings, &save, &load, &reset })
    {
        button->setColour(juce::TextButton::buttonColourId, panel);
        button->setColour(juce::TextButton::textColourOffId, text);
        addAndMakeVisible(*button);
    }
    bypass.setClickingTogglesState(true);
    bypass.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff884c48));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.state, "bypass", bypass);

    reset.onClick = [this] {
        for (auto* param : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
                ranged->setValueNotifyingHost(ranged->getDefaultValue());
    };
    save.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save LEVEL preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.levelpreset");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fileChooser) {
                auto file = fileChooser.getResult();
                if (file != juce::File())
                {
                    juce::MemoryBlock data;
                    processor.getStateInformation(data);
                    file.replaceWithData(data.getData(), data.getSize());
                }
            });
    };
    load.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load LEVEL preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.levelpreset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fileChooser) {
                juce::MemoryBlock data;
                if (fileChooser.getResult().loadFileAsData(data))
                    processor.setStateInformation(data.getData(), (int)data.getSize());
            });
    };
    settings.onClick = [this] {
        juce::PopupMenu menu;
        menu.addItem(1, "Open Manual");
        menu.addItem(2, "Update status");
        menu.addSeparator();
        menu.addItem(3, "LEVEL v0.3 development", false, false);
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
    select(3);
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
        if (active)
        {
            moduleLabels[i].setText(titles[selected][i], juce::dontSendNotification);
            moduleAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state, id, moduleControls[i]);
        }
    }
    for (int i = 0; i < 7; ++i)
        tabs[(size_t)i].setColour(juce::TextButton::buttonColourId,
            i == selected ? juce::Colour(0xff536b78) : panel);
    repaint();
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

    // This is live recent audio; it does not pretend to be a clip-based editable analysis.
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
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    g.drawText(juce::String(names[selected]) + "  /  LIVE AUDIO", (int)x + 16, (int)y + 12, 280, 20, juce::Justification::left);

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
    g.drawText("LEVEL  •  NATIVOS AUDIO  •  DEVELOPMENT BUILD", 210, (int)h - 41, (int)w - 225, 22, juce::Justification::left);
}

juce::AudioProcessorEditor* LevelProcessor::createEditor()
{
    return new LevelEditor(*this);
}
