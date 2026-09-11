#include "Bus/BusManager.h"

namespace dsd
{
    BusManager::BusManager()
    {
        initializeDefaultBuses(NUM_SUBMIX_BUSES);
    }

    void BusManager::initializeDefaultBuses(int count)
    {
        buses.clear();
        buses.reserve(count);

        const std::string names[] = {
            "BUS 1: VOCALS", "BUS 2: MUSIC", "BUS 3: CHAT", "BUS 4: FX"
        };

        for (int i = 0; i < count; ++i)
        {
            std::string name = (i < 4) ? names[i] : ("BUS " + std::to_string(i + 1));
            buses.push_back(std::make_unique<AudioBus>(i + 1, name));
        }
    }

    void BusManager::prepare(double sampleRate, int maxBlockSize)
    {
        for (auto& b : buses)
        {
            if (b != nullptr)
                b->prepare(sampleRate, maxBlockSize);
        }
    }

    void BusManager::releaseResources()
    {
        for (auto& b : buses)
        {
            if (b != nullptr)
                b->releaseResources();
        }
    }

    void BusManager::processBuses(int numSamples)
    {
        for (auto& b : buses)
        {
            if (b != nullptr)
                b->processBlock(numSamples);
        }
    }

    AudioBus* BusManager::getBus(int index) noexcept
    {
        if (index >= 0 && index < static_cast<int>(buses.size()))
            return buses[index].get();
        return nullptr;
    }

    const AudioBus* BusManager::getBus(int index) const noexcept
    {
        if (index >= 0 && index < static_cast<int>(buses.size()))
            return buses[index].get();
        return nullptr;
    }
} // namespace dsd
