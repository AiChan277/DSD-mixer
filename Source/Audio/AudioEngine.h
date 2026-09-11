#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Channel/ChannelManager.h"
#include "Routing/RoutingEngine.h"
#include "Master/MasterBus.h"
#include <memory>

namespace dsd
{
    class AudioEngine : public juce::AudioIODeviceCallback
    {
    public:
        AudioEngine();
        ~AudioEngine() override;

        // AudioIODeviceCallback implementation
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

        RoutingEngine& getRoutingEngine() noexcept { return routingEngine; }
        const RoutingEngine& getRoutingEngine() const noexcept { return routingEngine; }

        MasterBus& getMasterBus() noexcept { return masterBus; }
        const MasterBus& getMasterBus() const noexcept { return masterBus; }

        EnginePerformanceStats& getPerformanceStats() noexcept { return perfStats; }
        const EnginePerformanceStats& getPerformanceStats() const noexcept { return perfStats; }

        double getCurrentSampleRate() const noexcept { return currentSampleRate; }
        int getCurrentBlockSize() const noexcept { return currentBlockSize; }

    private:
        ChannelManager channelManager;
        RoutingEngine routingEngine;
        MasterBus masterBus;

        EnginePerformanceStats perfStats;

        double currentSampleRate{DEFAULT_SAMPLE_RATE};
        int currentBlockSize{DEFAULT_BUFFER_SIZE};

        // Preallocated real-time scratch buffers
        juce::AudioBuffer<float> tempInputBuffer;
        juce::AudioBuffer<float> masterMixBuffer;
    };
} // namespace dsd
