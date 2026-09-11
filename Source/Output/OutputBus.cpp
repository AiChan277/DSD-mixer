#include "Output/OutputBus.h"
#include "Audio/MultiDeviceManager.h"

namespace dsd
{
    OutputBus::OutputBus(BusID id, const std::string& initialName, int targetDeviceChOffset)
        : busID(id), name(initialName), deviceChannelOffset(targetDeviceChOffset)
    {
    }

    OutputBus::~OutputBus() = default;

    void OutputBus::setOutputDeviceName(const std::string& name)
    {
        outputDeviceName = name;
        outputSink = MultiDeviceManager::getInstance().createOutputSinkFor(name);
    }

    bool OutputBus::hasDedicatedSink() const noexcept
    {
        return outputSink != nullptr;
    }

    void OutputBus::prepare(double sampleRate, int maxBlockSize)
    {
        busBuffer.setSize(2, maxBlockSize, false, true, true);
        busBuffer.clear();

        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        meterProcessor.prepare(sampleRate);

        if (outputSink != nullptr)
            outputSink->prepare(sampleRate, maxBlockSize);
    }

    void OutputBus::releaseResources()
    {
        busBuffer.setSize(0, 0);
        meterProcessor.reset();

        if (outputSink != nullptr)
            outputSink->releaseResources();
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

        // Render to dedicated Windows audio device if active
        if (outputSink != nullptr && !isMuted)
        {
            outputSink->writeBlock(busBuffer, numSamples);
        }
    }
} // namespace dsd
