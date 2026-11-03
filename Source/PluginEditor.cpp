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


namespace {
class LevelKnobLook final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                          float pos,float from,float to,juce::Slider&) override
    {
        const float r=juce::jmax(12.f,juce::jmin((float)width,(float)height)*.43f);
        const float cx=(float)x+(float)width*.5f,cy=(float)y+(float)height*.5f;
        const auto gold=juce::Colour(0xffffd02e);
        g.setColour(juce::Colour(0xff0b0d10));
        g.fillEllipse(cx-r,cy-r,2.f*r,2.f*r);
        g.setColour(juce::Colour(0xff2e323a));
        g.drawEllipse(cx-r+2.f,cy-r+2.f,2.f*r-4.f,2.f*r-4.f,3.f);
        juce::Path background,progress;
        background.addCentredArc(cx,cy,r-5.f,r-5.f,0.f,from,to,true);
        progress.addCentredArc(cx,cy,r-5.f,r-5.f,0.f,from,from+pos*(to-from),true);
        g.setColour(juce::Colour(0xff373b42));
        g.strokePath(background,juce::PathStrokeType(4.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(gold);
        g.strokePath(progress,juce::PathStrokeType(4.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float angle=from+pos*(to-from);
        g.setColour(juce::Colour(0xffeeeeee));
        g.drawLine(cx+std::sin(angle)*(r*.19f),cy-std::cos(angle)*(r*.19f),
                   cx+std::sin(angle)*(r*.77f),cy-std::cos(angle)*(r*.77f),2.f);
    }
};
LevelKnobLook levelKnobLook;
} // namespace

void LevelEditor::configureSlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setLookAndFeel(&levelKnobLook);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 92, 20);
    s.setNumDecimalPlacesToDisplay(1);
    s.setColour(juce::Slider::rotarySliderFillColourId, yellow);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, outline);
    s.setColour(juce::Slider::thumbColourId, yellow);
    s.setColour(juce::Slider::textBoxTextColourId, text);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff101215));
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
    const int w=getWidth(),h=getHeight();
    const int rail=252, right=w-18;
    for(int i=0;i<7;++i)
    {
        const int y=140+i*59;
        tabs[(size_t)i].setBounds(17,y,186,49);
        switches[(size_t)i].setBounds(209,y+4,38,40);
    }
    // The reference's header is implemented with functional JUCE buttons.
    undo.setBounds(506,27,38,34);
    redo.setBounds(546,27,38,34);
    play.setBounds(600,27,40,34);
    slotToggle.setBounds(659,27,67,34);
    prevPreset.setBounds(748,27,35,34);
    presetName.setBounds(785,27,166,34);
    nextPreset.setBounds(956,27,35,34);
    save.setBounds(w-350,27,38,34);
    load.setBounds(w-304,27,38,34);
    settings.setBounds(w-257,27,38,34);
    help.setBounds(w-210,27,38,34);
    bypass.setBounds(w-164,20,146,46);
    const int controlsTop=h-205;
    for(int i=0;i<3;++i)
    {
        const int x=rail+200+i*185;
        moduleControls[(size_t)i].setBounds(x,controlsTop+16,125,113);
        moduleLabels[(size_t)i].setBounds(x-10,controlsTop+120,145,19);
    }
    delta.setBounds(w-452,controlsTop+51,96,40);
    cleanMode.setBounds(w-272,controlsTop+76,202,31);
    inputGain.setBounds(300,h-62,76,50);
    outputGain.setBounds(w-156,h-62,76,50);
    inputLabel.setBounds(312,h-23,75,16);
    outputLabel.setBounds(w-144,h-23,75,16);
    zoomOut.setBounds(w-167,118,34,33);
    zoomIn.setBounds(w-126,118,34,33);
    waveView.setBounds(w-330,118,67,33);
    spectralView.setBounds(w-260,118,67,33);
    // File actions are accessible in Settings and do not clutter the reference UI.
    importButton.setBounds(0,0,0,0);
    exportButton.setBounds(0,0,0,0);
    reset.setBounds(0,0,0,0);
}

void LevelEditor::paint(juce::Graphics& g)
{
    const float w=(float)getWidth(),h=(float)getHeight();
    const juce::Colour background(0xff101114), panelBg(0xff191b20), panelHi(0xff202126);
    const juce::Colour gold(0xffffd02e), white(0xfff4f4f6), secondary(0xffb7bac1), darkLine(0xff36383e);
    g.fillAll(background);
    // Top shadow and header.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1e2025),0,0,background,0,112,false));
    g.fillRect(0,0,getWidth(),110);
    g.setColour(gold);
    // Graphic mark: drawn vectorially, not a raster screenshot.
    juce::Path mark;
    mark.startNewSubPath(88.f,19.f);mark.lineTo(88.f,63.f);
    mark.lineTo(112.f,38.f);mark.lineTo(112.f,63.f);
    g.strokePath(mark,juce::PathStrokeType(4.f));
    g.drawLine(93.f,17.f,116.f,44.f,3.f);
    g.setFont(juce::Font(juce::FontOptions(15.f,juce::Font::bold)));
    g.setColour(white);g.drawText("N A T I V O S  A U D I O",21,69,194,17,juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(9.f)));
    g.drawText("B Y   Y A I   Y   T O L Y",34,88,172,16,juce::Justification::centred);
    g.setColour(gold);g.fillRect(217,26,2,55);
    g.setColour(white);
    g.setFont(juce::Font(juce::FontOptions(39.f,juce::Font::bold)));
    g.drawText("L E V E L",257,17,240,49,juce::Justification::left);
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    g.drawText("A I   V O C A L   A S S I S T A N T",259,68,250,21,juce::Justification::left);
    // Approved left module rail.
    const juce::Colour dots[]={juce::Colour(0xffffd02e),juce::Colour(0xffff9f29),
        juce::Colour(0xffff5f63),juce::Colour(0xffb677df),juce::Colour(0xff5d9ddd),
        juce::Colour(0xff67ca72),juce::Colour(0xff53c8bf)};
    for(int i=0;i<7;++i)
    {
        const int yy=140+i*59;
        g.setColour(dots[i]);g.fillEllipse(28.f,(float)yy+20.f,11.f,11.f);
        if(selected==i){g.setColour(gold);g.drawRoundedRectangle(18.f,(float)yy+1.f,227.f,47.f,11.f,1.4f);}
    }
    // Analysis header above real waveform/spectrogram.
    g.setColour(juce::Colour(0xff22242a));
    g.fillRoundedRectangle(261.f,108.f,w-278.f,59.f,9.f);
    g.setColour(darkLine);
    g.drawLine(514.f,121.f,514.f,158.f);
    g.drawLine(757.f,121.f,757.f,158.f);
    g.drawLine(1010.f,121.f,1010.f,158.f);
    const char* topLabels[]={"NOISE REDUCTION","REVERB REDUCTION","RECOVER HIGHS","PROCESSING"};
    const float xs[]={278.f,530.f,772.f,1026.f};
    const float widths[]={220.f,216.f,220.f,175.f};
    g.setFont(juce::Font(juce::FontOptions(12.f)));
    for(int i=0;i<4;++i){g.setColour(white);g.drawText(topLabels[i],(int)xs[i],110,(int)widths[i],18,juce::Justification::centred);}
    const auto paramValue=[this](const char* id){return processor.state.getRawParameterValue(id)->load();};
    const bool cleanOn=paramValue("cleanOn")>.5f;
    g.setFont(juce::Font(juce::FontOptions(15.f,juce::Font::bold)));
    g.setColour(gold);
    g.drawText(juce::String(juce::roundToInt(paramValue("clean")))+"%",278,131,220,24,juce::Justification::centred);
    g.setColour(secondary);
    g.drawText("NOT AVAILABLE",530,131,216,24,juce::Justification::centred);
    g.drawText("NOT AVAILABLE",772,131,220,24,juce::Justification::centred);
    g.setColour(cleanOn?gold:secondary);
    g.drawText(cleanOn?"On":"Off",1026,131,175,24,juce::Justification::centred);
    // Import and live input are both drawn from actual samples, not canned waveforms.
    const auto waveArea=waveformBounds();
    g.setColour(juce::Colour(0xff0d0f13));
    g.fillRoundedRectangle(waveArea.expanded(3.f),9.f);
    g.setColour(juce::Colour(0xff2e3035));
    g.drawRoundedRectangle(waveArea.expanded(3.f),9.f,1.f);
    g.setColour(juce::Colour(0xff292b30));
    for(int i=1;i<8;++i)
    {
        const float x=waveArea.getX()+waveArea.getWidth()*(float)i/8.f;
        g.drawVerticalLine((int)x,waveArea.getY(),waveArea.getBottom());
    }
    for(int i=1;i<4;++i)
    {
        const float y=waveArea.getY()+waveArea.getHeight()*(float)i/4.f;
        g.drawHorizontalLine((int)y,waveArea.getX(),waveArea.getRight());
    }
    const double length=thumbnail.getTotalLength();
    if(hasImportedFile&&length>0.0)
    {
        const double start=length*viewStart;
        const double end=juce::jmin(length,start+length/(double)zoom);
        if(spectrogramView)
        {
            if(auto image=spectrogram.getImage();image.isValid())
                g.drawImage(image,waveArea,juce::RectanglePlacement::stretchToFit);
            else {g.setColour(secondary);g.drawText("SPECTROGRAM ANALYZING",waveArea.toNearestInt(),juce::Justification::centred);}
        }
        else
        {
            g.setColour(juce::Colour(0xffaeb0b5));
            thumbnail.drawChannels(g,waveArea.toNearestInt(),start,end,.84f);
        }
        for(const auto& region:regions.all())
        {
            const float left=waveArea.getX()+(float)((region.startSeconds-start)/(end-start))*waveArea.getWidth();
            const float right=waveArea.getX()+(float)((region.endSeconds-start)/(end-start))*waveArea.getWidth();
            const float l=juce::jlimit(waveArea.getX(),waveArea.getRight(),left);
            const float r=juce::jlimit(waveArea.getX(),waveArea.getRight(),right);
            if(r<=l)continue;
            g.setColour(gold.withAlpha(.2f));g.fillRect(l,waveArea.getY(),r-l,waveArea.getHeight());
            g.setColour(gold.withAlpha(.85f));g.drawVerticalLine((int)l,waveArea.getY(),waveArea.getBottom());
            g.drawVerticalLine((int)r,waveArea.getY(),waveArea.getBottom());
        }
        g.setColour(secondary);
        g.setFont(juce::Font(juce::FontOptions(10.f)));
        for(int i=0;i<7;++i)
        {
            const double sec=start+(end-start)*(double)i/6.0;
            const int minute=(int)sec/60, second=(int)sec%60;
            g.drawText(juce::String(minute)+":"+(second<10?"0":"")+juce::String(second),
                       (int)(waveArea.getX()+waveArea.getWidth()*i/6.f)-15,(int)waveArea.getBottom()+5,
                       48,18,juce::Justification::left);
        }
    }
    else
    {
        juce::Path upper,lower;
        const auto total=LevelProcessor::waveLength;
        const auto current=processor.wavePos.load(std::memory_order_relaxed);
        const int points=juce::jmax(1,(int)waveArea.getWidth()-10);
        for(int i=0;i<points;++i)
        {
            const unsigned offset=(unsigned)((points-i)*(int)total/points);
            const auto idx=(current+(unsigned)total-1u-offset)%(unsigned)total;
            const float sample=processor.wave[idx].load(std::memory_order_relaxed);
            const float amp=juce::jlimit(0.f,waveArea.getHeight()*.46f,std::abs(sample)*waveArea.getHeight()*1.5f);
            const float px=waveArea.getX()+5.f+i;
            const float mid=waveArea.getCentreY();
            if(i==0){upper.startNewSubPath(px,mid-amp);lower.startNewSubPath(px,mid+amp);}
            else {upper.lineTo(px,mid-amp);lower.lineTo(px,mid+amp);}
        }
        g.setColour(juce::Colour(0xffafb0b4));
        g.strokePath(upper,juce::PathStrokeType(1.3f));
        g.strokePath(lower,juce::PathStrokeType(1.3f));
        g.setColour(secondary);
        g.setFont(juce::Font(juce::FontOptions(12.f)));
        g.drawText("LIVE AUDIO — press Play in your DAW",waveArea.toNearestInt().reduced(17),juce::Justification::topLeft);
    }
    // Approved lower module control panel.
    const float lowerY=h-205.f;
    g.setGradientFill(juce::ColourGradient(panelHi,250.f,lowerY,panelBg,w-10.f,h-25.f,false));
    g.fillRoundedRectangle(18.f,lowerY,w-36.f,169.f,14.f);
    g.setColour(darkLine);
    g.drawRoundedRectangle(18.f,lowerY,w-36.f,169.f,14.f,1.f);
    g.setColour(white);
    g.setFont(juce::Font(juce::FontOptions(22.f,juce::Font::bold)));
    g.drawText(names[selected],41,(int)lowerY+18,175,35,juce::Justification::left);
    g.setColour(secondary);
    g.setFont(juce::Font(juce::FontOptions(13.f)));
    const char* desc[]={
        "Reduce low-level noise\nwith a gentle envelope.",
        "Shape low-frequency rumble\nand unwanted proximity.",
        "Reduce level between\nvocal phrases.",
        "Smooth out changes\nin vocal loudness.",
        "Requires vocal-event\ndetection model.",
        "Control excessive\nhigh-frequency energy.",
        "Requires verified\nclick detection."
    };
    g.drawFittedText(desc[selected],43,(int)lowerY+62,206,72,juce::Justification::topLeft,3);
    g.setFont(juce::Font(juce::FontOptions(12.f)));
    if(selected==0)
    {
        g.setColour(secondary);g.drawText("Delta",w-447,(int)lowerY+30,85,20,juce::Justification::centred);
        g.drawText("MODE",w-275,(int)lowerY+39,205,23,juce::Justification::left);
        g.setColour(gold);
        g.drawText("SPECTRAL MODELS NOT INSTALLED",710,(int)lowerY+143,350,17,juce::Justification::left);
    }
    // Bottom meters are driven by actual input/output peak atomics.
    const float footerY=h-27.f;
    auto meter=[&](float left,float peak,const char* label)
    {
        g.setColour(secondary);g.setFont(juce::Font(juce::FontOptions(11.f)));
        g.drawText(label,(int)left-60,(int)footerY-5,55,17,juce::Justification::right);
        g.setColour(juce::Colour(0xff2b2d32));
        g.fillRoundedRectangle(left,footerY,158.f,9.f,4.f);
        const float fraction=juce::jlimit(0.f,1.f,std::sqrt(juce::jmax(0.f,peak)));
        if(fraction>0.0001f)
        {
            g.setColour(gold);
            g.fillRoundedRectangle(left,footerY,158.f*fraction,9.f,4.f);
        }
    };
    meter(76.f,processor.meterIn.load(std::memory_order_relaxed),"INPUT");
    meter(w-279.f,processor.meterOut.load(std::memory_order_relaxed),"OUTPUT");
    g.setColour(secondary);
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    const char* status=exporter.currentStatus()==LevelOfflineExport::Status::Working?"EXPORTING...":
        exporter.currentStatus()==LevelOfflineExport::Status::Complete?"WAV EXPORTED":
        exporter.currentStatus()==LevelOfflineExport::Status::Failed?"EXPORT FAILED":
        hasImportedFile?"IMPORTED AUDIO READY":"LIVE INPUT";
    g.drawText(status,(int)(w*.46f),(int)footerY-7,210,25,juce::Justification::centred);
}

juce::AudioProcessorEditor* LevelProcessor::createEditor()
{
    return new LevelEditor(*this);
}

juce::Rectangle<float> LevelEditor::waveformBounds() const
{
    return { 261.f, 181.f, (float)getWidth()-279.f, (float)getHeight()-433.f };
}

double LevelEditor::waveformTimeAt(float x) const
{
    const auto bounds=waveformBounds();
    const auto length=thumbnail.getTotalLength();
    if(length<=0.0||bounds.getWidth()<=0.0f)return 0.0;
    const float pos=juce::jlimit(0.f,1.f,(x-bounds.getX())/bounds.getWidth());
    return length*(viewStart+(double)pos/(double)zoom);
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
