#pragma once
#include "PluginProcessor.h"
#include "RegionManager.h"
#include "Spectrogram.h"
#include "OfflineExport.h"
#include <array>
#include <memory>

class LevelEditor final : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit LevelEditor(LevelProcessor&);
    ~LevelEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void select(int module);
    void configureSlider(juce::Slider& slider);
    void importFile(const juce::File& file);
    juce::Rectangle<float> waveformBounds() const;
    double waveformTimeAt(float x) const;

    LevelProcessor& processor;
    int selected = 3;
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache{5};
    juce::AudioThumbnail thumbnail{256, formatManager, thumbnailCache};
    level::RegionManager regions;
    LevelSpectrogram spectrogram;
    LevelOfflineExport exporter;
    juce::File importedAudio;
    bool hasImportedFile = false;
    float mouseDownX = -1.f;
    juce::String importedFilename;
    std::array<juce::TextButton, 7> tabs;
    std::array<juce::TextButton, 7> switches;
    std::array<juce::Slider, 2> moduleControls;
    std::array<juce::Label, 2> moduleLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> moduleAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 7> switchAttachments;

    juce::Slider inputGain, outputGain;
    juce::Label inputLabel, outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment, outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::TextButton bypass{"BYPASS"}, settings{"SETTINGS"}, save{"SAVE"}, load{"LOAD"}, reset{"RESET"}, importButton{"IMPORT"}, exportButton{"EXPORT"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelEditor)
};
