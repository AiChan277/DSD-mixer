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

        void startWorkers(int numThreads = 0);
        void stopWorkers();

        // Dispatches processing of all channels
        void processChannelsParallel(ChannelManager& channelManager,
                                     const juce::AudioBuffer<float>& deviceInputBuffer,
                                     int numSamples);

        int getNumWorkers() const noexcept { return static_cast<int>(workers.size()); }
        const WorkerThreadStats* getWorkerStats(int index) const noexcept;

        void setParallelEnabled(bool enabled) noexcept { parallelEnabled.store(enabled, std::memory_order_relaxed); }
        bool isParallelEnabled() const noexcept { return parallelEnabled.load(std::memory_order_relaxed); }

    private:
        struct WorkerData
        {
            std::thread threadHandle;
            WorkerThreadStats stats;
        };

        std::vector<std::unique_ptr<WorkerData>> workers;

        // Default to false (clean single-thread DSP execution) for zero jitter and guaranteed sample-accurate timing
        std::atomic<bool> parallelEnabled{false};

        std::atomic<bool> shouldExit{false};
        std::atomic<bool> workReady{false};
        std::atomic<int> nextChannelIndex{0};
        std::atomic<int> tasksRemaining{0};

        std::mutex workMutex;
        std::condition_variable cvWork;

        std::mutex doneMutex;
        std::condition_variable cvDone;

        ChannelManager* currentChannelManager{nullptr};
        const juce::AudioBuffer<float>* currentInputBuffer{nullptr};
        int currentNumSamples{0};

        void workerLoop(int workerId);
    };
} // namespace dsd
