#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Audio/AudioInputSource.h"
#include "DSP/GainProcessor.h"
#include "DSP/PanProcessor.h"
#include "DSP/MeterProcessor.h"
#include <memory>
#include <string>
#include <atomic>

namespace dsd
{
    class AudioChannel
    {
    public:
        AudioChannel(ChannelID id, const std::string& channelName);
        ~AudioChannel() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processBlock(const juce::AudioBuffer<float>& deviceInputBuffer, int numSamples);

        // Channel Properties
        ChannelID getChannelID() const noexcept { return channelID; }
        std::string getName() const { return name; }
        void setName(const std::string& newName) { name = newName; }

        // Parameter Setters (Lock-free atomic writes from UI)
        void setGainDb(float db) noexcept;
        float getGainDb() const noexcept { return gainDb.load(std::memory_order_relaxed); }

        void setFaderDb(float db) noexcept;
        float getFaderDb() const noexcept { return faderDb.load(std::memory_order_relaxed); }

        void setPan(float panVal) noexcept;
        float getPan() const noexcept { return pan.load(std::memory_order_relaxed); }

        void setMute(bool isMuted) noexcept { mute.store(isMuted, std::memory_order_relaxed); }
        bool getMute() const noexcept { return mute.load(std::memory_order_relaxed); }

        void setSolo(bool isSolo) noexcept { solo.store(isSolo, std::memory_order_relaxed); }
        bool getSolo() const noexcept { return solo.load(std::memory_order_relaxed); }

        void setDirectMonitor(bool dm) noexcept { directMonitor.store(dm, std::memory_order_relaxed); }
        bool getDirectMonitor() const noexcept { return directMonitor.load(std::memory_order_relaxed); }

        void setDisableOutput(bool dis) noexcept { disableOutput.store(dis, std::memory_order_relaxed); }
        bool getDisableOutput() const noexcept { return disableOutput.load(std::memory_order_relaxed); }

        void setInputSource(std::unique_ptr<AudioInputSource> source);
        AudioInputSource* getInputSource() const noexcept { return inputSource.get(); }

        // Meter access
        MeterValues& getMeterValues() noexcept { return meterValues; }
        const MeterValues& getMeterValues() const noexcept { return meterValues; }

        // Audio Buffer access for Routing Engine
        const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return channelBuffer; }

    private:
        ChannelID channelID;
        std::string name;

        std::atomic<float> gainDb{0.0f};
        std::atomic<float> faderDb{0.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<bool> mute{false};
        std::atomic<bool> solo{false};
        std::atomic<bool> directMonitor{false};
        std::atomic<bool> disableOutput{false};

        // DSP Processors
        GainProcessor gainProcessor;
        GainProcessor faderProcessor;
        PanProcessor panProcessor;
        MeterProcessor meterProcessor;

        // Input abstraction
        std::unique_ptr<AudioInputSource> inputSource;

        // Channel buffer (Stereo, preallocated)
        juce::AudioBuffer<float> channelBuffer;

        // Meter atomic values
        MeterValues meterValues;
    };
} // namespace dsd
