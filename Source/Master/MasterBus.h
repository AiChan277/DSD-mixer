#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "DSP/GainProcessor.h"
#include "DSP/MeterProcessor.h"
#include <atomic>
#include <string>

namespace dsd
{
    class MasterBus
    {
    public:
        MasterBus();
        ~MasterBus() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples);

        void setFaderDb(float db) noexcept;
        float getFaderDb() const noexcept { return faderDb.load(std::memory_order_relaxed); }

        void setMute(bool isMuted) noexcept { mute.store(isMuted, std::memory_order_relaxed); }
        bool getMute() const noexcept { return mute.load(std::memory_order_relaxed); }

        void setMonitor(bool isMonitor) noexcept { monitor.store(isMonitor, std::memory_order_relaxed); }
        bool getMonitor() const noexcept { return monitor.load(std::memory_order_relaxed); }

        MeterValues& getMeterValues() noexcept { return meterValues; }
        const MeterValues& getMeterValues() const noexcept { return meterValues; }

        std::string getDeviceOutName() const { return deviceOutName; }
        void setDeviceOutName(const std::string& name) { deviceOutName = name; }

    private:
        std::string deviceOutName{"MAIN OUT: 1/2"};

        std::atomic<float> faderDb{0.0f};
        std::atomic<bool> mute{false};
        std::atomic<bool> monitor{true};

        GainProcessor faderProcessor;
        MeterProcessor meterProcessor;
        MeterValues meterValues;
    };
} // namespace dsd
