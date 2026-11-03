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
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override { repaint(); }
    void select(int module);
    void configureSlider(juce::Slider& slider);
    void importFile(const juce::File& file);
    juce::Rectangle<float> waveformBounds() const;
    double waveformTimeAt(float x) const;

    LevelProcessor& processor;
    int selected = 0;
    float zoom = 1.f, viewStart = 0.f;
    bool spectrogramView = false;
    juce::MemoryBlock slotA, slotB;
    bool activeSlotB = false;
    bool restoringSnapshot = false;
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
    std::array<juce::Slider, 3> moduleControls;
    std::array<juce::Label, 3> moduleLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> moduleAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 7> switchAttachments;

    juce::Slider inputGain, outputGain;
    juce::Label inputLabel, outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment, outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::TextButton bypass{"⏻ BYPASS"}, settings{"⚙"}, save{"▣"}, load{"▱"}, reset{"RESET"}, importButton{"IMPORT"}, exportButton{"EXPORT"};
    juce::TextButton zoomIn{"+"}, zoomOut{"−"}, waveView{"WAVE"}, spectralView{"FFT"}, slotToggle{"A | B"};
    juce::TextButton prevPreset{"<"}, nextPreset{">"}, undo{"↶"}, redo{"↷"}, play{"▶"}, help{"?"};
    juce::Label presetName;
    juce::ComboBox cleanMode;
    juce::ToggleButton delta{"Delta"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment;
    struct EditSnapshot {
        juce::MemoryBlock parameters;
        std::vector<level::AudioRegion> regions;
    };
    std::vector<EditSnapshot> past, future;
    void captureUndo();
    void undoEdit();
    void redoEdit();
    void restoreEdit(const EditSnapshot&);
    void choosePreset(int delta);
    juce::StringArray presetNames{"Default", "Gentle Clean", "Lowend Repair", "Soft Leveling", "De-ess"};
    int presetIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelEditor)
};
