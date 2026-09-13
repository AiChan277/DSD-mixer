#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Audio/AudioInputSource.h"
#include "DSP/GainProcessor.h"
#include "DSP/PanProcessor.h"
#include "DSP/MeterProcessor.h"
#include "Plugin/PluginRack.h"
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

        void setPhaseInvert(bool invert) noexcept { phaseInvert.store(invert, std::memory_order_relaxed); }
        bool getPhaseInvert() const noexcept { return phaseInvert.load(std::memory_order_relaxed); }

        void setForceMono(bool mono) noexcept { forceMono.store(mono, std::memory_order_relaxed); }
        bool getForceMono() const noexcept { return forceMono.load(std::memory_order_relaxed); }

        std::string getInputDeviceName() const { return inputDeviceName; }
        void setInputDeviceName(const std::string& name) { inputDeviceName = name; }
        int getInputChannelIndex() const noexcept { return inputChannelIndex; }
        void setInputChannelIndex(int idx) { inputChannelIndex = idx; }

        void setInputSource(std::unique_ptr<AudioInputSource> source);
        AudioInputSource* getInputSource() const noexcept { return inputSource.get(); }

        PluginRack& getPluginRack() noexcept { return pluginRack; }
        const PluginRack& getPluginRack() const noexcept { return pluginRack; }

        // Stage-by-Stage Metering (A: Input, B: Post-Gain, C: Post-VST, D: Post-Fader)
        MeterValues& getMeterInput() noexcept { return meterInputValues; }
        const MeterValues& getMeterInput() const noexcept { return meterInputValues; }

        MeterValues& getMeterPostGain() noexcept { return meterPostGainValues; }
        const MeterValues& getMeterPostGain() const noexcept { return meterPostGainValues; }

        MeterValues& getMeterPostVST() noexcept { return meterPostVSTValues; }
        const MeterValues& getMeterPostVST() const noexcept { return meterPostVSTValues; }

        MeterValues& getMeterValues() noexcept { return meterValues; }
        const MeterValues& getMeterValues() const noexcept { return meterValues; }

        const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return channelBuffer; }

    private:
        ChannelID channelID;
        std::string name;
        std::string inputDeviceName{"None"};
        int inputChannelIndex{-1};

        std::atomic<float> gainDb{0.0f};
        std::atomic<float> faderDb{0.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<bool> mute{false};
        std::atomic<bool> solo{false};
        std::atomic<bool> directMonitor{false};
        std::atomic<bool> disableOutput{false};
        std::atomic<bool> phaseInvert{false};
        std::atomic<bool> forceMono{false};

        GainProcessor gainProcessor;
        GainProcessor faderProcessor;
        PanProcessor panProcessor;

        // Stage Processors
        MeterProcessor meterInputProc;
        MeterProcessor meterPostGainProc;
        MeterProcessor meterPostVSTProc;
        MeterProcessor meterProcessor; // Post-Fader

        MeterValues meterInputValues;
        MeterValues meterPostGainValues;
        MeterValues meterPostVSTValues;
        MeterValues meterValues; // Post-Fader

        PluginRack pluginRack;

        std::unique_ptr<AudioInputSource> inputSource;
        mutable std::mutex sourceMutex;
        juce::AudioBuffer<float> channelBuffer;
        juce::MidiBuffer midiBuffer;

        double currentSampleRate{48000.0};
        int currentBlockSize{128};
        bool isPrepared{false};
    };
} // namespace dsd
