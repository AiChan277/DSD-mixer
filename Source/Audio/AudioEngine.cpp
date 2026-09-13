#include "Audio/AudioEngine.h"
#include <juce_core/juce_core.h>

namespace dsd
{
    AudioEngine::AudioEngine()
    {
        prepare(DEFAULT_SAMPLE_RATE, DEFAULT_BUFFER_SIZE, 2);
    }

    AudioEngine::~AudioEngine()
    {
        audioDeviceStopped();
    }

    void AudioEngine::prepare(double sampleRate, int maxBlockSize, int numInputs)
    {
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : DEFAULT_SAMPLE_RATE;
        currentBlockSize = (maxBlockSize > 0) ? maxBlockSize : DEFAULT_BUFFER_SIZE;
        const int maxIn = std::max(2, numInputs);

        // Preallocate scratch buffers
        tempInputBuffer.setSize(maxIn, currentBlockSize, false, true, true);
        tempInputBuffer.clear();

        channelManager.prepare(currentSampleRate, currentBlockSize);
        routingEngine.prepare(currentSampleRate, currentBlockSize);
        busManager.prepare(currentSampleRate, currentBlockSize);
        outputManager.prepare(currentSampleRate, currentBlockSize);
        dspScheduler.setSampleRate(currentSampleRate);

        const double deadlineMs = (static_cast<double>(currentBlockSize) / currentSampleRate) * 1000.0;
        perfStats.deadlineMs.store(deadlineMs, std::memory_order_relaxed);
        perfStats.currentHeadroomMs.store(deadlineMs, std::memory_order_relaxed);
        perfStats.minHeadroomMs.store(deadlineMs, std::memory_order_relaxed);
        perfStats.activeWorkersCount.store(dspScheduler.getNumWorkers(), std::memory_order_relaxed);
    }

    void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
    {
        if (device == nullptr)
            return;

        const double sr = device->getCurrentSampleRate();
        const int bs = device->getCurrentBufferSizeSamples();
        const int numIn = device->getActiveInputChannels().countNumberOfSetBits();
        prepare(sr, bs, numIn);
    }

    void AudioEngine::audioDeviceStopped()
    {
        channelManager.releaseResources();
        routingEngine.releaseResources();
        busManager.releaseResources();
        outputManager.releaseResources();

        tempInputBuffer.setSize(0, 0);
    }

    void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& /*context*/)
    {
        if (numSamples <= 0)
            return;

        if (tempInputBuffer.getNumSamples() < numSamples)
            tempInputBuffer.setSize(std::max(2, std::max(numInputChannels, tempInputBuffer.getNumChannels())), numSamples, false, true, true);

        const int64_t startTicks = juce::Time::getHighResolutionTicks();

        // ====================================================================
        // PHASE 1: Copy Device Audio Inputs (Physical Microphones & Line Inputs)
        // ====================================================================
        const int numChannelsToCopy = std::min(numInputChannels, tempInputBuffer.getNumChannels());
        for (int ch = 0; ch < numChannelsToCopy; ++ch)
        {
            if (inputChannelData != nullptr && inputChannelData[ch] != nullptr)
                tempInputBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
            else
                tempInputBuffer.clear(ch, 0, numSamples);
        }
        for (int ch = numChannelsToCopy; ch < tempInputBuffer.getNumChannels(); ++ch)
            tempInputBuffer.clear(ch, 0, numSamples);

        // ====================================================================
        // PHASE 2: Channel DSP Processing (Multicore Scheduler Parallel Dispatch)
        // ====================================================================
        dspScheduler.processChannelsParallel(channelManager, tempInputBuffer, numSamples);

        // ====================================================================
        // PHASE 3: Routing Matrix 16x4 Accumulation
        // ====================================================================
        routingEngine.routeChannelsToOutputs(channelManager, outputManager, numSamples);

        // ====================================================================
        // PHASE 4: Output Buses DSP Processing (4 Independent Output Strips)
        // ====================================================================
        outputManager.processOutputs(numSamples);

        // ====================================================================
        // PHASE 5: Output Hardware Copy
        // ====================================================================
        outputManager.writeToDeviceOutputs(outputChannelData, numOutputChannels, numSamples);

        // ====================================================================
        // Performance Timing & Deadline Monitoring
        // ====================================================================
        const int64_t endTicks = juce::Time::getHighResolutionTicks();
        const double elapsedSec = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks);
        const double elapsedMs = elapsedSec * 1000.0;
        const double deadlineSec = (currentSampleRate > 0.0) ? (static_cast<double>(numSamples) / currentSampleRate) : 0.010;
        const double deadlineMs = deadlineSec * 1000.0;

        // DSP Load (%) = (DSP Processing Time / DSP Deadline) * 100
        const float currentLoad = (deadlineSec > 0.0) ? static_cast<float>((elapsedSec / deadlineSec) * 100.0) : 0.0f;
        const double headroomMs = std::max(0.0, deadlineMs - elapsedMs);

        perfStats.currentProcessingTimeMs.store(elapsedMs, std::memory_order_relaxed);
        perfStats.processingTimeMs.store(elapsedMs, std::memory_order_relaxed);
        perfStats.currentLoadPercent.store(currentLoad, std::memory_order_relaxed);
        perfStats.cpuLoadPercent.store(currentLoad, std::memory_order_relaxed);
        perfStats.deadlineMs.store(deadlineMs, std::memory_order_relaxed);
        perfStats.currentHeadroomMs.store(headroomMs, std::memory_order_relaxed);

        // Rolling EMA smoothing (~250 ms window): alpha = 0.05
        float prevAvgLoad = perfStats.avgLoadPercent.load(std::memory_order_relaxed);
        float newAvgLoad = (prevAvgLoad <= 0.001f) ? currentLoad : (0.95f * prevAvgLoad + 0.05f * currentLoad);
        perfStats.avgLoadPercent.store(newAvgLoad, std::memory_order_relaxed);

        double prevAvgTime = perfStats.avgProcessingTimeMs.load(std::memory_order_relaxed);
        double newAvgTime = (prevAvgTime <= 0.001) ? elapsedMs : (0.95 * prevAvgTime + 0.05 * elapsedMs);
        perfStats.avgProcessingTimeMs.store(newAvgTime, std::memory_order_relaxed);

        // True Peak Tracking (Non-smoothed historical maximum)
        float prevPeakLoad = perfStats.peakLoadPercent.load(std::memory_order_relaxed);
        if (currentLoad > prevPeakLoad)
            perfStats.peakLoadPercent.store(currentLoad, std::memory_order_relaxed);

        double prevPeakTime = perfStats.peakProcessingTimeMs.load(std::memory_order_relaxed);
        if (elapsedMs > prevPeakTime)
            perfStats.peakProcessingTimeMs.store(elapsedMs, std::memory_order_relaxed);

        double prevMinHeadroom = perfStats.minHeadroomMs.load(std::memory_order_relaxed);
        if (headroomMs < prevMinHeadroom)
            perfStats.minHeadroomMs.store(headroomMs, std::memory_order_relaxed);

        // Deadline Miss (DSP Overrun) detection: calculation exceeded block duration
        if (elapsedSec > deadlineSec)
        {
            perfStats.deadlineMissCount.fetch_add(1, std::memory_order_relaxed);
            perfStats.isGlitching.store(true, std::memory_order_relaxed);
            perfStats.lastDeadlineMissTimestampMs.store(juce::Time::currentTimeMillis(), std::memory_order_relaxed);
        }
        else
        {
            perfStats.isGlitching.store(false, std::memory_order_relaxed);
        }

        perfStats.activeWorkersCount.store(dspScheduler.getActiveWorkerCount(), std::memory_order_relaxed);

        // ====================================================================
        // PHASE 6: Adaptive Worker Pool Boundary Evaluation
        // ====================================================================
        dspScheduler.updateAdaptiveWorkerCount(elapsedSec, deadlineSec, channelManager.getNumChannels());
    }

    std::vector<CaptureDiagnostics> AudioEngine::getActiveCaptureDiagnostics() const
    {
        std::vector<CaptureDiagnostics> diags;
        const int numCh = channelManager.getNumChannels();
        for (int i = 0; i < numCh; ++i)
        {
            if (const auto* ch = channelManager.getChannel(i))
            {
                if (const auto* wac = dynamic_cast<const WindowAudioCapture*>(ch->getInputSource()))
                {
                    diags.push_back(wac->getDiagnostics());
                }
            }
        }
        return diags;
    }

    double AudioEngine::getEstimatedOutputLatencyMs() const noexcept
    {
        return (currentSampleRate > 0.0) ? (static_cast<double>(currentBlockSize) / currentSampleRate * 1000.0) : 0.0;
    }

    double AudioEngine::getDspBlockMs() const noexcept
    {
        return (currentSampleRate > 0.0) ? (static_cast<double>(currentBlockSize) / currentSampleRate * 1000.0) : 0.0;
    }

    void AudioEngine::setCaptureLatencyMode(CaptureLatencyMode mode)
    {
        WindowAudioCapture::setGlobalLatencyMode(mode);
        const int numCh = channelManager.getNumChannels();
        for (int i = 0; i < numCh; ++i)
        {
            if (auto* ch = channelManager.getChannel(i))
            {
                if (auto* wac = dynamic_cast<WindowAudioCapture*>(ch->getInputSource()))
                {
                    wac->setLatencyMode(mode);
                }
            }
        }
    }

    CaptureLatencyMode AudioEngine::getCaptureLatencyMode() const noexcept
    {
        return WindowAudioCapture::getGlobalLatencyMode();
    }
} // namespace dsd
