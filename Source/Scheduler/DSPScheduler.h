#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Audio/AudioTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace dsd
{
    class ChannelManager;

    class DSPScheduler
    {
    public:
        DSPScheduler();
        ~DSPScheduler();

        void startWorkers(int numThreads = 8);
        void stopWorkers();

        // Dispatches processing of all channels in parallel across active workers
        void processChannelsParallel(ChannelManager& channelManager,
                                     const juce::AudioBuffer<float>& deviceInputBuffer,
                                     int numSamples);

        // Called at block boundary to adaptively scale active workers based on deadline & load
        void updateAdaptiveWorkerCount(double dspTimeSec, double deadlineSec, int activeTaskCount);

        int getNumWorkers() const noexcept { return static_cast<int>(workers.size()); }
        int getActiveWorkerCount() const noexcept { return activeWorkerCount.load(std::memory_order_relaxed); }
        int getMaxWorkers() const noexcept { return maxWorkers.load(std::memory_order_relaxed); }

        float getEmaLoadRatio() const noexcept { return emaLoadRatio.load(std::memory_order_relaxed); }
        float getPeakLoadRatio() const noexcept { return peakLoadRatio.load(std::memory_order_relaxed); }

        const WorkerThreadStats* getWorkerStats(int index) const noexcept;

        void setParallelEnabled(bool enabled) noexcept { parallelEnabled.store(enabled, std::memory_order_relaxed); }
        bool isParallelEnabled() const noexcept { return parallelEnabled.load(std::memory_order_relaxed); }

        void setAdaptiveScalingEnabled(bool enabled) noexcept { adaptiveScalingEnabled.store(enabled, std::memory_order_relaxed); }
        bool isAdaptiveScalingEnabled() const noexcept { return adaptiveScalingEnabled.load(std::memory_order_relaxed); }

        void setManualWorkerCount(int count) noexcept;

        void setSampleRate(double sr) noexcept { currentSampleRate = (sr > 0.0) ? sr : DEFAULT_SAMPLE_RATE; }
        double getSampleRate() const noexcept { return currentSampleRate; }

    private:
        struct WorkerData
        {
            std::thread threadHandle;
            WorkerThreadStats stats;
            std::mutex mutex;
            std::condition_variable cv;
            std::atomic<bool> workReady{false};
        };

        std::vector<std::unique_ptr<WorkerData>> workers;

        // Default to true for multicore DSP execution
        std::atomic<bool> parallelEnabled{true};
        std::atomic<bool> adaptiveScalingEnabled{true};

        std::atomic<int> maxWorkers{8};
        std::atomic<int> minWorkers{2};
        std::atomic<int> activeWorkerCount{4};

        // Telemetry & Hysteresis
        std::atomic<float> emaLoadRatio{0.0f};
        std::atomic<float> peakLoadRatio{0.0f};
        int consecutiveHighLoadBlocks{0};
        int consecutiveLowLoadBlocks{0};

        std::atomic<bool> shouldExit{false};
        std::atomic<int> nextChannelIndex{0};
        std::atomic<int> tasksRemaining{0};

        std::mutex doneMutex;
        std::condition_variable cvDone;

        ChannelManager* currentChannelManager{nullptr};
        const juce::AudioBuffer<float>* currentInputBuffer{nullptr};
        int currentNumSamples{0};
        double currentSampleRate{DEFAULT_SAMPLE_RATE};

        void workerLoop(int workerId);
    };
} // namespace dsd
