#include "Scheduler/DSPScheduler.h"
#include "Channel/ChannelManager.h"
#include <juce_core/juce_core.h>

namespace dsd
{
    DSPScheduler::DSPScheduler()
    {
        startWorkers();
    }

    DSPScheduler::~DSPScheduler()
    {
        stopWorkers();
    }

    void DSPScheduler::startWorkers(int numThreads)
    {
        stopWorkers();

        if (numThreads <= 0)
        {
            numThreads = static_cast<int>(std::thread::hardware_concurrency());
            if (numThreads <= 0) numThreads = 4;
            // Cap worker threads to 8 to avoid OS thread scheduling overhead in real-time callbacks
            numThreads = std::min(numThreads, 8);
        }

        shouldExit.store(false);

        workers.reserve(numThreads);
        for (int i = 0; i < numThreads; ++i)
        {
            auto worker = std::make_unique<WorkerData>();
            worker->threadHandle = std::thread(&DSPScheduler::workerLoop, this, i);
            workers.push_back(std::move(worker));
        }
    }

    void DSPScheduler::stopWorkers()
    {
        shouldExit.store(true);
        {
            std::lock_guard<std::mutex> lock(workMutex);
            workReady.store(true);
            cvWork.notify_all();
        }

        for (auto& w : workers)
        {
            if (w != nullptr && w->threadHandle.joinable())
                w->threadHandle.join();
        }
        workers.clear();
    }

    const WorkerThreadStats* DSPScheduler::getWorkerStats(int index) const noexcept
    {
        if (index >= 0 && index < static_cast<int>(workers.size()))
            return &workers[index]->stats;
        return nullptr;
    }

    void DSPScheduler::processChannelsParallel(ChannelManager& channelManager,
                                              const juce::AudioBuffer<float>& deviceInputBuffer,
                                              int numSamples)
    {
        const int totalChannels = channelManager.getNumChannels();
        if (totalChannels <= 0)
            return;

        // Clean, glitch-free serial processing on audio thread by default
        if (!parallelEnabled.load(std::memory_order_relaxed) || workers.empty())
        {
            channelManager.processChannels(deviceInputBuffer, numSamples);
            return;
        }

        currentChannelManager = &channelManager;
        currentInputBuffer = &deviceInputBuffer;
        currentNumSamples = numSamples;

        nextChannelIndex.store(0, std::memory_order_relaxed);
        tasksRemaining.store(totalChannels, std::memory_order_relaxed);

        // Signal workers to begin
        {
            std::lock_guard<std::mutex> lock(workMutex);
            workReady.store(true, std::memory_order_release);
        }
        cvWork.notify_all();

        // Audio thread also helps process channels while waiting
        while (true)
        {
            int chIdx = nextChannelIndex.fetch_add(1, std::memory_order_relaxed);
            if (chIdx >= totalChannels)
                break;

            if (auto* ch = channelManager.getChannel(chIdx))
                ch->processBlock(deviceInputBuffer, numSamples);

            int rem = tasksRemaining.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (rem <= 0)
            {
                std::lock_guard<std::mutex> doneLock(doneMutex);
                cvDone.notify_one();
            }
        }

        // Wait for all worker channels to finish
        std::unique_lock<std::mutex> lock(doneMutex);
        cvDone.wait(lock, [this]() {
            return tasksRemaining.load(std::memory_order_acquire) <= 0;
        });

        workReady.store(false, std::memory_order_relaxed);
    }

    void DSPScheduler::workerLoop(int workerId)
    {
        while (!shouldExit.load(std::memory_order_relaxed))
        {
            // Wait for work signal
            std::unique_lock<std::mutex> lock(workMutex);
            cvWork.wait(lock, [this]() {
                return workReady.load(std::memory_order_acquire) || shouldExit.load(std::memory_order_relaxed);
            });

            if (shouldExit.load(std::memory_order_relaxed))
                break;

            const int64_t startTicks = juce::Time::getHighResolutionTicks();
            int tasksDone = 0;

            if (currentChannelManager != nullptr && currentInputBuffer != nullptr)
            {
                const int totalChannels = currentChannelManager->getNumChannels();

                while (true)
                {
                    int chIdx = nextChannelIndex.fetch_add(1, std::memory_order_relaxed);
                    if (chIdx >= totalChannels)
                        break;

                    if (auto* ch = currentChannelManager->getChannel(chIdx))
                    {
                        ch->processBlock(*currentInputBuffer, currentNumSamples);
                        tasksDone++;
                    }

                    int remaining = tasksRemaining.fetch_sub(1, std::memory_order_acq_rel) - 1;
                    if (remaining <= 0)
                    {
                        std::lock_guard<std::mutex> doneLock(doneMutex);
                        cvDone.notify_one();
                    }
                }
            }

            const int64_t endTicks = juce::Time::getHighResolutionTicks();
            const double elapsedMs = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks) * 1000.0;

            if (workerId >= 0 && workerId < static_cast<int>(workers.size()))
            {
                workers[workerId]->stats.blocksProcessed.fetch_add(tasksDone, std::memory_order_relaxed);
                float load = static_cast<float>((elapsedMs / 2.67) * 100.0);
                workers[workerId]->stats.cpuLoadPercent.store(std::clamp(load, 0.0f, 100.0f), std::memory_order_relaxed);
            }
        }
    }
} // namespace dsd
