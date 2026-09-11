#pragma once

#include "Bus/AudioBus.h"
#include <vector>
#include <memory>

namespace dsd
{
    class BusManager
    {
    public:
        BusManager();
        ~BusManager() = default;

        void initializeDefaultBuses(int count = NUM_SUBMIX_BUSES);

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processBuses(int numSamples);

        int getNumBuses() const noexcept { return static_cast<int>(buses.size()); }
        AudioBus* getBus(int index) noexcept;
        const AudioBus* getBus(int index) const noexcept;

    private:
        std::vector<std::unique_ptr<AudioBus>> buses;
    };
} // namespace dsd
