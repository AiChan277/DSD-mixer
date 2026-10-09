#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Audio/AudioInputSource.h"
#include "DSP/GainProcessor.h"
#include "DSP/PanProcessor.h"
#include "DSP/MeterProcessor.h"
#include "DSP/BiquadFilter.h"
#include "Plugin/PluginRack.h"
#include <memory>
#include <string>
#include <atomic>

namespace dsd
{
    enum class MeterStage
    {
        PreGain = 0,   // Stage A: Raw input before gain/phase/mono
        PostGain,      // Stage B: Post input-gain, pre-VST
        PostVST,       // Stage C: Post-VST rack
        PostFader      // Stage D: Post-fader & mute
    };

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

        // DM and PFL are mutually exclusive — setting one clears the other
        void setDirectMonitor(bool dm) noexcept;
        bool getDirectMonitor() const noexcept { return directMonitor.load(std::memory_order_relaxed); }

        void setPfl(bool enable) noexcept;
        bool getPfl() const noexcept { return pfl.load(std::memory_order_relaxed); }

        void setHpfEnabled(bool enable) noexcept;
        bool getHpfEnabled() const noexcept { return hpfEnabled.load(std::memory_order_relaxed); }

        void setPadEnabled(bool enable) noexcept { padEnabled.store(enable, std::memory_order_relaxed); }
        bool getPadEnabled() const noexcept { return padEnabled.load(std::memory_order_relaxed); }

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

        // Stage-by-Stage Metering
        // Stage A: Raw Input Pre-Gain (Mandatory for channel input meter)
        MeterValues& getMeterPreGain() noexcept { return meterInputValues; }
        const MeterValues& getMeterPreGain() const noexcept { return meterInputValues; }

        // Stage B: Post-Gain / Pre-VST
        MeterValues& getMeterPostGain() noexcept { return meterPostGainValues; }
        const MeterValues& getMeterPostGain() const noexcept { return meterPostGainValues; }

        // Stage C: Post-VST
        MeterValues& getMeterPostVST() noexcept { return meterPostVSTValues; }
        const MeterValues& getMeterPostVST() const noexcept { return meterPostVSTValues; }

        // Stage D: Post-Fader
        MeterValues& getMeterPostFader() noexcept { return meterValues; }
        const MeterValues& getMeterPostFader() const noexcept { return meterValues; }

        // Legacy / Generic aliases
        MeterValues& getMeterInput() noexcept { return meterInputValues; }
        const MeterValues& getMeterInput() const noexcept { return meterInputValues; }

        MeterValues& getMeterValues() noexcept { return meterValues; }
        const MeterValues& getMeterValues() const noexcept { return meterValues; }

        const MeterValues& getMeter(MeterStage stage) const noexcept;
        MeterValues& getMeter(MeterStage stage) noexcept;

        // Buffers
        const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return channelBuffer; }
        const juce::AudioBuffer<float>& getPflBuffer() const noexcept { return pflBuffer; }   // Post-VST, pre-fader
        const juce::AudioBuffer<float>& getDmBuffer() const noexcept { return dmBuffer; }     // Post-fader

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
        std::atomic<bool> pfl{false};
        std::atomic<bool> disableOutput{false};
        std::atomic<bool> phaseInvert{false};
        std::atomic<bool> forceMono{false};
        std::atomic<bool> hpfEnabled{false};
        std::atomic<bool> padEnabled{false};

        GainProcessor gainProcessor;
        GainProcessor faderProcessor;
        PanProcessor panProcessor;
        BiquadFilter hpfFilter;

        // Stage Processors
        MeterProcessor meterInputProc;    // Stage A: Raw Input
        MeterProcessor meterPostGainProc; // Stage B: Post-Gain
        MeterProcessor meterPostVSTProc;  // Stage C: Post-VST
        MeterProcessor meterProcessor;    // Stage D: Post-Fader

        MeterValues meterInputValues;
        MeterValues meterPostGainValues;
        MeterValues meterPostVSTValues;
        MeterValues meterValues; // Post-Fader

        PluginRack pluginRack;

        std::unique_ptr<AudioInputSource> inputSource;
        mutable std::mutex sourceMutex;
        juce::AudioBuffer<float> channelBuffer;
        juce::AudioBuffer<float> pflBuffer; // PFL tap: post-VST, pre-fader
        juce::AudioBuffer<float> dmBuffer;  // DM tap: post-fader
        juce::MidiBuffer midiBuffer;

        double currentSampleRate{48000.0};
        int currentBlockSize{128};
        bool isPrepared{false};
    };
} // namespace dsd
