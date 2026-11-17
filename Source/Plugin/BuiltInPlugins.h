#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

namespace dsd
{
    // Zero-latency built-in channel processors that live in the normal plugin
    // rack (bypass crossfade, editor window, session save/restore all apply).
    // They are identified by pluginFormatName == kFormatName and an identifier
    // in fileOrIdentifier, so PluginManager::loadPlugin() can recreate them.
    namespace BuiltInPlugins
    {
        inline constexpr const char* kFormatName = "DSD Built-in";
        inline constexpr const char* kGateId = "dsd.builtin.gate";
        inline constexpr const char* kEqId = "dsd.builtin.eq";
        inline constexpr const char* kCompressorId = "dsd.builtin.compressor";

        juce::Array<juce::PluginDescription> getDescriptions();
        bool isBuiltIn(const juce::PluginDescription& desc);
        std::unique_ptr<juce::AudioPluginInstance> create(const juce::String& identifier);
    }

    // Shared plumbing: stereo in/out, parameters in an AudioProcessorValueTreeState,
    // generic parameter editor, XML state.
    class BuiltInProcessorBase : public juce::AudioPluginInstance
    {
    public:
        BuiltInProcessorBase(const juce::String& name, const juce::String& identifier,
                             juce::AudioProcessorValueTreeState::ParameterLayout layout);

        const juce::String getName() const override { return processorName; }
        void fillInPluginDescription(juce::PluginDescription& desc) const override;

        void releaseResources() override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return "Default"; }
        void changeProgramName(int, const juce::String&) override {}

        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

        juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

        // Convenience for tests and programmatic setup (value in real units).
        void setParameterValue(const juce::String& paramId, float value);
        float getParameterValue(const juce::String& paramId) const;

    protected:
        float param(const juce::String& id) const noexcept;

        juce::AudioProcessorValueTreeState parameters;

    private:
        juce::String processorName;
        juce::String processorIdentifier;
    };

    // ---------------------------------------------------------------- Gate
    class BuiltInGate : public BuiltInProcessorBase
    {
    public:
        BuiltInGate();
        void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

        bool isOpen() const noexcept { return open; }
        float getCurrentGainDb() const noexcept { return currentGainDb.load(std::memory_order_relaxed); }

    private:
        double sr{48000.0};
        float envelope{0.0f};
        float gain{0.0f};
        bool open{false};
        int holdRemaining{0};
        std::atomic<float> currentGainDb{-80.0f};
    };

    // ---------------------------------------------------------------- EQ
    class BuiltInEq : public BuiltInProcessorBase
    {
    public:
        BuiltInEq();
        void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

        struct Biquad
        {
            double b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
            double z1[2]{0, 0}, z2[2]{0, 0};
            bool active{false};

            inline float process(float x, int ch) noexcept
            {
                const double in = x;
                const double out = b0 * in + z1[ch];
                z1[ch] = b1 * in - a1 * out + z2[ch];
                z2[ch] = b2 * in - a2 * out;
                return static_cast<float>(out);
            }
        };

    private:
        static constexpr int kNumBands = 5; // HPF, low shelf, mid 1, mid 2, high shelf
        void updateCoefficients();

        double sr{48000.0};
        Biquad bands[kNumBands];
        float lastParams[16]{};
        bool coefficientsValid{false};
    };

    // ---------------------------------------------------------------- Compressor
    class BuiltInCompressor : public BuiltInProcessorBase
    {
    public:
        BuiltInCompressor();
        void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

        float getGainReductionDb() const noexcept { return gainReductionDb.load(std::memory_order_relaxed); }

        // Static gain curve (dB in -> dB of gain change), soft knee. Public for tests.
        static float computeGainDb(float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept;

    private:
        double sr{48000.0};
        float smoothedGrDb{0.0f};
        std::atomic<float> gainReductionDb{0.0f};
    };
} // namespace dsd
