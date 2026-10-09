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
        currentSampleRate = sampleRate;
        currentBlockSize = maxBlockSize;
        isPrepared = true;

        channelBuffer.setSize(2, maxBlockSize, false, true, true);
        channelBuffer.clear();

        pflBuffer.setSize(2, maxBlockSize, false, true, true);
        pflBuffer.clear();

        dmBuffer.setSize(2, maxBlockSize, false, true, true);
        dmBuffer.clear();

        gainProcessor.prepare(sampleRate);
        gainProcessor.reset(GainProcessor::dbToLinear(gainDb.load()));

        faderProcessor.prepare(sampleRate);
        faderProcessor.reset(GainProcessor::dbToLinear(faderDb.load()));

        hpfFilter.prepare(sampleRate);
        hpfFilter.setCutoffHz(80.0);
        hpfFilter.setBypassed(!hpfEnabled.load(std::memory_order_relaxed));

        meterInputProc.prepare(sampleRate);
        meterPostGainProc.prepare(sampleRate);
        meterPostVSTProc.prepare(sampleRate);
        meterProcessor.prepare(sampleRate);

        pluginRack.prepare(sampleRate, maxBlockSize);

        std::lock_guard<std::mutex> lock(sourceMutex);
        if (inputSource != nullptr)
            inputSource->prepare(sampleRate, maxBlockSize);
    }

    void AudioChannel::releaseResources()
    {
        isPrepared = false;
        channelBuffer.setSize(0, 0);
        pflBuffer.setSize(0, 0);
        dmBuffer.setSize(0, 0);

        hpfFilter.reset();

        meterInputProc.reset();
        meterPostGainProc.reset();
        meterPostVSTProc.reset();
        meterProcessor.reset();
        pluginRack.releaseResources();

        std::lock_guard<std::mutex> lock(sourceMutex);
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

    void AudioChannel::setDirectMonitor(bool dm) noexcept
    {
        directMonitor.store(dm, std::memory_order_relaxed);
        if (dm)
            pfl.store(false, std::memory_order_relaxed);
    }

    void AudioChannel::setPfl(bool enable) noexcept
    {
        pfl.store(enable, std::memory_order_relaxed);
        if (enable)
            directMonitor.store(false, std::memory_order_relaxed);
    }

    void AudioChannel::setHpfEnabled(bool enable) noexcept
    {
        hpfEnabled.store(enable, std::memory_order_relaxed);
        hpfFilter.setBypassed(!enable);
    }

    void AudioChannel::setInputSource(std::unique_ptr<AudioInputSource> source)
    {
        std::unique_ptr<AudioInputSource> oldSource;
        {
            std::lock_guard<std::mutex> lock(sourceMutex);
            oldSource = std::move(inputSource);
            inputSource = std::move(source);
            if (inputSource != nullptr && isPrepared)
                inputSource->prepare(currentSampleRate, currentBlockSize);
        }
    }

    void AudioChannel::processBlock(const juce::AudioBuffer<float>& deviceInputBuffer, int numSamples)
    {
        if (numSamples <= 0)
            return;

        if (channelBuffer.getNumSamples() < numSamples)
            channelBuffer.setSize(2, std::max(currentBlockSize, numSamples), false, true, true);

        // 1. Stage A: Raw Input Acquisition (Lock-free non-blocking try_lock)
        {
            std::unique_lock<std::mutex> lock(sourceMutex, std::try_to_lock);
            if (lock.owns_lock() && inputSource != nullptr)
                inputSource->readBlock(channelBuffer, deviceInputBuffer, numSamples);
            else
                channelBuffer.clear(0, numSamples);
        }

        // Meter Stage A: Raw Input PRE-GAIN (Measures incoming signal BEFORE PAD or any channel processing)
        meterInputProc.processBlock(channelBuffer.getReadPointer(0),
                                    channelBuffer.getReadPointer(1),
                                    numSamples,
                                    meterInputValues);

        // 2. PAD Attenuation (-20 dB ≈ 0.1x linear) applied after Stage A meter
        if (padEnabled.load(std::memory_order_relaxed))
        {
            channelBuffer.applyGain(0, 0, numSamples, 0.1f);
            channelBuffer.applyGain(1, 0, numSamples, 0.1f);
        }

        // 3. Polarity / Phase Invert
        if (phaseInvert.load(std::memory_order_relaxed))
        {
            channelBuffer.applyGain(0, 0, numSamples, -1.0f);
            channelBuffer.applyGain(1, 0, numSamples, -1.0f);
        }

        // 4. Force Mono (sum left & right)
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

        // 5. Pre-fader Input Gain Stage
        gainProcessor.setTargetGainDb(gainDb.load(std::memory_order_relaxed));
        gainProcessor.processBlock(channelBuffer.getArrayOfWritePointers(), 2, numSamples);

        // Meter Stage B: Post-Gain / Pre-HPF / Pre-VST
        meterPostGainProc.processBlock(channelBuffer.getReadPointer(0),
                                       channelBuffer.getReadPointer(1),
                                       numSamples,
                                       meterPostGainValues);

        // 6. High-Pass Filter (80 Hz 2nd-order Butterworth)
        hpfFilter.setBypassed(!hpfEnabled.load(std::memory_order_relaxed));
        hpfFilter.processBlock(channelBuffer.getWritePointer(0),
                               channelBuffer.getWritePointer(1),
                               numSamples);

        // 7. Per-Channel VST3 Plugin Rack
        midiBuffer.clear();
        pluginRack.processBlock(channelBuffer, midiBuffer, numSamples);

        // Meter Stage C: Post-VST
        meterPostVSTProc.processBlock(channelBuffer.getReadPointer(0),
                                      channelBuffer.getReadPointer(1),
                                      numSamples,
                                      meterPostVSTValues);

        // 8. PFL signal tap: captured POST-VST / PRE-FADER
        if (pflBuffer.getNumSamples() < numSamples)
            pflBuffer.setSize(2, std::max(currentBlockSize, numSamples), false, true, true);
        pflBuffer.copyFrom(0, 0, channelBuffer.getReadPointer(0), numSamples);
        pflBuffer.copyFrom(1, 0, channelBuffer.getReadPointer(1), numSamples);

        // 9. Constant-power Pan Processing
        panProcessor.setPan(pan.load(std::memory_order_relaxed));
        panProcessor.processStereo(channelBuffer.getWritePointer(0),
                                   channelBuffer.getWritePointer(1),
                                   numSamples);

        // 10. Post-fader Gain & Mute Processing
        const bool isMuted = mute.load(std::memory_order_relaxed);
        const float targetFader = isMuted ? -60.0f : faderDb.load(std::memory_order_relaxed);
        faderProcessor.setTargetGainDb(targetFader);
        faderProcessor.processBlock(channelBuffer.getArrayOfWritePointers(), 2, numSamples);

        // 11. Meter Stage D: Post-Fader
        meterProcessor.processBlock(channelBuffer.getReadPointer(0),
                                    channelBuffer.getReadPointer(1),
                                    numSamples,
                                    meterValues);

        // 12. Direct Monitor (DM) signal tap: captured POST-FADER
        if (dmBuffer.getNumSamples() < numSamples)
            dmBuffer.setSize(2, std::max(currentBlockSize, numSamples), false, true, true);
        dmBuffer.copyFrom(0, 0, channelBuffer.getReadPointer(0), numSamples);
        dmBuffer.copyFrom(1, 0, channelBuffer.getReadPointer(1), numSamples);
    }

    const MeterValues& AudioChannel::getMeter(MeterStage stage) const noexcept
    {
        switch (stage)
        {
            case MeterStage::PreGain:   return meterInputValues;
            case MeterStage::PostGain:  return meterPostGainValues;
            case MeterStage::PostVST:   return meterPostVSTValues;
            case MeterStage::PostFader: return meterValues;
        }
        return meterInputValues;
    }

    MeterValues& AudioChannel::getMeter(MeterStage stage) noexcept
    {
        switch (stage)
        {
            case MeterStage::PreGain:   return meterInputValues;
            case MeterStage::PostGain:  return meterPostGainValues;
            case MeterStage::PostVST:   return meterPostVSTValues;
            case MeterStage::PostFader: return meterValues;
        }
        return meterInputValues;
    }
} // namespace dsd
