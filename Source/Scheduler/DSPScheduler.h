#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdint>
#include "Audio/AudioTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace dsd
{
    class ChannelManager;

    // Fork/join scheduler for per-channel DSP.
    //
    // Real-time contract for processChannelsParallel() (called on the audio thread):
    //   * never takes a mutex and never sleeps on a condition variable;
    //   * the calling audio thread processes channels itself, so a block always
    //     completes even if every worker wakes late;
    //   * the only wait is a short spin for channels a worker has already
    //     claimed and is still processing.
    // Workers sleep on a per-worker auto-reset event (Win32 SetEvent never blocks
    // the caller) and run with MMCSS "Pro Audio" priority and denormals flushed.
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

        // Channels processed by the calling audio thread in the most recent block.
        int getCallerTasksLastBlock() const noexcept { return callerTasksLastBlock.load(std::memory_order_relaxed); }

        // Channels processed in the most recent block by the caller plus every
        // worker that took part in it. Exact once processChannelsParallel() has
        // returned; intended for diagnostics and tests.
        int getLastBlockTaskTotal() const noexcept;

        void setParallelEnabled(bool enabled) noexcept { parallelEnabled.store(enabled, std::memory_order_relaxed); }
        bool isParallelEnabled() const noexcept { return parallelEnabled.load(std::memory_order_relaxed); }

        void setAdaptiveScalingEnabled(bool enabled) noexcept { adaptiveScalingEnabled.store(enabled, std::memory_order_relaxed); }
        bool isAdaptiveScalingEnabled() const noexcept { return adaptiveScalingEnabled.load(std::memory_order_relaxed); }

        void setManualWorkerCount(int count) noexcept;

        void setSampleRate(double sr) noexcept { currentSampleRate = (sr > 0.0) ? sr : DEFAULT_SAMPLE_RATE; }
        double getSampleRate() const noexcept { return currentSampleRate; }

    private:
        struct alignas(64) WorkerData
        {
            std::thread threadHandle;
            WorkerThreadStats stats;

            // Block generation this worker should run. The audio thread stores a
            // new value, then signals wakeEvent.
            std::atomic<uint32_t> wakeGeneration{0};

            // Auto-reset event (Win32 HANDLE). Repeated signals coalesce, which is
            // fine: the worker always reads the latest wakeGeneration.
            void* wakeEvent{nullptr};
           #if !defined(_WIN32)
            juce::WaitableEvent fallbackEvent;
           #endif

            // Generation whose task count is held in tasksThisBlock.
            std::atomic<uint32_t> statsGeneration{0};
            std::atomic<int> tasksThisBlock{0};
        };

        // Packed claim word: [generation:32][taskCount:16][nextIndex:16].
        // Carrying the generation means a worker that wakes late can never
        // claim a task belonging to a newer block.
        static constexpr uint64_t packClaim(uint32_t generation, uint32_t taskCount, uint32_t index) noexcept
        {
            return (static_cast<uint64_t>(generation) << 32) | (static_cast<uint64_t>(taskCount & 0xFFFFu) << 16)
                   | static_cast<uint64_t>(index & 0xFFFFu);
        }

        // Claims and processes tasks of `generation` until none remain.
        // Returns the number of channels processed by the calling thread.
        int runTasks(uint32_t generation, std::atomic<int>* perWorkerCounter) noexcept;

        void workerLoop(int workerId);

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

        alignas(64) std::atomic<uint64_t> claimWord{0};
        alignas(64) std::atomic<int> tasksRemaining{0};

        // Owned by the audio thread; workers read these only after a successful
        // claim, which orders the read before the next block rewrites them.
        uint32_t blockGeneration{0};
        ChannelManager* currentChannelManager{nullptr};
        const juce::AudioBuffer<float>* currentInputBuffer{nullptr};
        std::atomic<int> currentNumSamples{0}; // also read by workers for telemetry
        double currentSampleRate{DEFAULT_SAMPLE_RATE};

        std::atomic<int> callerTasksLastBlock{0};
        std::atomic<uint32_t> lastCompletedGeneration{0};
    };
} // namespace dsd
