#include "DSP/MeterProcessor.h"

namespace dsd
{
    void MeterProcessor::prepare(double sampleRate) noexcept
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        holdSamplesMax = static_cast<float>(currentSampleRate * 1.5); // 1.5s hold
        // Decay rate roughly 30 dB per second
        decayPerSample = static_cast<float>(1.0 / (currentSampleRate * 1.0));
        reset();
    }

    void MeterProcessor::reset() noexcept
    {
        holdSamplesRemainingL = 0.0f;
        holdSamplesRemainingR = 0.0f;
        peakHoldL = 0.0f;
        peakHoldR = 0.0f;
    }

    void MeterProcessor::processBlock(const float* left, const float* right, int numSamples, MeterValues& outValues) noexcept
    {
        if (numSamples <= 0)
            return;

        float blockPeakL = 0.0f;
        float blockPeakR = 0.0f;
        float sumSqL = 0.0f;
        float sumSqR = 0.0f;
        bool blockClipped = false;

        if (left != nullptr && right != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float absL = std::abs(left[i]);
                const float absR = std::abs(right[i]);

                if (absL > blockPeakL) blockPeakL = absL;
                if (absR > blockPeakR) blockPeakR = absR;

                sumSqL += absL * absL;
                sumSqR += absR * absR;

                if (absL >= 1.0f || absR >= 1.0f)
                    blockClipped = true;
            }
        }
        else if (left != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float absL = std::abs(left[i]);
                if (absL > blockPeakL) blockPeakL = absL;
                sumSqL += absL * absL;
                if (absL >= 1.0f) blockClipped = true;
            }
            blockPeakR = blockPeakL;
            sumSqR = sumSqL;
        }

        const float rmsL = std::sqrt(sumSqL / static_cast<float>(numSamples));
        const float rmsR = std::sqrt(sumSqR / static_cast<float>(numSamples));

        // Peak Hold logic for Left
        if (blockPeakL >= peakHoldL)
        {
            peakHoldL = blockPeakL;
            holdSamplesRemainingL = holdSamplesMax;
        }
        else
        {
            if (holdSamplesRemainingL > 0.0f)
            {
                holdSamplesRemainingL -= static_cast<float>(numSamples);
            }
            else
            {
                peakHoldL -= decayPerSample * static_cast<float>(numSamples);
                if (peakHoldL < blockPeakL)
                    peakHoldL = blockPeakL;
            }
        }

        // Peak Hold logic for Right
        if (blockPeakR >= peakHoldR)
        {
            peakHoldR = blockPeakR;
            holdSamplesRemainingR = holdSamplesMax;
        }
        else
        {
            if (holdSamplesRemainingR > 0.0f)
            {
                holdSamplesRemainingR -= static_cast<float>(numSamples);
            }
            else
            {
                peakHoldR -= decayPerSample * static_cast<float>(numSamples);
                if (peakHoldR < blockPeakR)
                    peakHoldR = blockPeakR;
            }
        }

        // Store to atomics for lock-free UI readout
        outValues.peakL.store(blockPeakL, std::memory_order_relaxed);
        outValues.peakR.store(blockPeakR, std::memory_order_relaxed);
        outValues.rmsL.store(rmsL, std::memory_order_relaxed);
        outValues.rmsR.store(rmsR, std::memory_order_relaxed);
        outValues.peakHoldL.store(peakHoldL, std::memory_order_relaxed);
        outValues.peakHoldR.store(peakHoldR, std::memory_order_relaxed);

        if (blockClipped)
        {
            outValues.clipped.store(true, std::memory_order_relaxed);
        }
    }
} // namespace dsd
