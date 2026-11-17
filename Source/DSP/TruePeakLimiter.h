#pragma once

#include "DSP/TruePeakDetector.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace dsd
{
    // Look-ahead stereo-linked true-peak limiter for output buses.
    // When enabled it delays the bus by kLookaheadSamples (+ detector delay);
    // when disabled it is a pure pass-through with zero latency.
    // Enabling/disabling crossfades so the switch itself does not click.
    class TruePeakLimiter
    {
    public:
        static constexpr int kLookaheadMs10 = 15; // 1.5 ms

        void prepare(double sampleRate)
        {
            sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
            lookahead = static_cast<int>(sr * kLookaheadMs10 / 10000.0);
            delayLen = lookahead + TruePeakDetector::latency();
            delayL.assign(static_cast<size_t>(delayLen), 0.0f); // ring length == delay
            delayR.assign(static_cast<size_t>(delayLen), 0.0f);
            window.assign(static_cast<size_t>(lookahead + 1), 1.0f);
            writePos = 0;
            windowPos = 0;
            gain = 1.0f;
            mix = enabled.load() ? 1.0f : 0.0f;
            detL.reset();
            detR.reset();
            attackCoeff = static_cast<float>(1.0 - std::exp(-5.0 / std::max(1, lookahead))); // ~99% within lookahead
            mixStep = static_cast<float>(1.0 / (sr * 0.010));
        }

        void setEnabled(bool on) noexcept { enabled.store(on, std::memory_order_relaxed); }
        bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }
        void setCeilingDb(float db) noexcept { ceilingDb.store(std::clamp(db, -12.0f, 0.0f), std::memory_order_relaxed); }
        float getCeilingDb() const noexcept { return ceilingDb.load(std::memory_order_relaxed); }
        void setReleaseMs(float ms) noexcept { releaseMs.store(std::clamp(ms, 10.0f, 1000.0f), std::memory_order_relaxed); }
        float getReleaseMs() const noexcept { return releaseMs.load(std::memory_order_relaxed); }

        float getGainReductionDb() const noexcept { return grDb.load(std::memory_order_relaxed); }
        int getLatencySamples() const noexcept { return isEnabled() ? delayLen : 0; }

        void process(float* l, float* r, int n) noexcept
        {
            if (delayL.empty() || l == nullptr || r == nullptr)
                return;

            const bool on = enabled.load(std::memory_order_relaxed);
            if (!on && mix <= 0.0f)
            {
                grDb.store(0.0f, std::memory_order_relaxed);
                // Keep the delay line primed so enabling later is seamless.
                for (int i = 0; i < n; ++i)
                {
                    detL.push(l[i]);
                    detR.push(r[i]);
                    pushDelay(l[i], r[i]);
                }
                return;
            }

            const float ceiling = std::pow(10.0f, ceilingDb.load(std::memory_order_relaxed) / 20.0f);
            // 4x oversampled detection can under-read inter-sample peaks near
            // fs/4 by a few tenths of a dB; aim slightly below the ceiling.
            const float detectCeiling = ceiling * kDetectionMargin;
            const float releaseCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (sr * releaseMs.load(std::memory_order_relaxed) * 0.001)));
            float minGain = 1.0f;

            for (int i = 0; i < n; ++i)
            {
                const float inL = l[i], inR = r[i];
                const float tp = std::max(detL.push(inL), detR.push(inR));
                const float needed = tp > detectCeiling ? detectCeiling / tp : 1.0f;

                window[static_cast<size_t>(windowPos)] = needed;
                windowPos = (windowPos + 1) % static_cast<int>(window.size());
                float target = 1.0f;
                for (float w : window)
                    target = std::min(target, w);

                gain += (target < gain ? attackCoeff : releaseCoeff) * (target - gain);
                minGain = std::min(minGain, gain);

                float dl, dr;
                popDelay(dl, dr);
                pushDelay(inL, inR);

                float outL = dl * gain, outR = dr * gain;
                // Safety: anything still over the ceiling is clipped to it.
                outL = std::clamp(outL, -ceiling, ceiling);
                outR = std::clamp(outR, -ceiling, ceiling);

                // Crossfade between dry (undelayed) and limited when toggling.
                mix = on ? std::min(1.0f, mix + mixStep) : std::max(0.0f, mix - mixStep);
                l[i] = inL + (outL - inL) * mix;
                r[i] = inR + (outR - inR) * mix;
            }

            grDb.store(20.0f * std::log10(std::max(minGain, 1.0e-6f)), std::memory_order_relaxed);
        }

    private:
        inline void pushDelay(float a, float b) noexcept
        {
            delayL[static_cast<size_t>(writePos)] = a;
            delayR[static_cast<size_t>(writePos)] = b;
            writePos = (writePos + 1) % static_cast<int>(delayL.size());
        }
        inline void popDelay(float& a, float& b) const noexcept
        {
            // Oldest sample = the one at writePos, written delayLen samples ago.
            a = delayL[static_cast<size_t>(writePos)];
            b = delayR[static_cast<size_t>(writePos)];
        }

        static constexpr float kDetectionMargin = 0.9550f; // -0.4 dB

        double sr{48000.0};
        int lookahead{72};
        int delayLen{77};
        std::vector<float> delayL, delayR, window;
        int writePos{0}, windowPos{0};
        float gain{1.0f}, attackCoeff{0.07f}, mix{0.0f}, mixStep{0.002f};
        TruePeakDetector detL, detR;

        std::atomic<bool> enabled{false};
        std::atomic<float> ceilingDb{-1.0f};
        std::atomic<float> releaseMs{100.0f};
        std::atomic<float> grDb{0.0f};
    };
} // namespace dsd
