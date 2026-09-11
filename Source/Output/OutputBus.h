#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "DSP/GainProcessor.h"
#include "DSP/MeterProcessor.h"
#include <atomic>
#include <string>

namespace dsd
{
    class OutputBus
    {
    public:
        OutputBus(BusID id, const std::string& initialName, int targetDeviceChOffset = -1);
        ~OutputBus() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processBlock(int numSamples);

        BusID getBusID() const noexcept { return busID; }

        std::string getName() const { return name; }
        void setName(const std::string& newName) { name = newName; }

        std::string getOutputDeviceName() const { return outputDeviceName; }
        void setOutputDeviceName(const std::string& name) { outputDeviceName = name; }

        int getDeviceChannelOffset() const noexcept { return deviceChannelOffset.load(std::memory_order_relaxed); }
        void setDeviceChannelOffset(int offset) noexcept { deviceChannelOffset.store(offset, std::memory_order_relaxed); }

        void setFaderDb(float db) noexcept;
        float getFaderDb() const noexcept { return faderDb.load(std::memory_order_relaxed); }

        void setMute(bool isMuted) noexcept { mute.store(isMuted, std::memory_order_relaxed); }
        bool getMute() const noexcept { return mute.load(std::memory_order_relaxed); }

        void setMonitor(bool isMon) noexcept { monitor.store(isMon, std::memory_order_relaxed); }
        bool getMonitor() const noexcept { return monitor.load(std::memory_order_relaxed); }

        MeterValues& getMeterValues() noexcept { return meterValues; }
        const MeterValues& getMeterValues() const noexcept { return meterValues; }

        juce::AudioBuffer<float>& getBuffer() noexcept { return busBuffer; }
        const juce::AudioBuffer<float>& getBuffer() const noexcept { return busBuffer; }

    private:
        BusID busID;
        std::string name;
        std::string outputDeviceName{"None"};

        std::atomic<int> deviceChannelOffset{-1}; // e.g. 0 for Ch 1/2, 2 for Ch 3/4, -1 for null/unassigned
        std::atomic<float> faderDb{0.0f};
        std::atomic<bool> mute{false};
        std::atomic<bool> monitor{true};

        GainProcessor faderProcessor;
        MeterProcessor meterProcessor;
        MeterValues meterValues;

        juce::AudioBuffer<float> busBuffer;
    };
} // namespace dsd
