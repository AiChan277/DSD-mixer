#include "TestCheck.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

#include "Audio/AudioTypes.h"
#include "Audio/ProcessCpuTracker.h"
#include "DSP/GainProcessor.h"
#include "DSP/PanProcessor.h"
#include "DSP/MeterProcessor.h"
#include "Audio/AudioInputSource.h"
#include "Audio/WindowAudioCapture.h"
#include "Channel/AudioChannel.h"
#include "Channel/ChannelManager.h"
#include "Scheduler/DSPScheduler.h"
#include "Routing/RoutingEngine.h"
#include "Output/OutputManager.h"
#include "Session/SessionManager.h"
#include "Plugin/PluginRack.h"
#include <thread>
#include "Audio/AudioEngine.h"
#include "Audio/BlockTimingHistogram.h"
#include "Audio/AdaptiveResamplingFifo.h"

class MockAudioPluginInstance : public juce::AudioPluginInstance
{
public:
    MockAudioPluginInstance(const juce::String& name, float gainFactor, float offset = 0.0f)
        : AudioPluginInstance(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          pluginName(name), multiplier(gainFactor), addend(offset)
    {
    }

    const juce::String getName() const override { return pluginName; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = d[i] * multiplier + addend;
        }
    }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override
    {
        juce::String stateStr = juce::String(multiplier) + "," + juce::String(addend);
        destData.replaceAll(stateStr.toRawUTF8(), stateStr.getNumBytesAsUTF8());
    }
    void setStateInformation(const void* data, int sizeInBytes) override
    {
        if (sizeInBytes > 0 && data != nullptr)
        {
            juce::String s(static_cast<const char*>(data), (size_t)sizeInBytes);
            auto tokens = juce::StringArray::fromTokens(s, ",", "");
            if (tokens.size() >= 2)
            {
                multiplier = tokens[0].getFloatValue();
                addend = tokens[1].getFloatValue();
            }
        }
    }
    void fillInPluginDescription(juce::PluginDescription& desc) const override
    {
        desc.name = pluginName;
        desc.descriptiveName = pluginName;
        desc.pluginFormatName = "InternalMock";
        desc.category = "Test";
        desc.fileOrIdentifier = pluginName + "_uid";
    }

    juce::String pluginName;
    float multiplier{1.0f};
    float addend{0.0f};
};

// Pure delay line that reports its latency, like a look-ahead plugin.
class MockDelayPlugin : public MockAudioPluginInstance
{
public:
    explicit MockDelayPlugin(int delaySamples) : MockAudioPluginInstance("Delay", 1.0f), delay(delaySamples)
    {
        setLatencySamples(delaySamples);
        line.assign(static_cast<size_t>(delaySamples) * 2, 0.0f);
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float& slot = line[static_cast<size_t>(pos * 2 + ch)];
                const float in = buffer.getSample(ch, i);
                buffer.setSample(ch, i, slot);
                slot = in;
            }
            pos = (pos + 1) % delay;
        }
    }
private:
    int delay;
    int pos{0};
    std::vector<float> line;
};

void testGainProcessor()
{
    std::cout << "[TEST] Running GainProcessor tests..." << std::endl;

    CHECK(std::abs(dsd::GainProcessor::dbToLinear(0.0f) - 1.0f) < 0.0001f);
    CHECK(std::abs(dsd::GainProcessor::dbToLinear(-6.0f) - 0.501187f) < 0.001f);
    CHECK(std::abs(dsd::GainProcessor::dbToLinear(6.0f) - 1.99526f) < 0.001f);
    CHECK(dsd::GainProcessor::dbToLinear(-60.0f) == 0.0f);

    std::cout << "[PASS] GainProcessor tests passed." << std::endl;
}

void testPanProcessor()
{
    std::cout << "[TEST] Running PanProcessor tests..." << std::endl;

    dsd::PanProcessor pan;
    pan.setPan(0.0f);
    CHECK(std::abs(pan.getGainL() - 0.70710678f) < 0.001f);
    CHECK(std::abs(pan.getGainR() - 0.70710678f) < 0.001f);

    float powerCenter = pan.getGainL() * pan.getGainL() + pan.getGainR() * pan.getGainR();
    CHECK(std::abs(powerCenter - 1.0f) < 0.001f);

    pan.setPan(-1.0f);
    CHECK(std::abs(pan.getGainL() - 1.0f) < 0.001f);
    CHECK(std::abs(pan.getGainR() - 0.0f) < 0.001f);

    pan.setPan(1.0f);
    CHECK(std::abs(pan.getGainL() - 0.0f) < 0.001f);
    CHECK(std::abs(pan.getGainR() - 1.0f) < 0.001f);

    std::cout << "[PASS] PanProcessor tests passed." << std::endl;
}

void testMeterProcessor()
{
    std::cout << "[TEST] Running MeterProcessor tests..." << std::endl;

    dsd::MeterProcessor meter;
    meter.prepare(48000.0);
    dsd::MeterValues values;

    const int numSamples = 128;
    std::vector<float> left(numSamples, 0.5f);
    std::vector<float> right(numSamples, 0.8f);

    meter.processBlock(left.data(), right.data(), numSamples, values);

    CHECK(std::abs(values.peakL.load() - 0.5f) < 0.001f);
    CHECK(std::abs(values.peakR.load() - 0.8f) < 0.001f);
    CHECK(!values.clipped.load());

    std::vector<float> clippedSamples(numSamples, 1.05f);
    meter.processBlock(clippedSamples.data(), clippedSamples.data(), numSamples, values);
    CHECK(values.clipped.load() == true);

    std::cout << "[PASS] MeterProcessor tests passed." << std::endl;
}

void test16x4RoutingAndOutputs()
{
    std::cout << "[TEST] Running 16x4 Routing & Configurable Outputs tests..." << std::endl;

    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(dsd::NUM_CHANNELS_LEVEL1);
    channelMgr.prepare(48000.0, 128);

    dsd::OutputManager outputMgr;
    outputMgr.initializeDefaultOutputs(dsd::NUM_OUTPUT_BUSES_LEVEL1);
    outputMgr.prepare(48000.0, 128);

    CHECK(channelMgr.getNumChannels() == 16);
    CHECK(outputMgr.getNumOutputs() == 4);

    // Channel 1: Sine generator
    auto* ch1 = channelMgr.getChannel(0);
    auto sine1 = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.5f);
    sine1->setEnabled(true);
    ch1->setInputSource(std::move(sine1));

    // Dummy input process
    juce::AudioBuffer<float> dummyDevIn(2, 128);
    dummyDevIn.clear();
    channelMgr.processChannels(dummyDevIn, 128);

    dsd::RoutingEngine router;
    // Enable CH 1 to OUT 01 (default) and OUT 02
    router.setRouteEnabled(0, 1, true);
    router.setRouteGainDb(0, 1, -6.0f); // -6 dB send to OUT 02

    router.routeChannelsToOutputs(channelMgr, outputMgr, 128);
    outputMgr.processOutputs(128);

    // OUT 01 should receive full signal
    float out1Mag = outputMgr.getOutput(0)->getBuffer().getMagnitude(0, 0, 128);
    CHECK(out1Mag > 0.1f);

    // OUT 02 should receive attenuated signal (-6 dB approx half amplitude)
    float out2Mag = outputMgr.getOutput(1)->getBuffer().getMagnitude(0, 0, 128);
    CHECK(out2Mag > 0.05f);
    CHECK(out2Mag < out1Mag);

    std::cout << "[PASS] 16x4 Routing & Output tests passed." << std::endl;
}

void testMulticoreScheduler()
{
    std::cout << "[TEST] Running Multicore Scheduler & Adaptive Worker Pool tests..." << std::endl;

    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(16);
    channelMgr.prepare(48000.0, 128);

    dsd::DSPScheduler scheduler;
    // 1. Thread pool is fixed at 8 threads (zero runtime thread churn)
    CHECK(scheduler.getNumWorkers() == 8);
    CHECK(scheduler.getMaxWorkers() == 8);

    juce::AudioBuffer<float> dummyInput(2, 128);
    dummyInput.clear();

    // Initial state: default active workers is 4
    CHECK(scheduler.getActiveWorkerCount() == 4);

    // 2. Test parallel dispatch across blocks
    for (int block = 0; block < 10; ++block)
    {
        scheduler.processChannelsParallel(channelMgr, dummyInput, 128);
    }

    // Verify that active workers 0..3 are unparked, 4..7 are parked, and that the
    // caller plus the active workers processed all 16 channels exactly once.
    for (int i = 0; i < 4; ++i)
        CHECK(!scheduler.getWorkerStats(i)->isParked.load());
    CHECK(scheduler.getLastBlockTaskTotal() == 16);
    CHECK(scheduler.getCallerTasksLastBlock() <= 16);

    for (int i = 4; i < 8; ++i)
    {
        CHECK(scheduler.getWorkerStats(i)->isParked.load());
        CHECK(scheduler.getWorkerStats(i)->tasksAssigned.load() == 0);
        CHECK(scheduler.getWorkerStats(i)->workerUtilizationPercent.load() == 0.0f);
    }

    // 3. Test Adaptive Scaling - Fast Attack Scale UP
    // Audio deadline for 128 samples @ 48kHz = 128 / 48000 = ~0.0026667s (2.67 ms)
    const double deadlineSec = 128.0 / 48000.0;

    // Simulate high load: dspTime = 2.0 ms (75% of deadline)
    // Needs 4 consecutive high load blocks to trigger attack
    for (int b = 0; b < 3; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0020, deadlineSec, 16);
    // Still 4 before 4th block
    CHECK(scheduler.getActiveWorkerCount() == 4);

    // 4th block triggers fast attack (+2 -> 6 workers)
    scheduler.updateAdaptiveWorkerCount(0.0020, deadlineSec, 16);
    CHECK(scheduler.getActiveWorkerCount() == 6);

    // Extreme load spike: dspTime = 2.3 ms (>80% of deadline)
    for (int b = 0; b < 4; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0023, deadlineSec, 16);
    CHECK(scheduler.getActiveWorkerCount() == 8);
    std::cout << "  [PASS] Fast attack scaled up to 8 workers under high load." << std::endl;

    // 4. Test Task Graph Floor with Light Load
    // With 16 active channels, graph floor is 6. Even after 350 blocks of light load, it should decay to 6, not 2.
    for (int b = 0; b < 350; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0001, deadlineSec, 16);
    CHECK(scheduler.getActiveWorkerCount() == 6);
    std::cout << "  [PASS] Task graph floor for 16 channels held workers at 6." << std::endl;

    // With 2 active channels, graph floor is 2. After another 350 blocks, workers decay 6 -> 4 -> 2.
    for (int b = 0; b < 350; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0001, deadlineSec, 2);
    CHECK(scheduler.getActiveWorkerCount() == 4);

    for (int b = 0; b < 350; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0001, deadlineSec, 2);
    CHECK(scheduler.getActiveWorkerCount() == 2);
    std::cout << "  [PASS] Slow decay scaled down to 2 workers for low load (2 channels)." << std::endl;

    // 5. Test parallel dispatch with 2 active workers
    for (int block = 0; block < 10; ++block)
    {
        scheduler.processChannelsParallel(channelMgr, dummyInput, 128);
    }
    CHECK(!scheduler.getWorkerStats(0)->isParked.load());
    CHECK(!scheduler.getWorkerStats(1)->isParked.load());
    CHECK(scheduler.getLastBlockTaskTotal() == 16);
    CHECK(scheduler.getWorkerStats(2)->isParked.load());
    CHECK(scheduler.getWorkerStats(7)->isParked.load());

    // 6. Test manual override
    scheduler.setManualWorkerCount(5);
    CHECK(scheduler.getActiveWorkerCount() == 5);
    CHECK(!scheduler.isAdaptiveScalingEnabled());

    std::cout << "[PASS] Multicore Scheduler & Adaptive Worker Pool tests passed cleanly." << std::endl;
}

// Counts how many times each channel's input is read (== processed) per block.
class CountingInputSource : public dsd::AudioInputSource
{
public:
    explicit CountingInputSource(std::atomic<int>& counterRef) : counter(counterRef) {}
    void prepare(double, int) override {}
    void releaseResources() override {}
    void readBlock(juce::AudioBuffer<float>& target, const juce::AudioBuffer<float>&, int numSamples) override
    {
        counter.fetch_add(1, std::memory_order_relaxed);
        for (int ch = 0; ch < target.getNumChannels(); ++ch)
            target.clear(ch, 0, numSamples);
        // A little work so blocks overlap across threads.
        volatile float sink = 0.0f;
        for (int i = 0; i < 2000; ++i)
            sink = sink + static_cast<float>(i) * 1.0e-7f;
    }
private:
    std::atomic<int>& counter;
};

void testSchedulerExactlyOnceStress()
{
    std::cout << "[TEST] Running Scheduler exactly-once stress test..." << std::endl;

    constexpr int numChannels = 16;
    constexpr int numBlocks = 3000;

    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(numChannels);
    channelMgr.prepare(48000.0, 128);

    std::vector<std::atomic<int>> counters(numChannels);
    for (int i = 0; i < numChannels; ++i)
    {
        counters[static_cast<size_t>(i)].store(0);
        channelMgr.getChannel(i)->setInputSource(std::make_unique<CountingInputSource>(counters[static_cast<size_t>(i)]));
    }

    dsd::DSPScheduler scheduler;
    juce::AudioBuffer<float> dummyInput(2, 128);
    dummyInput.clear();

    int blocksWithWrongTotal = 0;
    for (int block = 0; block < numBlocks; ++block)
    {
        // Change the active worker count frequently to exercise park/unpark
        // and late-waking workers from earlier blocks.
        if (block % 7 == 0)
            scheduler.setManualWorkerCount(2 + (block / 7) % 7);

        scheduler.processChannelsParallel(channelMgr, dummyInput, 128);

        if (scheduler.getLastBlockTaskTotal() != numChannels)
            ++blocksWithWrongTotal;

        // After each block every channel must have been processed exactly block+1 times.
        for (int i = 0; i < numChannels; ++i)
            CHECK(counters[static_cast<size_t>(i)].load() == block + 1);
    }

    CHECK(blocksWithWrongTotal == 0);
    std::cout << "[PASS] Scheduler processed " << numChannels << " channels exactly once in each of "
              << numBlocks << " blocks." << std::endl;
}

// Constant DC source: makes gain ramps directly observable sample by sample.
class ConstantInputSource : public dsd::AudioInputSource
{
public:
    explicit ConstantInputSource(float v) : value(v) {}
    void prepare(double, int) override {}
    void releaseResources() override {}
    void readBlock(juce::AudioBuffer<float>& target, const juce::AudioBuffer<float>&, int numSamples) override
    {
        for (int ch = 0; ch < target.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill(target.getWritePointer(ch), value, numSamples);
    }
private:
    float value;
};

void testPanRampIsSmooth()
{
    std::cout << "[TEST] Running Pan ramp (zipper-free) tests..." << std::endl;

    dsd::PanProcessor pan;
    pan.setPan(0.0f);
    pan.snapToTarget();

    constexpr int n = 128;
    std::vector<float> l(n, 1.0f), r(n, 1.0f);

    // Hard left from centre: gains must move gradually across the block.
    pan.setPan(-1.0f);
    pan.processStereo(l.data(), r.data(), n);

    float maxStepL = 0.0f, maxStepR = 0.0f;
    for (int i = 1; i < n; ++i)
    {
        maxStepL = std::max(maxStepL, std::abs(l[static_cast<size_t>(i)] - l[static_cast<size_t>(i - 1)]));
        maxStepR = std::max(maxStepR, std::abs(r[static_cast<size_t>(i)] - r[static_cast<size_t>(i - 1)]));
    }
    CHECK(maxStepL < 0.01f);
    CHECK(maxStepR < 0.01f);
    CHECK(std::abs(l[0] - 0.7071f) < 0.01f);      // starts near the old (centre) gain
    CHECK(std::abs(l[n - 1] - 1.0f) < 0.001f);     // lands exactly on the target
    CHECK(std::abs(r[n - 1] - 0.0f) < 0.001f);

    // Next block is steady at the target.
    std::fill(l.begin(), l.end(), 1.0f);
    std::fill(r.begin(), r.end(), 1.0f);
    pan.processStereo(l.data(), r.data(), n);
    CHECK(std::abs(l[0] - 1.0f) < 0.0001f);
    CHECK(std::abs(r[0]) < 0.0001f);

    std::cout << "[PASS] Pan ramp tests passed." << std::endl;
}

void testRouteSendLevelRamp()
{
    std::cout << "[TEST] Running Routing send-level ramp tests..." << std::endl;

    constexpr int n = 128;
    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(dsd::NUM_CHANNELS_LEVEL1);
    channelMgr.prepare(48000.0, n);
    channelMgr.getChannel(0)->setInputSource(std::make_unique<ConstantInputSource>(0.5f));

    dsd::OutputManager outputMgr;
    outputMgr.initializeDefaultOutputs(dsd::NUM_OUTPUT_BUSES_LEVEL1);
    outputMgr.prepare(48000.0, n);

    dsd::RoutingEngine router; // CH1 -> OUT1 at 0 dB by default
    juce::AudioBuffer<float> devIn(2, n);
    devIn.clear();

    auto runBlock = [&]() {
        channelMgr.processChannels(devIn, n);
        router.routeChannelsToOutputs(channelMgr, outputMgr, n);
    };

    for (int b = 0; b < 3; ++b)
        runBlock(); // settle
    const auto& out = outputMgr.getOutput(0)->getBuffer();
    const float steady = out.getSample(0, n - 1);
    CHECK(steady > 0.1f);

    // Drop the send by 20 dB: the change must be spread over the block.
    router.setRouteGainDb(0, 0, -20.0f);
    runBlock();
    float maxStep = 0.0f;
    for (int i = 1; i < n; ++i)
        maxStep = std::max(maxStep, std::abs(out.getSample(0, i) - out.getSample(0, i - 1)));
    CHECK(maxStep < steady * 0.02f);
    CHECK(std::abs(out.getSample(0, n - 1) - steady * 0.1f) < steady * 0.01f);

    // Disabling the route stays an immediate cut (unchanged behaviour).
    router.setRouteEnabled(0, 0, false);
    runBlock();
    CHECK(out.getMagnitude(0, 0, n) == 0.0f);

    // Re-enabling starts directly at the configured level (no ramp from zero).
    router.setRouteEnabled(0, 0, true);
    runBlock();
    CHECK(std::abs(out.getSample(0, 0) - steady * 0.1f) < steady * 0.01f);

    std::cout << "[PASS] Routing send-level ramp tests passed." << std::endl;
}

void testBlockTimingHistogram()
{
    std::cout << "[TEST] Running Block timing histogram tests..." << std::endl;

    dsd::BlockTimingHistogram hist;
    CHECK(hist.snapshot().total == 0);
    CHECK(hist.snapshot().percentileUpperEdge(99.0) == 0.0f);

    const double deadline = 0.010;
    for (int i = 0; i < 980; ++i) hist.record(0.0005, deadline); //  5% -> bucket <=10%
    for (int i = 0; i < 15; ++i)  hist.record(0.0045, deadline); // 45% -> bucket <=50%
    for (int i = 0; i < 5; ++i)   hist.record(0.0120, deadline); // 120% -> over deadline

    auto snap = hist.snapshot();
    CHECK(snap.total == 1000);
    CHECK(snap.counts[0] == 980);
    CHECK(snap.counts[4] == 15);
    CHECK(snap.counts[dsd::BlockTimingHistogram::kNumBuckets - 1] == 5);
    CHECK(snap.percentileUpperEdge(95.0) == 0.1f);
    CHECK(snap.percentileUpperEdge(99.0) == 0.5f);
    CHECK(snap.percentileUpperEdge(99.9) > 1.0f);

    // Exactly on an edge belongs to that bucket.
    CHECK(dsd::BlockTimingHistogram::bucketFor(0.1) == 0);
    CHECK(dsd::BlockTimingHistogram::bucketFor(1.0) == 9);
    CHECK(dsd::BlockTimingHistogram::bucketFor(1.0001) == 10);

    hist.reset();
    CHECK(hist.snapshot().total == 0);

    // The engine records one entry per callback and clears it on reset.
    dsd::AudioEngine engine;
    engine.prepare(48000.0, 256, 2);
    juce::AudioBuffer<float> inBuf(2, 256), outBuf(2, 256);
    inBuf.clear();
    juce::AudioIODeviceCallbackContext ctx;
    for (int i = 0; i < 20; ++i)
        engine.audioDeviceIOCallbackWithContext(inBuf.getArrayOfReadPointers(), 2,
                                                outBuf.getArrayOfWritePointers(), 2, 256, ctx);
    CHECK(engine.getBlockTimingHistogram().snapshot().total == 20);
    engine.resetPerformanceMetrics();
    CHECK(engine.getBlockTimingHistogram().snapshot().total == 0);

    std::cout << "[PASS] Block timing histogram tests passed." << std::endl;
}

void testPluginCountIsLockFree()
{
    std::cout << "[TEST] Running lock-free plugin count tests..." << std::endl;

    dsd::PluginRack rack;
    rack.prepare(48000.0, 128);
    CHECK(rack.getNumPlugins() == 0);

    const auto idA = rack.addPlugin(std::make_unique<MockAudioPluginInstance>("A", 1.0f), "A");
    rack.addPlugin(std::make_unique<MockAudioPluginInstance>("B", 1.0f), "B");
    CHECK(rack.getNumPlugins() == 2);

    rack.removePluginById(idA);
    CHECK(rack.getNumPlugins() == 1);
    rack.removePlugin(0);
    CHECK(rack.getNumPlugins() == 0);

    rack.addPlugin(std::make_unique<MockAudioPluginInstance>("C", 1.0f), "C");
    rack.clear();
    CHECK(rack.getNumPlugins() == 0);

    // A UI thread hammering getNumPlugins() must never make the audio thread
    // skip the plugin chain. Plugin multiplies by 2: every block must be doubled.
    rack.addPlugin(std::make_unique<MockAudioPluginInstance>("Gain2", 2.0f), "Gain2");
    std::atomic<bool> stop{false};
    std::thread uiThread([&]() {
        int sink = 0;
        while (!stop.load(std::memory_order_relaxed))
            sink += rack.getNumPlugins();
        (void)sink;
    });

    juce::AudioBuffer<float> buf(2, 128);
    juce::MidiBuffer midi;
    int skippedBlocks = 0;
    for (int block = 0; block < 20000; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buf.getWritePointer(ch), 0.25f, 128);
        rack.processBlock(buf, midi, 128);
        if (std::abs(buf.getSample(0, 0) - 0.5f) > 1.0e-6f)
            ++skippedBlocks;
    }
    stop.store(true);
    uiThread.join();
    CHECK(skippedBlocks == 0);

    std::cout << "[PASS] Lock-free plugin count tests passed (" << skippedBlocks << " skipped blocks)." << std::endl;
}

void testBypassCrossfadeIsClickFree()
{
    std::cout << "[TEST] Running click-free bypass (zero-latency crossfade) tests..." << std::endl;

    dsd::PluginRack rack;
    rack.prepare(48000.0, 128);
    const auto id = rack.addPlugin(std::make_unique<MockAudioPluginInstance>("Gain2", 2.0f), "Gain2");

    juce::AudioBuffer<float> buf(2, 128);
    juce::MidiBuffer midi;
    auto runBlock = [&]() {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buf.getWritePointer(ch), 0.5f, 128);
        rack.processBlock(buf, midi, 128);
    };

    runBlock();
    CHECK(std::abs(buf.getSample(0, 127) - 1.0f) < 1.0e-5f);

    rack.setBypassById(id, true);
    float prev = 1.0f, maxJump = 0.0f, last = 0.0f;
    int blocks = 0;
    do
    {
        runBlock();
        for (int i = 0; i < 128; ++i)
        {
            maxJump = std::max(maxJump, std::abs(buf.getSample(0, i) - prev));
            prev = buf.getSample(0, i);
        }
        last = buf.getSample(0, 127);
        ++blocks;
    } while (rack.isBypassTransitionActive() && blocks < 20);

    CHECK(maxJump < 0.01f);                         // a hard switch would jump by 0.5
    CHECK(blocks >= 3 && blocks <= 5);              // 10 ms at 48 kHz = 480 samples
    CHECK(std::abs(last - 0.5f) < 1.0e-5f);         // fully dry at the end

    std::cout << "[PASS] Bypass crossfade max step " << maxJump << " over " << blocks << " blocks." << std::endl;
}

void testLatencyPluginBypassDoesNotCombFilter()
{
    std::cout << "[TEST] Running latency-plugin bypass (fade-out / switch / fade-in) tests..." << std::endl;

    dsd::PluginRack rack;
    rack.prepare(48000.0, 128);
    const auto id = rack.addPlugin(std::make_unique<MockDelayPlugin>(64), "Delay64");
    CHECK(rack.getPluginLatencySamples(id) == 64);

    juce::AudioBuffer<float> buf(2, 128);
    juce::MidiBuffer midi;
    for (int b = 0; b < 4; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buf.getWritePointer(ch), 0.5f, 128);
        rack.processBlock(buf, midi, 128);
    }
    CHECK(rack.getProcessingLatencySamples() == 64);

    rack.setBypassById(id, true);
    float maxOut = 0.0f, minOut = 1.0f, maxJump = 0.0f, prev = 0.5f;
    for (int b = 0; b < 6; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buf.getWritePointer(ch), 0.5f, 128);
        rack.processBlock(buf, midi, 128);
        for (int i = 0; i < 128; ++i)
        {
            const float v = buf.getSample(0, i);
            maxOut = std::max(maxOut, v);
            minOut = std::min(minOut, v);
            maxJump = std::max(maxJump, std::abs(v - prev));
            prev = v;
        }
    }
    CHECK(maxOut <= 0.5f + 1.0e-5f);  // never wet + dry summed (would reach 1.0)
    CHECK(minOut < 0.01f);            // passed through silence at the switch point
    CHECK(maxJump < 0.01f);           // and got there smoothly
    CHECK(std::abs(prev - 0.5f) < 1.0e-5f);
    CHECK(!rack.isBypassTransitionActive());
    CHECK(rack.getProcessingLatencySamples() == 0);

    std::cout << "[PASS] Latency-plugin bypass switched through silence without summing paths." << std::endl;
}

void testPluginRackConcurrentMutation()
{
    std::cout << "[TEST] Running plugin rack concurrent add/remove while processing..." << std::endl;

    dsd::PluginRack rack;
    rack.prepare(48000.0, 128);
    rack.addPlugin(std::make_unique<MockAudioPluginInstance>("Gain2", 2.0f), "Gain2");

    std::atomic<bool> stop{false};
    std::atomic<int> wrongBlocks{0};
    std::atomic<int> processed{0};

    std::thread audio([&]() {
        juce::AudioBuffer<float> buf(2, 128);
        juce::MidiBuffer midi;
        while (!stop.load(std::memory_order_relaxed))
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill(buf.getWritePointer(ch), 0.25f, 128);
            rack.processBlock(buf, midi, 128);
            // Unity plugins come and go; the x2 plugin is always there.
            if (std::abs(buf.getSample(0, 0) - 0.5f) > 1.0e-5f || std::abs(buf.getSample(1, 127) - 0.5f) > 1.0e-5f)
                wrongBlocks.fetch_add(1);
            processed.fetch_add(1);
        }
    });

    for (int i = 0; i < 400; ++i)
    {
        const auto id = rack.addPlugin(std::make_unique<MockAudioPluginInstance>("Unity", 1.0f), "Unity");
        if (i % 3 == 0)
            rack.movePlugin(0, rack.getNumPlugins() - 1);
        rack.removePluginById(id);
    }
    stop.store(true);
    audio.join();

    CHECK(wrongBlocks.load() == 0);
    CHECK(processed.load() > 0);
    CHECK(rack.getNumPlugins() == 1);

    std::cout << "[PASS] " << processed.load() << " blocks processed during 400 add/move/remove cycles, "
              << wrongBlocks.load() << " wrong." << std::endl;
}

// Emits a single unit impulse at sample 0 of the Nth block it is asked for.
class ImpulseInputSource : public dsd::AudioInputSource
{
public:
    explicit ImpulseInputSource(int blockIndex) : fireAt(blockIndex) {}
    void prepare(double, int) override {}
    void releaseResources() override {}
    void readBlock(juce::AudioBuffer<float>& target, const juce::AudioBuffer<float>&, int numSamples) override
    {
        for (int ch = 0; ch < target.getNumChannels(); ++ch)
            target.clear(ch, 0, numSamples);
        if (calls++ == fireAt)
            for (int ch = 0; ch < target.getNumChannels(); ++ch)
                target.setSample(ch, 0, 1.0f);
    }
private:
    int fireAt;
    int calls{0};
};

// Runs CH1 (64-sample latency plugin) and CH2 (no plugin) into OUT 1 and
// returns the sample offsets of every peak above `threshold`.
static std::vector<int> runPdcScenario(bool pdcOn, float& tallestPeak)
{
    constexpr int block = 256;
    dsd::AudioEngine engine;
    engine.prepare(48000.0, block, 2);
    engine.setPluginDelayCompensationEnabled(pdcOn);

    auto& chMgr = engine.getChannelManager();
    chMgr.getChannel(0)->setInputSource(std::make_unique<ImpulseInputSource>(20));
    chMgr.getChannel(1)->setInputSource(std::make_unique<ImpulseInputSource>(20));
    chMgr.getChannel(0)->getPluginRack().addPlugin(std::make_unique<MockDelayPlugin>(64), "Delay64");

    juce::AudioBuffer<float> inBuf(2, block), outBuf(2, block);
    inBuf.clear();
    juce::AudioIODeviceCallbackContext ctx;

    std::vector<int> peaks;
    tallestPeak = 0.0f;
    for (int b = 0; b < 24; ++b)
    {
        engine.audioDeviceIOCallbackWithContext(inBuf.getArrayOfReadPointers(), 2,
                                                outBuf.getArrayOfWritePointers(), 2, block, ctx);
        const auto& bus = engine.getOutputManager().getOutput(0)->getBuffer();
        for (int i = 0; i < block; ++i)
        {
            const float v = std::abs(bus.getSample(0, i));
            if (v > 0.2f)
                peaks.push_back(b * block + i);
            tallestPeak = std::max(tallestPeak, v);
        }
    }
    return peaks;
}

void testPluginDelayCompensation()
{
    std::cout << "[TEST] Running Plugin Delay Compensation tests..." << std::endl;

    float tallOff = 0.0f, tallOn = 0.0f;
    const auto off = runPdcScenario(false, tallOff);
    const auto on = runPdcScenario(true, tallOn);

    // Without PDC the delayed channel arrives 64 samples after the dry one.
    CHECK(off.size() == 2);
    if (off.size() == 2)
        CHECK(off[1] - off[0] == 64);

    // With PDC both impulses land on the same sample and sum.
    CHECK(on.size() == 1);
    CHECK(tallOn > tallOff * 1.8f);
    if (on.size() == 1 && off.size() == 2)
        CHECK(on[0] == off[1]); // the dry channel was delayed to meet the slow one

    // Compensation reported and cleared.
    dsd::AudioEngine engine;
    engine.prepare(48000.0, 256, 2);
    engine.getChannelManager().getChannel(0)->getPluginRack().addPlugin(std::make_unique<MockDelayPlugin>(100), "Delay100");
    juce::AudioBuffer<float> inBuf(2, 256), outBuf(2, 256);
    inBuf.clear();
    juce::AudioIODeviceCallbackContext ctx;
    engine.setPluginDelayCompensationEnabled(true);
    for (int b = 0; b < 3; ++b)
        engine.audioDeviceIOCallbackWithContext(inBuf.getArrayOfReadPointers(), 2, outBuf.getArrayOfWritePointers(), 2, 256, ctx);
    CHECK(engine.getCompensatedLatencySamples() == 100);
    CHECK(engine.getChannelManager().getChannel(1)->getCompensationDelaySamples() == 100);
    CHECK(engine.getChannelManager().getChannel(0)->getCompensationDelaySamples() == 0);
    engine.setPluginDelayCompensationEnabled(false);
    engine.audioDeviceIOCallbackWithContext(inBuf.getArrayOfReadPointers(), 2, outBuf.getArrayOfWritePointers(), 2, 256, ctx);
    CHECK(engine.getCompensatedLatencySamples() == 0);
    CHECK(engine.getChannelManager().getChannel(1)->getCompensationDelaySamples() == 0);

    std::cout << "[PASS] PDC aligned impulses (off: " << off.size() << " peaks, on: " << on.size() << " peak)." << std::endl;
}

// Simulates two free-running clocks and checks the adaptive FIFO keeps the
// fill at its target, tracks the drift and never produces a discontinuity.
static void runFifoDriftSimulation(double producerRate, double consumerRate, double driftPpm,
                                   int pushFrames, int pullFrames, double seconds,
                                   double& meanCorrectionPpm, double& correctionSpreadPpm, double& maxFillError,
                                   uint64_t& underrunsAfterStart, float& maxStep)
{
    dsd::AdaptiveResamplingFifo fifo;
    fifo.prepare(producerRate, consumerRate, 2, 32768);

    const double actualProducerRate = producerRate * (1.0 + driftPpm * 1.0e-6);
    const double toneHz = 997.0;
    double phase = 0.0;
    const double phaseInc = 2.0 * 3.14159265358979323846 * toneHz / actualProducerRate;

    std::vector<float> inL(static_cast<size_t>(pushFrames)), inR(static_cast<size_t>(pushFrames));
    std::vector<float> outL(static_cast<size_t>(pullFrames)), outR(static_cast<size_t>(pullFrames));
    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    double tProducer = 0.0, tConsumer = 0.0, tProducerNominal = 0.0;
    const double pushPeriod = pushFrames / actualProducerRate;
    const double pullPeriod = pullFrames / consumerRate;

    uint64_t underrunsAtStart = 0;
    bool started = false;
    double corrSum = 0.0, corrMin = 1.0e9, corrMax = -1.0e9;
    long corrCount = 0;
    maxFillError = 0.0;
    float prev = 0.0f;
    maxStep = 0.0f;
    uint32_t jitterSeed = 12345;

    while (tConsumer < seconds)
    {
        if (tProducer <= tConsumer)
        {
            for (int i = 0; i < pushFrames; ++i)
            {
                inL[static_cast<size_t>(i)] = inR[static_cast<size_t>(i)] = static_cast<float>(0.5 * std::sin(phase));
                phase += phaseInc;
            }
            fifo.push(inPtrs, 2, pushFrames);
            // Bounded +-1 ms delivery jitter around the producer's true clock,
            // like WASAPI event timing (jitter does not accumulate).
            jitterSeed = jitterSeed * 1664525u + 1013904223u;
            const double jitter = ((jitterSeed >> 8) / 16777216.0 - 0.5) * 0.002;
            tProducerNominal += pushPeriod;
            tProducer = tProducerNominal + jitter;
        }
        else
        {
            fifo.pull(outPtrs, 2, pullFrames);
            tConsumer += pullPeriod;
            if (!started && tConsumer > 2.0)
            {
                started = true;
                underrunsAtStart = fifo.getUnderrunCount();
                prev = outL[0];
            }
            if (tConsumer > seconds * 0.5)
            {
                const double c = fifo.getCorrectionPpm();
                corrSum += c;
                ++corrCount;
                corrMin = std::min(corrMin, c);
                corrMax = std::max(corrMax, c);
                maxFillError = std::max(maxFillError, std::abs(fifo.getAverageFillFrames() - fifo.getTargetFillFrames()));
            }
            if (started)
            {
                for (int i = 0; i < pullFrames; ++i)
                {
                    maxStep = std::max(maxStep, std::abs(outL[static_cast<size_t>(i)] - prev));
                    prev = outL[static_cast<size_t>(i)];
                }
            }
        }
    }

    meanCorrectionPpm = corrCount > 0 ? corrSum / corrCount : 0.0;
    correctionSpreadPpm = corrMax - corrMin;
    underrunsAfterStart = fifo.getUnderrunCount() - underrunsAtStart;
}

void testAdaptiveResamplingFifo()
{
    std::cout << "[TEST] Running Adaptive drift-correcting FIFO tests..." << std::endl;

    // Max sample-to-sample change of a 0.5-amplitude 997 Hz tone at 48 kHz is ~0.065.
    const float continuityLimit = 0.075f;

    struct Case { double pRate, cRate, ppm; int push, pull; const char* name; };
    const Case cases[] = {
        { 48000.0, 48000.0,  300.0, 480, 128, "48k->48k, producer +300 ppm" },
        { 48000.0, 48000.0, -250.0, 480, 128, "48k->48k, producer -250 ppm" },
        { 44100.0, 48000.0,   80.0, 441, 256, "44.1k->48k, +80 ppm" },
        { 48000.0, 44100.0, -120.0, 128, 441, "48k->44.1k, -120 ppm" },
    };

    for (const auto& c : cases)
    {
        double corr = 0.0, spread = 0.0, fillErr = 0.0;
        uint64_t underruns = 0;
        float maxStep = 0.0f;
        runFifoDriftSimulation(c.pRate, c.cRate, c.ppm, c.push, c.pull, 120.0, corr, spread, fillErr, underruns, maxStep);

        std::cout << "  " << c.name << ": mean correction " << corr << " ppm (spread " << spread
                  << "), max fill error " << fillErr << " frames, underruns " << underruns
                  << ", max step " << maxStep << std::endl;

        CHECK(std::abs(corr - c.ppm) < 15.0);  // tracked the clock difference
        CHECK(spread < 120.0);                 // ratio wobble < 0.2 cents
        CHECK(fillErr < 48.0);                 // latency held at target (within 1 ms)
        CHECK(underruns == 0);
        CHECK(maxStep < continuityLimit);      // no clicks / sample drops
    }

    // Producer stops: consumer fades to silence, then re-primes when data returns.
    dsd::AdaptiveResamplingFifo fifo;
    fifo.prepare(48000.0, 48000.0);
    std::vector<float> in(480, 0.25f), out(128, 1.0f);
    const float* ip[1] = { in.data() };
    float* op[1] = { out.data() };
    for (int i = 0; i < 4; ++i) fifo.push(ip, 1, 480);
    for (int i = 0; i < 40; ++i) fifo.pull(op, 1, 128);
    CHECK(fifo.getUnderrunCount() >= 1);
    CHECK(out[127] == 0.0f);
    for (int i = 0; i < 6; ++i) fifo.push(ip, 1, 480);
    for (int i = 0; i < 4; ++i) fifo.pull(op, 1, 128);
    CHECK(std::abs(out[127] - 0.25f) < 1.0e-4f);

    std::cout << "[PASS] Adaptive FIFO held latency and tracked drift without discontinuities." << std::endl;
}

void testPreFaderSends()
{
    std::cout << "[TEST] Running PRE/POST fader send tests..." << std::endl;

    constexpr int n = 128;
    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(dsd::NUM_CHANNELS_LEVEL1);
    channelMgr.prepare(48000.0, n);
    auto* ch = channelMgr.getChannel(0);
    ch->setInputSource(std::make_unique<ConstantInputSource>(0.5f));

    dsd::OutputManager outputMgr;
    outputMgr.initializeDefaultOutputs(dsd::NUM_OUTPUT_BUSES_LEVEL1);
    outputMgr.prepare(48000.0, n);
    // OUT 2/3 default to monitor role for PFL; make OUT 3 a plain program bus.
    outputMgr.getOutput(2)->setMonitor(false);

    dsd::RoutingEngine router;
    router.setRouteEnabled(0, 2, true);   // CH1 -> OUT3
    router.setRoutePreFader(0, 2, true);  // PRE
    CHECK(router.isRoutePreFader(0, 2));
    CHECK(!router.isRoutePreFader(0, 0)); // OUT1 stays POST

    juce::AudioBuffer<float> devIn(2, n);
    devIn.clear();
    auto run = [&](int blocks) {
        for (int b = 0; b < blocks; ++b)
        {
            channelMgr.processChannels(devIn, n);
            router.routeChannelsToOutputs(channelMgr, outputMgr, n);
        }
    };

    ch->setFaderDb(-60.0f); // fader fully down (15 ms exponential fader smoothing)
    run(100);
    const float post = outputMgr.getOutput(0)->getBuffer().getMagnitude(0, 0, n);
    const float pre = outputMgr.getOutput(2)->getBuffer().getMagnitude(0, 0, n);
    CHECK(post < 1.0e-4f);                // POST follows the fader
    CHECK(std::abs(pre - 0.5f) < 1.0e-3f); // PRE ignores fader and pan (pre-pan, unity)

    ch->setMute(true);
    run(2);
    CHECK(outputMgr.getOutput(2)->getBuffer().getMagnitude(0, 0, n) < 1.0e-6f); // MUTE kills PRE too

    ch->setMute(false);
    run(1);
    float maxStep = 0.0f;
    const auto& b = outputMgr.getOutput(2)->getBuffer();
    for (int i = 1; i < n; ++i)
        maxStep = std::max(maxStep, std::abs(b.getSample(0, i) - b.getSample(0, i - 1)));
    CHECK(maxStep < 0.01f); // unmute ramps in

    std::cout << "[PASS] PRE send level " << pre << " with fader down; POST " << post << "." << std::endl;
}

void testDeviceEnumeration()
{
    std::cout << "[TEST] Enumerating Windows Audio Devices via JUCE..." << std::endl;
    dsd::AudioDeviceManager devMgr;
    devMgr.initialize(2, 2);

    auto& juceMgr = devMgr.getJuceManager();

    juce::AudioIODeviceType* wasapiType = nullptr;
    for (auto* type : juceMgr.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == "Windows Audio")
        {
            wasapiType = type;
            break;
        }
    }

    if (wasapiType != nullptr)
    {
        wasapiType->scanForDevices();
        auto ins = wasapiType->getDeviceNames(true);
        auto outs = wasapiType->getDeviceNames(false);

        std::cout << "  Found WASAPI Type with " << ins.size() << " inputs and " << outs.size() << " outputs." << std::endl;

        if (!ins.isEmpty())
        {
            std::cout << "  Testing creation and open of input device: " << ins[0].toStdString() << std::endl;
            std::unique_ptr<juce::AudioIODevice> dev(wasapiType->createDevice(juce::String(), ins[0]));
            if (dev != nullptr)
            {
                std::cout << "  [SUCCESS] Created input device instance: " << dev->getName().toStdString() << std::endl;
                juce::BigInteger inChans;
                inChans.setRange(0, 2, true);
                juce::BigInteger outChans; // no output
                auto err = dev->open(inChans, outChans, 48000.0, 128);
                if (err.isEmpty())
                {
                    std::cout << "  [SUCCESS] Device opened cleanly at 48000 Hz, 128 buffer." << std::endl;
                    dev->close();
                }
                else
                {
                    std::cout << "  [INFO] Device open note: " << err.toStdString() << std::endl;
                }
            }
        }

        if (!outs.isEmpty())
        {
            std::cout << "  Testing creation and open of output device: " << outs[0].toStdString() << std::endl;
            std::unique_ptr<juce::AudioIODevice> outDev(wasapiType->createDevice(outs[0], juce::String()));
            if (outDev != nullptr)
            {
                juce::BigInteger inChans;
                juce::BigInteger outChans;
                outChans.setRange(0, 2, true);
                auto err = outDev->open(inChans, outChans, 48000.0, 128);
                if (err.isEmpty())
                {
                    std::cout << "  [OUTPUT DEVICE AUDIT] Name: " << outDev->getName().toStdString() << std::endl;
                    std::cout << "  [OUTPUT DEVICE AUDIT] Buffer size samples: " << outDev->getCurrentBufferSizeSamples()
                              << " (" << (outDev->getCurrentBufferSizeSamples() / 48.0) << " ms)" << std::endl;
                    std::cout << "  [OUTPUT DEVICE AUDIT] Output Latency samples: " << outDev->getOutputLatencyInSamples()
                              << " (" << (outDev->getOutputLatencyInSamples() / 48.0) << " ms)" << std::endl;
                    outDev->close();
                }
                else
                {
                    std::cout << "  [OUTPUT DEVICE AUDIT] Note: " << err.toStdString() << std::endl;
                }
            }
        }
    }
}

void testLowLatencyCaptureFifo()
{
    std::cout << "[TEST] Running LowLatencyCaptureFifo Bounded Buffer & Drift Steering tests..." << std::endl;

    dsd::LowLatencyCaptureFifo fifo(2, 4096);
    fifo.setTargetOccupancy(192); // Low-Latency ~4.0 ms trough cushion @ 48 kHz
    CHECK(fifo.getTargetOccupancy() == 192);
    CHECK(fifo.getNumReady() == 0);

    // 1. Test startup burst flush: write 2000 samples at once
    juce::AudioBuffer<float> burstBuffer(2, 2000);
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 2000; ++i)
            burstBuffer.setSample(ch, i, 0.5f);
    }
    fifo.write(burstBuffer.getArrayOfReadPointers(), 2, 2000);

    // Initial burst must be flushed down to optimal peak cushion (target 192 + normal packet 480 = 672 samples)
    int readyAfterBurst = fifo.getNumReady();
    std::cout << "  Startup burst: 2000 frames -> Bounded Occupancy: " << readyAfterBurst
              << " frames (Flushed " << fifo.getBurstFlushedFrames() << " excess frames)" << std::endl;
    CHECK(readyAfterBurst == (192 + 480));
    CHECK(fifo.getBurstFlushedFrames() == (2000 - (192 + 480)));

    // 2. Test reading blocks of 256 samples
    juce::AudioBuffer<float> readBuf(2, 256);
    int read1 = fifo.read(readBuf.getArrayOfWritePointers(), 2, 256, 48000.0);
    CHECK(read1 == 256);
    CHECK(fifo.getNumReady() == (672 - 256));

    // 3. Test steady-state drift tracking with balanced I/O (480 in, 480 out)
    juce::AudioBuffer<float> packet(2, 480);
    packet.clear();
    juce::AudioBuffer<float> readBuf2(2, 224);
    for (int i = 0; i < 50; ++i)
    {
        fifo.write(packet.getArrayOfReadPointers(), 2, 480);
        fifo.read(readBuf.getArrayOfWritePointers(), 2, 256, 48000.0);
        fifo.read(readBuf2.getArrayOfWritePointers(), 2, 224, 48000.0);
    }

    std::cout << "  Steady-state Smoothed Occupancy: " << fifo.getSmoothedOccupancy()
              << " frames (Target Trough: " << fifo.getTargetOccupancy() << ", Peak: " << (fifo.getTargetOccupancy() + 480) << ")" << std::endl;
    std::cout << "  Clock Drift Rate: " << fifo.getClockDriftPpm() << " PPM, Drift Corrections: "
              << fifo.getDriftCorrections() << ", Underruns: " << fifo.getUnderrunCount() << std::endl;

    // 4. Test dynamic target occupancy reduction (switching to Ultra-Low Latency Mode: 96 frames)
    fifo.setTargetOccupancy(96); // Ultra-low ~2 ms trough target
    CHECK(fifo.getTargetOccupancy() == 96);
    // Write packet
    fifo.write(packet.getArrayOfReadPointers(), 2, 480);
    // Read next block; dynamic backlog flush keeps buffer strictly bounded
    fifo.read(readBuf.getArrayOfWritePointers(), 2, 128, 48000.0);
    CHECK(fifo.getNumReady() <= (96 + 480));
    std::cout << "  Switched to Ultra-Low Latency (Target 96 frames): Active Occupancy = "
              << fifo.getNumReady() << " frames" << std::endl;

    std::cout << "[PASS] LowLatencyCaptureFifo tests passed cleanly." << std::endl;
}

void testWindowAudioCapture()
{
    std::cout << "[TEST] Running WindowAudioCapture (OBS Process Loopback) tests..." << std::endl;

    auto apps = dsd::WindowAudioCapture::getRunningApplications();
    std::cout << "  Enumerated " << apps.size() << " running application windows:" << std::endl;
    for (size_t i = 0; i < std::min<size_t>(apps.size(), 5); ++i)
    {
        std::cout << "    [" << i << "] PID " << apps[i].processId << ": "
                  << apps[i].appName.toStdString() << " - \""
                  << apps[i].windowTitle.substring(0, 30).toStdString() << "\"" << std::endl;
    }

    dsd::AudioChannel channel(1, "App Channel");
    channel.prepare(48000.0, 256);

    if (!apps.empty())
    {
        auto capture = std::make_unique<dsd::WindowAudioCapture>(apps[0].processId, apps[0].appName);
        CHECK(capture->getTargetPid() == apps[0].processId);
        CHECK(capture->getProcessName() == apps[0].appName);

        auto* rawCap = capture.get();
        channel.setInputSource(std::move(capture));

        // Allow background capture thread time to activate and connect to process loopback
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        juce::AudioBuffer<float> dummyInput(2, 256);
        dummyInput.clear();

        // Simulate real-time audio playback consumption (256 samples every ~5.33 ms)
        for (int i = 0; i < 60; ++i)
        {
            channel.processBlock(dummyInput, 256);
            std::this_thread::sleep_for(std::chrono::microseconds(5333));
        }

        auto diag = rawCap->getDiagnostics();
        std::cout << "  Capture Diagnostics for " << diag.appName.toStdString() << " (PID " << diag.processId << "):" << std::endl;
        std::cout << "    Format: " << diag.captureSampleRate << " Hz, " << (diag.isFloat ? "32-bit Float" : "16-bit PCM")
                  << " " << (diag.captureChannels >= 2 ? "Stereo" : "Mono") << std::endl;
        std::cout << "    WASAPI Buffer: " << diag.wasapiBufferFrames << " frames (" << diag.wasapiBufferMs << " ms), Period: "
                  << diag.wasapiPeriodMs << " ms" << std::endl;
        std::cout << "    Ring Buffer: " << diag.ringBufferOccupancy << " frames (" << diag.ringBufferOccupancyMs
                  << " ms), Target: " << diag.targetOccupancy << " frames (" << diag.targetOccupancyMs << " ms)" << std::endl;
        std::cout << "    Clock Drift: " << diag.clockDriftPpm << " PPM, Discontinuities: " << diag.discontinuityCount
                  << ", Burst Flushed: " << diag.burstFlushedFrames << " frames" << std::endl;
        std::cout << "    Hardware Packet Age (QPC): " << diag.hardwarePacketAgeMs << " ms" << std::endl;
        std::cout << "    Estimated Capture Latency: " << diag.estimatedCaptureLatencyMs << " ms" << std::endl;
    }

    std::cout << "[PASS] WindowAudioCapture tests passed cleanly." << std::endl;
}

void testSessionSaveLoad()
{
    std::cout << "[TEST] Running SessionManager Save/Load tests..." << std::endl;

    dsd::AudioEngine engine;
    dsd::AudioDeviceManager devMgr;

    auto& chMgr = engine.getChannelManager();
    auto* ch0 = chMgr.getChannel(0);
    auto* ch1 = chMgr.getChannel(1);
    CHECK(ch0 != nullptr && ch1 != nullptr);

    ch0->setName("Lead Vocal");
    ch0->setGainDb(3.5f);
    ch0->setFaderDb(-4.0f);
    ch0->setPan(-0.25f);
    ch0->setInputDeviceName("Primary In 1 (Mic / L)");
    ch0->setInputChannelIndex(0);
    ch0->setDisableOutput(true);
    ch0->setPhaseInvert(true);
    ch0->setForceMono(true);
    ch0->setDirectMonitor(true);

    auto plug = std::make_unique<MockAudioPluginInstance>("MockEQ", 1.0f, 0.0f);
    auto pId = ch0->getPluginRack().addPlugin(std::move(plug), "MockEQ");
    ch0->getPluginRack().setBypassById(pId, true);

    ch1->setName("Spotify Music");
    ch1->setGainDb(0.0f);
    ch1->setFaderDb(-12.0f);
    ch1->setPan(0.5f);
    ch1->setMute(true);
    ch1->setInputDeviceName("Spotify");
    ch1->setInputChannelIndex(1);

    auto& router = engine.getRoutingEngine();
    router.setRouteEnabled(0, 0, true);
    router.setRouteGainDb(0, 0, -2.5f);
    router.setRouteEnabled(1, 1, true);

    auto& outMgr = engine.getOutputManager();
    auto* out0 = outMgr.getOutput(0);
    CHECK(out0 != nullptr);
    out0->setName("Main Monitor");
    out0->setFaderDb(-3.0f);
    out0->setOutputDeviceName("Speakers");

    juce::File sessionFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("test_dsd_session.dsd");
    if (sessionFile.existsAsFile())
        sessionFile.deleteFile();

    bool saved = dsd::SessionManager::saveSessionToFile(sessionFile, engine, devMgr);
    CHECK(saved);
    CHECK(sessionFile.existsAsFile());

    // Verify JSON contents have new channel attributes and plugin serialization
    juce::String jsonStr = sessionFile.loadFileAsString();
    CHECK(jsonStr.contains("\"disableOutput\": true"));
    CHECK(jsonStr.contains("\"phaseInvert\": true"));
    CHECK(jsonStr.contains("\"forceMono\": true"));
    CHECK(jsonStr.contains("\"directMonitor\": true"));
    CHECK(jsonStr.contains("\"MockEQ\""));
    CHECK(jsonStr.contains("\"bypassed\": true"));
    CHECK(jsonStr.contains("\"instanceId\":"));

    // Scramble values to verify restoration
    ch0->setName("Changed");
    ch0->setGainDb(0.0f);
    ch0->setFaderDb(0.0f);
    ch0->setDisableOutput(false);
    ch0->setPhaseInvert(false);
    ch0->setForceMono(false);
    ch0->setDirectMonitor(false);
    ch0->setInputDeviceName("None");
    ch0->getPluginRack().clear();

    ch1->setName("Changed");
    ch1->setMute(false);
    ch1->setInputDeviceName("None");

    router.setRouteEnabled(0, 0, false);
    out0->setName("Changed");

    // Load session
    bool loaded = dsd::SessionManager::loadSessionFromFile(sessionFile, engine, devMgr);
    CHECK(loaded);

    // Verify channel 0 restoration
    CHECK(ch0->getName() == "Lead Vocal");
    CHECK(std::abs(ch0->getGainDb() - 3.5f) < 0.01f);
    CHECK(std::abs(ch0->getFaderDb() - (-4.0f)) < 0.01f);
    CHECK(std::abs(ch0->getPan() - (-0.25f)) < 0.01f);
    CHECK(ch0->getDisableOutput() == true);
    CHECK(ch0->getPhaseInvert() == true);
    CHECK(ch0->getForceMono() == true);
    CHECK(ch0->getDirectMonitor() == true);
    CHECK(ch0->getInputDeviceName() == "Primary In 1 (Mic / L)");
    CHECK(ch0->getInputChannelIndex() == 0);
    CHECK(ch0->getInputSource() != nullptr);

    // Verify channel 1 restoration
    CHECK(ch1->getName() == "Spotify Music");
    CHECK(std::abs(ch1->getFaderDb() - (-12.0f)) < 0.01f);
    CHECK(ch1->getMute() == true);
    CHECK(ch1->getInputDeviceName() == "Spotify");

    // Verify routing and output restoration
    CHECK(router.isRouteEnabled(0, 0) == true);
    CHECK(std::abs(router.getRouteGainDb(0, 0) - (-2.5f)) < 0.01f);
    CHECK(router.isRouteEnabled(1, 1) == true);
    CHECK(out0->getName() == "Main Monitor");
    CHECK(std::abs(out0->getFaderDb() - (-3.0f)) < 0.01f);
    CHECK(out0->getOutputDeviceName() == "Speakers");

    sessionFile.deleteFile();

    std::cout << "[PASS] SessionManager Save/Load tests passed cleanly." << std::endl;
}

void testPerformanceTelemetryAndOverrun()
{
    std::cout << "[TEST] Running Performance Telemetry, True Peak & Overrun tests..." << std::endl;

    dsd::AudioEngine engine;
    engine.prepare(48000.0, 480, 2);
    auto& stats = engine.getPerformanceStats();

    // Initial state
    CHECK(stats.deadlineMissCount.load() == 0);
    CHECK(stats.xrunCount.load() == 0);
    CHECK(!stats.isGlitching.load());

    // Prepare dummy audio I/O buffers
    juce::AudioBuffer<float> inBuf(2, 480);
    juce::AudioBuffer<float> outBuf(2, 480);
    inBuf.clear();
    outBuf.clear();

    juce::AudioIODeviceCallbackContext dummyCtx;

    // Run 50 callbacks
    for (int i = 0; i < 50; ++i)
    {
        engine.audioDeviceIOCallbackWithContext(inBuf.getArrayOfReadPointers(), 2,
                                              outBuf.getArrayOfWritePointers(), 2,
                                              480, dummyCtx);
    }

    CHECK(stats.deadlineMs.load() == 10.0); // 480 / 48000 * 1000 = 10.0 ms
    CHECK(stats.currentProcessingTimeMs.load() > 0.0);
    CHECK(stats.currentLoadPercent.load() > 0.0f);
    CHECK(stats.deadlineMissCount.load() == 0);
    CHECK(stats.currentHeadroomMs.load() > 0.0);
    CHECK(stats.minHeadroomMs.load() <= 10.0);
    CHECK(stats.peakProcessingTimeMs.load() >= stats.currentProcessingTimeMs.load());
    CHECK(stats.peakLoadPercent.load() >= stats.currentLoadPercent.load());

    std::cout << "  DSP Time: " << stats.currentProcessingTimeMs.load() << " ms (Peak: "
              << stats.peakProcessingTimeMs.load() << " ms), Load: "
              << stats.currentLoadPercent.load() << "% (Peak: "
              << stats.peakLoadPercent.load() << "%), Headroom: "
              << stats.currentHeadroomMs.load() << " ms (Min: "
              << stats.minHeadroomMs.load() << " ms)" << std::endl;

    // Test Reset Metrics
    engine.resetPerformanceMetrics();
    CHECK(stats.deadlineMissCount.load() == 0);
    CHECK(stats.xrunCount.load() == 0);
    CHECK(!stats.isGlitching.load());
    CHECK(stats.peakProcessingTimeMs.load() == 0.0);
    CHECK(stats.peakLoadPercent.load() == 0.0f);
    CHECK(stats.minHeadroomMs.load() == 10.0);

    // Test ProcessCpuTracker
    dsd::ProcessCpuTracker cpuTracker;
    float cpu = cpuTracker.getCpuUsagePercent();
    CHECK(cpu >= 0.0f && cpu <= 100.0f);
    std::cout << "  [PASS] ProcessCpuTracker returned: " << cpu << "%" << std::endl;

    std::cout << "[PASS] Performance Telemetry, True Peak & Overrun tests passed cleanly." << std::endl;
}

// Bypass toggles crossfade over ~10 ms; run enough blocks for them to finish.
static void settleBypassTransitions(dsd::PluginRack& rack)
{
    juce::AudioBuffer<float> b(2, 128);
    juce::MidiBuffer m;
    for (int i = 0; i < 8; ++i)
    {
        b.clear();
        rack.processBlock(b, m, 128);
    }
    CHECK(!rack.isBypassTransitionActive());
}

void testVstBypassAndReorder()
{
    std::cout << "[TEST] Running VST Bypass, Reorder & Processing Order tests..." << std::endl;

    dsd::PluginRack rack;
    rack.prepare(48000.0, 128);

    // Create 2 mock plugins:
    // Plugin A: multiply by 2.0, add 1.0 ( y = 2x + 1 )
    // Plugin B: multiply by 3.0, add 0.0 ( y = 3x )
    auto plugA = std::make_unique<MockAudioPluginInstance>("PluginA", 2.0f, 1.0f);
    auto plugB = std::make_unique<MockAudioPluginInstance>("PluginB", 3.0f, 0.0f);

    auto idA = rack.addPlugin(std::move(plugA), "Plugin A");
    auto idB = rack.addPlugin(std::move(plugB), "Plugin B");

    CHECK(idA != 0);
    CHECK(idB != 0);
    CHECK(idA != idB);
    CHECK(rack.getNumPlugins() == 2);

    // Initial order: A (index 0), B (index 1)
    CHECK(rack.getSlot(0)->instanceId == idA);
    CHECK(rack.getSlot(1)->instanceId == idB);

    // Test buffer: x = 1.0f
    juce::AudioBuffer<float> buf(2, 128);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buf.setSample(ch, i, 1.0f);

    juce::MidiBuffer midi;
    rack.processBlock(buf, midi, 128);

    // Expected: First A: 1.0 * 2 + 1 = 3.0. Then B: 3.0 * 3 = 9.0.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            CHECK(std::abs(buf.getSample(ch, i) - 9.0f) < 0.001f);

    // 1. Test Bypass Plugin A
    rack.setBypassById(idA, true);
    CHECK(rack.isBypassedById(idA) == true);
    CHECK(rack.isBypassedById(idB) == false);
    settleBypassTransitions(rack);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buf.setSample(ch, i, 1.0f);

    rack.processBlock(buf, midi, 128);
    // Expected: A bypassed -> x stays 1.0. B active -> 1.0 * 3 = 3.0.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            CHECK(std::abs(buf.getSample(ch, i) - 3.0f) < 0.001f);

    // 2. Test Bypass Both
    rack.setBypassById(idB, true);
    CHECK(rack.isBypassedById(idA) == true);
    CHECK(rack.isBypassedById(idB) == true);
    settleBypassTransitions(rack);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buf.setSample(ch, i, 1.0f);

    rack.processBlock(buf, midi, 128);
    // Both bypassed -> x stays 1.0.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            CHECK(std::abs(buf.getSample(ch, i) - 1.0f) < 0.001f);

    // 3. Unbypass A, keep B bypassed
    rack.setBypassById(idA, false);
    CHECK(rack.isBypassedById(idA) == false);
    CHECK(rack.isBypassedById(idB) == true);
    settleBypassTransitions(rack);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buf.setSample(ch, i, 1.0f);

    rack.processBlock(buf, midi, 128);
    // A active (1.0 * 2 + 1 = 3.0). B bypassed -> 3.0.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            CHECK(std::abs(buf.getSample(ch, i) - 3.0f) < 0.001f);

    // 4. Unbypass B: both active again
    rack.setBypassById(idB, false);
    settleBypassTransitions(rack);

    // 5. Test Reordering: Move B (currently at index 1) to target index 0
    bool moved = rack.movePluginById(idB, 0);
    CHECK(moved);
    CHECK(rack.getSlot(0)->instanceId == idB);
    CHECK(rack.getSlot(1)->instanceId == idA);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buf.setSample(ch, i, 1.0f);

    rack.processBlock(buf, midi, 128);
    // Now order is B, then A!
    // First B: 1.0 * 3 = 3.0.
    // Then A: 3.0 * 2 + 1 = 7.0!
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            CHECK(std::abs(buf.getSample(ch, i) - 7.0f) < 0.001f);

    // 6. Test Removal by ID
    bool removed = rack.removePluginById(idB);
    CHECK(removed);
    CHECK(rack.getNumPlugins() == 1);
    CHECK(rack.getSlot(0)->instanceId == idA);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buf.setSample(ch, i, 1.0f);

    rack.processBlock(buf, midi, 128);
    // Only A: 1.0 * 2 + 1 = 3.0.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            CHECK(std::abs(buf.getSample(ch, i) - 3.0f) < 0.001f);

    std::cout << "[PASS] VST Bypass, Reorder & Processing Order tests passed." << std::endl;
}

void testPreGainMetering()
{
    std::cout << "[TEST] Running Pre-Gain Metering (Stage A) tests..." << std::endl;

    dsd::AudioChannel ch(0, "CH 01");
    ch.prepare(48000.0, 128);

    // Set input source to Sine generator with fixed peak 0.12589f (~ -18 dBFS)
    const float inputAmplitude = 0.12589f;
    auto sine = std::make_unique<dsd::SineGeneratorSource>(1000.0f, inputAmplitude);
    sine->setEnabled(true);
    ch.setInputSource(std::move(sine));

    // Apply +12 dB gain (+12 dB = ~3.98x)
    ch.setGainDb(12.0f);

    juce::AudioBuffer<float> devIn(2, 128);
    devIn.clear();

    // Process a block
    ch.processBlock(devIn, 128);

    // Meter Pre-Gain (Stage A) must read the raw input (~0.12589f) unaffected by +12 dB gain
    const auto& preGain = ch.getMeterPreGain();
    CHECK(std::abs(preGain.peakL.load() - inputAmplitude) < 0.02f);

    // Meter Post-Gain must reflect the +12 dB gain (~0.12589 * 3.98 ~ 0.50f)
    const auto& postGain = ch.getMeterPostGain();
    CHECK(postGain.peakL.load() > 0.40f);
    CHECK(postGain.peakL.load() > preGain.peakL.load() * 2.5f);

    // Now test extreme cut: Gain = -60 dB
    ch.setGainDb(-60.0f);
    ch.processBlock(devIn, 128);

    // Stage A Pre-Gain meter must STILL read ~0.12589f
    const auto& preGain2 = ch.getMeterPreGain();
    CHECK(std::abs(preGain2.peakL.load() - inputAmplitude) < 0.02f);

    // Post-gain meter must be 0
    const auto& postGain2 = ch.getMeterPostGain();
    CHECK(postGain2.peakL.load() < 0.001f);

    std::cout << "[PASS] Pre-Gain Metering (Stage A) tests passed." << std::endl;
}

void testGainAndFaderControls()
{
    std::cout << "[TEST] Running Gain and Fader controls tests..." << std::endl;

    dsd::AudioChannel ch(0, "CH 01");
    ch.prepare(48000.0, 128);

    auto sine = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.5f);
    sine->setEnabled(true);
    ch.setInputSource(std::move(sine));

    juce::AudioBuffer<float> devIn(2, 128);
    devIn.clear();

    // 1. Gain at 0 dB, Fader at 0 dB
    ch.setGainDb(0.0f);
    ch.setFaderDb(0.0f);
    ch.processBlock(devIn, 128);

    float postGain1 = ch.getMeterPostGain().peakL.load();
    float postFader1 = ch.getMeterPostFader().peakL.load();
    CHECK(std::abs(postGain1 - 0.5f) < 0.02f);
    CHECK(std::abs(postFader1 - 0.5f) < 0.02f);

    // 2. Gain at +6 dB (~2x -> 1.0f), Fader at -6 dB (~0.5x -> 0.5f)
    ch.setGainDb(6.0f);
    ch.setFaderDb(-6.0f);
    ch.processBlock(devIn, 128);

    float postGain2 = ch.getMeterPostGain().peakL.load();
    float postFader2 = ch.getMeterPostFader().peakL.load();
    CHECK(std::abs(postGain2 - 1.0f) < 0.05f);
    CHECK(std::abs(postFader2 - 0.5f) < 0.05f);

    // 3. Gain at -60 dB (silence)
    ch.setGainDb(-60.0f);
    ch.processBlock(devIn, 128);
    CHECK(ch.getMeterPostGain().peakL.load() < 0.001f);
    CHECK(ch.getMeterPostFader().peakL.load() < 0.001f);

    std::cout << "[PASS] Gain and Fader controls tests passed." << std::endl;
}

void testOnOffVsMuteSemantics()
{
    std::cout << "[TEST] Running ON/OFF vs MUTE decoupling semantics tests..." << std::endl;

    dsd::AudioChannel ch(0, "CH 01");
    ch.prepare(48000.0, 128);

    auto sine = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.5f);
    sine->setEnabled(true);
    ch.setInputSource(std::move(sine));

    juce::AudioBuffer<float> devIn(2, 128);
    devIn.clear();

    // Verify initial states
    CHECK(ch.getDisableOutput() == false);
    CHECK(ch.getMute() == false);

    // 1. Toggle ON/OFF (disableOutput) -> does NOT touch Mute
    ch.setDisableOutput(true);
    CHECK(ch.getDisableOutput() == true);
    CHECK(ch.getMute() == false);

    ch.processBlock(devIn, 128);
    // Channel processing itself keeps running (meters are active)
    CHECK(ch.getMeterPreGain().peakL.load() > 0.4f);
    CHECK(ch.getMeterPostFader().peakL.load() > 0.4f);

    // 2. Toggle MUTE -> does NOT touch disableOutput
    ch.setDisableOutput(false);
    ch.setMute(true);
    CHECK(ch.getDisableOutput() == false);
    CHECK(ch.getMute() == true);

    ch.processBlock(devIn, 128);
    // With Mute, PreGain is still active, but PostFader is 0.0f
    CHECK(ch.getMeterPreGain().peakL.load() > 0.4f);
    CHECK(ch.getMeterPostFader().peakL.load() < 0.001f);

    // Unmute
    ch.setMute(false);
    CHECK(ch.getMute() == false);
    CHECK(ch.getDisableOutput() == false);

    std::cout << "[PASS] ON/OFF vs MUTE decoupling semantics tests passed." << std::endl;
}

void testHpfDsp()
{
    std::cout << "[TEST] Running HPF (80 Hz Butterworth 2nd-order) DSP tests..." << std::endl;

    dsd::AudioChannel ch(0, "HPF Channel");
    ch.prepare(48000.0, 256);

    juce::AudioBuffer<float> devIn(2, 256);
    devIn.clear();

    // 1. Low-frequency test (30 Hz sine wave, well below 80 Hz cutoff)
    auto lfSine = std::make_unique<dsd::SineGeneratorSource>(30.0f, 0.8f);
    lfSine->setEnabled(true);
    ch.setInputSource(std::move(lfSine));

    // HPF OFF: low-frequency content passes unattenuated
    ch.setHpfEnabled(false);
    for (int i = 0; i < 10; ++i)
        ch.processBlock(devIn, 256);

    float outLfBypass = ch.getOutputBuffer().getMagnitude(0, 0, 256);
    CHECK(outLfBypass > 0.7f); // Full 30 Hz signal passes

    // HPF ON: 30 Hz should be significantly attenuated (>12 dB down for 80 Hz 2nd-order)
    ch.setHpfEnabled(true);
    for (int i = 0; i < 20; ++i) // allow IIR filter state to settle
        ch.processBlock(devIn, 256);

    float outLfFiltered = ch.getOutputBuffer().getMagnitude(0, 0, 256);
    CHECK(outLfFiltered < 0.25f); // 30 Hz attenuated significantly (< 25% of input)
    CHECK(outLfFiltered < outLfBypass * 0.35f);

    // 2. High-frequency test (1000 Hz sine wave, well above 80 Hz cutoff)
    auto hfSine = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.8f);
    hfSine->setEnabled(true);
    ch.setInputSource(std::move(hfSine));

    for (int i = 0; i < 10; ++i)
        ch.processBlock(devIn, 256);

    float outHfFiltered = ch.getOutputBuffer().getMagnitude(0, 0, 256);
    CHECK(outHfFiltered > 0.75f); // 1 kHz passes cleanly with virtually zero loss

    std::cout << "[PASS] HPF DSP tests passed cleanly." << std::endl;
}

void testPadDsp()
{
    std::cout << "[TEST] Running PAD (-20 dB input attenuation) DSP tests..." << std::endl;

    dsd::AudioChannel ch(0, "PAD Channel");
    ch.prepare(48000.0, 128);

    auto sine = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.5f);
    sine->setEnabled(true);
    ch.setInputSource(std::move(sine));

    juce::AudioBuffer<float> devIn(2, 128);
    devIn.clear();

    ch.setGainDb(0.0f);
    ch.setFaderDb(0.0f);

    // 1. PAD OFF: normal signal level
    ch.setPadEnabled(false);
    ch.processBlock(devIn, 128);

    float preGainOff = ch.getMeterPreGain().peakL.load();
    float postGainOff = ch.getMeterPostGain().peakL.load();
    CHECK(std::abs(preGainOff - 0.5f) < 0.02f);
    CHECK(std::abs(postGainOff - 0.5f) < 0.02f);

    // 2. PAD ON: -20 dB attenuation (~0.1x linear, 0.5 * 0.1 = 0.05)
    // PRE-GAIN meter must STILL read raw input (~0.5f) before PAD!
    ch.setPadEnabled(true);
    ch.processBlock(devIn, 128);

    float preGainOn = ch.getMeterPreGain().peakL.load();
    float postGainOn = ch.getMeterPostGain().peakL.load();

    // Stage A PRE-GAIN meter remains untouched by PAD (measures raw input)
    CHECK(std::abs(preGainOn - 0.5f) < 0.02f);

    // Stage B Post-Gain meter and processed signal must be attenuated by ~-20 dB (0.05f)
    CHECK(std::abs(postGainOn - 0.05f) < 0.015f);

    std::cout << "[PASS] PAD DSP tests passed cleanly." << std::endl;
}

void testDirectMonitorAndPflRouting()
{
    std::cout << "[TEST] Running Direct Monitor (DM), PFL, and Mutual Exclusion routing tests..." << std::endl;

    dsd::ChannelManager chMgr;
    chMgr.initializeDefaultChannels(2);
    chMgr.prepare(48000.0, 128);

    dsd::OutputManager outMgr;
    outMgr.initializeDefaultOutputs(2);
    outMgr.prepare(48000.0, 128);

    auto* ch0 = chMgr.getChannel(0);
    auto* ch1 = chMgr.getChannel(1);
    auto* outMain = outMgr.getOutput(0);
    auto* outMon  = outMgr.getOutput(1);

    outMain->setMonitor(false); // Program mix
    outMon->setMonitor(true);   // Monitor bus

    // ── 1. Test PFL / DM Mutual Exclusion on Channel ──
    ch0->setDirectMonitor(true);
    CHECK(ch0->getDirectMonitor() == true);
    CHECK(ch0->getPfl() == false);

    ch0->setPfl(true); // Activating PFL automatically turns DM OFF
    CHECK(ch0->getPfl() == true);
    CHECK(ch0->getDirectMonitor() == false);

    ch0->setDirectMonitor(true); // Activating DM automatically turns PFL OFF
    CHECK(ch0->getDirectMonitor() == true);
    CHECK(ch0->getPfl() == false);

    ch0->setDirectMonitor(false);
    CHECK(ch0->getDirectMonitor() == false);
    CHECK(ch0->getPfl() == false);

    // ── 2. Test Direct Monitor (DM) follows fader and VST ──
    auto sine0 = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.8f);
    sine0->setEnabled(true);
    ch0->setInputSource(std::move(sine0));
    ch0->setGainDb(0.0f);
    ch0->setFaderDb(0.0f);
    ch0->setDirectMonitor(true);

    auto sine1 = std::make_unique<dsd::SineGeneratorSource>(500.0f, 0.3f);
    sine1->setEnabled(true);
    ch1->setInputSource(std::move(sine1));
    ch1->setGainDb(0.0f);
    ch1->setFaderDb(0.0f);
    ch1->setDirectMonitor(false);

    dsd::RoutingEngine router;
    router.setRouteEnabled(0, 0, true);
    router.setRouteGainDb(0, 0, 0.0f);
    router.setRouteEnabled(0, 1, true);
    router.setRouteGainDb(0, 1, 0.0f);
    router.setRouteEnabled(1, 0, true);
    router.setRouteGainDb(1, 0, 0.0f);
    router.setRouteEnabled(1, 1, true);
    router.setRouteGainDb(1, 1, 0.0f);

    juce::AudioBuffer<float> dummyIn(2, 128);
    dummyIn.clear();

    // At 0 dB fader, DM output on Monitor bus should be ~0.8f
    chMgr.processChannels(dummyIn, 128);
    router.routeChannelsToOutputs(chMgr, outMgr, 128);
    outMgr.processOutputs(128);
    float outMonDm1 = outMon->getBuffer().getMagnitude(0, 0, 128);
    CHECK(outMonDm1 > 0.7f && outMonDm1 < 0.9f);

    // Lower fader to -6 dB (~0.5x linear): DM MUST follow fader!
    ch0->setFaderDb(-6.0f);
    chMgr.processChannels(dummyIn, 128);
    router.routeChannelsToOutputs(chMgr, outMgr, 128);
    outMgr.processOutputs(128);
    float outMonDm2 = outMon->getBuffer().getMagnitude(0, 0, 128);
    CHECK(outMonDm2 > 0.35f && outMonDm2 < 0.45f); // Half level: follows fader!

    // Add VST plugin (multiplier 2.0x): DM MUST reflect VST processing!
    auto plug = std::make_unique<MockAudioPluginInstance>("GainX2", 2.0f, 0.0f);
    ch0->getPluginRack().addPlugin(std::move(plug), "GainX2");
    chMgr.processChannels(dummyIn, 128);
    router.routeChannelsToOutputs(chMgr, outMgr, 128);
    outMgr.processOutputs(128);
    float outMonDmVst = outMon->getBuffer().getMagnitude(0, 0, 128);
    CHECK(outMonDmVst > 0.7f && outMonDmVst < 0.9f); // Doubled by VST!

    // ── 3. Test PFL bypasses fader but follows VST ──
    // Switch ch0 to PFL (which turns DM OFF)
    ch0->setPfl(true);
    CHECK(ch0->getDirectMonitor() == false);

    // Pull fader down to -60 dB (silence in mix): PFL MUST still be heard (bypasses fader)!
    ch0->setFaderDb(-60.0f);
    chMgr.processChannels(dummyIn, 128);
    router.routeChannelsToOutputs(chMgr, outMgr, 128);
    outMgr.processOutputs(128);

    // Program bus (outMain) is silent for ch0 because fader is -60 dB (only ch1 at ~0.3f present)
    float outMainMag = outMain->getBuffer().getMagnitude(0, 0, 128);
    CHECK(outMainMag > 0.2f && outMainMag < 0.4f);

    // Monitor bus (outMon) hears ch0 via PFL tap (post-VST with 2.0x gain = ~1.6f)!
    float outMonPfl = outMon->getBuffer().getMagnitude(0, 0, 128);
    CHECK(outMonPfl > 1.4f); // PFL is unaffected by -60 dB fader!

    // Move fader to +6 dB: PFL level on monitor output must NOT change!
    ch0->setFaderDb(6.0f);
    chMgr.processChannels(dummyIn, 128);
    router.routeChannelsToOutputs(chMgr, outMgr, 128);
    outMgr.processOutputs(128);
    float outMonPfl2 = outMon->getBuffer().getMagnitude(0, 0, 128);
    CHECK(std::abs(outMonPfl2 - outMonPfl) < 0.05f); // PFL level invariant to fader!

    // ── 4. Test Dynamic Monitor Output Selection ──
    // Change monitor designation: outMain becomes monitor, outMon becomes non-monitor
    outMain->setMonitor(true);
    outMon->setMonitor(false);

    chMgr.processChannels(dummyIn, 128);
    router.routeChannelsToOutputs(chMgr, outMgr, 128);
    outMgr.processOutputs(128);

    // Now outMain receives the PFL feed (>1.4f)!
    float newMonMag = outMain->getBuffer().getMagnitude(0, 0, 128);
    CHECK(newMonMag > 1.4f);

    // And outMon receives the normal program mix (only ch1 at ~0.3f)!
    float newProgMag = outMon->getBuffer().getMagnitude(0, 0, 128);
    CHECK(newProgMag > 0.2f && newProgMag < 0.4f);

    std::cout << "[PASS] Direct Monitor (DM), PFL, and Mutual Exclusion routing tests passed." << std::endl;
}

void testPhaseAndMono()
{
    std::cout << "[TEST] Running Phase Invert and Force Mono tests..." << std::endl;

    dsd::AudioChannel ch(0, "CH 01");
    ch.prepare(48000.0, 128);

    // Setup stereo input buffer: Left = 0.6f, Right = 0.2f
    class StereoTestSource : public dsd::AudioInputSource
    {
    public:
        void prepare(double, int) override {}
        void releaseResources() override {}
        void readBlock(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>&, int numSamples) override
        {
            for (int i = 0; i < numSamples; ++i)
            {
                buffer.setSample(0, i, 0.6f);
                buffer.setSample(1, i, 0.2f);
            }
        }
    };

    ch.setInputSource(std::make_unique<StereoTestSource>());
    ch.setGainDb(0.0f);
    ch.setFaderDb(0.0f);

    juce::AudioBuffer<float> devIn(2, 128);
    devIn.clear();

    // 1. Force Mono test: (0.6 + 0.2) * 0.5 = 0.4
    ch.setForceMono(true);
    ch.processBlock(devIn, 128);

    auto& bufMono = ch.getOutputBuffer();
    for (int i = 0; i < 128; ++i)
    {
        CHECK(std::abs(bufMono.getSample(0, i) - 0.4f) < 0.001f);
        CHECK(std::abs(bufMono.getSample(1, i) - 0.4f) < 0.001f);
    }

    // 2. Phase Invert test: -0.4f
    ch.setPhaseInvert(true);
    ch.processBlock(devIn, 128);

    auto& bufPhase = ch.getOutputBuffer();
    for (int i = 0; i < 128; ++i)
    {
        CHECK(std::abs(bufPhase.getSample(0, i) - (-0.4f)) < 0.001f);
        CHECK(std::abs(bufPhase.getSample(1, i) - (-0.4f)) < 0.001f);
    }

    std::cout << "[PASS] Phase Invert and Force Mono tests passed." << std::endl;
}

int main()
{
    std::cout << "=================================================" << std::endl;
    std::cout << " DSD Mixer Level 1 - Complete Core Verification  " << std::endl;
    std::cout << "=================================================" << std::endl;

    testDeviceEnumeration();
    testGainProcessor();
    testPanProcessor();
    testMeterProcessor();
    test16x4RoutingAndOutputs();
    testMulticoreScheduler();
    testSchedulerExactlyOnceStress();
    testPanRampIsSmooth();
    testRouteSendLevelRamp();
    testBlockTimingHistogram();
    testPluginCountIsLockFree();
    testBypassCrossfadeIsClickFree();
    testLatencyPluginBypassDoesNotCombFilter();
    testPluginRackConcurrentMutation();
    testPluginDelayCompensation();
    testAdaptiveResamplingFifo();
    testPreFaderSends();
    testPerformanceTelemetryAndOverrun();
    testLowLatencyCaptureFifo();
    testWindowAudioCapture();
    testSessionSaveLoad();
    testVstBypassAndReorder();
    testPreGainMetering();
    testGainAndFaderControls();
    testOnOffVsMuteSemantics();
    testHpfDsp();
    testPadDsp();
    testDirectMonitorAndPflRouting();
    testPhaseAndMono();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Level 1 Engine Tests Successfully Passed!   " << std::endl;
    std::cout << "=================================================" << std::endl;

    return dsd_test::exitCode();
}
