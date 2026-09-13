#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <algorithm>
#include <juce_core/juce_core.h>


namespace dsd
{
    using ChannelID = int;
    using BusID = int;

    constexpr double DEFAULT_SAMPLE_RATE = 48000.0;
    constexpr int DEFAULT_BUFFER_SIZE = 128;

    // DSD Mixer Level 1 Specifications
    constexpr int NUM_CHANNELS_LEVEL1 = 16;
    constexpr int NUM_OUTPUT_BUSES_LEVEL1 = 4;
    constexpr int NUM_SUBMIX_BUSES = 4;

    constexpr float MIN_GAIN_DB = -60.0f;
    constexpr float MAX_GAIN_DB = 24.0f;
    constexpr float MIN_FADER_DB = -60.0f;
    constexpr float MAX_FADER_DB = 12.0f;

    struct MeterValues
    {
        std::atomic<float> peakL{0.0f};
        std::atomic<float> peakR{0.0f};
        std::atomic<float> rmsL{0.0f};
        std::atomic<float> rmsR{0.0f};
        std::atomic<float> peakHoldL{0.0f};
        std::atomic<float> peakHoldR{0.0f};
        std::atomic<bool> clipped{false};

        void resetClip() noexcept
        {
            clipped.store(false, std::memory_order_relaxed);
        }
    };

    struct RouteMatrixCell
    {
        std::atomic<float> sendGainDb{0.0f}; // 0 dB default send level
        std::atomic<float> sendPan{0.0f};    // Center
        std::atomic<bool>  enabled{false};   // Route enabled flag
    };

    struct WorkerThreadStats
    {
        std::atomic<float> workerUtilizationPercent{0.0f};
        std::atomic<float> cpuLoadPercent{0.0f}; // Maintained for legacy compatibility
        std::atomic<double> busyTimeMs{0.0};
        std::atomic<uint32_t> blocksProcessed{0};
        std::atomic<bool> isParked{false};
        std::atomic<int> tasksAssigned{0};
    };

    struct EnginePerformanceStats
    {
        std::atomic<float> currentLoadPercent{0.0f};
        std::atomic<float> cpuLoadPercent{0.0f}; // Legacy alias for currentLoadPercent
        std::atomic<float> avgLoadPercent{0.0f};
        std::atomic<float> peakLoadPercent{0.0f};

        std::atomic<double> currentProcessingTimeMs{0.0};
        std::atomic<double> processingTimeMs{0.0}; // Legacy alias for currentProcessingTimeMs
        std::atomic<double> avgProcessingTimeMs{0.0};
        std::atomic<double> peakProcessingTimeMs{0.0};

        std::atomic<double> deadlineMs{10.0};
        std::atomic<double> currentHeadroomMs{10.0};
        std::atomic<double> minHeadroomMs{10.0};

        std::atomic<uint64_t> deadlineMissCount{0}; // DSP execution exceeded deadline
        std::atomic<uint64_t> xrunCount{0};         // Physical audio device / WASAPI glitch
        std::atomic<bool> isGlitching{false};

        std::atomic<int64_t> lastDeadlineMissTimestampMs{0};
        std::atomic<int64_t> lastXrunTimestampMs{0};
        std::atomic<int> activeWorkersCount{4};

        void resetPeaks() noexcept
        {
            peakProcessingTimeMs.store(0.0, std::memory_order_relaxed);
            peakLoadPercent.store(0.0f, std::memory_order_relaxed);
            minHeadroomMs.store(deadlineMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }

        void resetMetrics() noexcept
        {
            deadlineMissCount.store(0, std::memory_order_relaxed);
            xrunCount.store(0, std::memory_order_relaxed);
            isGlitching.store(false, std::memory_order_relaxed);
            lastDeadlineMissTimestampMs.store(0, std::memory_order_relaxed);
            lastXrunTimestampMs.store(0, std::memory_order_relaxed);
            resetPeaks();
        }
    };


} // namespace dsd
