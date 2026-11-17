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

        // PRE: send taken after the inserts, before fader and pan (independent
        // of the channel fader, ideal for monitor/IFB mixes). POST: default.
        bool isRoutePreFader(int channelIdx, int outputIdx) const noexcept;
        void setRoutePreFader(int channelIdx, int outputIdx, bool preFader) noexcept;
        void setRoutePan(int channelIdx, int outputIdx, float pan) noexcept;

    private:
        RouteMatrixCell matrix[NUM_CHANNELS_LEVEL1][NUM_OUTPUT_BUSES_LEVEL1];

        // Audio-thread-only smoothing state for each send. Level changes ramp
        // across one block; a route that (re)activates starts at its target, so
        // ON/OFF, solo and enable keep their immediate behaviour.
        struct SendSmoothingState
        {
            float appliedGain{0.0f};
            bool activeLastBlock{false};
            bool preFaderLastBlock{false};
        };
        SendSmoothingState sendState[NUM_CHANNELS_LEVEL1][NUM_OUTPUT_BUSES_LEVEL1];
    };
} // namespace dsd
