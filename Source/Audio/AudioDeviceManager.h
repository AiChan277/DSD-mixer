#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "Audio/AudioTypes.h"
#include <memory>
#include <string>

namespace dsd
{
    class AudioDeviceManager
    {
    public:
        AudioDeviceManager();
        ~AudioDeviceManager();

        bool initialize(int numInputChannels = 2, int numOutputChannels = 2);
        void shutdown();

        void addAudioCallback(juce::AudioIODeviceCallback* callback);
        void removeAudioCallback(juce::AudioIODeviceCallback* callback);

        juce::AudioDeviceManager& getJuceManager() noexcept { return juceDeviceManager; }

        double getCurrentSampleRate() const;
        int getCurrentBufferSize() const;
        std::string getCurrentOutputDeviceName() const;
        std::string getCurrentInputDeviceName() const;

    private:
        juce::AudioDeviceManager juceDeviceManager;
    };
} // namespace dsd
