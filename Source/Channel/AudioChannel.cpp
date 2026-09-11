#include "Channel/AudioChannel.h"

namespace dsd
{
    AudioChannel::AudioChannel(ChannelID id, const std::string& channelName)
        : channelID(id), name(channelName)
    {
        // Default hardware mapping: CH 1 uses input 0/1, CH 2 uses 0/1 or mono 1, etc.
        inputSource = std::make_unique<HardwareInputSource>(0, 1);
    }

    void AudioChannel::prepare(double sampleRate, int maxBlockSize)
    {
        // Preallocate 2-channel stereo buffer for the channel
        channelBuffer.setSize(2, maxBlockSize, false, true, true);
        channelBuffer.clear();

        gainProcessor.prepare(sampleRate);
        gainProcessor.reset(GainProcessor::dbToLinear(gainDb.load()));

        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        meterProcessor.prepare(sampleRate);

        if (inputSource != nullptr)
            inputSource->prepare(sampleRate, maxBlockSize);
    }

    void AudioChannel::releaseResources()
    {
        channelBuffer.setSize(0, 0);
        meterProcessor.reset();
        if (inputSource != nullptr)
            inputSource->releaseResources();
    }

    void AudioChannel::setGainDb(float db) noexcept
    {
        const float clamped = std::clamp(db, MIN_GAIN_DB, MAX_GAIN_DB);
        gainDb.store(clamped, std::memory_order_relaxed);
    }

    void AudioChannel::setFaderDb(float db) noexcept
    {
        const float clamped = std::clamp(db, MIN_FADER_DB, MAX_FADER_DB);
        faderDb.store(clamped, std::memory_order_relaxed);
    }

    void AudioChannel::setPan(float panVal) noexcept
    {
        const float clamped = std::clamp(panVal, -1.0f, 1.0f);
        pan.store(clamped, std::memory_order_relaxed);
    }

    void AudioChannel::setInputSource(std::unique_ptr<AudioInputSource> source)
    {
        inputSource = std::move(source);
    }

    void AudioChannel::processBlock(const juce::AudioBuffer<float>& deviceInputBuffer, int numSamples)
    {
        if (numSamples <= 0 || channelBuffer.getNumSamples() < numSamples)
            return;

        // 1. Input acquisition into preallocated channelBuffer
        if (inputSource != nullptr)
        {
            inputSource->readBlock(channelBuffer, deviceInputBuffer, numSamples);
        }
        else
        {
            channelBuffer.clear(0, numSamples);
        }

        // 2. Gain Stage
        gainProcessor.setTargetGainDb(gainDb.load(std::memory_order_relaxed));
        gainProcessor.processBlock(channelBuffer.getArrayOfWritePointers(), 2, numSamples);

        // 3. Pan Processing
        panProcessor.setPan(pan.load(std::memory_order_relaxed));
        panProcessor.processStereo(channelBuffer.getWritePointer(0),
                                   channelBuffer.getWritePointer(1),
                                   numSamples);

        // 4. Fader & Mute Processing
        const bool isMuted = mute.load(std::memory_order_relaxed);
        const float targetFader = isMuted ? -60.0f : faderDb.load(std::memory_order_relaxed);
        faderProcessor.setTargetGainDb(targetFader);
        faderProcessor.processBlock(channelBuffer.getArrayOfWritePointers(), 2, numSamples);

        // 5. Meter calculation on processed audio
        meterProcessor.processBlock(channelBuffer.getReadPointer(0),
                                    channelBuffer.getReadPointer(1),
                                    numSamples,
                                    meterValues);
    }
} // namespace dsd
