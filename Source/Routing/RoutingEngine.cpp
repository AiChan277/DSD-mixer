#include "Routing/RoutingEngine.h"

namespace dsd
{
    void RoutingEngine::prepare(double /*sampleRate*/, int /*maxBlockSize*/)
    {
    }

    void RoutingEngine::releaseResources()
    {
    }

    void RoutingEngine::routeChannelsToMaster(const ChannelManager& channelManager,
                                             juce::AudioBuffer<float>& masterBuffer,
                                             int numSamples)
    {
        // First clear destination master buffer
        masterBuffer.clear(0, numSamples);

        const bool anySolo = channelManager.hasAnySoloChannel();
        const int numChannels = channelManager.getNumChannels();

        for (int i = 0; i < numChannels; ++i)
        {
            const auto* ch = channelManager.getChannel(i);
            if (ch == nullptr)
                continue;

            // Check disableOutput switch (from user sketch: "Disable output")
            if (ch->getDisableOutput())
                continue;

            // If any channel in the mixer has Solo enabled, mute all non-solo channels
            if (anySolo && !ch->getSolo())
                continue;

            // Channel is active and routed to master: accumulate into master buffer
            const auto& chBuf = ch->getOutputBuffer();
            if (chBuf.getNumChannels() >= 2 && masterBuffer.getNumChannels() >= 2)
            {
                masterBuffer.addFrom(0, 0, chBuf, 0, 0, numSamples);
                masterBuffer.addFrom(1, 0, chBuf, 1, 0, numSamples);
            }
        }
    }
} // namespace dsd
