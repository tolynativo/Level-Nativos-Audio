#pragma once
#include <JuceHeader.h>
#include <array>
class LevelProcessor : public juce::AudioProcessor {
public:
 LevelProcessor();
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "Level";}
 bool acceptsMidi() const override{return false;}
 bool producesMidi() const override{return false;}
 bool isMidiEffect() const override{return false;}
 double getTailLengthSeconds() const override{return 0;}
 int getNumPrograms() override{return 1;}
 int getCurrentProgram() override{return 0;}
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override{return "Default";}
 void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 juce::AudioProcessorValueTreeState state;
 std::atomic<float> meterIn{0},meterOut{0};
 static constexpr size_t waveLength=1024;
 std::array<std::atomic<float>,waveLength> wave{};
 std::atomic<unsigned> wavePos{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout parameters();
private:
 double sampleRateHz=44100;
 // DSP histories are allocated up-front; the audio callback never allocates or locks.
 std::array<float,2> lows{},levelEnv{},gateEnv{},gateSmoothed{},prev{};
 std::array<float,2> cleanEnv{}, cleanGain{{1.f,1.f}}, dynamicsGain{{1.f,1.f}};
 std::array<float,2> deessLow{}, deessEnv{}, deessGain{{1.f,1.f}};
 std::array<float,2> lowendWet{}, cleanWet{}, gateWet{}, dynamicsWet{}, deessWet{};
 float inputSmoothed = 1.f, outputSmoothed = 1.f, processingWet = 1.f;
 float lowCutSmoothed = 75.f, bodySmoothed = 0.f;
 unsigned downsample=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelProcessor)
};
