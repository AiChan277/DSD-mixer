#include "Audio/MultiDeviceManager.h"
#include <iostream>

namespace dsd
{
    // =========================================================================
    // WindowsDeviceInputSource
    // =========================================================================
    WindowsDeviceInputSource::WindowsDeviceInputSource(const juce::String& deviceName, juce::AudioIODeviceType* deviceType)
        : devName(deviceName), devType(deviceType)
    {
        openDevice();
    }

    WindowsDeviceInputSource::~WindowsDeviceInputSource()
    {
        closeDevice();
    }

    bool WindowsDeviceInputSource::openDevice()
    {
        if (devType == nullptr || devName.isEmpty() || devName == "None")
            return false;

        try
        {
            devType->scanForDevices();
            device.reset(devType->createDevice(juce::String(), devName));

            if (device != nullptr)
            {
                juce::BigInteger inChans;
                auto defaultLayout = device->getDefaultInputChannels();
                if (defaultLayout.has_value())
                    inChans = defaultLayout.value();
                else
                    inChans.setRange(0, 2, true);

                juce::BigInteger outChans; // Capture only

                // Open with 0.0, 0 to let WASAPI shared mode negotiate the device's native rate and buffer size
                auto err = device->open(inChans, outChans, 0.0, 0);
                if (err.isEmpty())
                {
                    deviceSampleRate = device->getCurrentSampleRate();
                    if (deviceSampleRate <= 0.0)
                        deviceSampleRate = 48000.0;

                    interpolator[0].reset();
                    interpolator[1].reset();

                    ringBuffer.reset();
                    device->start(this);
                    return true;
                }
                else
                {
                    device.reset();
                }
            }
        }
        catch (...)
        {
            device.reset();
        }

        return false;
    }

    void WindowsDeviceInputSource::closeDevice()
    {
        if (device != nullptr)
        {
            device->stop();
            device->close();
            device.reset();
        }
        ringBuffer.reset();
    }

    void WindowsDeviceInputSource::prepare(double sampleRate, int /*maxBlockSize*/)
    {
        targetSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        if (device == nullptr)
            openDevice();
    }

    void WindowsDeviceInputSource::releaseResources()
    {
        closeDevice();
    }

    void WindowsDeviceInputSource::readBlock(juce::AudioBuffer<float>& buffer,
                                            const juce::AudioBuffer<float>& /*deviceInput*/,
                                            int numSamples)
    {
        if (device == nullptr || !device->isPlaying())
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.clear(ch, 0, numSamples);
            return;
        }

        ringBuffer.read(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);
    }

    void WindowsDeviceInputSource::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/)
    {
        interpolator[0].reset();
        interpolator[1].reset();
        ringBuffer.reset();
    }

    void WindowsDeviceInputSource::audioDeviceStopped()
    {
        ringBuffer.reset();
    }

    void WindowsDeviceInputSource::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                                   int numInputChannels,
                                                                   float* const* /*outputChannelData*/,
                                                                   int /*numOutputChannels*/,
                                                                   int numSamples,
                                                                   const juce::AudioIODeviceCallbackContext& /*context*/)
    {
        if (inputChannelData == nullptr || numInputChannels <= 0 || numSamples <= 0)
            return;

        // Check if sample rate conversion is required
        if (std::abs(deviceSampleRate - targetSampleRate) < 1.0)
        {
            // Same sample rate (e.g. 48 kHz native)
            ringBuffer.write(inputChannelData, numInputChannels, numSamples);
        }
        else
        {
            // Resample from device native rate (e.g. 44.1 kHz, 16 kHz) to engine rate (48 kHz)
            const double speedRatio = deviceSampleRate / targetSampleRate;
            const int numProduced = static_cast<int>(std::round(static_cast<double>(numSamples) / speedRatio));
            if (numProduced > 0)
            {
                resampleBuffer.setSize(2, numProduced, false, false, true);

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* src = (ch < numInputChannels && inputChannelData[ch] != nullptr)
                                       ? inputChannelData[ch]
                                       : ((numInputChannels > 0 && inputChannelData[0] != nullptr) ? inputChannelData[0] : nullptr);

                    if (src != nullptr)
                    {
                        interpolator[ch].process(speedRatio, src, resampleBuffer.getWritePointer(ch),
                                                 numProduced, numSamples, 0);
                    }
                    else
                    {
                        resampleBuffer.clear(ch, 0, numProduced);
                    }
                }

                ringBuffer.write(resampleBuffer.getArrayOfReadPointers(), 2, numProduced);
            }
        }
    }

    // =========================================================================
    // WindowsDeviceOutputSink
    // =========================================================================
    WindowsDeviceOutputSink::WindowsDeviceOutputSink(const juce::String& deviceName, juce::AudioIODeviceType* deviceType)
        : devName(deviceName), devType(deviceType)
    {
        openDevice();
    }

    WindowsDeviceOutputSink::~WindowsDeviceOutputSink()
    {
        closeDevice();
    }

    bool WindowsDeviceOutputSink::openDevice()
    {
        if (devType == nullptr || devName.isEmpty() || devName == "None")
            return false;

        try
        {
            devType->scanForDevices();
            device.reset(devType->createDevice(devName, juce::String()));

            if (device != nullptr)
            {
                juce::BigInteger inChans;  // Render only
                juce::BigInteger outChans;
                auto defaultLayout = device->getDefaultOutputChannels();
                if (defaultLayout.has_value())
                    outChans = defaultLayout.value();
                else
                    outChans.setRange(0, 2, true);

                // Open with 0.0, 0 to let WASAPI shared mode negotiate native rate and buffer size
                auto err = device->open(inChans, outChans, 0.0, 0);
                if (err.isEmpty())
                {
                    deviceSampleRate = device->getCurrentSampleRate();
                    if (deviceSampleRate <= 0.0)
                        deviceSampleRate = 48000.0;

                    interpolator[0].reset();
                    interpolator[1].reset();

                    ringBuffer.reset();
                    device->start(this);
                    return true;
                }
                else
                {
                    device.reset();
                }
            }
        }
        catch (...)
        {
            device.reset();
        }

        return false;
    }

    void WindowsDeviceOutputSink::closeDevice()
    {
        if (device != nullptr)
        {
            device->stop();
            device->close();
            device.reset();
        }
        ringBuffer.reset();
    }

    void WindowsDeviceOutputSink::prepare(double sampleRate, int /*maxBlockSize*/)
    {
        targetSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        if (device == nullptr)
            openDevice();
    }

    void WindowsDeviceOutputSink::releaseResources()
    {
        closeDevice();
    }

    void WindowsDeviceOutputSink::writeBlock(const juce::AudioBuffer<float>& buffer, int numSamples)
    {
        if (device == nullptr || !device->isPlaying() || numSamples <= 0)
            return;

        if (std::abs(deviceSampleRate - targetSampleRate) < 1.0)
        {
            ringBuffer.write(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), numSamples);
        }
        else
        {
            // Resample from target engine rate (48 kHz) to device native rate (e.g. 44.1 kHz)
            const double speedRatio = targetSampleRate / deviceSampleRate;
            const int numProduced = static_cast<int>(std::round(static_cast<double>(numSamples) / speedRatio));
            if (numProduced > 0)
            {
                resampleBuffer.setSize(2, numProduced, false, false, true);

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* src = (ch < buffer.getNumChannels()) ? buffer.getReadPointer(ch) : nullptr;
                    if (src != nullptr)
                    {
                        interpolator[ch].process(speedRatio, src, resampleBuffer.getWritePointer(ch),
                                                 numProduced, numSamples, 0);
                    }
                    else
                    {
                        resampleBuffer.clear(ch, 0, numProduced);
                    }
                }

                ringBuffer.write(resampleBuffer.getArrayOfReadPointers(), 2, numProduced);
            }
        }
    }

    void WindowsDeviceOutputSink::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/)
    {
        interpolator[0].reset();
        interpolator[1].reset();
        ringBuffer.reset();
    }

    void WindowsDeviceOutputSink::audioDeviceStopped()
    {
        ringBuffer.reset();
    }

    void WindowsDeviceOutputSink::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                                  int /*numInputChannels*/,
                                                                  float* const* outputChannelData,
                                                                  int numOutputChannels,
                                                                  int numSamples,
                                                                  const juce::AudioIODeviceCallbackContext& /*context*/)
    {
        if (outputChannelData != nullptr && numOutputChannels > 0 && numSamples > 0)
        {
            ringBuffer.read(outputChannelData, numOutputChannels, numSamples);

            // Output limiting protection on dedicated DAC output
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                {
                    float* p = outputChannelData[ch];
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float x = p[i];
                        if (x > 0.988f)
                            p[i] = 0.988f + 0.0119f * std::tanh((x - 0.988f) / 0.0119f);
                        else if (x < -0.988f)
                            p[i] = -0.988f + 0.0119f * std::tanh((x + 0.988f) / 0.0119f);
                    }
                }
            }
        }
    }

    // =========================================================================
    // MultiDeviceManager
    // =========================================================================
    MultiDeviceManager& MultiDeviceManager::getInstance()
    {
        static MultiDeviceManager instance;
        return instance;
    }

    void MultiDeviceManager::initialize(juce::AudioDeviceManager& hostManager)
    {
        std::lock_guard<std::mutex> lock(mutex);
        juceManagerRef = &hostManager;
    }

    void MultiDeviceManager::shutdown()
    {
        std::lock_guard<std::mutex> lock(mutex);
        juceManagerRef = nullptr;
    }

    juce::String MultiDeviceManager::getPrimaryInputDeviceName() const
    {
        if (juceManagerRef != nullptr)
        {
            if (auto* dev = juceManagerRef->getCurrentAudioDevice())
                return dev->getName();
        }
        return {};
    }

    juce::String MultiDeviceManager::getPrimaryOutputDeviceName() const
    {
        if (juceManagerRef != nullptr)
        {
            if (auto* dev = juceManagerRef->getCurrentAudioDevice())
                return dev->getName();
        }
        return {};
    }

    juce::AudioIODeviceType* MultiDeviceManager::getWasapiDeviceType()
    {
        if (juceManagerRef == nullptr)
            return nullptr;

        for (auto* type : juceManagerRef->getAvailableDeviceTypes())
        {
            if (type != nullptr && type->getTypeName() == "Windows Audio")
                return type;
        }

        // Fallback to first available type if Windows Audio not matched exactly
        const auto& types = juceManagerRef->getAvailableDeviceTypes();
        if (types.size() > 0)
            return types[0];

        return nullptr;
    }

    juce::StringArray MultiDeviceManager::getAvailableInputDevices()
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto* type = getWasapiDeviceType();
        if (type != nullptr)
        {
            type->scanForDevices();
            return type->getDeviceNames(true);
        }
        return {};
    }

    juce::StringArray MultiDeviceManager::getAvailableOutputDevices()
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto* type = getWasapiDeviceType();
        if (type != nullptr)
        {
            type->scanForDevices();
            return type->getDeviceNames(false);
        }
        return {};
    }

    std::unique_ptr<AudioInputSource> MultiDeviceManager::createInputSourceFor(const juce::String& selectionName)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (selectionName.isEmpty() || selectionName == "None")
        {
            return std::make_unique<NullInputSource>();
        }

        if (selectionName.containsIgnoreCase("Sine Wave") || selectionName.containsIgnoreCase("1 kHz"))
        {
            return std::make_unique<SineInputSource>(440.0f);
        }

        if (selectionName.containsIgnoreCase("Noise"))
        {
            return std::make_unique<NoiseInputSource>();
        }

        auto* wasapi = getWasapiDeviceType();
        if (wasapi != nullptr)
        {
            return std::make_unique<WindowsDeviceInputSource>(selectionName, wasapi);
        }

        return std::make_unique<NullInputSource>();
    }

    std::unique_ptr<WindowsDeviceOutputSink> MultiDeviceManager::createOutputSinkFor(const juce::String& deviceName)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (deviceName.isEmpty() || deviceName == "None")
            return nullptr;

        auto* wasapi = getWasapiDeviceType();
        if (wasapi != nullptr)
        {
            return std::make_unique<WindowsDeviceOutputSink>(deviceName, wasapi);
        }

        return nullptr;
    }
} // namespace dsd
