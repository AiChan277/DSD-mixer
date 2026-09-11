#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Channel/ChannelManager.h"
#include "Scheduler/DSPScheduler.h"
#include "Routing/RoutingEngine.h"
#include "Bus/BusManager.h"
#include "Output/OutputManager.h"
#include <memory>

namespace dsd
{
    class AudioEngine : public juce::AudioIODeviceCallback
    {
    public:
        AudioEngine();
        ~AudioEngine() override;

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override;

        ChannelManager& getChannelManager() noexcept { return channelManager; }
        const ChannelManager& getChannelManager() const noexcept { return channelManager; }

        DSPScheduler& getScheduler() noexcept { return dspScheduler; }
        const DSPScheduler& getScheduler() const noexcept { return dspScheduler; }

        RoutingEngine& getRoutingEngine() noexcept { return routingEngine; }
        const RoutingEngine& getRoutingEngine() const noexcept { return routingEngine; }

        BusManager& getBusManager() noexcept { return busManager; }
        const BusManager& getBusManager() const noexcept { return busManager; }

        OutputManager& getOutputManager() noexcept { return outputManager; }
        const OutputManager& getOutputManager() const noexcept { return outputManager; }

        EnginePerformanceStats& getPerformanceStats() noexcept { return perfStats; }
        const EnginePerformanceStats& getPerformanceStats() const noexcept { return perfStats; }

        double getCurrentSampleRate() const noexcept { return currentSampleRate; }
        int getCurrentBlockSize() const noexcept { return currentBlockSize; }

    private:
        ChannelManager channelManager;
        DSPScheduler   dspScheduler;
        RoutingEngine  routingEngine;
        BusManager     busManager;
        OutputManager  outputManager;

        EnginePerformanceStats perfStats;

        double currentSampleRate{DEFAULT_SAMPLE_RATE};
        int currentBlockSize{DEFAULT_BUFFER_SIZE};

        juce::AudioBuffer<float> tempInputBuffer;
    };
} // namespace dsd
