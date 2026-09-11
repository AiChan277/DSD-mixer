#pragma once

#include <atomic>
#include <cstdint>
#include <string>

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
        std::atomic<float> cpuLoadPercent{0.0f};
        std::atomic<uint32_t> blocksProcessed{0};
    };

    struct EnginePerformanceStats
    {
        std::atomic<float> cpuLoadPercent{0.0f};
        std::atomic<double> processingTimeMs{0.0};
        std::atomic<double> deadlineMs{2.67};
        std::atomic<uint64_t> xrunCount{0};
        std::atomic<bool> isGlitching{false};
        std::atomic<int> activeWorkersCount{0};
    };
} // namespace dsd
