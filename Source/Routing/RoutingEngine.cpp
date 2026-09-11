#include "Routing/RoutingEngine.h"
#include "DSP/GainProcessor.h"
#include <cmath>

namespace dsd
{
    RoutingEngine::RoutingEngine()
    {
        // Default routing setup: All channels route to OUT 01 (Studio Monitor) @ 0 dB
        for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
        {
            matrix[ch][0].enabled.store(true, std::memory_order_relaxed);
            matrix[ch][0].sendGainDb.store(0.0f, std::memory_order_relaxed);
            matrix[ch][0].sendPan.store(0.0f, std::memory_order_relaxed);

            // Default disabled for other outputs until enabled by user
            for (int out = 1; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                matrix[ch][out].enabled.store(false, std::memory_order_relaxed);
                matrix[ch][out].sendGainDb.store(0.0f, std::memory_order_relaxed);
                matrix[ch][out].sendPan.store(0.0f, std::memory_order_relaxed);
            }
        }
    }

    void RoutingEngine::prepare(double /*sampleRate*/, int /*maxBlockSize*/)
    {
    }

    void RoutingEngine::releaseResources()
    {
    }

    bool RoutingEngine::isRouteEnabled(int channelIdx, int outputIdx) const noexcept
    {
        if (channelIdx >= 0 && channelIdx < NUM_CHANNELS_LEVEL1 &&
            outputIdx >= 0 && outputIdx < NUM_OUTPUT_BUSES_LEVEL1)
        {
            return matrix[channelIdx][outputIdx].enabled.load(std::memory_order_relaxed);
        }
        return false;
    }

    void RoutingEngine::setRouteEnabled(int channelIdx, int outputIdx, bool enabled) noexcept
    {
        if (channelIdx >= 0 && channelIdx < NUM_CHANNELS_LEVEL1 &&
            outputIdx >= 0 && outputIdx < NUM_OUTPUT_BUSES_LEVEL1)
        {
            matrix[channelIdx][outputIdx].enabled.store(enabled, std::memory_order_relaxed);
        }
    }

    float RoutingEngine::getRouteGainDb(int channelIdx, int outputIdx) const noexcept
    {
        if (channelIdx >= 0 && channelIdx < NUM_CHANNELS_LEVEL1 &&
            outputIdx >= 0 && outputIdx < NUM_OUTPUT_BUSES_LEVEL1)
        {
            return matrix[channelIdx][outputIdx].sendGainDb.load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

    void RoutingEngine::setRouteGainDb(int channelIdx, int outputIdx, float gainDb) noexcept
    {
        if (channelIdx >= 0 && channelIdx < NUM_CHANNELS_LEVEL1 &&
            outputIdx >= 0 && outputIdx < NUM_OUTPUT_BUSES_LEVEL1)
        {
            matrix[channelIdx][outputIdx].sendGainDb.store(gainDb, std::memory_order_relaxed);
        }
    }

    float RoutingEngine::getRoutePan(int channelIdx, int outputIdx) const noexcept
    {
        if (channelIdx >= 0 && channelIdx < NUM_CHANNELS_LEVEL1 &&
            outputIdx >= 0 && outputIdx < NUM_OUTPUT_BUSES_LEVEL1)
        {
            return matrix[channelIdx][outputIdx].sendPan.load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

    void RoutingEngine::setRoutePan(int channelIdx, int outputIdx, float pan) noexcept
    {
        if (channelIdx >= 0 && channelIdx < NUM_CHANNELS_LEVEL1 &&
            outputIdx >= 0 && outputIdx < NUM_OUTPUT_BUSES_LEVEL1)
        {
            matrix[channelIdx][outputIdx].sendPan.store(pan, std::memory_order_relaxed);
        }
    }

    void RoutingEngine::routeChannelsToOutputs(const ChannelManager& channelManager,
                                              OutputManager& outputManager,
                                              int numSamples)
    {
        const int numOutputs = outputManager.getNumOutputs();

        // 1. Clear all output bus buffers
        for (int outIdx = 0; outIdx < numOutputs; ++outIdx)
        {
            if (auto* outBus = outputManager.getOutput(outIdx))
                outBus->getBuffer().clear(0, numSamples);
        }

        const bool anySolo = channelManager.hasAnySoloChannel();
        const int numChannels = channelManager.getNumChannels();

        // 2. Sum each channel to enabled outputs
        for (int chIdx = 0; chIdx < numChannels; ++chIdx)
        {
            const auto* ch = channelManager.getChannel(chIdx);
            if (ch == nullptr)
                continue;

            // Global channel disable output check
            if (ch->getDisableOutput())
                continue;

            // Solo filtering: if any channel is soloed, exclude non-solo channels
            if (anySolo && !ch->getSolo())
                continue;

            const auto& chBuf = ch->getOutputBuffer();
            const float* chReadL = chBuf.getReadPointer(0);
            const float* chReadR = chBuf.getReadPointer(1);

            for (int outIdx = 0; outIdx < numOutputs; ++outIdx)
            {
                if (chIdx < NUM_CHANNELS_LEVEL1 && outIdx < NUM_OUTPUT_BUSES_LEVEL1)
                {
                    if (!matrix[chIdx][outIdx].enabled.load(std::memory_order_relaxed))
                        continue;

                    auto* outBus = outputManager.getOutput(outIdx);
                    if (outBus == nullptr)
                        continue;

                    const float sendGainDb = matrix[chIdx][outIdx].sendGainDb.load(std::memory_order_relaxed);
                    const float sendLinear = GainProcessor::dbToLinear(sendGainDb);
                    if (sendLinear <= 0.0f)
                        continue;

                    auto& outBuf = outBus->getBuffer();
                    float* outWriteL = outBuf.getWritePointer(0);
                    float* outWriteR = outBuf.getWritePointer(1);

                    for (int s = 0; s < numSamples; ++s)
                    {
                        outWriteL[s] += chReadL[s] * sendLinear;
                        outWriteR[s] += chReadR[s] * sendLinear;
                    }
                }
            }
        }
    }
} // namespace dsd
