#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "DSP/GainProcessor.h"
#include "DSP/MeterProcessor.h"
#include "Plugin/PluginRack.h"
#include <atomic>
#include <string>

namespace dsd
{
    class AudioBus
    {
    public:
        AudioBus(BusID id, const std::string& busName);
        ~AudioBus() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processBlock(int numSamples);

        BusID getBusID() const noexcept { return busID; }
        std::string getName() const { return name; }
        void setName(const std::string& newName) { name = newName; }

        void setFaderDb(float db) noexcept;
        float getFaderDb() const noexcept { return faderDb.load(std::memory_order_relaxed); }

        void setMute(bool isMuted) noexcept { mute.store(isMuted, std::memory_order_relaxed); }
        bool getMute() const noexcept { return mute.load(std::memory_order_relaxed); }

        void setSolo(bool isSolo) noexcept { solo.store(isSolo, std::memory_order_relaxed); }
        bool getSolo() const noexcept { return solo.load(std::memory_order_relaxed); }

        PluginRack& getPluginRack() noexcept { return pluginRack; }
        const PluginRack& getPluginRack() const noexcept { return pluginRack; }

        MeterValues& getMeterValues() noexcept { return meterValues; }
        const MeterValues& getMeterValues() const noexcept { return meterValues; }

        juce::AudioBuffer<float>& getBuffer() noexcept { return busBuffer; }
        const juce::AudioBuffer<float>& getBuffer() const noexcept { return busBuffer; }

    private:
        BusID busID;
        std::string name;

        std::atomic<float> faderDb{0.0f};
        std::atomic<bool> mute{false};
        std::atomic<bool> solo{false};

        GainProcessor faderProcessor;
        MeterProcessor meterProcessor;
        MeterValues meterValues;
        PluginRack pluginRack;

        juce::AudioBuffer<float> busBuffer;
        juce::MidiBuffer midiBuffer;
    };
} // namespace dsd
