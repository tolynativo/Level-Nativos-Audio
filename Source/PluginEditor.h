#pragma once
#include "PluginProcessor.h"
class LevelEditor:public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit LevelEditor(LevelProcessor&);
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 void timerCallback() override {repaint();}
 void select(int);
 LevelProcessor& processor;
 int selected=3;
 std::array<juce::TextButton,7> tabs, switches;
 std::array<juce::Slider,3> controls;
 std::array<juce::Label,3> labels;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,3> controlLinks;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,7> switchLinks;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassLink;
 juce::TextButton bypass{"BYPASS"},settings{"SETTINGS"},save{"SAVE"},load{"LOAD"},reset{"RESET"};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelEditor)
};
