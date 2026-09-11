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

        // Dispatches processing of all channels across worker threads in parallel
        void processChannelsParallel(ChannelManager& channelManager,
                                     const juce::AudioBuffer<float>& deviceInputBuffer,
                                     int numSamples);

        int getNumWorkers() const noexcept { return static_cast<int>(workers.size()); }
        const WorkerThreadStats* getWorkerStats(int index) const noexcept;

    private:
        struct WorkerData
        {
            std::thread threadHandle;
            WorkerThreadStats stats;
        };

        std::vector<std::unique_ptr<WorkerData>> workers;

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
