#pragma once
#include "PluginProcessor.h"
#include <array>
#include <memory>

class LevelEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LevelEditor(LevelProcessor&);
    ~LevelEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void select(int module);
    void configureSlider(juce::Slider& slider);

    LevelProcessor& processor;
    int selected = 3;
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
    juce::TextButton bypass{"BYPASS"}, settings{"SETTINGS"}, save{"SAVE"}, load{"LOAD"}, reset{"RESET"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelEditor)
};
