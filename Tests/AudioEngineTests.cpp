#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

#include "Audio/AudioTypes.h"
#include "DSP/GainProcessor.h"
#include "DSP/PanProcessor.h"
#include "DSP/MeterProcessor.h"
#include "Audio/AudioInputSource.h"
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
    std::cout << "[TEST] Running Multicore Scheduler parallel dispatch tests..." << std::endl;

    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(16);
    channelMgr.prepare(48000.0, 128);

    dsd::DSPScheduler scheduler;
    assert(scheduler.getNumWorkers() >= 2);

    juce::AudioBuffer<float> dummyInput(2, 128);
    dummyInput.clear();

    // Process 100 blocks in parallel
    for (int block = 0; block < 100; ++block)
    {
        scheduler.processChannelsParallel(channelMgr, dummyInput, 128);
    }

    std::cout << "[PASS] Multicore Scheduler processed 100 blocks successfully with "
              << scheduler.getNumWorkers() << " worker threads." << std::endl;
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
    }
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

    std::cout << "=================================================" << std::endl;
    std::cout << " All Level 1 Engine Tests Successfully Passed!   " << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
