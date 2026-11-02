#include "PluginEditor.h"
static constexpr const char* moduleNames[]={"CLEAN","LOWEND","GATE","DYNAMICS","BREATH","SIBILANCE","MOUTH NOISE"};
static constexpr const char* enableIDs[]={"cleanOn","lowendOn","gateOn","dynamicsOn","breathOn","sibilanceOn","mouthOn"};
static constexpr const char* moduleParams[][3]={
 {"clean","",""},{"cut","body",""},{"threshold","reduction",""},{"target","speed",""},
 {"breath","",""},{"deess","",""},{"click","",""}};
LevelEditor::LevelEditor(LevelProcessor& p):AudioProcessorEditor(&p),processor(p){
 setSize(1080,650);setResizable(true,true);setResizeLimits(940,560,1680,1000);
 for(int i=0;i<7;++i){
  tabs[i].setButtonText(moduleNames[i]); tabs[i].onClick=[this,i]{select(i);};addAndMakeVisible(tabs[i]);
  switches[i].setButtonText("ON");switches[i].setClickingTogglesState(true);addAndMakeVisible(switches[i]);
  switchLinks[i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,enableIDs[i],switches[i]);
 }
 for(int i=0;i<3;++i){
  controls[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  controls[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,105,22);addAndMakeVisible(controls[i]);
  labels[i].setJustificationType(juce::Justification::centred);labels[i].setColour(juce::Label::textColourId,juce::Colours::lightgrey);
  addAndMakeVisible(labels[i]);
 }
 for(auto* btn:{&bypass,&settings,&save,&load,&reset})addAndMakeVisible(*btn);
 bypass.setClickingTogglesState(true);
 bypassLink=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"bypass",bypass);
 reset.onClick=[this]{for(auto* x:processor.getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(x))p->setValueNotifyingHost(p->getDefaultValue());};
 save.onClick=[this]{
  auto chooser=std::make_shared<juce::FileChooser>("Save Level preset",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.levelpreset");
  chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,
   [this,chooser](const juce::FileChooser& f){auto file=f.getResult();if(file!=juce::File()){juce::MemoryBlock mem;processor.getStateInformation(mem);file.replaceWithData(mem.getData(),mem.getSize());}});
 };
 load.onClick=[this]{
  auto chooser=std::make_shared<juce::FileChooser>("Load Level preset",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.levelpreset");
  chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
   [this,chooser](const juce::FileChooser& f){juce::MemoryBlock mem;if(f.getResult().loadFileAsData(mem))processor.setStateInformation(mem.getData(),int(mem.getSize()));});
 };
 settings.onClick=[this]{
  juce::PopupMenu menu;menu.addItem(1,"Manual");menu.addItem(2,"Check updates");menu.addSeparator();menu.addItem(3,"Version 0.2.1 (development)",false,false);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settings),[this](int id){
   if(id==1){
    #if JUCE_MAC
    auto root=juce::File("/Library/Application Support/Nativos Audio/Level");
    #elif JUCE_WINDOWS
    auto root=juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("Nativos Audio").getChildFile("Level");
    #else
    auto root=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    #endif
    auto manual=root.getChildFile("Level-Manual.html");
    if(manual.existsAsFile())manual.startAsProcess();
    else juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"LEVEL Manual","Manual not installed.");
   }
   if(id==2)juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"LEVEL Updates","No verified signed release feed is configured yet. Updates are not automatically downloadable.");
  });
 };
 select(3);startTimerHz(24);
}
void LevelEditor::select(int module){
 selected=juce::jlimit(0,6,module);
 for(int i=0;i<3;++i){
  controlLinks[i].reset();
  const juce::String id=moduleParams[selected][i];
  const bool visible=id.isNotEmpty();controls[i].setVisible(visible);labels[i].setVisible(visible);
  if(visible){labels[i].setText(id.toUpperCase(),juce::dontSendNotification);
   controlLinks[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,id,controls[i]);}
 }
 for(int i=0;i<7;++i)tabs[i].setColour(juce::TextButton::buttonColourId,i==selected?juce::Colour(0xff315b72):juce::Colour(0xff1b2734));
 repaint();
}
void LevelEditor::resized(){
 const int width=getWidth();
 for(int i=0;i<7;++i){tabs[i].setBounds(14,88+i*59,119,44);switches[i].setBounds(138,96+i*59,33,28);}
 for(int i=0;i<3;++i){controls[i].setBounds(206+i*164,98,132,99);labels[i].setBounds(206+i*164,196,132,22);}
 reset.setBounds(width-460,23,67,32);load.setBounds(width-386,23,62,32);save.setBounds(width-318,23,60,32);
 settings.setBounds(width-250,23,98,32);bypass.setBounds(width-144,23,125,32);
}
void LevelEditor::paint(juce::Graphics& g){
 const float W=float(getWidth()),H=float(getHeight());
 g.fillAll(juce::Colour(0xff0e151d));
 g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(27.f,juce::Font::bold)));
 g.drawText("LEVEL",194,8,175,46,juce::Justification::left);
 g.setFont(juce::Font(juce::FontOptions(13.f,juce::Font::bold)));g.drawText("NATIVOS AUDIO",17,11,165,24,juce::Justification::left);
 g.setColour(juce::Colour(0xff9dafc2));g.setFont(11);g.drawText("by Yai y Toly",17,34,155,21,juce::Justification::left);
 g.drawText("REAL-TIME VOCAL PROCESSOR",194,48,255,22,juce::Justification::left);
 g.setColour(juce::Colour(0xff17232f));g.fillRoundedRectangle(188.f,85.f,W-207,144.f,9.f);
 const auto x=188.f,y=240.f,w=W-208,h=H-290;
 g.setColour(juce::Colour(0xff101b26));g.fillRoundedRectangle(x,y,w,h,9.f);
 g.setColour(juce::Colour(0xff304456));g.drawRoundedRectangle(x,y,w,h,9.f,1.f);
 for(int i=1;i<8;++i){g.setColour(juce::Colour(0xff253444));g.drawHorizontalLine(int(y+h*i/8),x+1,x+w-1);}
 juce::Path path;unsigned current=processor.wavePos.load(std::memory_order_relaxed);
 for(int j=0;j<int(w)-20;++j){
  auto offset=unsigned((int(w)-20-j)*LevelProcessor::waveLength/juce::jmax(1.f,w-20));
  auto v=processor.wave[(current+LevelProcessor::waveLength-1-offset)%LevelProcessor::waveLength].load(std::memory_order_relaxed);
  float amp=juce::jlimit(-h*.45f,h*.45f,v*h);
  path.startNewSubPath(x+10+j,y+h*.5f-amp);path.lineTo(x+10+j,y+h*.5f+amp);
 }
 static const juce::uint32 accents[]={0xff48c5c0,0xffd084b6,0xffe98283,0xff80cd91,0xff62cfaa,0xff64b7ea,0xffd3b36e};
 g.setColour(juce::Colour(accents[selected]));g.strokePath(path,juce::PathStrokeType(1.f));
 g.setColour(juce::Colour(0xffaebdcc));g.setFont(12);
 g.drawText(juce::String(moduleNames[selected])+"  •  LIVE WAVEFORM",int(x)+13,int(y)+8,350,20,juce::Justification::left);
 auto meter=[&](float px,float peak,const juce::String& label){
  g.setColour(juce::Colour(0xff5e6d7f));g.drawText(label,int(px)-4,104,42,20,juce::Justification::centred);
  g.setColour(juce::Colour(0xff293849));g.fillRoundedRectangle(px,132,15,72,3);
  const float magnitude=juce::jlimit(0.f,1.f,std::sqrt(juce::jmax(0.f,peak)));
  if(magnitude>0){g.setColour(magnitude>.9f?juce::Colour(0xffe48977):juce::Colour(0xff76d5b2));g.fillRoundedRectangle(px+2,202.f-magnitude*68,11,magnitude*68,2);}
 };
 meter(W-142,processor.meterIn.load(),"IN");meter(W-83,processor.meterOut.load(),"OUT");
 g.setColour(juce::Colour(0xff889bad));g.drawText("LEVEL • Nativos Audio • by Yai y Toly  |  v0.2.1 development",193,getHeight()-39,getWidth()-218,20,juce::Justification::left);
}
juce::AudioProcessorEditor* LevelProcessor::createEditor(){return new LevelEditor(*this);}
