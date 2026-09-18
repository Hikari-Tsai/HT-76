#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

class FieldEffectProcessor;

class FieldEffectEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FieldEffectEditor (FieldEffectProcessor&);
    ~FieldEffectEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Panel;
    FieldEffectProcessor& ownerProcessor;
    std::unique_ptr<Panel> panel;
    void setMode (bool dynamic, int restoredWidth = 0);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FieldEffectEditor)
};
