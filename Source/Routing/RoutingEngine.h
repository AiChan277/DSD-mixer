#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Channel/ChannelManager.h"
#include <vector>

namespace dsd
{
    struct RouteRule
    {
        ChannelID sourceChannelId;
        BusID destinationBusId;
        float sendGainLinear{1.0f};
        bool enabled{true};
    };

    class RoutingEngine
    {
    public:
        RoutingEngine() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        // Accumulates routed channel outputs into the target destination buffer
        void routeChannelsToMaster(const ChannelManager& channelManager,
                                   juce::AudioBuffer<float>& masterBuffer,
                                   int numSamples);

    private:
        // Future: full dynamic routing matrix storage
    };
} // namespace dsd
