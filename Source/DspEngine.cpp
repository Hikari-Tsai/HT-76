#include "DspEngine.h"
#include "FetLimiterDsp.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace field
{
namespace
{
float boundedParameter (float value, float fallback, float minimum, float maximum) noexcept
{
    return std::isfinite (value) ? juce::jlimit (minimum, maximum, value) : fallback;
}

float safeSample (float value) noexcept
{
    // Defensive input guard: ordinary headroom is preserved, invalid data and
    // pathological magnitudes cannot enter recursive filter/core state.
    return std::isfinite (value) ? juce::jlimit (-64.0f, 64.0f, value) : 0.0f;
}

float meterDb (float value) noexcept
{
    return juce::Decibels::gainToDecibels (value, -100.0f);
}

int remainingMeterDelay (int totalLatency, bool oversampled)
{
    if (! oversampled) return 0;
    // JUCE 8 maximum-quality 2x upsampler: 0.05 transition, -90 dB.
    // Both branches are allpasses. At near-DC their mean phase yields the
    // upsampling group delay, converted from 2x to base-rate samples.
    // The remaining delay includes the down filter and integer compensation.
    const auto up = juce::dsp::FilterDesign<float>::designIIRLowpassHalfBandPolyphaseAllpassMethod (0.05f, -90.0f);
    double phase = 0.0;
    for (const auto* coefficient : up.directPath)
        phase += coefficient->getPhaseForFrequency (0.0001, 1.0);
    for (const auto* coefficient : up.delayedPath)
        phase += coefficient->getPhaseForFrequency (0.0001, 1.0);
    const double upLatency = -phase / (4.0 * 0.0001 * juce::MathConstants<double>::twoPi);
    return juce::jlimit (0, totalLatency, int (std::lround (double (totalLatency) - upLatency)));
}
}

struct DspEngine::Impl
{
    FetLimiterDsp fet, fetH;
    juce::AudioBuffer<float> wet, wetH, dry, meterInput, audioDelay;
    std::vector<float> reductionTrace, reductionTraceH, reductionDelay;
    juce::SmoothedValue<float> inputGain { 1.0f }, wetAmount { 1.0f }, revisionBlend { 0.0f };
    double sampleRate = 48000.0, meterPhase = 0.0;
    int blockSize = 0, channels = 2, latency = 0, audioCursor = 0, reductionCursor = 0;
    int frameSamples = 0;
    std::uint64_t sequence = 0;
    bool primed = false;
    std::array<float, 2> inputPeak {}, outputPeak {};
    std::array<double, 2> inputEnergy {}, outputEnergy {};
    float reductionPeak = 0.0f;

    void clearFrame() noexcept
    {
        frameSamples = 0;
        inputPeak.fill (0.0f);
        outputPeak.fill (0.0f);
        inputEnergy.fill (0.0);
        outputEnergy.fill (0.0);
        reductionPeak = 0.0f;
    }

    void publish (MeterBridge& destination, bool bypassed) noexcept
    {
        MeterFrame frame;
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            frame.inputDb[ch] = meterDb (inputPeak[ch]);
            frame.outputDb[ch] = meterDb (outputPeak[ch]);
            frame.inputRmsDb[ch] = meterDb (float (std::sqrt (inputEnergy[ch] / double (frameSamples))));
            frame.outputRmsDb[ch] = meterDb (float (std::sqrt (outputEnergy[ch] / double (frameSamples))));
        }
        frame.reductionDb = reductionPeak;
        frame.bypassed = bypassed;
        frame.sequence = ++sequence;
        destination.push (frame);
        clearFrame();
    }
};

DspEngine::DspEngine() : impl (std::make_unique<Impl>()) {}
DspEngine::~DspEngine() = default;

void DspEngine::prepare (double sampleRate, int maximumBlockSize, int channels)
{
    auto& d = *impl;
    d.sampleRate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
    d.blockSize = juce::jlimit (1, 8192, maximumBlockSize);
    d.channels = juce::jlimit (1, 2, channels);
    const bool oversampled = d.sampleRate < 88200.0;
    d.fet.setOversamplingLog2 (oversampled ? 1 : 0);
    d.fet.prepare (d.sampleRate, d.blockSize, d.channels);
    d.fetH.setOversamplingLog2 (oversampled ? 1 : 0);
    d.fetH.prepare (d.sampleRate, d.blockSize, d.channels);
    d.latency = d.fet.getLatencySamples();
    d.wet.setSize (d.channels, d.blockSize);
    d.wetH.setSize (d.channels, d.blockSize);
    d.dry.setSize (d.channels, d.blockSize);
    d.meterInput.setSize (d.channels, d.blockSize);
    d.audioDelay.setSize (d.channels * 2, d.latency + 1);
    d.reductionTrace.resize (static_cast<std::size_t> (d.blockSize));
    d.reductionTraceH.resize (static_cast<std::size_t> (d.blockSize));
    d.reductionDelay.resize (static_cast<std::size_t> (remainingMeterDelay (d.latency, oversampled) + 1));
    d.inputGain.reset (d.sampleRate, 0.020);
    d.wetAmount.reset (d.sampleRate, 0.005);
    d.revisionBlend.reset (d.sampleRate, 0.020);
    reset();
}

void DspEngine::reset()
{
    auto& d = *impl;
    d.fet.reset();
    d.fetH.reset();
    d.audioDelay.clear();
    d.wet.clear();
    d.wetH.clear();
    d.dry.clear();
    d.meterInput.clear();
    std::fill (d.reductionDelay.begin(), d.reductionDelay.end(), 0.0f);
    d.audioCursor = d.reductionCursor = 0;
    d.inputGain.setCurrentAndTargetValue (1.0f);
    d.wetAmount.setCurrentAndTargetValue (1.0f);
    d.revisionBlend.setCurrentAndTargetValue (0.0f);
    d.primed = false;
    d.meterPhase = 0.0;
    d.clearFrame();
    // The bridge is concurrently consumed by the editor: never reset its
    // consumer cursor here. Keep sequence numbers monotonic across resets.
}

int DspEngine::getLatencySamples() const noexcept { return impl->latency; }

void DspEngine::process (juce::AudioBuffer<float>& buffer, const Settings& settings, bool hostBypassed)
{
    auto& d = *impl;
    if (d.blockSize == 0) { buffer.clear(); return; }
    const int count = buffer.getNumSamples();
    if (count == 0) return;
    juce::ScopedNoDenormals noDenormals;
    const int actualChannels = juce::jmin (d.channels, buffer.getNumChannels());
    if (actualChannels == 0) return;

    FetLimiterDsp::Parameters parameters;
    // Input is applied ONCE here, before oversampling, for an exact input
    // signal tap. Output remains in the model, before its transformer.
    parameters.inputDb = 0.0f;
    parameters.outputDb = boundedParameter (settings.outputDb, 0.0f, -20.0f, 20.0f);
    parameters.attackUs = boundedParameter (settings.attackUs, 400.0f, 20.0f, 800.0f);
    parameters.releaseMs = boundedParameter (settings.releaseMs, 400.0f, 50.0f, 1100.0f);
    parameters.ratio = static_cast<FetLimiterDsp::Ratio> (juce::jlimit (0, 3, settings.ratioIndex));
    parameters.allButtons = settings.allButtons;
    d.fet.setParameters (parameters);
    parameters.revisionH = true;
    parameters.inputDb = boundedParameter (settings.inputDb, 0.0f, -20.0f, 40.0f);
    d.fetH.setParameters (parameters);
    const float revision = settings.revision == 1 ? 1.0f : 0.0f;
    const auto input = juce::Decibels::decibelsToGain (boundedParameter (settings.inputDb, 0.0f, -20.0f, 40.0f));
    const auto wet = settings.bypass || hostBypassed ? 0.0f : 1.0f;
    if (! d.primed)
    {
        d.inputGain.setCurrentAndTargetValue (input);
        d.wetAmount.setCurrentAndTargetValue (wet);
        d.revisionBlend.setCurrentAndTargetValue (revision);
        d.primed = true;
    }
    else
    {
        d.inputGain.setTargetValue (input);
        d.wetAmount.setTargetValue (wet);
        d.revisionBlend.setTargetValue (revision);
    }

    for (int start = 0; start < count; start += d.blockSize)
    {
        const int n = juce::jmin (d.blockSize, count - start);
        for (int i = 0; i < n; ++i)
        {
            const auto gain = d.inputGain.getNextValue();
            const int delayedPosition = (d.audioCursor + 1) % d.audioDelay.getNumSamples();
            for (int ch = 0; ch < d.channels; ++ch)
            {
                const auto raw = ch < actualChannels ? safeSample (buffer.getSample (ch, start + i)) : 0.0f;
                const auto driven = raw * gain;
                d.wet.setSample (ch, i, driven);
                d.wetH.setSample (ch, i, raw);
                d.audioDelay.setSample (ch, d.audioCursor, raw);
                d.audioDelay.setSample (ch + d.channels, d.audioCursor, driven);
                d.dry.setSample (ch, i, d.audioDelay.getSample (ch, delayedPosition));
                d.meterInput.setSample (ch, i, d.audioDelay.getSample (ch + d.channels, delayedPosition));
            }
            d.audioCursor = delayedPosition;
        }

        // A stack-only non-owning view prevents the oversampler seeing spare
        // capacity at the end of a short host block. All scratch lives in prepare.
        juce::AudioBuffer<float> wetView (d.wet.getArrayOfWritePointers(), d.channels, n);
        d.fet.process (wetView, n, d.reductionTrace.data());
        juce::AudioBuffer<float> wetHView (d.wetH.getArrayOfWritePointers(), d.channels, n);
        d.fetH.process (wetHView, n, d.reductionTraceH.data());
        for (int i = 0; i < n; ++i)
        {
            const auto blend = d.wetAmount.getNextValue();
            const auto h = d.revisionBlend.getNextValue();
            const auto grPosition = static_cast<std::size_t> (d.reductionCursor);
            const auto traceIndex = static_cast<std::size_t> (i);
            const auto trace = h == 0.0f ? d.reductionTrace[traceIndex]
                             : h == 1.0f ? d.reductionTraceH[traceIndex]
                             : d.reductionTrace[traceIndex] * (1.0f - h) + d.reductionTraceH[traceIndex] * h;
            d.reductionDelay[grPosition] = juce::jmax (0.0f, -trace);
            d.reductionCursor = (d.reductionCursor + 1) % static_cast<int> (d.reductionDelay.size());
            const auto gr = d.reductionDelay[static_cast<std::size_t> (d.reductionCursor)] * blend;
            d.reductionPeak = juce::jmax (d.reductionPeak, gr);
            for (int ch = 0; ch < actualChannels; ++ch)
            {
                const auto wetSample = h == 0.0f ? d.wet.getSample (ch, i)
                                     : h == 1.0f ? d.wetH.getSample (ch, i)
                                     : d.wet.getSample (ch, i) * (1.0f - h) + d.wetH.getSample (ch, i) * h;
                const float output = safeSample (d.dry.getSample (ch, i) * (1.0f - blend)
                                               + wetSample * blend);
                buffer.setSample (ch, start + i, output);
                const auto c = static_cast<std::size_t> (ch);
                // Meter the same bypass transition as the delivered path:
                // bypass removes the input control, so fully dry IN == OUT.
                const auto inputSample = d.meterInput.getSample (ch, i) * blend
                                       + d.dry.getSample (ch, i) * (1.0f - blend);
                d.inputPeak[c] = juce::jmax (d.inputPeak[c], std::abs (inputSample));
                d.outputPeak[c] = juce::jmax (d.outputPeak[c], std::abs (output));
                d.inputEnergy[c] += double (inputSample) * double (inputSample);
                d.outputEnergy[c] += double (output) * double (output);
            }
            if (actualChannels == 1)
            {
                d.inputPeak[1] = d.inputPeak[0];
                d.outputPeak[1] = d.outputPeak[0];
                d.inputEnergy[1] = d.inputEnergy[0];
                d.outputEnergy[1] = d.outputEnergy[0];
            }
            ++d.frameSamples;
            d.meterPhase += 60.0;
            if (d.meterPhase >= d.sampleRate)
            {
                d.meterPhase -= d.sampleRate;
                d.publish (bridge, blend <= 0.0001f);
            }
        }
    }
    for (int ch = actualChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, count);
}
}
