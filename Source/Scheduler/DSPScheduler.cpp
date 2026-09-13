#include "Scheduler/DSPScheduler.h"
#include "Channel/ChannelManager.h"
#include <juce_core/juce_core.h>

namespace dsd
{
    DSPScheduler::DSPScheduler()
    {
        startWorkers(8);
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
            // Cap persistent worker threads to 8 to avoid OS thread scheduling overhead
            numThreads = std::min(numThreads, 8);
        }

        maxWorkers.store(numThreads, std::memory_order_relaxed);
        activeWorkerCount.store(std::min(4, numThreads), std::memory_order_relaxed);
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
        for (auto& w : workers)
        {
            if (w != nullptr)
            {
                std::lock_guard<std::mutex> lock(w->mutex);
                w->workReady.store(true, std::memory_order_release);
                w->cv.notify_one();
            }
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

    void DSPScheduler::setManualWorkerCount(int count) noexcept
    {
        int clamped = std::clamp(count, minWorkers.load(std::memory_order_relaxed), maxWorkers.load(std::memory_order_relaxed));
        activeWorkerCount.store(clamped, std::memory_order_relaxed);
        adaptiveScalingEnabled.store(false, std::memory_order_relaxed);
    }

    void DSPScheduler::updateAdaptiveWorkerCount(double dspTimeSec, double deadlineSec, int activeTaskCount)
    {
        if (!adaptiveScalingEnabled.load(std::memory_order_relaxed) || deadlineSec <= 0.0)
            return;

        const double loadRatio = dspTimeSec / deadlineSec;
        const float currentLoad = static_cast<float>(loadRatio * 100.0);

        // Exponential Moving Average (EMA) smoothing: alpha = 0.05
        float prevEma = emaLoadRatio.load(std::memory_order_relaxed);
        float newEma = (prevEma <= 0.01f) ? currentLoad : (0.95f * prevEma + 0.05f * currentLoad);
        emaLoadRatio.store(newEma, std::memory_order_relaxed);

        // Peak load tracking with slow decay
        float prevPeak = peakLoadRatio.load(std::memory_order_relaxed);
        float newPeak = std::max(currentLoad, prevPeak * 0.99f);
        peakLoadRatio.store(newPeak, std::memory_order_relaxed);

        // 1. Determine task graph floor
        int graphFloor = 2;
        if (activeTaskCount >= 16)
            graphFloor = 6;
        else if (activeTaskCount >= 8)
            graphFloor = 4;
        else
            graphFloor = 2;

        int currentActive = activeWorkerCount.load(std::memory_order_relaxed);
        int targetWorkers = currentActive;

        // 2. Fast Attack Scale UP (DSP > 60% of deadline or peak > 70%)
        if (loadRatio > 0.60 || newEma > 60.0f || newPeak > 70.0f)
        {
            consecutiveHighLoadBlocks++;
            consecutiveLowLoadBlocks = 0;

            if (consecutiveHighLoadBlocks >= 4)
            {
                if (loadRatio > 0.80 || newPeak > 85.0f)
                    targetWorkers = 8; // Extreme emergency scale up
                else
                    targetWorkers = std::min(maxWorkers.load(std::memory_order_relaxed), currentActive + 2);

                consecutiveHighLoadBlocks = 0;
            }
        }
        else if (loadRatio > 0.40 && currentActive < 4)
        {
            consecutiveHighLoadBlocks++;
            if (consecutiveHighLoadBlocks >= 8)
            {
                targetWorkers = 4;
                consecutiveHighLoadBlocks = 0;
            }
        }
        else if (loadRatio < 0.30 && newEma < 30.0f)
        {
            // 3. Slow Decay Scale DOWN (DSP < 30% for >= 300 blocks ~0.8s)
            consecutiveLowLoadBlocks++;
            consecutiveHighLoadBlocks = 0;

            if (consecutiveLowLoadBlocks >= 300)
            {
                if (currentActive > graphFloor)
                {
                    targetWorkers = std::max(graphFloor, currentActive - 2);
                }
                consecutiveLowLoadBlocks = 0;
            }
        }
        else
        {
            consecutiveLowLoadBlocks = 0;
            consecutiveHighLoadBlocks = 0;
        }

        // Clamp to limits and graph floor
        targetWorkers = std::clamp(targetWorkers, minWorkers.load(std::memory_order_relaxed), maxWorkers.load(std::memory_order_relaxed));
        targetWorkers = std::max(targetWorkers, graphFloor);

        // Deterministic boundary transition: applied strictly between blocks
        activeWorkerCount.store(targetWorkers, std::memory_order_relaxed);
    }

    void DSPScheduler::processChannelsParallel(ChannelManager& channelManager,
                                              const juce::AudioBuffer<float>& deviceInputBuffer,
                                              int numSamples)
    {
        const int totalChannels = channelManager.getNumChannels();
        if (totalChannels <= 0)
            return;

        // Fallback to single-thread serial if parallel disabled or workers empty
        if (!parallelEnabled.load(std::memory_order_relaxed) || workers.empty())
        {
            channelManager.processChannels(deviceInputBuffer, numSamples);
            return;
        }

        const int numAvailableWorkers = static_cast<int>(workers.size());
        const int numActive = std::clamp(activeWorkerCount.load(std::memory_order_relaxed), 1, numAvailableWorkers);

        currentChannelManager = &channelManager;
        currentInputBuffer = &deviceInputBuffer;
        currentNumSamples = numSamples;

        nextChannelIndex.store(0, std::memory_order_relaxed);
        tasksRemaining.store(totalChannels, std::memory_order_relaxed);

        // Mark active workers and parked workers
        for (int i = 0; i < numActive; ++i)
        {
            workers[i]->stats.isParked.store(false, std::memory_order_relaxed);
        }
        for (int i = numActive; i < numAvailableWorkers; ++i)
        {
            workers[i]->stats.isParked.store(true, std::memory_order_relaxed);
            workers[i]->stats.workerUtilizationPercent.store(0.0f, std::memory_order_relaxed);
            workers[i]->stats.cpuLoadPercent.store(0.0f, std::memory_order_relaxed);
            workers[i]->stats.busyTimeMs.store(0.0, std::memory_order_relaxed);
            workers[i]->stats.tasksAssigned.store(0, std::memory_order_relaxed);
        }

        // Targeted wakeup: Signal ONLY the active workers
        for (int i = 0; i < numActive; ++i)
        {
            std::lock_guard<std::mutex> lock(workers[i]->mutex);
            workers[i]->workReady.store(true, std::memory_order_release);
            workers[i]->cv.notify_one();
        }

        // Deterministic completion barrier: wait for active workers to complete all ready channel tasks
        std::unique_lock<std::mutex> lock(doneMutex);
        cvDone.wait(lock, [this]() {
            return tasksRemaining.load(std::memory_order_acquire) <= 0;
        });

        // Reset workReady on active workers
        for (int i = 0; i < numActive; ++i)
        {
            workers[i]->workReady.store(false, std::memory_order_relaxed);
        }
    }

    void DSPScheduler::workerLoop(int workerId)
    {
        while (!shouldExit.load(std::memory_order_relaxed))
        {
            // Per-worker conditional wait: parked workers stay in low-power kernel wait
            {
                std::unique_lock<std::mutex> lock(workers[workerId]->mutex);
                workers[workerId]->cv.wait(lock, [this, workerId]() {
                    return workers[workerId]->workReady.load(std::memory_order_acquire) || shouldExit.load(std::memory_order_relaxed);
                });
            }

            if (shouldExit.load(std::memory_order_relaxed))
                break;

            const int64_t startTicks = juce::Time::getHighResolutionTicks();
            int tasksDone = 0;

            if (currentChannelManager != nullptr && currentInputBuffer != nullptr)
            {
                const int totalChannels = currentChannelManager->getNumChannels();

                // Dynamic task queue: pull ready tasks atomically
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
            const double elapsedSec = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks);
            const double elapsedMs = elapsedSec * 1000.0;
            const double deadlineSec = (currentNumSamples > 0 && currentSampleRate > 0.0)
                                     ? (static_cast<double>(currentNumSamples) / currentSampleRate)
                                     : 0.010;
            const double deadlineMs = deadlineSec * 1000.0;

            workers[workerId]->stats.busyTimeMs.store(elapsedMs, std::memory_order_relaxed);
            workers[workerId]->stats.tasksAssigned.store(tasksDone, std::memory_order_relaxed);
            workers[workerId]->stats.blocksProcessed.fetch_add(tasksDone, std::memory_order_relaxed);

            // Worker Utilization (%) = (Worker Busy Time / DSP Deadline) * 100
            float instantUtil = (deadlineMs > 0.0) ? static_cast<float>((elapsedMs / deadlineMs) * 100.0) : 0.0f;
            instantUtil = std::clamp(instantUtil, 0.0f, 100.0f);

            // Rolling EMA smoothing (~250ms window): alpha = 0.15
            float prevUtil = workers[workerId]->stats.workerUtilizationPercent.load(std::memory_order_relaxed);
            float smoothedUtil = (prevUtil <= 0.001f) ? instantUtil : (0.85f * prevUtil + 0.15f * instantUtil);

            workers[workerId]->stats.workerUtilizationPercent.store(smoothedUtil, std::memory_order_relaxed);
            workers[workerId]->stats.cpuLoadPercent.store(smoothedUtil, std::memory_order_relaxed);
        }
    }
} // namespace dsd
