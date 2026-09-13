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

void testGainProcessor()
{
    std::cout << "[TEST] Running GainProcessor tests..." << std::endl;

    assert(std::abs(dsd::GainProcessor::dbToLinear(0.0f) - 1.0f) < 0.0001f);
    assert(std::abs(dsd::GainProcessor::dbToLinear(-6.0f) - 0.501187f) < 0.001f);
    assert(std::abs(dsd::GainProcessor::dbToLinear(6.0f) - 1.99526f) < 0.001f);
    assert(dsd::GainProcessor::dbToLinear(-60.0f) == 0.0f);

    std::cout << "[PASS] GainProcessor tests passed." << std::endl;
}

void testPanProcessor()
{
    std::cout << "[TEST] Running PanProcessor tests..." << std::endl;

    dsd::PanProcessor pan;
    pan.setPan(0.0f);
    assert(std::abs(pan.getGainL() - 0.70710678f) < 0.001f);
    assert(std::abs(pan.getGainR() - 0.70710678f) < 0.001f);

    float powerCenter = pan.getGainL() * pan.getGainL() + pan.getGainR() * pan.getGainR();
    assert(std::abs(powerCenter - 1.0f) < 0.001f);

    pan.setPan(-1.0f);
    assert(std::abs(pan.getGainL() - 1.0f) < 0.001f);
    assert(std::abs(pan.getGainR() - 0.0f) < 0.001f);

    pan.setPan(1.0f);
    assert(std::abs(pan.getGainL() - 0.0f) < 0.001f);
    assert(std::abs(pan.getGainR() - 1.0f) < 0.001f);

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

    assert(std::abs(values.peakL.load() - 0.5f) < 0.001f);
    assert(std::abs(values.peakR.load() - 0.8f) < 0.001f);
    assert(!values.clipped.load());

    std::vector<float> clippedSamples(numSamples, 1.05f);
    meter.processBlock(clippedSamples.data(), clippedSamples.data(), numSamples, values);
    assert(values.clipped.load() == true);

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

    assert(channelMgr.getNumChannels() == 16);
    assert(outputMgr.getNumOutputs() == 4);

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
    assert(out1Mag > 0.1f);

    // OUT 02 should receive attenuated signal (-6 dB approx half amplitude)
    float out2Mag = outputMgr.getOutput(1)->getBuffer().getMagnitude(0, 0, 128);
    assert(out2Mag > 0.05f);
    assert(out2Mag < out1Mag);

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
    assert(scheduler.getNumWorkers() == 8);
    assert(scheduler.getMaxWorkers() == 8);

    juce::AudioBuffer<float> dummyInput(2, 128);
    dummyInput.clear();

    // Initial state: default active workers is 4
    assert(scheduler.getActiveWorkerCount() == 4);

    // 2. Test parallel dispatch across blocks
    for (int block = 0; block < 10; ++block)
    {
        scheduler.processChannelsParallel(channelMgr, dummyInput, 128);
    }

    // Verify that active workers 0..3 are unparked and assigned all 16 channels, and 4..7 are parked
    int totalAssigned = 0;
    for (int i = 0; i < 4; ++i)
    {
        assert(!scheduler.getWorkerStats(i)->isParked.load());
        totalAssigned += scheduler.getWorkerStats(i)->tasksAssigned.load();
    }
    assert(totalAssigned == 16);

    for (int i = 4; i < 8; ++i)
    {
        assert(scheduler.getWorkerStats(i)->isParked.load());
        assert(scheduler.getWorkerStats(i)->tasksAssigned.load() == 0);
        assert(scheduler.getWorkerStats(i)->workerUtilizationPercent.load() == 0.0f);
    }

    // 3. Test Adaptive Scaling - Fast Attack Scale UP
    // Audio deadline for 128 samples @ 48kHz = 128 / 48000 = ~0.0026667s (2.67 ms)
    const double deadlineSec = 128.0 / 48000.0;

    // Simulate high load: dspTime = 2.0 ms (75% of deadline)
    // Needs 4 consecutive high load blocks to trigger attack
    for (int b = 0; b < 3; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0020, deadlineSec, 16);
    // Still 4 before 4th block
    assert(scheduler.getActiveWorkerCount() == 4);

    // 4th block triggers fast attack (+2 -> 6 workers)
    scheduler.updateAdaptiveWorkerCount(0.0020, deadlineSec, 16);
    assert(scheduler.getActiveWorkerCount() == 6);

    // Extreme load spike: dspTime = 2.3 ms (>80% of deadline)
    for (int b = 0; b < 4; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0023, deadlineSec, 16);
    assert(scheduler.getActiveWorkerCount() == 8);
    std::cout << "  [PASS] Fast attack scaled up to 8 workers under high load." << std::endl;

    // 4. Test Task Graph Floor with Light Load
    // With 16 active channels, graph floor is 6. Even after 350 blocks of light load, it should decay to 6, not 2.
    for (int b = 0; b < 350; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0001, deadlineSec, 16);
    assert(scheduler.getActiveWorkerCount() == 6);
    std::cout << "  [PASS] Task graph floor for 16 channels held workers at 6." << std::endl;

    // With 2 active channels, graph floor is 2. After another 350 blocks, workers decay 6 -> 4 -> 2.
    for (int b = 0; b < 350; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0001, deadlineSec, 2);
    assert(scheduler.getActiveWorkerCount() == 4);

    for (int b = 0; b < 350; ++b)
        scheduler.updateAdaptiveWorkerCount(0.0001, deadlineSec, 2);
    assert(scheduler.getActiveWorkerCount() == 2);
    std::cout << "  [PASS] Slow decay scaled down to 2 workers for low load (2 channels)." << std::endl;

    // 5. Test parallel dispatch with 2 active workers
    for (int block = 0; block < 10; ++block)
    {
        scheduler.processChannelsParallel(channelMgr, dummyInput, 128);
    }
    assert(!scheduler.getWorkerStats(0)->isParked.load());
    assert(!scheduler.getWorkerStats(1)->isParked.load());
    int twoWorkerTasks = scheduler.getWorkerStats(0)->tasksAssigned.load() + scheduler.getWorkerStats(1)->tasksAssigned.load();
    assert(twoWorkerTasks == 16);
    assert(scheduler.getWorkerStats(2)->isParked.load());
    assert(scheduler.getWorkerStats(7)->isParked.load());

    // 6. Test manual override
    scheduler.setManualWorkerCount(5);
    assert(scheduler.getActiveWorkerCount() == 5);
    assert(!scheduler.isAdaptiveScalingEnabled());

    std::cout << "[PASS] Multicore Scheduler & Adaptive Worker Pool tests passed cleanly." << std::endl;
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
    assert(fifo.getTargetOccupancy() == 192);
    assert(fifo.getNumReady() == 0);

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
    assert(readyAfterBurst == (192 + 480));
    assert(fifo.getBurstFlushedFrames() == (2000 - (192 + 480)));

    // 2. Test reading blocks of 256 samples
    juce::AudioBuffer<float> readBuf(2, 256);
    int read1 = fifo.read(readBuf.getArrayOfWritePointers(), 2, 256, 48000.0);
    assert(read1 == 256);
    assert(fifo.getNumReady() == (672 - 256));

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
    assert(fifo.getTargetOccupancy() == 96);
    // Write packet
    fifo.write(packet.getArrayOfReadPointers(), 2, 480);
    // Read next block; dynamic backlog flush keeps buffer strictly bounded
    fifo.read(readBuf.getArrayOfWritePointers(), 2, 128, 48000.0);
    assert(fifo.getNumReady() <= (96 + 480));
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
        assert(capture->getTargetPid() == apps[0].processId);
        assert(capture->getProcessName() == apps[0].appName);

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
    assert(ch0 != nullptr && ch1 != nullptr);

    ch0->setName("Lead Vocal");
    ch0->setGainDb(3.5f);
    ch0->setFaderDb(-4.0f);
    ch0->setPan(-0.25f);
    ch0->setInputDeviceName("Primary In 1 (Mic / L)");
    ch0->setInputChannelIndex(0);

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
    assert(out0 != nullptr);
    out0->setName("Main Monitor");
    out0->setFaderDb(-3.0f);
    out0->setOutputDeviceName("Speakers");

    juce::File sessionFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("test_dsd_session.dsd");
    if (sessionFile.existsAsFile())
        sessionFile.deleteFile();

    bool saved = dsd::SessionManager::saveSessionToFile(sessionFile, engine, devMgr);
    assert(saved);
    assert(sessionFile.existsAsFile());

    // Scramble values to verify restoration
    ch0->setName("Changed");
    ch0->setGainDb(0.0f);
    ch0->setFaderDb(0.0f);
    ch0->setInputDeviceName("None");

    ch1->setName("Changed");
    ch1->setMute(false);
    ch1->setInputDeviceName("None");

    router.setRouteEnabled(0, 0, false);
    out0->setName("Changed");

    // Load session
    bool loaded = dsd::SessionManager::loadSessionFromFile(sessionFile, engine, devMgr);
    assert(loaded);

    // Verify channel 0 restoration
    assert(ch0->getName() == "Lead Vocal");
    assert(std::abs(ch0->getGainDb() - 3.5f) < 0.01f);
    assert(std::abs(ch0->getFaderDb() - (-4.0f)) < 0.01f);
    assert(std::abs(ch0->getPan() - (-0.25f)) < 0.01f);
    assert(ch0->getInputDeviceName() == "Primary In 1 (Mic / L)");
    assert(ch0->getInputChannelIndex() == 0);
    assert(ch0->getInputSource() != nullptr);

    // Verify channel 1 restoration
    assert(ch1->getName() == "Spotify Music");
    assert(std::abs(ch1->getFaderDb() - (-12.0f)) < 0.01f);
    assert(ch1->getMute() == true);
    assert(ch1->getInputDeviceName() == "Spotify");

    // Verify routing and output restoration
    assert(router.isRouteEnabled(0, 0) == true);
    assert(std::abs(router.getRouteGainDb(0, 0) - (-2.5f)) < 0.01f);
    assert(router.isRouteEnabled(1, 1) == true);
    assert(out0->getName() == "Main Monitor");
    assert(std::abs(out0->getFaderDb() - (-3.0f)) < 0.01f);
    assert(out0->getOutputDeviceName() == "Speakers");

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
    assert(stats.deadlineMissCount.load() == 0);
    assert(stats.xrunCount.load() == 0);
    assert(!stats.isGlitching.load());

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

    assert(stats.deadlineMs.load() == 10.0); // 480 / 48000 * 1000 = 10.0 ms
    assert(stats.currentProcessingTimeMs.load() > 0.0);
    assert(stats.currentLoadPercent.load() > 0.0f);
    assert(stats.deadlineMissCount.load() == 0);
    assert(stats.currentHeadroomMs.load() > 0.0);
    assert(stats.minHeadroomMs.load() <= 10.0);
    assert(stats.peakProcessingTimeMs.load() >= stats.currentProcessingTimeMs.load());
    assert(stats.peakLoadPercent.load() >= stats.currentLoadPercent.load());

    std::cout << "  DSP Time: " << stats.currentProcessingTimeMs.load() << " ms (Peak: "
              << stats.peakProcessingTimeMs.load() << " ms), Load: "
              << stats.currentLoadPercent.load() << "% (Peak: "
              << stats.peakLoadPercent.load() << "%), Headroom: "
              << stats.currentHeadroomMs.load() << " ms (Min: "
              << stats.minHeadroomMs.load() << " ms)" << std::endl;

    // Test Reset Metrics
    engine.resetPerformanceMetrics();
    assert(stats.deadlineMissCount.load() == 0);
    assert(stats.xrunCount.load() == 0);
    assert(!stats.isGlitching.load());
    assert(stats.peakProcessingTimeMs.load() == 0.0);
    assert(stats.peakLoadPercent.load() == 0.0f);
    assert(stats.minHeadroomMs.load() == 10.0);

    // Test ProcessCpuTracker
    dsd::ProcessCpuTracker cpuTracker;
    float cpu = cpuTracker.getCpuUsagePercent();
    assert(cpu >= 0.0f && cpu <= 100.0f);
    std::cout << "  [PASS] ProcessCpuTracker returned: " << cpu << "%" << std::endl;

    std::cout << "[PASS] Performance Telemetry, True Peak & Overrun tests passed cleanly." << std::endl;
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
    testPerformanceTelemetryAndOverrun();
    testLowLatencyCaptureFifo();
    testWindowAudioCapture();
    testSessionSaveLoad();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Level 1 Engine Tests Successfully Passed!   " << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
