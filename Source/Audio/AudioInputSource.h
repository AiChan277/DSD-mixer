#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace dsd
{
    class AudioInputSource
    {
    public:
        virtual ~AudioInputSource() = default;

        virtual void prepare(double sampleRate, int maxBlockSize) = 0;
        virtual void releaseResources() = 0;

        // Reads input audio into the target destination buffer
        virtual void readBlock(juce::AudioBuffer<float>& targetBuffer,
                               const juce::AudioBuffer<float>& deviceInputBuffer,
                               int numSamples) = 0;
    };

    // Hardware Input Source: maps specified device input channels to channel buffer
    class HardwareInputSource : public AudioInputSource
    {
    public:
        HardwareInputSource(int deviceLeftChannelIndex, int deviceRightChannelIndex)
            : inputChL(deviceLeftChannelIndex), inputChR(deviceRightChannelIndex)
        {
        }

        void prepare(double /*sampleRate*/, int /*maxBlockSize*/) override {}
        void releaseResources() override {}

        void setDeviceChannels(int left, int right) noexcept
        {
            inputChL = left;
            inputChR = right;
        }

        void readBlock(juce::AudioBuffer<float>& targetBuffer,
                               const juce::AudioBuffer<float>& deviceInputBuffer,
                               int numSamples) override
        {
            const int totalDevIn = deviceInputBuffer.getNumChannels();

            // Channel 0 (Left)
            if (inputChL >= 0 && inputChL < totalDevIn)
            {
                targetBuffer.copyFrom(0, 0, deviceInputBuffer, inputChL, 0, numSamples);
            }
            else
            {
                targetBuffer.clear(0, 0, numSamples);
            }

            // Channel 1 (Right)
            if (inputChR >= 0 && inputChR < totalDevIn)
            {
                targetBuffer.copyFrom(1, 0, deviceInputBuffer, inputChR, 0, numSamples);
            }
            else if (inputChL >= 0 && inputChL < totalDevIn)
            {
                // If mono input specified, mirror Left to Right internally
                targetBuffer.copyFrom(1, 0, deviceInputBuffer, inputChL, 0, numSamples);
            }
            else
            {
                targetBuffer.clear(1, 0, numSamples);
            }
        }

    private:
        int inputChL{0};
        int inputChR{1};
    };

    // Sine Wave Generator Source for offline/internal testing and tone calibration
    class SineGeneratorSource : public AudioInputSource
    {
    public:
        SineGeneratorSource(float frequency = 1000.0f, float amplitude = 0.25f)
            : freq(frequency), amp(amplitude)
        {
        }

        void prepare(double sampleRate, int /*maxBlockSize*/) override
        {
            currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
            phaseIncrement = static_cast<float>(2.0 * 3.14159265358979323846 * freq / currentSampleRate);
            phase = 0.0f;
        }

        void releaseResources() override {}

        void setEnabled(bool enabled) noexcept { isEnabled = enabled; }
        bool getEnabled() const noexcept { return isEnabled; }

        void readBlock(juce::AudioBuffer<float>& targetBuffer,
                               const juce::AudioBuffer<float>& /*deviceInputBuffer*/,
                               int numSamples) override
        {
            if (!isEnabled)
            {
                targetBuffer.clear(0, numSamples);
                return;
            }

            float* l = targetBuffer.getWritePointer(0);
            float* r = targetBuffer.getWritePointer(1);

            for (int i = 0; i < numSamples; ++i)
            {
                const float val = amp * std::sin(phase);
                phase += phaseIncrement;
                if (phase >= 6.28318530718f)
                    phase -= 6.28318530718f;

                l[i] = val;
                r[i] = val;
            }
        }

    private:
        float freq{1000.0f};
        float amp{0.25f};
        double currentSampleRate{48000.0};
        float phase{0.0f};
        float phaseIncrement{0.0f};
        bool isEnabled{false};
    };

    class NullInputSource : public AudioInputSource
    {
    public:
        NullInputSource() = default;
        ~NullInputSource() override = default;

        void prepare(double /*sampleRate*/, int /*maxBlockSize*/) override {}
        void releaseResources() override {}

        void readBlock(juce::AudioBuffer<float>& buffer,
                       const juce::AudioBuffer<float>& /*deviceInput*/,
                       int numSamples) override
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.clear(ch, 0, numSamples);
        }
    };
} // namespace dsd
