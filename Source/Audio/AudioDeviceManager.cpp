#include "Audio/AudioDeviceManager.h"

namespace dsd
{
    AudioDeviceManager::AudioDeviceManager()
    {
    }

    AudioDeviceManager::~AudioDeviceManager()
    {
        shutdown();
    }

    bool AudioDeviceManager::initialize(int numInputChannels, int numOutputChannels)
    {
        // Initialise with WASAPI / DirectSound
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = DEFAULT_SAMPLE_RATE;
        setup.bufferSize = DEFAULT_BUFFER_SIZE;

        juce::String err = juceDeviceManager.initialise(numInputChannels,
                                                       numOutputChannels,
                                                       nullptr, // default XML setup
                                                       true,    // select default device on failure
                                                       {},
                                                       &setup);

        if (err.isNotEmpty())
        {
            // Try fallback with default system settings
            juceDeviceManager.initialiseWithDefaultDevices(numInputChannels, numOutputChannels);
        }

        return true;
    }

    void AudioDeviceManager::shutdown()
    {
        juceDeviceManager.closeAudioDevice();
    }

    void AudioDeviceManager::addAudioCallback(juce::AudioIODeviceCallback* callback)
    {
        juceDeviceManager.addAudioCallback(callback);
    }

    void AudioDeviceManager::removeAudioCallback(juce::AudioIODeviceCallback* callback)
    {
        juceDeviceManager.removeAudioCallback(callback);
    }

    double AudioDeviceManager::getCurrentSampleRate() const
    {
        if (auto* device = juceDeviceManager.getCurrentAudioDevice())
            return device->getCurrentSampleRate();
        return DEFAULT_SAMPLE_RATE;
    }

    int AudioDeviceManager::getCurrentBufferSize() const
    {
        if (auto* device = juceDeviceManager.getCurrentAudioDevice())
            return device->getCurrentBufferSizeSamples();
        return DEFAULT_BUFFER_SIZE;
    }

    std::string AudioDeviceManager::getCurrentOutputDeviceName() const
    {
        if (auto* device = juceDeviceManager.getCurrentAudioDevice())
            return device->getName().toStdString();
        return "Default Output";
    }

    std::string AudioDeviceManager::getCurrentInputDeviceName() const
    {
        if (auto* device = juceDeviceManager.getCurrentAudioDevice())
        {
            auto activeInputs = device->getActiveInputChannels();
            if (activeInputs.countNumberOfSetBits() > 0)
                return device->getName().toStdString();
        }
        return "Default Input";
    }
} // namespace dsd
