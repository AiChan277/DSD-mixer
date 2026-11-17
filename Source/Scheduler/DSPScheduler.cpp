#include "Scheduler/DSPScheduler.h"
#include "Channel/ChannelManager.h"
#include <juce_core/juce_core.h>
#include <algorithm>

#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <avrt.h>
#endif

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
 #include <immintrin.h>
 #define DSD_CPU_PAUSE() _mm_pause()
#else
 #define DSD_CPU_PAUSE() std::this_thread::yield()
#endif

namespace dsd
{
    namespace
    {
        // Gives a DSP worker the same scheduling class as the audio callback
        // thread and flushes denormals for its whole lifetime.
        struct WorkerRealtimeScope
        {
            WorkerRealtimeScope() noexcept
            {
                juce::FloatVectorOperations::disableDenormalisedNumberSupport(true);
               #if defined(_WIN32)
                DWORD taskIndex = 0;
                task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
                if (task != nullptr)
                    AvSetMmThreadPriority(task, AVRT_PRIORITY_HIGH);
                else
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
               #endif
            }

            ~WorkerRealtimeScope() noexcept
            {
               #if defined(_WIN32)
                if (task != nullptr)
                    AvRevertMmThreadCharacteristics(task);
               #endif
            }

           #if defined(_WIN32)
            HANDLE task{nullptr};
           #endif
        };

        // Spins briefly, then yields: used only while workers finish channels
        // they have already claimed, so the wait is bounded by one channel's DSP.
        constexpr int kSpinsBeforeYield = 2048;
    } // namespace

    static void signalWorker(void* wakeEvent, juce::WaitableEvent* fallback) noexcept
    {
       #if defined(_WIN32)
        juce::ignoreUnused(fallback);
        if (wakeEvent != nullptr)
            SetEvent(static_cast<HANDLE>(wakeEvent));
       #else
        juce::ignoreUnused(wakeEvent);
        fallback->signal();
       #endif
    }

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
        shouldExit.store(false, std::memory_order_release);

        workers.reserve(static_cast<size_t>(numThreads));
        for (int i = 0; i < numThreads; ++i)
        {
            auto worker = std::make_unique<WorkerData>();
           #if defined(_WIN32)
            worker->wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr); // auto-reset
           #endif
            workers.push_back(std::move(worker));
        }

        // Start threads only after the vector is complete: workerLoop indexes it.
        for (int i = 0; i < numThreads; ++i)
            workers[static_cast<size_t>(i)]->threadHandle = std::thread(&DSPScheduler::workerLoop, this, i);
    }

    void DSPScheduler::stopWorkers()
    {
        shouldExit.store(true, std::memory_order_release);
        for (auto& w : workers)
        {
            if (w != nullptr)
            {
                w->wakeGeneration.fetch_add(1, std::memory_order_release);
               #if defined(_WIN32)
                signalWorker(w->wakeEvent, nullptr);
               #else
                signalWorker(nullptr, &w->fallbackEvent);
               #endif
            }
        }

        for (auto& w : workers)
        {
            if (w != nullptr && w->threadHandle.joinable())
                w->threadHandle.join();
           #if defined(_WIN32)
            if (w != nullptr && w->wakeEvent != nullptr)
            {
                CloseHandle(static_cast<HANDLE>(w->wakeEvent));
                w->wakeEvent = nullptr;
            }
           #endif
        }
        workers.clear();
    }

    const WorkerThreadStats* DSPScheduler::getWorkerStats(int index) const noexcept
    {
        if (index >= 0 && index < static_cast<int>(workers.size()))
            return &workers[static_cast<size_t>(index)]->stats;
        return nullptr;
    }

    int DSPScheduler::getLastBlockTaskTotal() const noexcept
    {
        const uint32_t generation = lastCompletedGeneration.load(std::memory_order_acquire);
        int total = callerTasksLastBlock.load(std::memory_order_relaxed);
        for (const auto& w : workers)
        {
            if (w != nullptr && w->statsGeneration.load(std::memory_order_acquire) == generation)
                total += w->tasksThisBlock.load(std::memory_order_relaxed);
        }
        return total;
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

    int DSPScheduler::runTasks(uint32_t generation, std::atomic<int>* perWorkerCounter) noexcept
    {
        int done = 0;
        uint64_t word = claimWord.load(std::memory_order_acquire);

        while (true)
        {
            if (static_cast<uint32_t>(word >> 32) != generation)
                break; // A newer block has been published: nothing of ours is left.

            const uint32_t taskCount = static_cast<uint32_t>((word >> 16) & 0xFFFFu);
            const uint32_t index = static_cast<uint32_t>(word & 0xFFFFu);
            if (index >= taskCount)
                break;

            // Claim exactly one channel. On failure `word` is refreshed and we retry.
            if (!claimWord.compare_exchange_weak(word, word + 1,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire))
                continue;

            if (auto* ch = currentChannelManager->getChannel(static_cast<int>(index)))
                ch->processBlock(*currentInputBuffer, currentNumSamples.load(std::memory_order_relaxed));

            ++done;
            if (perWorkerCounter != nullptr)
                perWorkerCounter->store(done, std::memory_order_relaxed);

            // Release: publishes this channel's buffers to the audio thread.
            tasksRemaining.fetch_sub(1, std::memory_order_acq_rel);
            word = claimWord.load(std::memory_order_acquire);
        }

        return done;
    }

    void DSPScheduler::processChannelsParallel(ChannelManager& channelManager,
                                              const juce::AudioBuffer<float>& deviceInputBuffer,
                                              int numSamples)
    {
        const int totalChannels = std::min(channelManager.getNumChannels(), 0xFFFF);
        if (totalChannels <= 0)
            return;

        if (++blockGeneration == 0)
            ++blockGeneration; // 0 is the workers' initial "already seen" value
        const uint32_t generation = blockGeneration;

        // Fallback to single-thread serial if parallel disabled or workers empty
        if (!parallelEnabled.load(std::memory_order_relaxed) || workers.empty())
        {
            channelManager.processChannels(deviceInputBuffer, numSamples);
            callerTasksLastBlock.store(totalChannels, std::memory_order_relaxed);
            lastCompletedGeneration.store(generation, std::memory_order_release);
            return;
        }

        const int numAvailableWorkers = static_cast<int>(workers.size());
        const int numActive = std::clamp(activeWorkerCount.load(std::memory_order_relaxed), 1, numAvailableWorkers);

        currentChannelManager = &channelManager;
        currentInputBuffer = &deviceInputBuffer;
        currentNumSamples.store(numSamples, std::memory_order_relaxed);

        tasksRemaining.store(totalChannels, std::memory_order_relaxed);
        // Release: block parameters above become visible to whoever claims a task.
        claimWord.store(packClaim(generation, static_cast<uint32_t>(totalChannels), 0), std::memory_order_release);

        // Mark active workers and parked workers
        for (int i = 0; i < numActive; ++i)
            workers[static_cast<size_t>(i)]->stats.isParked.store(false, std::memory_order_relaxed);

        for (int i = numActive; i < numAvailableWorkers; ++i)
        {
            auto& stats = workers[static_cast<size_t>(i)]->stats;
            stats.isParked.store(true, std::memory_order_relaxed);
            stats.workerUtilizationPercent.store(0.0f, std::memory_order_relaxed);
            stats.cpuLoadPercent.store(0.0f, std::memory_order_relaxed);
            stats.busyTimeMs.store(0.0, std::memory_order_relaxed);
            stats.tasksAssigned.store(0, std::memory_order_relaxed);
        }

        // Targeted wakeup of the active workers only (no mutex involved).
        for (int i = 0; i < numActive; ++i)
        {
            auto& w = *workers[static_cast<size_t>(i)];
            w.wakeGeneration.store(generation, std::memory_order_release);
           #if defined(_WIN32)
            signalWorker(w.wakeEvent, nullptr);
           #else
            signalWorker(nullptr, &w.fallbackEvent);
           #endif
        }

        // The audio thread works too, so the block finishes even if workers are late.
        callerTasksLastBlock.store(runTasks(generation, nullptr), std::memory_order_relaxed);

        // Every channel is claimed; wait only for ones still running on workers.
        int spins = 0;
        while (tasksRemaining.load(std::memory_order_acquire) > 0)
        {
            if (++spins < kSpinsBeforeYield)
                DSD_CPU_PAUSE();
            else
                std::this_thread::yield();
        }

        lastCompletedGeneration.store(generation, std::memory_order_release);
    }

    void DSPScheduler::workerLoop(int workerId)
    {
        WorkerRealtimeScope realtimeScope;
        auto& self = *workers[static_cast<size_t>(workerId)];

        uint32_t seen = 0;

        while (true)
        {
           #if defined(_WIN32)
            WaitForSingleObject(static_cast<HANDLE>(self.wakeEvent), INFINITE);
           #else
            self.fallbackEvent.wait(-1);
           #endif

            if (shouldExit.load(std::memory_order_acquire))
                break;

            const uint32_t generation = self.wakeGeneration.load(std::memory_order_acquire);
            if (generation == seen)
                continue; // spurious wakeup
            seen = generation;

            const int64_t startTicks = juce::Time::getHighResolutionTicks();

            self.tasksThisBlock.store(0, std::memory_order_relaxed);
            self.statsGeneration.store(generation, std::memory_order_release);

            const int tasksDone = runTasks(generation, &self.tasksThisBlock);

            const int64_t endTicks = juce::Time::getHighResolutionTicks();
            const double elapsedMs = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks) * 1000.0;
            const int numSamples = currentNumSamples.load(std::memory_order_relaxed);
            const double deadlineMs = (numSamples > 0 && currentSampleRate > 0.0)
                                    ? (static_cast<double>(numSamples) / currentSampleRate) * 1000.0
                                    : 10.0;

            self.stats.busyTimeMs.store(elapsedMs, std::memory_order_relaxed);
            self.stats.tasksAssigned.store(tasksDone, std::memory_order_relaxed);
            self.stats.blocksProcessed.fetch_add(static_cast<uint32_t>(tasksDone), std::memory_order_relaxed);

            // Worker Utilization (%) = (Worker Busy Time / DSP Deadline) * 100
            float instantUtil = (deadlineMs > 0.0) ? static_cast<float>((elapsedMs / deadlineMs) * 100.0) : 0.0f;
            instantUtil = std::clamp(instantUtil, 0.0f, 100.0f);

            // Rolling EMA smoothing (~250ms window): alpha = 0.15
            const float prevUtil = self.stats.workerUtilizationPercent.load(std::memory_order_relaxed);
            const float smoothedUtil = (prevUtil <= 0.001f) ? instantUtil : (0.85f * prevUtil + 0.15f * instantUtil);

            self.stats.workerUtilizationPercent.store(smoothedUtil, std::memory_order_relaxed);
            self.stats.cpuLoadPercent.store(smoothedUtil, std::memory_order_relaxed);
        }
    }
} // namespace dsd
