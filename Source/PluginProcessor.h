#pragma once
#include <JuceHeader.h>
#include <atomic>
namespace field { class DspEngine; class MeterBridge; }

class FieldEffectProcessor final : public juce::AudioProcessor
{
public:
    FieldEffectProcessor();
    ~FieldEffectProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Allow the experimental H coupling/transformer states to decay on silence.
    double getTailLengthSeconds() const override { return 1.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    field::MeterBridge& meterBridge() noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<int> editorMode{0}, editorWidth{1280};
private:
    void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
    std::unique_ptr<field::DspEngine> engine;
    std::array<std::atomic<float>*,8> values{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FieldEffectProcessor)
};
