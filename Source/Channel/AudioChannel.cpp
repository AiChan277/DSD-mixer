#include "Channel/AudioChannel.h"

namespace dsd
{
    AudioChannel::AudioChannel(ChannelID id, const std::string& channelName)
        : channelID(id), name(channelName)
    {
        inputSource = std::make_unique<NullInputSource>();
    }

    void AudioChannel::prepare(double sampleRate, int maxBlockSize)
    {
        channelBuffer.setSize(2, maxBlockSize, false, true, true);
        channelBuffer.clear();

        gainProcessor.prepare(sampleRate);
        gainProcessor.reset(GainProcessor::dbToLinear(gainDb.load()));

        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        meterInputProc.prepare(sampleRate);
        meterPostGainProc.prepare(sampleRate);
        meterPostVSTProc.prepare(sampleRate);
        meterProcessor.prepare(sampleRate);

        pluginRack.prepare(sampleRate, maxBlockSize);

        if (inputSource != nullptr)
            inputSource->prepare(sampleRate, maxBlockSize);
    }

    void AudioChannel::releaseResources()
    {
        channelBuffer.setSize(0, 0);
        meterInputProc.reset();
        meterPostGainProc.reset();
        meterPostVSTProc.reset();
        meterProcessor.reset();
        pluginRack.releaseResources();

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

        // 1. Stage A: Input Acquisition
        if (inputSource != nullptr)
            inputSource->readBlock(channelBuffer, deviceInputBuffer, numSamples);
        else
            channelBuffer.clear(0, numSamples);

        // Meter Stage A (Raw Input)
        meterInputProc.processBlock(channelBuffer.getReadPointer(0),
                                    channelBuffer.getReadPointer(1),
                                    numSamples,
                                    meterInputValues);

        // 2. Phase Invert
        if (phaseInvert.load(std::memory_order_relaxed))
        {
            channelBuffer.applyGain(0, 0, numSamples, -1.0f);
            channelBuffer.applyGain(1, 0, numSamples, -1.0f);
        }

        // 3. Force Mono (sum left & right)
        if (forceMono.load(std::memory_order_relaxed))
        {
            float* l = channelBuffer.getWritePointer(0);
            float* r = channelBuffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                const float mono = (l[i] + r[i]) * 0.5f;
                l[i] = mono;
                r[i] = mono;
            }
        }

        // 4. Pre-fader Gain Stage
        gainProcessor.setTargetGainDb(gainDb.load(std::memory_order_relaxed));
        gainProcessor.processBlock(channelBuffer.getArrayOfWritePointers(), 2, numSamples);

        // Meter Stage B (Post-Gain)
        meterPostGainProc.processBlock(channelBuffer.getReadPointer(0),
                                       channelBuffer.getReadPointer(1),
                                       numSamples,
                                       meterPostGainValues);

        // 5. Per-Channel VST3 Plugin Rack
        midiBuffer.clear();
        pluginRack.processBlock(channelBuffer, midiBuffer);

        // Meter Stage C (Post-VST)
        meterPostVSTProc.processBlock(channelBuffer.getReadPointer(0),
                                      channelBuffer.getReadPointer(1),
                                      numSamples,
                                      meterPostVSTValues);

        // 6. Constant-power Pan Processing
        panProcessor.setPan(pan.load(std::memory_order_relaxed));
        panProcessor.processStereo(channelBuffer.getWritePointer(0),
                                   channelBuffer.getWritePointer(1),
                                   numSamples);

        // 7. Post-fader Gain & Mute Processing
        const bool isMuted = mute.load(std::memory_order_relaxed);
        const float targetFader = isMuted ? -60.0f : faderDb.load(std::memory_order_relaxed);
        faderProcessor.setTargetGainDb(targetFader);
        faderProcessor.processBlock(channelBuffer.getArrayOfWritePointers(), 2, numSamples);

        // 8. Meter Stage D (Post-Fader)
        meterProcessor.processBlock(channelBuffer.getReadPointer(0),
                                    channelBuffer.getReadPointer(1),
                                    numSamples,
                                    meterValues);
    }
} // namespace dsd
