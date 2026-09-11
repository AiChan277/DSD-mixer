#pragma once

#include "Audio/AudioTypes.h"
#include <algorithm>
#include <cmath>

namespace dsd
{
    class MeterProcessor
    {
    public:
        MeterProcessor() = default;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void processBlock(const float* left, const float* right, int numSamples, MeterValues& outValues) noexcept;

    private:
        double currentSampleRate{48000.0};
        
        // Decay parameters
        float holdSamplesMax{72000.0f}; // 1.5 seconds @ 48kHz
        float holdSamplesRemainingL{0.0f};
        float holdSamplesRemainingR{0.0f};

        float peakHoldL{0.0f};
        float peakHoldR{0.0f};

        float decayPerSample{0.00005f}; // Linear decay after hold
        int warmupSamplesRemaining{0};  // Startup transient suppression
    };
} // namespace dsd
