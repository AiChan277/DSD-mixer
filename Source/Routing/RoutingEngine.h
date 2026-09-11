#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Channel/ChannelManager.h"
#include "Output/OutputManager.h"
#include <vector>

namespace dsd
{
    class RoutingEngine
    {
    public:
        RoutingEngine();
        ~RoutingEngine() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        // Routes all 16 channels to the 4 customizable output buses via the 16x4 matrix
        void routeChannelsToOutputs(const ChannelManager& channelManager,
                                    OutputManager& outputManager,
                                    int numSamples);

        // Matrix Cell access
        bool isRouteEnabled(int channelIdx, int outputIdx) const noexcept;
        void setRouteEnabled(int channelIdx, int outputIdx, bool enabled) noexcept;

        float getRouteGainDb(int channelIdx, int outputIdx) const noexcept;
        void setRouteGainDb(int channelIdx, int outputIdx, float gainDb) noexcept;

        float getRoutePan(int channelIdx, int outputIdx) const noexcept;
        void setRoutePan(int channelIdx, int outputIdx, float pan) noexcept;

    private:
        RouteMatrixCell matrix[NUM_CHANNELS_LEVEL1][NUM_OUTPUT_BUSES_LEVEL1];
    };
} // namespace dsd
