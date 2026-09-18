#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
namespace {
void expect(bool condition,const char* what) { if(!condition) throw std::runtime_error(what); }
void set(FieldEffectProcessor& p,const char* id,float v){auto* x=p.parameters.getParameter(id);expect(x!=nullptr,"parameter missing");x->setValueNotifyingHost(x->convertTo0to1(v));}
}
int runProcessorTests()
{
    int failed=0;
    const auto test=[&](const char* name,auto fn){try{fn();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    test("host parameter contract",[]{
        FieldEffectProcessor p;
        for(auto* id:{"inputDb","outputDb","attackUs","releaseMs","ratio","allButtons","bypass","revision"})
            expect(p.parameters.getParameter(id)!=nullptr,"required automatable parameter missing");
        const auto range=p.parameters.getParameter("inputDb")->getNormalisableRange();
        expect(range.start==-20.0f&&range.end==40.0f,"input gain range");
        expect(p.getBypassParameter()==p.parameters.getParameter("bypass"),"host bypass must use the same parameter");
    });
    test("state roundtrip restores audio and editor state",[]{
        FieldEffectProcessor a;set(a,"inputDb",19.5f);set(a,"outputDb",-11.0f);set(a,"attackUs",51.0f);set(a,"releaseMs",850.0f);set(a,"ratio",2);set(a,"allButtons",1);set(a,"bypass",1);set(a,"revision",1);
        a.editorMode.store(1);a.editorWidth.store(1440);juce::MemoryBlock saved;a.getStateInformation(saved);
        FieldEffectProcessor b;b.setStateInformation(saved.getData(),(int)saved.getSize());
        for(auto* id:{"inputDb","outputDb","attackUs","releaseMs","ratio","allButtons","bypass","revision"})expect(std::abs(a.parameters.getRawParameterValue(id)->load()-b.parameters.getRawParameterValue(id)->load())<.01f,"parameter did not restore");
        expect(b.editorMode.load()==1&&b.editorWidth.load()==1440,"editor state did not restore");
    });
    test("legacy sessions restore Rev D over an active Rev H",[]{
        FieldEffectProcessor p;set(p,"revision",1);
        auto state=p.parameters.copyState();
        for(int i=state.getNumChildren()-1;i>=0;--i)
            if(state.getChild(i).getProperty("id").toString()=="revision")state.removeChild(i,nullptr);
        state.setProperty("schemaVersion",1,nullptr);
        juce::MemoryBlock data;juce::AudioProcessor::copyXmlToBinary(*state.createXml(),data);
        p.setStateInformation(data.getData(),(int)data.getSize());
        expect(p.parameters.getRawParameterValue("revision")->load()==0,"legacy session retained Rev H");
    });
    test("corrupt state is ignored",[]{FieldEffectProcessor p;set(p,"inputDb",7);const auto before=p.parameters.getRawParameterValue("inputDb")->load();const char bad[]="not plugin state";p.setStateInformation(bad,sizeof(bad));expect(p.parameters.getRawParameterValue("inputDb")->load()==before,"corrupt state reset parameters");});
    test("mono and stereo buses only",[]{FieldEffectProcessor p;auto layout=p.getBusesLayout();for(auto s:{juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()}){layout.inputBuses.set(0,s);layout.outputBuses.set(0,s);expect(p.isBusesLayoutSupported(layout),"supported channel layout rejected");}layout.outputBuses.set(0,juce::AudioChannelSet::mono());expect(!p.isBusesLayoutSupported(layout),"mismatched channels accepted");});
    test("UI view changes leave the audio stream identical",[]{
        FieldEffectProcessor a,b;a.prepareToPlay(48000,256);b.prepareToPlay(48000,256);
        juce::AudioBuffer<float> x(2,256),y(2,256);juce::MidiBuffer midi;
        for(int block=0;block<30;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)x.setSample(ch,i,.2f*std::sin((float)(block*256+i)*.073f));y.makeCopyOf(x);if(block==10)b.editorMode.store(1);a.processBlock(x,midi);b.processBlock(y,midi);for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)expect(x.getSample(ch,i)==y.getSample(ch,i),"view switch modified audio");}
    });
    return failed;
}
