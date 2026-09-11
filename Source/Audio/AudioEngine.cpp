#include "Audio/AudioEngine.h"
#include <juce_core/juce_core.h>

namespace dsd
{
    AudioEngine::AudioEngine()
    {
    }

    AudioEngine::~AudioEngine()
    {
        audioDeviceStopped();
    }

    void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
    {
        if (device == nullptr)
            return;

        currentSampleRate = device->getCurrentSampleRate();
        currentBlockSize = device->getCurrentBufferSizeSamples();

        if (currentSampleRate <= 0.0) currentSampleRate = DEFAULT_SAMPLE_RATE;
        if (currentBlockSize <= 0)    currentBlockSize = DEFAULT_BUFFER_SIZE;

        const int maxIn = std::max(2, device->getActiveInputChannels().countNumberOfSetBits());

        // Preallocate scratch buffers
        tempInputBuffer.setSize(maxIn, currentBlockSize, false, true, true);
        tempInputBuffer.clear();

        channelManager.prepare(currentSampleRate, currentBlockSize);
        routingEngine.prepare(currentSampleRate, currentBlockSize);
        busManager.prepare(currentSampleRate, currentBlockSize);
        outputManager.prepare(currentSampleRate, currentBlockSize);

        const double deadlineMs = (static_cast<double>(currentBlockSize) / currentSampleRate) * 1000.0;
        perfStats.deadlineMs.store(deadlineMs, std::memory_order_relaxed);
        perfStats.activeWorkersCount.store(dspScheduler.getNumWorkers(), std::memory_order_relaxed);
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
        const int64_t startTicks = juce::Time::getHighResolutionTicks();

        if (numSamples <= 0)
            return;

        // ====================================================================
        // PHASE 1: Input Acquisition
        // ====================================================================
        const int channelsToCopy = std::min(numInputChannels, tempInputBuffer.getNumChannels());
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            if (inputChannelData != nullptr && inputChannelData[ch] != nullptr)
                tempInputBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
            else
                tempInputBuffer.clear(ch, 0, numSamples);
        }
        for (int ch = channelsToCopy; ch < tempInputBuffer.getNumChannels(); ++ch)
            tempInputBuffer.clear(ch, 0, numSamples);

        // ====================================================================
        // PHASE 2: Multicore Parallel Channel DSP (16 Channels across CPU Cores)
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
        const double deadlineSec = static_cast<double>(numSamples) / currentSampleRate;
        const double deadlineMs = deadlineSec * 1000.0;

        const float currentLoad = static_cast<float>((elapsedSec / deadlineSec) * 100.0);

        perfStats.processingTimeMs.store(elapsedMs, std::memory_order_relaxed);
        perfStats.cpuLoadPercent.store(currentLoad, std::memory_order_relaxed);
        perfStats.deadlineMs.store(deadlineMs, std::memory_order_relaxed);

        if (elapsedSec > deadlineSec)
        {
            perfStats.xrunCount.fetch_add(1, std::memory_order_relaxed);
            perfStats.isGlitching.store(true, std::memory_order_relaxed);
        }
        else
        {
            perfStats.isGlitching.store(false, std::memory_order_relaxed);
        }
    }
} // namespace dsd
