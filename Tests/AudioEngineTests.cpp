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
#include "Routing/RoutingEngine.h"
#include "Master/MasterBus.h"

void testGainProcessor()
{
    std::cout << "[TEST] Running GainProcessor tests..." << std::endl;

    // 0 dB should be exactly 1.0 linear
    assert(std::abs(dsd::GainProcessor::dbToLinear(0.0f) - 1.0f) < 0.0001f);

    // -6 dB should be approx 0.501187 linear
    assert(std::abs(dsd::GainProcessor::dbToLinear(-6.0f) - 0.501187f) < 0.001f);

    // +6 dB should be approx 1.995 linear
    assert(std::abs(dsd::GainProcessor::dbToLinear(6.0f) - 1.99526f) < 0.001f);

    // <= -60 dB should clamp to 0.0 linear
    assert(dsd::GainProcessor::dbToLinear(-60.0f) == 0.0f);
    assert(dsd::GainProcessor::dbToLinear(-80.0f) == 0.0f);

    std::cout << "[PASS] GainProcessor tests passed." << std::endl;
}

void testPanProcessor()
{
    std::cout << "[TEST] Running PanProcessor tests..." << std::endl;

    dsd::PanProcessor pan;

    // Center pan: L and R should both equal ~0.7071 (cos(pi/4) and sin(pi/4))
    pan.setPan(0.0f);
    assert(std::abs(pan.getGainL() - 0.70710678f) < 0.001f);
    assert(std::abs(pan.getGainR() - 0.70710678f) < 0.001f);

    // Constant power check: gainL^2 + gainR^2 ~= 1.0
    float powerCenter = pan.getGainL() * pan.getGainL() + pan.getGainR() * pan.getGainR();
    assert(std::abs(powerCenter - 1.0f) < 0.001f);

    // Full Left (-1.0f): L=1.0, R=0.0
    pan.setPan(-1.0f);
    assert(std::abs(pan.getGainL() - 1.0f) < 0.001f);
    assert(std::abs(pan.getGainR() - 0.0f) < 0.001f);

    // Full Right (+1.0f): L=0.0, R=1.0
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

    // Generate test block: 128 samples with peak 0.8
    const int numSamples = 128;
    std::vector<float> left(numSamples, 0.5f);
    std::vector<float> right(numSamples, 0.8f);

    meter.processBlock(left.data(), right.data(), numSamples, values);

    assert(std::abs(values.peakL.load() - 0.5f) < 0.001f);
    assert(std::abs(values.peakR.load() - 0.8f) < 0.001f);
    assert(std::abs(values.peakHoldR.load() - 0.8f) < 0.001f);
    assert(!values.clipped.load());

    // Test clipping detection
    std::vector<float> clippedSamples(numSamples, 1.05f);
    meter.processBlock(clippedSamples.data(), clippedSamples.data(), numSamples, values);
    assert(values.clipped.load() == true);

    std::cout << "[PASS] MeterProcessor tests passed." << std::endl;
}

void testRoutingAndSolo()
{
    std::cout << "[TEST] Running Routing & Solo tests..." << std::endl;

    dsd::ChannelManager channelMgr;
    channelMgr.initializeDefaultChannels(4);
    channelMgr.prepare(48000.0, 128);

    // Channel 1: enable sine wave generator (amplitude 0.5)
    auto* ch1 = channelMgr.getChannel(0);
    assert(ch1 != nullptr);
    auto sineGen1 = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.5f);
    sineGen1->setEnabled(true);
    ch1->setInputSource(std::move(sineGen1));

    // Channel 2: enable sine wave generator (amplitude 0.25)
    auto* ch2 = channelMgr.getChannel(1);
    assert(ch2 != nullptr);
    auto sineGen2 = std::make_unique<dsd::SineGeneratorSource>(1000.0f, 0.25f);
    sineGen2->setEnabled(true);
    ch2->setInputSource(std::move(sineGen2));

    // Process channels with empty dummy device input
    juce::AudioBuffer<float> dummyDeviceIn(2, 128);
    dummyDeviceIn.clear();
    channelMgr.processChannels(dummyDeviceIn, 128);

    // Route to master
    dsd::RoutingEngine routing;
    juce::AudioBuffer<float> masterBuffer(2, 128);
    routing.routeChannelsToMaster(channelMgr, masterBuffer, 128);

    // Verify master got mixed audio
    float masterPeakL = masterBuffer.getMagnitude(0, 0, 128);
    assert(masterPeakL > 0.1f);

    // Now test Solo logic: Solo Channel 1
    ch1->setSolo(true);
    assert(channelMgr.hasAnySoloChannel() == true);

    routing.routeChannelsToMaster(channelMgr, masterBuffer, 128);
    // Channel 2 should be excluded now, only Channel 1 present

    // Test Disable Output switch (from sketch)
    ch1->setDisableOutput(true);
    routing.routeChannelsToMaster(channelMgr, masterBuffer, 128);
    float masterPeakDisabled = masterBuffer.getMagnitude(0, 0, 128);
    assert(masterPeakDisabled == 0.0f); // Master is completely silent when routed output disabled!

    std::cout << "[PASS] Routing & Solo tests passed." << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " DSD Mixer - Engine & DSP Verification " << std::endl;
    std::cout << "========================================" << std::endl;

    testGainProcessor();
    testPanProcessor();
    testMeterProcessor();
    testRoutingAndSolo();

    std::cout << "========================================" << std::endl;
    std::cout << " All Engine Tests Successfully Passed!  " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
