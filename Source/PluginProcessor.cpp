#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
static void f(juce::AudioProcessorValueTreeState::ParameterLayout& p,const char* id,float lo,float hi,float init) {
 p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},id,juce::NormalisableRange<float>(lo,hi),init));
}
static void b(juce::AudioProcessorValueTreeState::ParameterLayout& p,const char* id) {
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},id,true));
}
juce::AudioProcessorValueTreeState::ParameterLayout LevelProcessor::parameters(){
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
 for(const char* id:{"cleanOn","lowendOn","gateOn","dynamicsOn","breathOn","sibilanceOn","mouthOn"}) b(p,id);
 f(p,"input",-24,24,0);f(p,"output",-24,24,0);f(p,"clean",0,100,25);
 f(p,"cut",20,250,75);f(p,"body",-12,12,0);
 f(p,"threshold",-80,-15,-45);f(p,"reduction",-72,0,-36);
 f(p,"target",-36,-9,-18);f(p,"speed",50,1500,400);
 f(p,"breath",-18,0,-4);f(p,"deess",0,100,30);f(p,"click",0,100,25);
 return p;
}
LevelProcessor::LevelProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"State",parameters()){
 for(auto& v:wave)v.store(0);
}
bool LevelProcessor::isBusesLayoutSupported(const BusesLayout& l)const{
 return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainInputChannelSet()==juce::AudioChannelSet::mono()||l.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void LevelProcessor::prepareToPlay(double sr,int){
 sampleRateHz=juce::jmax(1.0,sr);
 lows.fill(0);levelEnv.fill(0);gateEnv.fill(0);gateSmoothed.fill(1);prev.fill(0);downsample=0;
 meterIn.store(0);meterOut.store(0);
}
void LevelProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&){
 juce::ScopedNoDenormals noDenormals;
 const auto n=audio.getNumSamples(), chans=audio.getNumChannels();
 if(n<=0||chans<=0){meterIn.store(0);meterOut.store(0);return;}
 const auto val=[this](const char* id){return state.getRawParameterValue(id)->load();};
 const auto on=[&](const char* id){return val(id)>.5f;};
 const bool bypass=on("bypass");
 const float input=juce::Decibels::decibelsToGain(val("input"));
 const float output=juce::Decibels::decibelsToGain(val("output"));
 const float clean=val("clean")/100.0f, deess=val("deess")/100.0f,click=val("click")/100.0f;
 const float gateThreshold=juce::Decibels::decibelsToGain(val("threshold"));
 const float gateReduction=juce::Decibels::decibelsToGain(val("reduction"));
 const float target=juce::Decibels::decibelsToGain(val("target"));
 const float lowAlpha=std::exp(-2.0f*juce::MathConstants<float>::pi*val("cut")/float(sampleRateHz));
 const float levAlpha=std::exp(-1.0f/(float(sampleRateHz)*val("speed")*.001f));
 const float body=juce::Decibels::decibelsToGain(val("body"));
 const float breath=juce::Decibels::decibelsToGain(val("breath"));
 float maxIn=0,maxOut=0;
 for(int i=0;i<n;++i){
  float sum=0;
  for(int c=0;c<chans;++c){
   const int k=juce::jmin(c,1);
   auto* dest=audio.getWritePointer(c);
   const float raw=dest[i]; maxIn=juce::jmax(maxIn,std::abs(raw));
   float v=raw;
   if(!bypass){
    v*=input;
    lows[k]=lowAlpha*lows[k]+(1-lowAlpha)*v;
    if(on("lowendOn"))v=(v-lows[k])+body*lows[k];
    if(on("cleanOn")&&std::abs(v)<.018f)v*=1-.6f*clean;
    if(on("gateOn")){
      gateEnv[k]=.995f*gateEnv[k]+.005f*std::abs(v);
      const float gateAim=gateEnv[k]<gateThreshold?gateReduction:1.f;
      gateSmoothed[k]=.998f*gateSmoothed[k]+.002f*gateAim;
      v*=gateSmoothed[k];
    }else gateSmoothed[k]=1.f;
    if(on("dynamicsOn")){
      levelEnv[k]=levAlpha*levelEnv[k]+(1-levAlpha)*std::abs(v);
      const float gain=juce::jlimit(.25f,3.f,target/juce::jmax(.002f,levelEnv[k]));
      v*=1+.6f*(gain-1);
    }
    if(on("breathOn")&&std::abs(v)>.0008f&&std::abs(v)<.012f)v*=1+(breath-1)*.25f;
    if(on("sibilanceOn"))v-=.25f*deess*(v-prev[k]);
    if(on("mouthOn")&&std::abs(v-prev[k])>.2f)v-=.3f*click*(v-prev[k]);
    prev[k]=v;
    v*=output;
   }
   if(!std::isfinite(v))v=0;
   dest[i]=v;maxOut=juce::jmax(maxOut,std::abs(v));sum+=v;
  }
  if(++downsample>=32){
   downsample=0;const auto pos=wavePos.fetch_add(1,std::memory_order_relaxed);
   wave[pos%waveLength].store(sum/float(chans),std::memory_order_relaxed);
  }
 }
 meterIn.store(maxIn,std::memory_order_relaxed);
 meterOut.store(maxOut,std::memory_order_relaxed);
}
void LevelProcessor::getStateInformation(juce::MemoryBlock& mem){
 if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,mem);
}
void LevelProcessor::setStateInformation(const void* src,int size){
 if(auto xml=getXmlFromBinary(src,size))if(xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LevelProcessor();}
