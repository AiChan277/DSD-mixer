#include "Master/MasterBus.h"

namespace dsd
{
    MasterBus::MasterBus()
    {
    }

    void MasterBus::prepare(double sampleRate, int /*maxBlockSize*/)
    {
        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        meterProcessor.prepare(sampleRate);
    }

    void MasterBus::releaseResources()
    {
        meterProcessor.reset();
    }

    void MasterBus::setFaderDb(float db) noexcept
    {
        const float clamped = std::clamp(db, MIN_FADER_DB, MAX_FADER_DB);
        faderDb.store(clamped, std::memory_order_relaxed);
    }

    void MasterBus::processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples)
    {
        if (numSamples <= 0 || outputBuffer.getNumChannels() < 2)
            return;

        const bool isMuted = mute.load(std::memory_order_relaxed);
        const float targetFader = isMuted ? -60.0f : faderDb.load(std::memory_order_relaxed);
        faderProcessor.setTargetGainDb(targetFader);
        faderProcessor.processBlock(outputBuffer.getArrayOfWritePointers(), 2, numSamples);

        // Meter processing on master output
        meterProcessor.processBlock(outputBuffer.getReadPointer(0),
                                    outputBuffer.getReadPointer(1),
                                    numSamples,
                                    meterValues);
    }
} // namespace dsd
