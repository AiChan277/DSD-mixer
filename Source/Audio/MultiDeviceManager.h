#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Audio/AudioInputSource.h"
#include "Audio/AdaptiveResamplingFifo.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

namespace dsd
{
    // Common base for a secondary WASAPI device bridged into the engine clock.
    //
    // The device runs on its own clock; an AdaptiveResamplingFifo converts
    // between device and engine rates and absorbs the drift between them.
    // The engine-side call (readBlock / writeBlock) only touches the FIFO, and
    // only while `streamRunning` is set, so the device can be reopened at any
    // time from the control thread (hot-plug recovery) without racing it.
    class DeviceBridgeBase
    {
    public:
        virtual ~DeviceBridgeBase() = default;

        // Control thread: try to (re)open the device if it is not streaming.
        // Returns true if the device is streaming afterwards.
        virtual bool reopenIfStopped() = 0;
        virtual bool isStreaming() const noexcept = 0;
        virtual juce::String getBridgeDeviceName() const = 0;

        const AdaptiveResamplingFifo& getFifo() const noexcept { return fifo; }
        uint64_t getReconnectCount() const noexcept { return reconnects.load(std::memory_order_relaxed); }

    protected:
        // Engine-side guard: returns false if the stream is down (caller outputs silence).
        bool beginEngineAccess() noexcept
        {
            engineSideActive.store(true, std::memory_order_seq_cst);
            if (streamRunning.load(std::memory_order_seq_cst))
                return true;
            engineSideActive.store(false, std::memory_order_release);
            return false;
        }
        void endEngineAccess() noexcept { engineSideActive.store(false, std::memory_order_release); }

        // Control side: stop engine access and wait until it has left the FIFO.
        void blockEngineAccess() noexcept
        {
            streamRunning.store(false, std::memory_order_seq_cst);
            while (engineSideActive.load(std::memory_order_seq_cst))
                std::this_thread::yield();
        }

        AdaptiveResamplingFifo fifo;
        std::atomic<bool> streamRunning{false};
        std::atomic<uint64_t> deviceCallbacks{0}; // device thread heartbeat
        uint64_t lastSeenCallbacks{0};            // control thread: stall detection
        std::atomic<bool> engineSideActive{false};
        std::atomic<uint64_t> reconnects{0};
    };

    // Dedicated input source capturing from a specific Windows audio device
    class WindowsDeviceInputSource : public AudioInputSource,
                                     public juce::AudioIODeviceCallback,
                                     public DeviceBridgeBase
    {
    public:
        WindowsDeviceInputSource(const juce::String& deviceName, juce::AudioIODeviceType* deviceType);
        ~WindowsDeviceInputSource() override;

        void prepare(double sampleRate, int maxBlockSize) override;
        void releaseResources() override;
        void readBlock(juce::AudioBuffer<float>& buffer,
                       const juce::AudioBuffer<float>& deviceInput,
                       int numSamples) override;

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceError(const juce::String& errorMessage) override;
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override;

        bool reopenIfStopped() override;
        bool isStreaming() const noexcept override { return streamRunning.load(std::memory_order_relaxed); }
        juce::String getBridgeDeviceName() const override { return devName; }

        bool isDeviceActive() const noexcept { return isStreaming(); }
        juce::String getDeviceName() const noexcept { return devName; }
        double getDeviceSampleRate() const noexcept { return deviceSampleRate; }

    private:
        juce::String devName;
        juce::AudioIODeviceType* devType{nullptr};
        std::unique_ptr<juce::AudioIODevice> device;
        std::mutex deviceMutex; // control-thread open/close only

        double targetSampleRate{48000.0};
        double deviceSampleRate{48000.0};

        bool openDevice();
        void closeDevice();
    };

    // Dedicated output sink rendering to a specific Windows audio device
    class WindowsDeviceOutputSink : public juce::AudioIODeviceCallback,
                                    public DeviceBridgeBase
    {
    public:
        WindowsDeviceOutputSink(const juce::String& deviceName, juce::AudioIODeviceType* deviceType);
        ~WindowsDeviceOutputSink() override;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();
        void writeBlock(const juce::AudioBuffer<float>& buffer, int numSamples);

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceError(const juce::String& errorMessage) override;
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override;

        bool reopenIfStopped() override;
        bool isStreaming() const noexcept override { return streamRunning.load(std::memory_order_relaxed); }
        juce::String getBridgeDeviceName() const override { return devName; }

        bool isDeviceActive() const noexcept { return isStreaming(); }
        juce::String getDeviceName() const noexcept { return devName; }
        double getDeviceSampleRate() const noexcept { return deviceSampleRate; }

    private:
        juce::String devName;
        juce::AudioIODeviceType* devType{nullptr};
        std::unique_ptr<juce::AudioIODevice> device;
        std::mutex deviceMutex; // control-thread open/close only

        double targetSampleRate{48000.0};
        double deviceSampleRate{48000.0};

        bool openDevice();
        void closeDevice();
    };

    // Snapshot of one bridge for diagnostics UI.
    struct DeviceBridgeStatus
    {
        juce::String deviceName;
        bool isInput{true};
        bool streaming{false};
        double driftPpm{0.0};
        double bufferedMs{0.0};
        uint64_t underruns{0};
        uint64_t reconnects{0};
    };

    // Central manager for enumerating, routing, and sharing Windows audio devices.
    // Also runs device recovery: every 2 s (message thread) it reopens secondary
    // devices that stopped and moves the primary device back to the user's
    // chosen device once it reappears after a fallback.
    class MultiDeviceManager : private juce::ChangeListener
    {
    public:
        static MultiDeviceManager& getInstance();

        void initialize(juce::AudioDeviceManager& hostManager);
        void shutdown();

        juce::StringArray getAvailableInputDevices();
        juce::StringArray getAvailableOutputDevices();

        juce::String getPrimaryInputDeviceName() const;
        juce::String getPrimaryOutputDeviceName() const;

        juce::AudioIODeviceType* getWasapiDeviceType();

        // Factory to create an input source for a channel
        std::unique_ptr<AudioInputSource> createInputSourceFor(const juce::String& selectionName);

        // Factory to create or get an output sink for an output bus
        std::unique_ptr<WindowsDeviceOutputSink> createOutputSinkFor(const juce::String& deviceName);

        // Bridges register themselves for recovery and diagnostics.
        void registerBridge(DeviceBridgeBase* bridge);
        void unregisterBridge(DeviceBridgeBase* bridge);
        std::vector<DeviceBridgeStatus> getBridgeStatus() const;

        // One recovery pass (normally driven by the internal timer).
        void runRecoveryPass();

        juce::String getPreferredPrimaryDeviceName() const;
        uint64_t getPrimaryRecoveryCount() const noexcept { return primaryRecoveries.load(std::memory_order_relaxed); }

    private:
        MultiDeviceManager() = default;
        ~MultiDeviceManager() override = default;

        // Owned timer (created in initialize, destroyed in shutdown). The
        // singleton itself must not be a juce::Timer: a Timer pins JUCE's
        // shared timer thread, and a static one would tear it down after
        // JUCE has shut down (crash on exit).
        struct RecoveryTimer : public juce::Timer
        {
            explicit RecoveryTimer(MultiDeviceManager& o) : owner(o) {}
            ~RecoveryTimer() override { stopTimer(); }
            void timerCallback() override { owner.runRecoveryPass(); }
            MultiDeviceManager& owner;
        };
        std::unique_ptr<RecoveryTimer> recoveryTimer;

        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        bool isDeviceNamePresent(const juce::String& name) const;
        void capturePreferredSetup();

        juce::AudioDeviceManager* juceManagerRef{nullptr};
        mutable std::mutex mutex;

        mutable std::mutex bridgeMutex;
        std::vector<DeviceBridgeBase*> bridges;

        bool havePreferredSetup{false};
        juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
        juce::String preferredTypeName;
        bool restoringPrimary{false};
        std::atomic<uint64_t> primaryRecoveries{0};
    };
} // namespace dsd
