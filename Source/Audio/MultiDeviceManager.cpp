#include "Audio/MultiDeviceManager.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace dsd
{
    namespace
    {
        // Soft knee just below full scale on dedicated DAC outputs.
        inline float softClip(float x) noexcept
        {
            if (x > 0.988f)
                return 0.988f + 0.0119f * std::tanh((x - 0.988f) / 0.0119f);
            if (x < -0.988f)
                return -0.988f + 0.0119f * std::tanh((x + 0.988f) / 0.0119f);
            return x;
        }
    }

    // =========================================================================
    // WindowsDeviceInputSource
    // =========================================================================
    WindowsDeviceInputSource::WindowsDeviceInputSource(const juce::String& deviceName, juce::AudioIODeviceType* deviceType)
        : devName(deviceName), devType(deviceType)
    {
        openDevice();
        MultiDeviceManager::getInstance().registerBridge(this);
    }

    WindowsDeviceInputSource::~WindowsDeviceInputSource()
    {
        MultiDeviceManager::getInstance().unregisterBridge(this);
        closeDevice();
    }

    bool WindowsDeviceInputSource::openDevice()
    {
        std::lock_guard<std::mutex> lock(deviceMutex);
        blockEngineAccess();

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

                    // Device (producer) rate -> engine (consumer) rate, drift-corrected.
                    fifo.prepare(deviceSampleRate, targetSampleRate, 2);
                    device->start(this);
                    streamRunning.store(device->isPlaying(), std::memory_order_seq_cst);
                    return device->isPlaying();
                }

                device.reset();
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
        std::lock_guard<std::mutex> lock(deviceMutex);
        blockEngineAccess();
        if (device != nullptr)
        {
            device->stop();
            device->close();
            device.reset();
        }
    }

    bool WindowsDeviceInputSource::reopenIfStopped()
    {
        const uint64_t callbacks = deviceCallbacks.load(std::memory_order_relaxed);
        const bool stalled = callbacks == lastSeenCallbacks;
        lastSeenCallbacks = callbacks;

        if (isStreaming() && !stalled)
            return true;

        closeDevice();
        const bool ok = openDevice();
        if (ok)
            reconnects.fetch_add(1, std::memory_order_relaxed);
        lastSeenCallbacks = deviceCallbacks.load(std::memory_order_relaxed);
        return ok;
    }

    void WindowsDeviceInputSource::prepare(double sampleRate, int /*maxBlockSize*/)
    {
        const double newRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        const bool rateChanged = std::abs(newRate - targetSampleRate) > 0.5;
        targetSampleRate = newRate;

        if (rateChanged || !isStreaming())
        {
            closeDevice();
            openDevice();
        }
    }

    void WindowsDeviceInputSource::releaseResources()
    {
        closeDevice();
    }

    void WindowsDeviceInputSource::readBlock(juce::AudioBuffer<float>& buffer,
                                            const juce::AudioBuffer<float>& /*deviceInput*/,
                                            int numSamples)
    {
        if (!beginEngineAccess())
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.clear(ch, 0, numSamples);
            return;
        }

        fifo.pull(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);
        endEngineAccess();
    }

    void WindowsDeviceInputSource::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/)
    {
    }

    void WindowsDeviceInputSource::audioDeviceStopped()
    {
        streamRunning.store(false, std::memory_order_seq_cst);
    }

    void WindowsDeviceInputSource::audioDeviceError(const juce::String& /*errorMessage*/)
    {
        // Device lost (unplugged / driver reset): go silent; recovery reopens it.
        streamRunning.store(false, std::memory_order_seq_cst);
    }

    void WindowsDeviceInputSource::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                                   int numInputChannels,
                                                                   float* const* /*outputChannelData*/,
                                                                   int /*numOutputChannels*/,
                                                                   int numSamples,
                                                                   const juce::AudioIODeviceCallbackContext& /*context*/)
    {
        deviceCallbacks.fetch_add(1, std::memory_order_relaxed);
        if (inputChannelData == nullptr || numInputChannels <= 0 || numSamples <= 0)
            return;

        juce::ScopedNoDenormals noDenormals;
        fifo.push(inputChannelData, numInputChannels, numSamples);
    }

    // =========================================================================
    // WindowsDeviceOutputSink
    // =========================================================================
    WindowsDeviceOutputSink::WindowsDeviceOutputSink(const juce::String& deviceName, juce::AudioIODeviceType* deviceType)
        : devName(deviceName), devType(deviceType)
    {
        openDevice();
        MultiDeviceManager::getInstance().registerBridge(this);
    }

    WindowsDeviceOutputSink::~WindowsDeviceOutputSink()
    {
        MultiDeviceManager::getInstance().unregisterBridge(this);
        closeDevice();
    }

    bool WindowsDeviceOutputSink::openDevice()
    {
        std::lock_guard<std::mutex> lock(deviceMutex);
        blockEngineAccess();

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

                    // Engine (producer) rate -> device (consumer) rate, drift-corrected.
                    fifo.prepare(targetSampleRate, deviceSampleRate, 2);
                    device->start(this);
                    streamRunning.store(device->isPlaying(), std::memory_order_seq_cst);
                    return device->isPlaying();
                }

                device.reset();
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
        std::lock_guard<std::mutex> lock(deviceMutex);
        blockEngineAccess();
        if (device != nullptr)
        {
            device->stop();
            device->close();
            device.reset();
        }
    }

    bool WindowsDeviceOutputSink::reopenIfStopped()
    {
        const uint64_t callbacks = deviceCallbacks.load(std::memory_order_relaxed);
        const bool stalled = callbacks == lastSeenCallbacks;
        lastSeenCallbacks = callbacks;

        if (isStreaming() && !stalled)
            return true;

        closeDevice();
        const bool ok = openDevice();
        if (ok)
            reconnects.fetch_add(1, std::memory_order_relaxed);
        lastSeenCallbacks = deviceCallbacks.load(std::memory_order_relaxed);
        return ok;
    }

    void WindowsDeviceOutputSink::prepare(double sampleRate, int /*maxBlockSize*/)
    {
        const double newRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        const bool rateChanged = std::abs(newRate - targetSampleRate) > 0.5;
        targetSampleRate = newRate;

        if (rateChanged || !isStreaming())
        {
            closeDevice();
            openDevice();
        }
    }

    void WindowsDeviceOutputSink::releaseResources()
    {
        closeDevice();
    }

    void WindowsDeviceOutputSink::writeBlock(const juce::AudioBuffer<float>& buffer, int numSamples)
    {
        if (numSamples <= 0 || !beginEngineAccess())
            return;

        fifo.push(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), numSamples);
        endEngineAccess();
    }

    void WindowsDeviceOutputSink::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/)
    {
    }

    void WindowsDeviceOutputSink::audioDeviceStopped()
    {
        streamRunning.store(false, std::memory_order_seq_cst);
    }

    void WindowsDeviceOutputSink::audioDeviceError(const juce::String& /*errorMessage*/)
    {
        streamRunning.store(false, std::memory_order_seq_cst);
    }

    void WindowsDeviceOutputSink::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                                  int /*numInputChannels*/,
                                                                  float* const* outputChannelData,
                                                                  int numOutputChannels,
                                                                  int numSamples,
                                                                  const juce::AudioIODeviceCallbackContext& /*context*/)
    {
        deviceCallbacks.fetch_add(1, std::memory_order_relaxed);
        if (outputChannelData == nullptr || numOutputChannels <= 0 || numSamples <= 0)
            return;

        juce::ScopedNoDenormals noDenormals;
        fifo.pull(outputChannelData, numOutputChannels, numSamples);

        // Output limiting protection on dedicated DAC output
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (float* p = outputChannelData[ch])
                for (int i = 0; i < numSamples; ++i)
                    p[i] = softClip(p[i]);
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
        {
            std::lock_guard<std::mutex> lock(mutex);
            juceManagerRef = &hostManager;
        }
        capturePreferredSetup();
        hostManager.addChangeListener(this);
        startTimer(2000);
    }

    void MultiDeviceManager::shutdown()
    {
        stopTimer();
        std::lock_guard<std::mutex> lock(mutex);
        if (juceManagerRef != nullptr)
            juceManagerRef->removeChangeListener(this);
        juceManagerRef = nullptr;
        havePreferredSetup = false;
    }

    void MultiDeviceManager::registerBridge(DeviceBridgeBase* bridge)
    {
        std::lock_guard<std::mutex> lock(bridgeMutex);
        if (bridge != nullptr && std::find(bridges.begin(), bridges.end(), bridge) == bridges.end())
            bridges.push_back(bridge);
    }

    void MultiDeviceManager::unregisterBridge(DeviceBridgeBase* bridge)
    {
        std::lock_guard<std::mutex> lock(bridgeMutex);
        bridges.erase(std::remove(bridges.begin(), bridges.end(), bridge), bridges.end());
    }

    std::vector<DeviceBridgeStatus> MultiDeviceManager::getBridgeStatus() const
    {
        std::lock_guard<std::mutex> lock(bridgeMutex);
        std::vector<DeviceBridgeStatus> out;
        out.reserve(bridges.size());
        for (const auto* b : bridges)
        {
            DeviceBridgeStatus st;
            st.deviceName = b->getBridgeDeviceName();
            st.isInput = dynamic_cast<const WindowsDeviceInputSource*>(b) != nullptr;
            st.streaming = b->isStreaming();
            st.driftPpm = b->getFifo().getCorrectionPpm();
            st.bufferedMs = b->getFifo().getAverageFillMs();
            st.underruns = b->getFifo().getUnderrunCount();
            st.reconnects = b->getReconnectCount();
            out.push_back(st);
        }
        return out;
    }

    void MultiDeviceManager::capturePreferredSetup()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (juceManagerRef == nullptr || juceManagerRef->getCurrentAudioDevice() == nullptr)
            return;

        preferredSetup = juceManagerRef->getAudioDeviceSetup();
        preferredTypeName = juceManagerRef->getCurrentAudioDeviceType();
        havePreferredSetup = true;
    }

    juce::String MultiDeviceManager::getPreferredPrimaryDeviceName() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!havePreferredSetup)
            return {};
        return preferredSetup.outputDeviceName.isNotEmpty() ? preferredSetup.outputDeviceName : preferredSetup.inputDeviceName;
    }

    bool MultiDeviceManager::isDeviceNamePresent(const juce::String& name) const
    {
        if (juceManagerRef == nullptr || name.isEmpty())
            return false;

        for (auto* type : juceManagerRef->getAvailableDeviceTypes())
        {
            if (type == nullptr || (preferredTypeName.isNotEmpty() && type->getTypeName() != preferredTypeName))
                continue;
            if (type->getDeviceNames(false).contains(name) || type->getDeviceNames(true).contains(name))
                return true;
        }
        return false;
    }

    void MultiDeviceManager::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
    {
        // A device change we did not cause: if the preferred device is still
        // present, the user chose a new device -> make it the preferred one.
        // If it vanished, this is JUCE's fallback -> keep the old preference.
        bool shouldCapture = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (juceManagerRef == nullptr || juceManagerRef->getCurrentAudioDevice() == nullptr || restoringPrimary)
                return;

            const juce::String preferredName = preferredSetup.outputDeviceName.isNotEmpty()
                                             ? preferredSetup.outputDeviceName : preferredSetup.inputDeviceName;
            shouldCapture = !havePreferredSetup || isDeviceNamePresent(preferredName);
        }

        if (shouldCapture)
            capturePreferredSetup();
    }

    void MultiDeviceManager::timerCallback()
    {
        runRecoveryPass();
    }

    void MultiDeviceManager::runRecoveryPass()
    {
        // 1. Secondary device bridges: reopen any that stopped or stalled.
        {
            std::lock_guard<std::mutex> lock(bridgeMutex);
            for (auto* b : bridges)
                if (b != nullptr)
                    b->reopenIfStopped();
        }

        // 2. Primary device: return to the user's device once it is back.
        std::lock_guard<std::mutex> lock(mutex);
        if (juceManagerRef == nullptr || !havePreferredSetup)
            return;

        const juce::String preferredName = preferredSetup.outputDeviceName.isNotEmpty()
                                         ? preferredSetup.outputDeviceName : preferredSetup.inputDeviceName;
        auto* current = juceManagerRef->getCurrentAudioDevice();
        const juce::String currentName = (current != nullptr) ? current->getName() : juce::String();

        if (currentName == preferredName || !isDeviceNamePresent(preferredName))
            return;

        restoringPrimary = true;
        if (preferredTypeName.isNotEmpty() && juceManagerRef->getCurrentAudioDeviceType() != preferredTypeName)
            juceManagerRef->setCurrentAudioDeviceType(preferredTypeName, false);
        const auto err = juceManagerRef->setAudioDeviceSetup(preferredSetup, true);
        restoringPrimary = false;

        if (err.isEmpty() && juceManagerRef->getCurrentAudioDevice() != nullptr)
            primaryRecoveries.fetch_add(1, std::memory_order_relaxed);
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
