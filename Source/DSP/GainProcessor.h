#pragma once

#include <cmath>
#include <algorithm>

namespace dsd
{
    class GainProcessor
    {
    public:
        GainProcessor() = default;

        static inline float dbToLinear(float db) noexcept
        {
            if (db <= -60.0f)
                return 0.0f;
            return std::pow(10.0f, db / 20.0f);
        }

        static inline float linearToDb(float linear) noexcept
        {
            if (linear <= 0.000001f)
                return -60.0f;
            return 20.0f * std::log10(linear);
        }

        void prepare(double sampleRate) noexcept
        {
            // Smoothing filter coefficient for ~15ms transition
            const float timeConstantSec = 0.015f;
            smoothingAlpha = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * timeConstantSec));
        }

        void setTargetGainLinear(float linearGain) noexcept
        {
            targetGain = linearGain;
        }

        void setTargetGainDb(float gainDb) noexcept
        {
            targetGain = dbToLinear(gainDb);
        }

        void reset(float initialGain = 1.0f) noexcept
        {
            currentGain = initialGain;
            targetGain = initialGain;
        }

        inline void processBlock(float* const* channelData, int numChannels, int numSamples) noexcept
        {
            // If gain is essentially stationary and zero or one, optimize
            if (std::abs(currentGain - targetGain) < 0.0001f)
            {
                currentGain = targetGain;
                if (currentGain <= 0.0f)
                {
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        if (channelData[ch] != nullptr)
                            std::fill_n(channelData[ch], numSamples, 0.0f);
                    }
                    return;
                }
                if (std::abs(currentGain - 1.0f) < 0.0001f)
                {
                    return; // unity gain, no-op
                }

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (channelData[ch] != nullptr)
                    {
                        for (int i = 0; i < numSamples; ++i)
                            channelData[ch][i] *= currentGain;
                    }
                }
                return;
            }

            // Smoothly interpolate sample-by-sample
            for (int i = 0; i < numSamples; ++i)
            {
                currentGain += smoothingAlpha * (targetGain - currentGain);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (channelData[ch] != nullptr)
                        channelData[ch][i] *= currentGain;
                }
            }
        }

    private:
        float currentGain{1.0f};
        float targetGain{1.0f};
        float smoothingAlpha{0.05f};
    };
} // namespace dsd
