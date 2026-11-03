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
 lows.fill(0.f); levelEnv.fill(0.f); gateEnv.fill(0.f); gateSmoothed.fill(1.f); prev.fill(0.f);
 cleanEnv.fill(0.f); cleanGain.fill(1.f); dynamicsGain.fill(1.f);
 deessLow.fill(0.f); deessEnv.fill(0.f); deessGain.fill(1.f);
 lowendWet.fill(0.f); cleanWet.fill(0.f); gateWet.fill(0.f);
 dynamicsWet.fill(0.f); deessWet.fill(0.f);
 inputSmoothed=1.f; outputSmoothed=1.f; lowCutSmoothed=75.f; bodySmoothed=0.f;
 downsample=0;
 meterIn.store(0.f);meterOut.store(0.f);
}
void LevelProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&){
 juce::ScopedNoDenormals noDenormals;
 const int n=audio.getNumSamples(), chans=audio.getNumChannels();
 if(n<=0||chans<=0){meterIn.store(0.f);meterOut.store(0.f);return;}
 const auto val=[this](const char* id){return state.getRawParameterValue(id)->load(std::memory_order_relaxed);};
 const auto on=[&](const char* id){return val(id)>.5f;};
 const bool bypass=on("bypass");
 const float targetInput=juce::Decibels::decibelsToGain(val("input"));
 const float targetOutput=juce::Decibels::decibelsToGain(val("output"));
 const float targetCut=juce::jlimit(20.f,250.f,val("cut"));
 const float targetBody=juce::Decibels::decibelsToGain(val("body"));
 const float cleanAmount=juce::jlimit(0.f,1.f,val("clean")*.01f);
 const float gateThreshold=juce::Decibels::decibelsToGain(val("threshold"));
 const float gateFloor=juce::Decibels::decibelsToGain(val("reduction"));
 const float targetLevel=juce::Decibels::decibelsToGain(val("target"));
 const float speedMs=juce::jlimit(50.f,1500.f,val("speed"));
 const float deessAmount=juce::jlimit(0.f,1.f,val("deess")*.01f);
 const float fs=(float)sampleRateHz;
 const auto coeff=[fs](float ms){return std::exp(-1.f/(fs*ms*.001f));};
 const float fast=coeff(12.f), control=coeff(25.f);
 const float attack=coeff(5.f), release=coeff(130.f);
 const float gateAttack=coeff(8.f), gateRelease=coeff(110.f);
 const float cleanRelease=coeff(200.f);
 const float levAttack=coeff(30.f), levRelease=coeff(speedMs);
 const float deessAttack=coeff(3.f), deessRelease=coeff(85.f);
 const float maxBoost=juce::Decibels::decibelsToGain(6.f);
 const float maxCut=juce::Decibels::decibelsToGain(-9.f);
 const float cleanTarget=bypass?0.f:(on("cleanOn")?1.f:0.f);
 const float lowTarget=bypass?0.f:(on("lowendOn")?1.f:0.f);
 const float gateTarget=bypass?0.f:(on("gateOn")?1.f:0.f);
 const float dynTarget=bypass?0.f:(on("dynamicsOn")?1.f:0.f);
 const float deessTarget=bypass?0.f:(on("sibilanceOn")?1.f:0.f);
 // Breath and Mouth Noise require reliable event detection. Do not fabricate it
 // by modifying every quiet sample or abrupt transient; those old rules damaged speech.
 float maxIn=0.f,maxOut=0.f;
 for(int i=0;i<n;++i){
  inputSmoothed=control*inputSmoothed+(1.f-control)*targetInput;
  outputSmoothed=control*outputSmoothed+(1.f-control)*targetOutput;
  lowCutSmoothed=control*lowCutSmoothed+(1.f-control)*targetCut;
  bodySmoothed=control*bodySmoothed+(1.f-control)*targetBody;
  const float hpAlpha=std::exp(-2.f*juce::MathConstants<float>::pi*lowCutSmoothed/fs);
  const float essAlpha=std::exp(-2.f*juce::MathConstants<float>::pi*4500.f/fs);
  float sum=0.f;
  for(int c=0;c<chans;++c){
   const int k=juce::jmin(c,1);
   auto* dest=audio.getWritePointer(c);
   const float raw=std::isfinite(dest[i])?dest[i]:0.f;
   maxIn=juce::jmax(maxIn,std::abs(raw));
   // Module bypasses and gain controls move with sample-by-sample ramps.
   const auto move=[fast](float& current,float wanted){current=fast*current+(1.f-fast)*wanted;};
   move(lowendWet[k],lowTarget); move(cleanWet[k],cleanTarget);
   move(gateWet[k],gateTarget);move(dynamicsWet[k],dynTarget);move(deessWet[k],deessTarget);
   float v=raw;
   if(!bypass) v*=inputSmoothed;
   // Lowend: first-order high-pass plus a separate smooth low-body shelf.
   // At BODY=0 dB the low-pass component is not boosted back into the HP path.
   lows[k]=hpAlpha*lows[k]+(1.f-hpAlpha)*v;
   const float highpassed=v-lows[k];
   const float bodyDelta=juce::jlimit(-.8f,1.0f,(bodySmoothed-1.f)*.25f);
   const float lowProcessed=highpassed+bodyDelta*lows[k];
   v+=lowendWet[k]*(lowProcessed-v);
   // Clean: conservative slow downward expansion below -52 dBFS.
   // The envelope, not individual zero-crossings, drives the reduction.
   const float ab=std::abs(v);
   const float ce=ab>cleanEnv[k]?attack:cleanRelease;
   cleanEnv[k]=ce*cleanEnv[k]+(1.f-ce)*ab;
   const float noiseThreshold=juce::Decibels::decibelsToGain(-52.f);
   const float cleanRatio=juce::jlimit(0.f,1.f,cleanEnv[k]/noiseThreshold);
   const float cleanAim=1.f-cleanAmount*.35f*(1.f-cleanRatio);
   move(cleanGain[k],cleanAim);
   v*=1.f+cleanWet[k]*(cleanGain[k]-1.f);
   // Gate: amplitude envelope plus soft knee, independent of instantaneous polarity.
   const float ga=std::abs(v)>gateEnv[k]?gateAttack:gateRelease;
   gateEnv[k]=ga*gateEnv[k]+(1.f-ga)*std::abs(v);
   const float gateRatio=juce::jlimit(0.f,1.f,gateEnv[k]/juce::jmax(1e-7f,gateThreshold));
   const float gateAim=gateFloor+(1.f-gateFloor)*gateRatio*gateRatio*(3.f-2.f*gateRatio);
   move(gateSmoothed[k],gateAim);
   v*=1.f+gateWet[k]*(gateSmoothed[k]-1.f);
   // Dynamics: slow, limited level correction; no boost of silence/noise.
   const float da=std::abs(v)>levelEnv[k]?levAttack:levRelease;
   levelEnv[k]=da*levelEnv[k]+(1.f-da)*std::abs(v);
   const float desired=levelEnv[k]<juce::Decibels::decibelsToGain(-48.f)
       ?1.f:juce::jlimit(maxCut,maxBoost,targetLevel/juce::jmax(levelEnv[k],1e-6f));
   move(dynamicsGain[k],desired);
   v*=1.f+dynamicsWet[k]*(dynamicsGain[k]-1.f);
   // De-esser: only the high-frequency band receives gradual attenuation,
   // and only when its smoothed energy is elevated relative to the full band.
   deessLow[k]=essAlpha*deessLow[k]+(1.f-essAlpha)*v;
   const float hf=v-deessLow[k];
   const float absHf=std::abs(hf);
   const float ea=absHf>deessEnv[k]?deessAttack:deessRelease;
   deessEnv[k]=ea*deessEnv[k]+(1.f-ea)*absHf;
   const float trigger=juce::jlimit(0.f,1.f,
      (deessEnv[k]-juce::Decibels::decibelsToGain(-31.f))/
       juce::Decibels::decibelsToGain(-19.f));
   const float essAim=1.f-.65f*deessAmount*trigger;
   move(deessGain[k],essAim);
   v-=deessWet[k]*(1.f-deessGain[k])*hf;
   if(!bypass) v*=outputSmoothed;
   if(!std::isfinite(v))v=0.f;
   dest[i]=v;
   maxOut=juce::jmax(maxOut,std::abs(v));
   sum+=v;
  }
  if(++downsample>=32){
   downsample=0;const auto pos=wavePos.fetch_add(1,std::memory_order_relaxed);
   wave[pos%waveLength].store(sum/(float)chans,std::memory_order_relaxed);
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
