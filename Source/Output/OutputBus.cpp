#include "Output/OutputBus.h"

namespace dsd
{
    OutputBus::OutputBus(BusID id, const std::string& initialName, int targetDeviceChOffset)
        : busID(id), name(initialName), deviceChannelOffset(targetDeviceChOffset)
    {
    }

    void OutputBus::prepare(double sampleRate, int maxBlockSize)
    {
        busBuffer.setSize(2, maxBlockSize, false, true, true);
        busBuffer.clear();

        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        meterProcessor.prepare(sampleRate);
    }

    void OutputBus::releaseResources()
    {
        busBuffer.setSize(0, 0);
        meterProcessor.reset();
    }

    void OutputBus::setFaderDb(float db) noexcept
    {
        const float clamped = std::clamp(db, MIN_FADER_DB, MAX_FADER_DB);
        faderDb.store(clamped, std::memory_order_relaxed);
    }

    void OutputBus::processBlock(int numSamples)
    {
        if (numSamples <= 0 || busBuffer.getNumChannels() < 2)
            return;

        const bool isMuted = mute.load(std::memory_order_relaxed);
        const float targetFader = isMuted ? -60.0f : faderDb.load(std::memory_order_relaxed);
        faderProcessor.setTargetGainDb(targetFader);
        faderProcessor.processBlock(busBuffer.getArrayOfWritePointers(), 2, numSamples);

        meterProcessor.processBlock(busBuffer.getReadPointer(0),
                                    busBuffer.getReadPointer(1),
                                    numSamples,
                                    meterValues);
    }
} // namespace dsd
