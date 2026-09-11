#include "Bus/AudioBus.h"

namespace dsd
{
    AudioBus::AudioBus(BusID id, const std::string& busName)
        : busID(id), name(busName)
    {
    }

    void AudioBus::prepare(double sampleRate, int maxBlockSize)
    {
        busBuffer.setSize(2, maxBlockSize, false, true, true);
        busBuffer.clear();

        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        meterProcessor.prepare(sampleRate);
        pluginRack.prepare(sampleRate, maxBlockSize);
    }

    void AudioBus::releaseResources()
    {
        busBuffer.setSize(0, 0);
        meterProcessor.reset();
        pluginRack.releaseResources();
    }

    void AudioBus::setFaderDb(float db) noexcept
    {
        const float clamped = std::clamp(db, MIN_FADER_DB, MAX_FADER_DB);
        faderDb.store(clamped, std::memory_order_relaxed);
    }

    void AudioBus::processBlock(int numSamples)
    {
        if (numSamples <= 0 || busBuffer.getNumChannels() < 2)
            return;

        // 1. Bus VST3 plugins (e.g. bus glue compressor/limiter)
        midiBuffer.clear();
        pluginRack.processBlock(busBuffer, midiBuffer);

        // 2. Bus fader & mute
        const bool isMuted = mute.load(std::memory_order_relaxed);
        const float targetFader = isMuted ? -60.0f : faderDb.load(std::memory_order_relaxed);
        faderProcessor.setTargetGainDb(targetFader);
        faderProcessor.processBlock(busBuffer.getArrayOfWritePointers(), 2, numSamples);

        // 3. Bus meter
        meterProcessor.processBlock(busBuffer.getReadPointer(0),
                                    busBuffer.getReadPointer(1),
                                    numSamples,
                                    meterValues);
    }
} // namespace dsd
