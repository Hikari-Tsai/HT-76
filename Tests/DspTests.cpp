#include "DspEngine.h"
#include "FetLimiterDsp.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#if defined (__APPLE__)
#include <dlfcn.h>
#include <cstdlib>
namespace allocation_probe
{
thread_local bool armed = false;
thread_local std::size_t count = 0;
using Logger = void (*) (std::uint32_t, std::uintptr_t, std::uintptr_t,
                          std::uintptr_t, std::uintptr_t, std::uint32_t);
static void record (std::uint32_t type, std::uintptr_t, std::uintptr_t,
                    std::uintptr_t, std::uintptr_t, std::uint32_t)
{
    if (armed && (type & 2u) != 0) ++count;
}
struct Scope
{
    // Darwin's allocator logger observes malloc/calloc/realloc including calls
    // from JUCE HeapBlock and libc++; no product code or dylib is interposed.
    Logger* slot = reinterpret_cast<Logger*> (dlsym (RTLD_DEFAULT, "malloc_logger"));
    Logger previous = slot != nullptr ? *slot : nullptr;
    Scope() { if (slot != nullptr) *slot = &record; }
    ~Scope() { armed = false; if (slot != nullptr) *slot = previous; }
};
}
#endif

namespace
{
void require (bool ok, const char* message)
{
    if (! ok) throw std::runtime_error (message);
}

void fillTone (juce::AudioBuffer<float>& audio, int offset, double rate, float level = 0.3f)
{
    for (int i = 0; i < audio.getNumSamples(); ++i)
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            audio.setSample (ch, i, level * (ch == 0 ? 1.0f : 0.5f)
                * std::sin (float (juce::MathConstants<double>::twoPi * 1000.0 * (offset + i) / rate)));
}

void silenceAndSampleRates()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (const int channels : { 1, 2 })
        {
            field::DspEngine engine;
            engine.prepare (rate, 127, channels);
            require (rate < 88200.0 ? engine.getLatencySamples() > 0 : engine.getLatencySamples() == 0,
                     "oversampling latency policy");
            juce::AudioBuffer<float> audio (channels, int (rate));
            audio.clear();
            engine.process (audio, {});
            require (audio.getMagnitude (0, audio.getNumSamples()) < 1.0e-8f, "silent input generates audio");
            field::MeterFrame frame;
            int frames = 0;
            while (engine.meters().pop (frame))
            {
                ++frames;
                require (frame.inputDb[0] == -100.0f && frame.outputDb[0] == -100.0f,
                         "silent input generates level meters");
                require (frame.reductionDb < 0.001f, "silent input generates reduction");
            }
            require (frames > 0 && frame.sequence == 60, "meter sample clock must emit 60 frames/second");
        }
}

void bypassLatency()
{
    for (const double rate : { 48000.0, 96000.0 })
        for (const bool hostBypass : { false, true })
          for (const int revision : {0, 1})
        {
            field::DspEngine engine;
            engine.prepare (rate, 17, 2);
            field::DspEngine::Settings settings;
            settings.revision = revision;
            settings.inputDb = 24.0f;
            settings.outputDb = -12.0f;
            settings.bypass = ! hostBypass;
            juce::AudioBuffer<float> audio (2, 1000);
            audio.clear();
            audio.setSample (0, 0, 0.7f);
            audio.setSample (1, 0, -0.4f);
            engine.process (audio, settings, hostBypass);
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                require (std::abs (audio.getSample (0, i) - (i == engine.getLatencySamples() ? 0.7f : 0.0f)) < 1.0e-7f,
                         "bypass dry signal must be unity and latency matched");
                require (std::abs (audio.getSample (1, i) - (i == engine.getLatencySamples() ? -0.4f : 0.0f)) < 1.0e-7f,
                         "bypass stereo dry signal");
            }
        }
}

void partitionAndStereoLink()
{
    field::DspEngine one, many;
    one.prepare (48000.0, 257, 2);
    many.prepare (48000.0, 257, 2);
    field::DspEngine::Settings settings;
    settings.inputDb = 18.0f;
    settings.attackUs = 100.0f;
    juce::AudioBuffer<float> whole (2, 16000), divided (2, 16000);
    fillTone (whole, 0, 48000.0);
    divided.makeCopyOf (whole);
    one.process (whole, settings);
    for (int pos = 0; pos < divided.getNumSamples();)
    {
        const int count = juce::jmin (1 + (pos * 13) % 311, divided.getNumSamples() - pos);
        juce::AudioBuffer<float> view (divided.getArrayOfWritePointers(), 2, pos, count);
        many.process (view, settings);
        pos += count;
    }
    double error = 0.0, energyL = 0.0, energyR = 0.0;
    for (int i = 0; i < whole.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
            error = std::max (error, double (std::abs (whole.getSample (ch, i) - divided.getSample (ch, i))));
        if (i > 10000)
        {
            energyL += std::pow (whole.getSample (0, i), 2);
            energyR += std::pow (whole.getSample (1, i), 2);
        }
    }
    require (error < 2.0e-5, "audio depends on host block partition");
    const auto separation = 10.0 * std::log10 (energyL / energyR);
    require (separation > 5.2 && separation < 6.8, "stereo link fails to preserve channel separation");
    field::MeterFrame frame;
    float reduction = 0.0f;
    while (one.meters().pop (frame)) reduction = std::max (reduction, frame.reductionDb);
    require (reduction > 6.0f, "driven FET signal must show actual compression");
    require (std::abs (frame.inputDb[0] - (18.0f - 10.457575f)) < 0.05f,
             "input meter does not measure actual post-input-gain signal");
    require (std::abs (frame.inputRmsDb[0] - (18.0f - 13.467875f)) < 0.05f,
             "input RMS meter wrong");
}

void measuredOutputFrames()
{
    field::DspEngine engine;
    engine.prepare (96000.0, 37, 2);
    field::DspEngine::Settings settings;
    settings.inputDb = 6.0f;
    juce::AudioBuffer<float> audio (2, 1600);
    for (int block = 0; block < 4; ++block)
    {
        fillTone (audio, block * 1600, 96000.0, 0.2f);
        engine.process (audio, settings);
        field::MeterFrame frame;
        require (engine.meters().pop (frame), "missing one-frame output meter");
        require (frame.sequence == static_cast<std::uint64_t> (block + 1), "meter sequence wrong");
        for (int ch = 0; ch < 2; ++ch)
        {
            double sum = 0.0;
            float peak = 0.0f;
            for (int i = 0; i < 1600; ++i)
            {
                const auto sample = audio.getSample (ch, i);
                sum += double (sample) * double (sample);
                peak = std::max (peak, std::abs (sample));
            }
            const auto c = static_cast<std::size_t> (ch);
            const auto rms = 20.0 * std::log10 (std::sqrt (sum / 1600.0));
            require (std::abs (frame.outputDb[c] - 20.0f * std::log10 (peak)) < 0.001f,
                     "output peak meter differs from delivered audio");
            require (std::abs (double (frame.outputRmsDb[c]) - rms) < 0.001,
                     "output RMS meter differs from delivered audio");
        }
    }
}

void bypassMeterIdentity()
{
    for (const bool hostBypass : { false, true })
    {
        field::DspEngine engine;
        engine.prepare (48000.0, 128, 2);
        field::DspEngine::Settings settings;
        settings.inputDb = 24.0f;
        settings.outputDb = -12.0f;
        settings.bypass = ! hostBypass;
        juce::AudioBuffer<float> audio (2, 800);
        fillTone (audio, 0, 48000.0, 0.2f);
        engine.process (audio, settings, hostBypass);
        field::MeterFrame frame;
        require (engine.meters().pop (frame), "bypass frame missing");
        require (frame.bypassed && frame.reductionDb < 0.0001f, "bypass meter reports compression");
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            require (std::abs (frame.inputDb[ch] - frame.outputDb[ch]) < 0.0001f,
                     "fully bypassed IN/OUT peak meters disagree");
            require (std::abs (frame.inputRmsDb[ch] - frame.outputRmsDb[ch]) < 0.0001f,
                     "fully bypassed IN/OUT RMS meters disagree");
        }
    }
}

void resetMatchesFreshPrepare()
{
    field::DspEngine resetEngine, freshEngine;
    resetEngine.prepare (48000.0, 128, 2);
    freshEngine.prepare (48000.0, 128, 2);
    field::DspEngine::Settings oldSettings, newSettings;
    oldSettings.outputDb = -12.0f;
    newSettings.outputDb = 12.0f;
    juce::AudioBuffer<float> resetAudio (2, 2048), freshAudio (2, 2048);
    fillTone (resetAudio, 0, 48000.0);
    resetEngine.process (resetAudio, oldSettings);
    resetEngine.reset();
    fillTone (resetAudio, 0, 48000.0);
    freshAudio.makeCopyOf (resetAudio);
    resetEngine.process (resetAudio, newSettings);
    freshEngine.process (freshAudio, newSettings);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 2048; ++i)
            require (std::abs (resetAudio.getSample (ch, i) - freshAudio.getSample (ch, i)) < 1.0e-6f,
                     "reset retains an earlier output gain ramp");
}

void nonfiniteRecovery()
{
  for (int revision : {0, 1})
  {
    field::DspEngine engine;
    engine.prepare (48000.0, 64, 2);
    juce::AudioBuffer<float> audio (2, 256);
    audio.clear();
    audio.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
    audio.setSample (0, 1, std::numeric_limits<float>::infinity());
    audio.setSample (1, 2, 1.0e30f);
    field::DspEngine::Settings settings;
    settings.revision = revision;
    settings.inputDb = std::numeric_limits<float>::quiet_NaN();
    settings.releaseMs = std::numeric_limits<float>::infinity();
    engine.process (audio, settings);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            require (std::isfinite (audio.getSample (ch, i)), "invalid input escapes sanitization");
    for (int b = 0; b < 50; ++b)
    {
        fillTone (audio, b * audio.getNumSamples(), 48000.0);
        field::DspEngine::Settings recovered; recovered.revision = revision;
        engine.process (audio, recovered);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < audio.getNumSamples(); ++i)
                require (std::isfinite (audio.getSample (ch, i)) && std::abs (audio.getSample (ch, i)) < 100.0f,
                         "invalid sample poisons DSP state");
    }
    require (audio.getMagnitude (0, audio.getNumSamples()) > 0.01f, "DSP fails to recover after nonfinite input");
  }
}

void bypassTransition()
{
    field::DspEngine engine;
    engine.prepare (96000.0, 128, 1);
    juce::AudioBuffer<float> audio (1, 1024);
    field::DspEngine::Settings settings;
    for (int block = 0; block < 80; ++block)
    {
        juce::FloatVectorOperations::fill (audio.getWritePointer (0), 0.1f, 1024);
        engine.process (audio, settings);
    }
    const auto before = audio.getSample (0, 1023);
    juce::FloatVectorOperations::fill (audio.getWritePointer (0), 0.1f, 1024);
    settings.bypass = true;
    engine.process (audio, settings);
    require (std::abs (audio.getSample (0, 0) - before) < 0.005f, "bypass engagement clicks");
    require (std::abs (audio.getSample (0, 1023) - 0.1f) < 1.0e-7f, "bypass fade does not reach dry");
    juce::FloatVectorOperations::fill (audio.getWritePointer (0), 0.1f, 1024);
    settings.bypass = false;
    engine.process (audio, settings);
    require (std::abs (audio.getSample (0, 0) - 0.1f) < 0.005f, "bypass disengagement clicks");
    require (std::abs (audio.getSample (0, 1023)) < 0.005f, "bypass fade does not reach wet");
}

void upstreamTransientReduction()
{
    FetLimiterDsp wholeCore, sampleCore;
    wholeCore.setOversamplingLog2 (0);
    sampleCore.setOversamplingLog2 (0);
    wholeCore.prepare (96000.0, 16000, 1);
    sampleCore.prepare (96000.0, 1, 1);
    FetLimiterDsp::Parameters p;
    p.inputDb = 20.0f;
    p.attackUs = 20.0f;
    p.releaseMs = 50.0f;
    wholeCore.setParameters (p);
    sampleCore.setParameters (p);
    juce::AudioBuffer<float> whole (1, 16000), bySample (1, 16000);
    fillTone (whole, 0, 96000.0, 0.4f);
    whole.clear (2000, 14000);
    bySample.makeCopyOf (whole);
    wholeCore.process (whole, 16000);
    float minimum = 0.0f;
    for (int i = 0; i < 16000; ++i)
    {
        juce::AudioBuffer<float> view (bySample.getArrayOfWritePointers(), 1, i, 1);
        sampleCore.process (view, 1);
        minimum = std::min (minimum, sampleCore.getGainReductionDb());
    }
    require (minimum < -6.0f, "transient fixture must compress");
    require (std::abs (minimum - wholeCore.getGainReductionDb()) < 0.01f,
             "upstream GR reports final state instead of actual transient maximum");
}

void callbackAllocations()
{
#if defined (__APPLE__)
    allocation_probe::Scope logger;
    // Confirm the allocator logger is active before trusting a zero count.
    allocation_probe::count = 0;
    allocation_probe::armed = true;
    void* (*volatile allocate) (std::size_t) = &std::malloc;
    void* probe = allocate (71);
    allocation_probe::armed = false;
    std::free (probe);
    require (allocation_probe::count > 0, "allocation instrumentation is inactive");

    for (const int channels : { 1, 2 })
    {
        field::DspEngine engine;
        engine.prepare (48000.0, 64, channels);
        juce::AudioBuffer<float> audio (channels, 1024);
        fillTone (audio, 0, 48000.0);
        field::DspEngine::Settings settings;
        allocation_probe::count = 0;
        allocation_probe::armed = true;
        // Includes first callback, an oversized block, and automating every control.
        for (int block = 0; block < 20; ++block)
        {
            const int samples = block % 2 == 0 ? 1024 : 31;
            juce::AudioBuffer<float> view (audio.getArrayOfWritePointers(), channels, samples);
            settings.inputDb = float (block);
            settings.outputDb = -float (block);
            settings.ratioIndex = block % 4;
            settings.allButtons = block % 3 == 0;
            settings.attackUs = 20.0f + float (block * 30);
            settings.releaseMs = 50.0f + float (block * 40);
            settings.bypass = block % 2 != 0;
            settings.revision = block % 2;
            engine.process (view, settings, block % 3 == 0);
        }
        allocation_probe::armed = false;
        require (allocation_probe::count == 0, "audio callback allocates memory");
    }
#endif
}

void bridgeConcurrency()
{
    field::MeterBridge bridge;
    std::atomic<bool> done { false };
    std::thread producer ([&]
    {
        for (std::uint64_t i = 1; i <= 100000; ++i)
        {
            field::MeterFrame f;
            f.sequence = i;
            f.inputDb[0] = float (i);
            f.inputDb[1] = -float (i);
            bridge.push (f);
        }
        done.store (true, std::memory_order_release);
    });
    bool valid = true;
    std::uint64_t previous = 0;
    do
    {
        field::MeterFrame f;
        while (bridge.pop (f))
        {
            valid = valid && f.sequence > previous && f.inputDb[0] == float (f.sequence)
                          && f.inputDb[1] == -float (f.sequence);
            previous = f.sequence;
        }
    } while (! done.load (std::memory_order_acquire));
    producer.join();
    require (valid, "SPSC bridge returns torn or reordered frames");
    for (std::uint64_t i = 100001; i < 110000; ++i)
    {
        field::MeterFrame f;
        f.sequence = i;
        bridge.push (f);
    }
    field::MeterFrame stale;
    require (! bridge.pop (stale), "closed editor replays stale metering backlog");
    field::MeterFrame latest;
    latest.sequence = 110000;
    bridge.push (latest);
    require (bridge.pop (latest) && latest.sequence == 110000, "bridge fails to resume fresh frames");
}

void revisionStagesAndRendering()
{
    for (double x : {0.0, 0.0001, 0.01, 0.1, 1.0, 4.0, 40.0, 640.0})
    {
        const auto y = field::RevHStages::pushPull (x);
        require (std::isfinite (y) && std::abs (y) <= 4.000001, "H amplifier escaped its supply bound");
        require (std::abs (y + field::RevHStages::pushPull (-x)) < 1.0e-7,
                 "H push-pull stage lost polarity symmetry");
        const auto u = (8.0 + 1.0 / 0.7) * x - 8.0 * y;
        const auto f = 4.0 * std::tanh ((u - 0.012 * std::tanh (u / 0.04)) / 4.0);
        require (std::abs (y - f) < 1.0e-6, "H feedback solver did not converge");
    }
    require (std::abs (field::RevHStages::pushPull (0.0001) / 0.0001 - 1.0) < 0.001,
             "H low-level amplifier gain is not normalized");
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
    {
        field::DspEngine d, h;
        d.prepare (rate, 127, 2); h.prepare (rate, 127, 2);
        field::DspEngine::Settings ds, hs;
        ds.inputDb = hs.inputDb = 12.0f;
        ds.outputDb = hs.outputDb = -6.0f;
        hs.revision = 1;
        juce::AudioBuffer<float> a (2, int (rate / 4)), b (2, int (rate / 4));
        a.clear(); b.clear(); h.process (b, hs);
        require (b.getMagnitude (0, b.getNumSamples()) == 0.0f, "H emits audio on silence");
        h.reset();
        fillTone (a, 0, rate); b.makeCopyOf (a);
        d.process (a, ds); h.process (b, hs);
        double delta = 0.0, energy = 0.0;
        for (int i = 0; i < a.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto value = b.getSample (ch, i);
                require (std::isfinite (value) && std::abs (value) < 10.0f, "H output unstable");
                delta += std::abs (value - a.getSample (ch, i));
                energy += std::abs (value);
            }
        require (energy > 1.0 && delta / energy > 0.01, "H model did not change the signal path");
        h.reset(); fillTone (a, 0, rate); h.process (a, hs);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                require (a.getSample (ch, i) == b.getSample (ch, i), "H reset differs from initial state");
    }
}

void revisionSwitchAndPartitions()
{
    field::DspEngine whole, split, referenceD;
    for (auto* engine : {&whole, &split, &referenceD}) engine->prepare (48000, 128, 2);
    field::DspEngine::Settings settings;
    juce::AudioBuffer<float> a (2, 1024), b (2, 1024), d (2, 1024);
    for (int block = 0; block < 12; ++block)
    {
        fillTone (a, block * 1024, 48000); b.makeCopyOf (a); d.makeCopyOf (a);
        settings.revision = block >= 4 && block < 8 ? 1 : 0;
        whole.process (a, settings);
        for (int start = 0; start < 1024; start += 31)
        {
            const int n = std::min (31, 1024 - start);
            juce::AudioBuffer<float> part (b.getArrayOfWritePointers(), 2, start, n);
            split.process (part, settings);
        }
        auto ds = settings; ds.revision = 0;
        referenceD.process (d, ds);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 1024; ++i)
            {
                require (std::abs (a.getSample (ch, i) - b.getSample (ch, i)) < 2.0e-5f,
                         "model switching depends on host block size");
                if (block < 4 || block >= 9)
                    require (a.getSample (ch, i) == d.getSample (ch, i),
                             "returning to D changed its running circuit state");
            }
        if (block == 4)
            require (std::abs (a.getSample (0, 0) - d.getSample (0, 0)) < 0.001f,
                     "model switch introduced an immediate signal jump");
    }
    settings.revision = 1; settings.bypass = true;
    for (int block = 0; block < 5; ++block) { fillTone (a, block * 1024, 48000); whole.process (a, settings); }
    field::MeterFrame f;
    bool received = false;
    while (whole.meters().pop (f)) received = true;
    // A 60 Hz frame spanning the 5 ms fade can contain pre-bypass GR.
    // Validate the latest fully settled frame, not that transition aggregate.
    require (received && f.bypassed && f.reductionDb == 0.0f, "H bypass retained GR");
    require (f.inputDb == f.outputDb, "H settled bypass meters differ");
}
}

int runDspTests();
int runDspTests()
{
    int failed = 0;
    const auto run = [&] (const char* name, auto fn)
    {
        try { fn(); std::cout << "PASS DSP: " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL DSP: " << name << ": " << e.what() << '\n'; }
    };
    run ("Rev H solver, sample rates, sound and reset", revisionStagesAndRendering);
    run ("D/H switching, partition invariance and D state continuity", revisionSwitchAndPartitions);
    run ("silence/sample rates/mono/meter sample clock", silenceAndSampleRates);
    run ("plugin and host bypass latency", bypassLatency);
    run ("block partitions/stereo link/real gain meters", partitionAndStereoLink);
    run ("nonfinite signal and parameter recovery", nonfiniteRecovery);
    run ("output meter equals delivered sample peaks/RMS", measuredOutputFrames);
    run ("fully bypassed IN/OUT meter identity", bypassMeterIdentity);
    run ("reset matches fresh prepare", resetMatchesFreshPrepare);
    run ("concurrent bridge and stale backlog", bridgeConcurrency);
    run ("first callback/oversize/automation allocations", callbackAllocations);
    run ("smooth bypass transition", bypassTransition);
    run ("upstream transient gain-cell maximum", upstreamTransientReduction);
    return failed;
}
