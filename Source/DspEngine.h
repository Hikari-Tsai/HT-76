#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "MeterBridge.h"
#include <memory>

namespace field
{
class DspEngine
{
public:
    struct Settings
    {
        float inputDb = 0.0f, outputDb = 0.0f;
        float attackUs = 400.0f, releaseMs = 400.0f;
        int ratioIndex = 0, revision = 0;
        bool allButtons = false, bypass = false;
    };
    DspEngine();
    ~DspEngine();
    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset();
    void process (juce::AudioBuffer<float>&, const Settings&, bool hostBypassed = false);
    int getLatencySamples() const noexcept;
    MeterBridge& meters() noexcept { return bridge; }
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    MeterBridge bridge;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DspEngine)
};
}
