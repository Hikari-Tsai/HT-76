#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DspEngine.h"
#include <cmath>

namespace
{
constexpr const char* ids[]{"inputDb","outputDb","attackUs","releaseMs","ratio","allButtons","bypass","revision"};
juce::NormalisableRange<float> logarithmicRange(float low,float high)
{
    return {low,high,
        [](float a,float b,float n){return a*std::pow(b/a,n);},
        [](float a,float b,float v){return std::log(v/a)/std::log(b/a);},
        [](float a,float b,float v){return juce::jlimit(a,b,v);}};
}
float safeValue(const std::atomic<float>* p,float low,float high,float fallback)
{
    const float v=p->load(std::memory_order_relaxed);
    return std::isfinite(v)?juce::jlimit(low,high,v):fallback;
}
}

FieldEffectProcessor::FieldEffectProcessor()
    :AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
     parameters(*this,nullptr,"FieldEffectState",createParameterLayout()),
     engine(std::make_unique<field::DspEngine>())
{
    for(std::size_t i=0;i<values.size();++i)
        values[i]=parameters.getRawParameterValue(ids[i]);
}
FieldEffectProcessor::~FieldEffectProcessor()=default;

juce::AudioProcessorValueTreeState::ParameterLayout FieldEffectProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto id=[](const char* name){return juce::ParameterID{name,1};};
    layout.add(std::make_unique<juce::AudioParameterFloat>(id("inputDb"),"Input",juce::NormalisableRange<float>{-20,40,.01f},12,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id("outputDb"),"Output",juce::NormalisableRange<float>{-20,20,.01f},-6,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id("attackUs"),"Attack",logarithmicRange(20,800),200,
        juce::AudioParameterFloatAttributes().withLabel("us")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id("releaseMs"),"Release",logarithmicRange(50,1100),400,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(id("ratio"),"Ratio",juce::StringArray{"4:1","8:1","12:1","20:1"},0));
    layout.add(std::make_unique<juce::AudioParameterBool>(id("allButtons"),"All buttons",false));
    layout.add(std::make_unique<juce::AudioParameterBool>(id("bypass"),"Bypass",false));
    // Append to preserve all existing parameter IDs and ordering.
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"revision",2},"Circuit revision",
        juce::StringArray{"Rev D", "Rev H (Experimental)"},0));
    return layout;
}
void FieldEffectProcessor::prepareToPlay(double sampleRate,int maximumBlockSize)
{
    engine->prepare(sampleRate,juce::jmax(1,maximumBlockSize),juce::jmax(1,getTotalNumInputChannels()));
    setLatencySamples(engine->getLatencySamples());
}
void FieldEffectProcessor::releaseResources(){engine->reset();}
void FieldEffectProcessor::reset(){engine->reset();}

bool FieldEffectProcessor::isBusesLayoutSupported(const BusesLayout& layout)const
{
    const auto input=layout.getMainInputChannelSet();
    return (input==juce::AudioChannelSet::mono()||input==juce::AudioChannelSet::stereo())
        && input==layout.getMainOutputChannelSet();
}
void FieldEffectProcessor::process(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi,bool hostBypassed)
{
    juce::ScopedNoDenormals guard;
    midi.clear();
    for(int ch=getTotalNumInputChannels();ch<audio.getNumChannels();++ch)
        audio.clear(ch,0,audio.getNumSamples());
    if(audio.getNumSamples()==0)return;
    field::DspEngine::Settings s;
    s.inputDb=safeValue(values[0],-20,40,0);
    s.outputDb=safeValue(values[1],-20,20,0);
    s.attackUs=safeValue(values[2],20,800,200);
    s.releaseMs=safeValue(values[3],50,1100,400);
    s.ratioIndex=juce::roundToInt(safeValue(values[4],0,3,0));
    s.allButtons=safeValue(values[5],0,1,0)>.5f;
    s.bypass=safeValue(values[6],0,1,0)>.5f;
    s.revision=juce::roundToInt(safeValue(values[7],0,1,0));
    engine->process(audio,s,hostBypassed);
}
void FieldEffectProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){process(b,m,false);}
void FieldEffectProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){process(b,m,true);}
juce::AudioProcessorParameter* FieldEffectProcessor::getBypassParameter()const{return parameters.getParameter("bypass");}
field::MeterBridge& FieldEffectProcessor::meterBridge()noexcept{return engine->meters();}
juce::AudioProcessorEditor* FieldEffectProcessor::createEditor(){return new FieldEffectEditor(*this);}

void FieldEffectProcessor::getStateInformation(juce::MemoryBlock& output)
{
    auto state=parameters.copyState();
    state.setProperty("schemaVersion",2,nullptr);
    state.setProperty("editorMode",editorMode.load(),nullptr);
    state.setProperty("editorWidth",editorWidth.load(),nullptr);
    if(auto xml=state.createXml())copyXmlToBinary(*xml,output);
}
void FieldEffectProcessor::setStateInformation(const void* data,int size)
{
    if(data==nullptr||size<=0||size>1024*1024)return;
    const auto xml=getXmlFromBinary(data,size);
    if(xml==nullptr||!xml->hasTagName("FieldEffectState"))return;
    auto state=juce::ValueTree::fromXml(*xml);
    if(!state.isValid())return;
    for(auto child:state)
    {
        if(auto* parameter=parameters.getParameter(child.getProperty("id").toString()))
        {
            const float v=(float)child.getProperty("value");
            if(!std::isfinite(v))return;
            const auto& r=parameter->getNormalisableRange();
            child.setProperty("value",juce::jlimit(r.start,r.end,v),nullptr);
        }
    }
    editorMode.store(juce::jlimit(0,1,(int)state.getProperty("editorMode",0)));
    editorWidth.store(juce::jlimit(960,1920,(int)state.getProperty("editorWidth",1280)));
    // Older sessions must select D even when loaded over a currently active H.
    bool hasRevision=false;
    for(auto child:state) hasRevision |= child.getProperty("id").toString()=="revision";
    if(!hasRevision){juce::ValueTree child("PARAM");child.setProperty("id","revision",nullptr);
        child.setProperty("value",0.0f,nullptr);state.appendChild(child,nullptr);}
    parameters.replaceState(state);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FieldEffectProcessor;}
