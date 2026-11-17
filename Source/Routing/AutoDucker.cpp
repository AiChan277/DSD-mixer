#include "Routing/AutoDucker.h"
#include "Channel/ChannelManager.h"
#include <algorithm>
#include <cmath>

namespace dsd
{
    void AutoDucker::prepare(double sr) noexcept
    {
        sampleRate = sr > 1000.0 ? sr : 48000.0;
        reset();
    }

    void AutoDucker::reset() noexcept
    {
        for (auto& r : rules)
        {
            r.gainDb = 0.0f;
            r.holdRemaining = 0;
            r.currentGainDb.store(0.0f, std::memory_order_relaxed);
            r.triggered.store(false, std::memory_order_relaxed);
        }
        lastChannelDb.fill(0.0f);
        for (auto& v : channelDbUi)
            v.store(0.0f, std::memory_order_relaxed);
    }

    float AutoDucker::getChannelDuckDb(int channel) const noexcept
    {
        if (channel < 0 || channel >= kMaxChannels)
            return 0.0f;
        return channelDbUi[static_cast<size_t>(channel)].load(std::memory_order_relaxed);
    }

    void AutoDucker::process(ChannelManager& channels, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        const int numChannels = std::min(channels.getNumChannels(), kMaxChannels);
        std::array<float, kMaxChannels> targetDb{};
        targetDb.fill(0.0f);
        bool anyActivity = false;

        const double blockSec = static_cast<double>(numSamples) / sampleRate;

        for (auto& rule : rules)
        {
            const int trig = rule.triggerChannel.load(std::memory_order_relaxed);
            const uint32_t mask = rule.targetMask.load(std::memory_order_relaxed);
            const bool usable = rule.enabled.load(std::memory_order_relaxed) && trig >= 0 && trig < numChannels && mask != 0;

            float wantedDb = 0.0f;
            if (usable)
            {
                float keyPeak = 0.0f;
                if (const auto* ch = channels.getChannel(trig))
                {
                    if (!ch->getMute() && !ch->getDisableOutput())
                    {
                        const auto& buf = ch->getOutputBuffer(); // post-fader key
                        keyPeak = std::max(buf.getMagnitude(0, 0, numSamples), buf.getMagnitude(1, 0, numSamples));
                    }
                }

                const float threshold = std::pow(10.0f, rule.thresholdDb.load(std::memory_order_relaxed) / 20.0f);
                const int holdSamples = static_cast<int>(rule.holdMs.load(std::memory_order_relaxed) * 0.001 * sampleRate);

                if (keyPeak >= threshold)
                    rule.holdRemaining = holdSamples + numSamples;

                const bool active = rule.holdRemaining > 0;
                rule.holdRemaining = std::max(0, rule.holdRemaining - numSamples);
                rule.triggered.store(active, std::memory_order_relaxed);
                wantedDb = active ? std::min(0.0f, rule.depthDb.load(std::memory_order_relaxed)) : 0.0f;
            }
            else
            {
                rule.holdRemaining = 0;
                rule.triggered.store(false, std::memory_order_relaxed);
            }

            const bool goingDown = wantedDb < rule.gainDb;
            const float ms = goingDown ? rule.attackMs.load(std::memory_order_relaxed) : rule.releaseMs.load(std::memory_order_relaxed);
            const double tau = std::max(0.5, static_cast<double>(ms)) * 0.001;
            const float coeff = static_cast<float>(1.0 - std::exp(-blockSec / tau));
            rule.gainDb += coeff * (wantedDb - rule.gainDb);
            if (std::abs(rule.gainDb) < 0.01f && wantedDb == 0.0f)
                rule.gainDb = 0.0f;
            rule.currentGainDb.store(rule.gainDb, std::memory_order_relaxed);

            if (rule.gainDb < 0.0f && usable)
            {
                for (int c = 0; c < numChannels; ++c)
                    if ((mask >> c) & 1u)
                        if (c != trig)
                            targetDb[static_cast<size_t>(c)] = std::min(targetDb[static_cast<size_t>(c)], rule.gainDb);
                anyActivity = true;
            }
        }

        // Apply per channel as a linear ramp from last block's gain to this block's.
        for (int c = 0; c < numChannels; ++c)
        {
            const float startDb = lastChannelDb[static_cast<size_t>(c)];
            const float endDb = targetDb[static_cast<size_t>(c)];
            if (startDb == 0.0f && endDb == 0.0f)
                continue;

            if (auto* ch = channels.getChannel(c))
            {
                const float g0 = std::pow(10.0f, startDb / 20.0f);
                const float g1 = std::pow(10.0f, endDb / 20.0f);
                ch->applyOutputGainRamp(g0, g1, numSamples);
            }
            lastChannelDb[static_cast<size_t>(c)] = endDb;
            channelDbUi[static_cast<size_t>(c)].store(endDb, std::memory_order_relaxed);
        }
        (void)anyActivity;
    }
} // namespace dsd
