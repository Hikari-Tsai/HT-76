#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template <typename Predicate>
void waitUntil(Predicate ready,const char* message)
{
    // CI message-thread timers need not run within a single 40–60 ms window.
    // Check the actual result, while retaining a bounded failure for broken UI.
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000.0;
    while(!ready())
    {
        require(juce::Time::getMillisecondCounterHiRes()<deadline,message);
        require(juce::MessageManager::getInstance()->runDispatchLoopUntil(10),"message loop stopped during editor test");
    }
}

struct ClickObserver final : juce::Button::Listener
{
    explicit ClickObserver(juce::Button& b):button(b){button.addListener(this);}
    ~ClickObserver() override {button.removeListener(this);}
    void buttonClicked(juce::Button*) override {delivered=true;}
    juce::Button& button;
    bool delivered=false;
};
juce::Component* find(juce::Component& c,const std::function<bool(juce::Component&)>& match)
{
    if(match(c))return &c;
    for(auto* child:c.getChildren())if(auto* found=find(*child,match))return found;
    return nullptr;
}
juce::Slider& slider(juce::Component& root,const char* name)
{
    auto* c=find(root,[&](auto& x){return x.getName()==name&&dynamic_cast<juce::Slider*>(&x)!=nullptr;});
    require(c!=nullptr,"named native knob missing");return *dynamic_cast<juce::Slider*>(c);
}
void click(juce::Component& root,const char* name)
{
    auto* c=find(root,[&](auto& x){auto* b=dynamic_cast<juce::Button*>(&x);return b&&b->getButtonText()==name;});
    require(c!=nullptr,"native button missing");
    auto& button=*dynamic_cast<juce::Button*>(c);
    ClickObserver observer(button);
    button.triggerClick();
    waitUntil([&]{return observer.delivered;},"native button click was not delivered");
}
}
int runEditorTests()
{
    try{
        FieldEffectProcessor p;p.prepareToPlay(48000,800);
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());require(e!=nullptr,"editor missing");
        require(e->getWidth()==1280&&std::abs(e->getHeight()-334)<=1,"Rack reference size changed");
        auto& gain=slider(*e,"Input gain");gain.setValue(18,juce::sendNotificationSync);
        require(std::abs(p.parameters.getRawParameterValue("inputDb")->load()-18)<.01f,"knob did not update APVTS");
        auto* out=p.parameters.getParameter("outputDb");out->setValueNotifyingHost(out->convertTo0to1(-10));
        waitUntil([&]{return std::abs(slider(*e,"Output gain").getValue()+10)<.01;},"automation did not update knob");
        auto& atk=slider(*e,"Attack time, clockwise faster");const auto before=atk.getValue();atk.keyPressed(juce::KeyPress(juce::KeyPress::rightKey));require(atk.getValue()<before,"clockwise attack is not faster");
        click(*e,"8");click(*e,"ALL");require(p.parameters.getRawParameterValue("allButtons")->load()>.5,"ALL did not activate");click(*e,"ALL");require(p.parameters.getRawParameterValue("ratio")->load()==1,"ALL did not preserve ratio");
        click(*e,"REV H*");require(p.parameters.getRawParameterValue("revision")->load()==1,"Rev H button did not select audio model");
        click(*e,"REV D");require(p.parameters.getRawParameterValue("revision")->load()==0,"Rev D button did not restore model");
        click(*e,"DYNAMIC");require(p.editorMode.load()==1&&std::abs(e->getHeight()-508)<=1,"Dynamic reference size");require(std::abs(gain.getValue()-18)<.01,"view switch lost gain");
        click(*e,"RACK");require(std::abs(e->getHeight()-334)<=1,"Rack switch height");
        FieldEffectProcessor restored;restored.editorMode.store(1);restored.editorWidth.store(1440);juce::MemoryBlock state;restored.getStateInformation(state);p.setStateInformation(state.getData(),(int)state.getSize());
        waitUntil([&]{return e->getWidth()==1440&&std::abs(e->getHeight()-572)<=1;},"open editor did not follow restored view/width");
        juce::AudioBuffer<float> silence(2,800);juce::MidiBuffer midi;
        auto* power=find(*e,[](auto& c){return c.getTitle()=="Bypass compressor";});require(power!=nullptr,"bypass control missing");
        waitUntil([&]{
            // Model a running host: keep supplying fresh meter frames while
            // the editor observes the bypass guard and consecutive frames.
            silence.clear();p.processBlockBypassed(silence,midi);
            return dynamic_cast<juce::Button*>(power)->getButtonText()=="OUT";
        },"host bypass did not reach effective bypass display");
        require(p.parameters.getRawParameterValue("bypass")->load()<.5f,"host bypass corrupted user bypass parameter");
        std::cout<<"PASS native editor controls/automation/timing/ALL/view dimensions/state restore/host bypass\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL native editor: "<<e.what()<<'\n';return 1;}
}
int renderEditors(const juce::File& directory,bool animation)
{
    require(directory.createDirectory().wasOk(),"cannot create render directory");
    for(int revision=0;revision<(animation?1:2);++revision)
    for(int mode=0;mode<2;++mode){
        FieldEffectProcessor p;p.editorMode.store(mode);
        auto* revisionParameter=p.parameters.getParameter("revision");
        revisionParameter->setValueNotifyingHost(revisionParameter->convertTo0to1((float)revision));
        p.prepareToPlay(48000,800);
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        const auto frames=directory.getChildFile(mode?"dynamic-revd":"rack-revd");
        if(animation)require(frames.createDirectory().wasOk(),"cannot create animation directory");
        const auto snapshot=[&](const juce::File& file,float scale){
            const auto image=e->createComponentSnapshot(e->getLocalBounds(),true,scale);
            file.deleteFile();auto stream=file.createOutputStream();require(stream!=nullptr,"cannot write native snapshot");
            juce::PNGImageFormat png;require(png.writeImageToStream(image,*stream),"PNG failed");
        };
        juce::AudioBuffer<float> audio(2,800);juce::MidiBuffer midi;
        // Fill the 12-second history first, then capture 8 seconds at 20 fps.
        // The audio clock supplies 60 meter frames per second in both modes.
        for(int frame=0;frame<(animation?1200:720);++frame){
            for(int i=0;i<800;++i){const double t=(frame*800+i)/48000.0;
                const float envelope=(float)(.015+.22*std::exp(-std::fmod(t,.52)/.085)+.09*std::exp(-std::fmod(t+.26,1.04)/.14));
                const float sample=envelope*(float)(std::sin(t*juce::MathConstants<double>::twoPi*96)+.3*std::sin(t*juce::MathConstants<double>::twoPi*740));
                audio.setSample(0,i,sample);audio.setSample(1,i,sample*.83f);}
            p.processBlock(audio,midi);
            if(animation)
            {
                // Allow the native timer to update the meters and VU ballistics.
                juce::MessageManager::getInstance()->runDispatchLoopUntil(17);
                if(frame>=720&&(frame-720)%3==2)
                    snapshot(frames.getChildFile("frame-"+juce::String((frame-720)/3).paddedLeft('0',4)+".png"),1.0f);
            }
            else if(frame%2==0)juce::MessageManager::getInstance()->runDispatchLoopUntil(18);
        }
        if(animation){std::cout<<frames.getFullPathName()<<" (160 frames at 20 fps)"<<std::endl;continue;}
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        auto file=directory.getChildFile(revision ? (mode?"dynamic-revh.png":"rack-revh.png") : (mode?"dynamic-native.png":"rack-native.png"));snapshot(file,1.5f);std::cout<<file.getFullPathName()<<'\n';
    }
    return 0;
}
