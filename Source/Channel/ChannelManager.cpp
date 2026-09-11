#include "Channel/ChannelManager.h"

namespace dsd
{
    ChannelManager::ChannelManager()
    {
        initializeDefaultChannels(NUM_CHANNELS_MVP);
    }

    void ChannelManager::initializeDefaultChannels(int count)
    {
        channels.clear();
        channels.reserve(count);

        const std::string defaultNames[] = {
            "MIC 01", "DISCORD", "CHROME", "GAME",
            "LINE IN", "VIRTUAL", "AUX 1", "AUX 2"
        };

        for (int i = 0; i < count; ++i)
        {
            std::string name = (i < 8) ? defaultNames[i] : ("CH " + std::to_string(i + 1));
            auto channel = std::make_unique<AudioChannel>(i + 1, name);
            // Default input channel mapping: CH1 -> Dev 0/1, CH2 -> Dev 0/1, etc.
            // On MiniFuse or stereo device, maps stereo 0/1
            channel->setInputSource(std::make_unique<HardwareInputSource>(0, 1));
            channels.push_back(std::move(channel));
        }
    }

    void ChannelManager::prepare(double sampleRate, int maxBlockSize)
    {
        for (auto& ch : channels)
        {
            if (ch != nullptr)
                ch->prepare(sampleRate, maxBlockSize);
        }
    }

    void ChannelManager::releaseResources()
    {
        for (auto& ch : channels)
        {
            if (ch != nullptr)
                ch->releaseResources();
        }
    }

    void ChannelManager::processChannels(const juce::AudioBuffer<float>& deviceInputBuffer, int numSamples)
    {
        for (auto& ch : channels)
        {
            if (ch != nullptr)
                ch->processBlock(deviceInputBuffer, numSamples);
        }
    }

    AudioChannel* ChannelManager::getChannel(int index) noexcept
    {
        if (index >= 0 && index < static_cast<int>(channels.size()))
            return channels[index].get();
        return nullptr;
    }

    const AudioChannel* ChannelManager::getChannel(int index) const noexcept
    {
        if (index >= 0 && index < static_cast<int>(channels.size()))
            return channels[index].get();
        return nullptr;
    }

    bool ChannelManager::hasAnySoloChannel() const noexcept
    {
        for (const auto& ch : channels)
        {
            if (ch != nullptr && ch->getSolo())
                return true;
        }
        return false;
    }
} // namespace dsd
