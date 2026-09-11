#pragma once

#include "Channel/AudioChannel.h"
#include <vector>
#include <memory>
#include <mutex>

namespace dsd
{
    class ChannelManager
    {
    public:
        ChannelManager();
        ~ChannelManager() = default;

        void initializeDefaultChannels(int count = NUM_CHANNELS_LEVEL1);

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processChannels(const juce::AudioBuffer<float>& deviceInputBuffer, int numSamples);

        int getNumChannels() const noexcept { return static_cast<int>(channels.size()); }
        AudioChannel* getChannel(int index) noexcept;
        const AudioChannel* getChannel(int index) const noexcept;

        bool hasAnySoloChannel() const noexcept;

    private:
        std::vector<std::unique_ptr<AudioChannel>> channels;
    };
} // namespace dsd
