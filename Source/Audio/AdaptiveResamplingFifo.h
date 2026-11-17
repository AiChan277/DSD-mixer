#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dsd
{
    // Single-producer / single-consumer bridge between two audio clocks.
    //
    // The producer pushes frames at its own rate (e.g. a second WASAPI device).
    // The consumer pulls exactly the frames it needs at its rate. A cubic
    // (Catmull-Rom) resampler converts between the nominal rates, and a PI
    // controller trims the resampling ratio by a few hundred ppm so the buffer
    // fill stays at a target. Result: constant latency, no periodic sample
    // drops, no clicks from clock drift.
    //
    // prepare()/reset() must be called with both streams stopped.
    // push()/pull() never allocate, lock or block.
    class AdaptiveResamplingFifo
    {
    public:
        static constexpr double kMaxCorrectionPpm = 2000.0;

        bool prepare(double producerRate, double consumerRate, int numChannels = 2, int capacityFrames = 32768)
        {
            if (!(producerRate > 1000.0) || !(consumerRate > 1000.0) || numChannels < 1 || numChannels > 8)
                return false;

            int cap = 1024;
            while (cap < capacityFrames)
                cap <<= 1;

            channels = numChannels;
            capacity = cap;
            mask = static_cast<uint64_t>(cap - 1);
            nominalStep = producerRate / consumerRate;
            producerSampleRate = producerRate;
            consumerSampleRate = consumerRate;
            ring.assign(static_cast<size_t>(cap) * static_cast<size_t>(channels), 0.0f);
            minimumMarginFrames = std::max(96.0, producerRate * 0.002); // >= 2 ms jitter allowance
            reset();
            return true;
        }

        void reset() noexcept
        {
            writeCount.store(0, std::memory_order_relaxed);
            readTail.store(0, std::memory_order_relaxed);
            maxPushFrames.store(0, std::memory_order_relaxed);
            underruns.store(0, std::memory_order_relaxed);
            overrunFrames.store(0, std::memory_order_relaxed);
            correctionPpmSnapshot.store(0.0, std::memory_order_relaxed);
            fillSnapshot.store(0.0, std::memory_order_relaxed);
            targetSnapshot.store(0.0, std::memory_order_relaxed);
            readPos = 0.0;
            primeFloor = 0.0;
            primed = false;
            outputGain = 0.0f;
            integratorPpm = 0.0;
            correctionPpm = 0.0;
            fillAverage = -1.0;
            marginFrames = minimumMarginFrames;
        }

        // ------------------------------------------------------------ producer
        void push(const float* const* src, int numSrcChannels, int numFrames) noexcept
        {
            if (capacity == 0 || src == nullptr || numFrames <= 0 || numSrcChannels <= 0)
                return;

            int prevMax = maxPushFrames.load(std::memory_order_relaxed);
            if (numFrames > prevMax)
                maxPushFrames.store(numFrames, std::memory_order_relaxed);

            const uint64_t w = writeCount.load(std::memory_order_relaxed);
            const uint64_t tail = readTail.load(std::memory_order_acquire);
            const uint64_t used = w - std::min(w, tail);
            const int space = static_cast<int>(std::max<int64_t>(0, static_cast<int64_t>(capacity) - static_cast<int64_t>(used)));
            const int accepted = std::min(space, numFrames);

            for (int i = 0; i < accepted; ++i)
            {
                const size_t base = static_cast<size_t>((w + static_cast<uint64_t>(i)) & mask) * static_cast<size_t>(channels);
                for (int c = 0; c < channels; ++c)
                {
                    const float* s = src[std::min(c, numSrcChannels - 1)];
                    const float v = (s != nullptr) ? s[i] : 0.0f;
                    ring[base + static_cast<size_t>(c)] = std::isfinite(v) ? v : 0.0f;
                }
            }

            writeCount.store(w + static_cast<uint64_t>(accepted), std::memory_order_release);
            if (accepted < numFrames)
                overrunFrames.fetch_add(static_cast<uint64_t>(numFrames - accepted), std::memory_order_relaxed);
        }

        // ------------------------------------------------------------ consumer
        void pull(float* const* dst, int numDstChannels, int numFrames) noexcept
        {
            if (dst == nullptr || numFrames <= 0)
                return;

            if (capacity == 0)
            {
                clearOutput(dst, numDstChannels, 0, numFrames);
                return;
            }

            const uint64_t w = writeCount.load(std::memory_order_acquire);
            const double target = computeTargetFill(numFrames);
            targetSnapshot.store(target, std::memory_order_relaxed);

            if (!primed)
            {
                // Only fresh input counts: never replay frames already played
                // before an underrun.
                if (static_cast<double>(w) - primeFloor >= target + 4.0)
                {
                    // Start exactly `target` frames behind the newest input. The
                    // integrator (learned clock drift) survives a re-prime.
                    readPos = static_cast<double>(w) - target;
                    primed = true;
                    fillAverage = target;
                }
                else
                {
                    clearOutput(dst, numDstChannels, 0, numFrames);
                    return;
                }
            }

            // Fill measured right before consuming; averaged to remove the sawtooth.
            const double fill = static_cast<double>(w) - readPos;
            const double alpha = std::clamp(static_cast<double>(numFrames) / (consumerSampleRate * 2.0), 0.0005, 0.2);
            fillAverage = (fillAverage < 0.0) ? fill : fillAverage + alpha * (fill - fillAverage);
            fillSnapshot.store(fillAverage, std::memory_order_relaxed);

            // Way too far behind (e.g. consumer stalled): jump forward once.
            if (fill > target + static_cast<double>(capacity) * 0.5)
            {
                readPos = static_cast<double>(w) - target;
                fillAverage = target;
                integratorPpm = 0.0;
            }

            // PI controller: positive error (too much buffered) -> read faster.
            const double error = fillAverage - target;
            const double dt = static_cast<double>(numFrames) / consumerSampleRate;
            const double proposedIntegrator = std::clamp(integratorPpm + error * kIntegralGain * dt,
                                                         -kMaxCorrectionPpm, kMaxCorrectionPpm);
            const double proposed = error * kProportionalGain + proposedIntegrator;
            correctionPpm = std::clamp(proposed, -kMaxCorrectionPpm, kMaxCorrectionPpm);
            if (correctionPpm == proposed)
                integratorPpm = proposedIntegrator; // anti-windup
            correctionPpmSnapshot.store(correctionPpm, std::memory_order_relaxed);

            const double step = nominalStep * (1.0 + correctionPpm * 1.0e-6);

            for (int i = 0; i < numFrames; ++i)
            {
                const double base = std::floor(readPos);
                const auto idx = static_cast<uint64_t>(base);

                if (idx < 1 || idx + 2 >= w)
                {
                    // Ran out of input: fade out the rest of this block and re-prime.
                    underruns.fetch_add(1, std::memory_order_relaxed);
                    primed = false;
                    primeFloor = readPos;
                    marginFrames = std::min(marginFrames * 1.5, consumerSampleRate * 0.05);
                    fadeTail(dst, numDstChannels, i, numFrames);
                    break;
                }

                const float t = static_cast<float>(readPos - base);
                outputGain = std::min(1.0f, outputGain + kGainStep);

                for (int c = 0; c < numDstChannels; ++c)
                {
                    if (dst[c] == nullptr)
                        continue;
                    const int sc = std::min(c, channels - 1);
                    const float y0 = sampleAt(idx - 1, sc);
                    const float y1 = sampleAt(idx, sc);
                    const float y2 = sampleAt(idx + 1, sc);
                    const float y3 = sampleAt(idx + 2, sc);
                    dst[c][i] = catmullRom(y0, y1, y2, y3, t) * outputGain;
                }

                readPos += step;
            }

            const double tailPos = std::floor(readPos) - 2.0;
            readTail.store(tailPos > 0.0 ? static_cast<uint64_t>(tailPos) : 0, std::memory_order_release);
        }

        // ------------------------------------------------------------ diagnostics (any thread)
        // Buffered audio expressed in milliseconds (fill is counted in producer frames).
        double getAverageFillMs() const noexcept { return getAverageFillFrames() * 1000.0 / producerSampleRate; }
        double getCorrectionPpm() const noexcept { return correctionPpmSnapshot.load(std::memory_order_relaxed); }
        double getAverageFillFrames() const noexcept { return fillSnapshot.load(std::memory_order_relaxed); }
        double getTargetFillFrames() const noexcept { return targetSnapshot.load(std::memory_order_relaxed); }
        uint64_t getUnderrunCount() const noexcept { return underruns.load(std::memory_order_relaxed); }
        uint64_t getOverrunFrames() const noexcept { return overrunFrames.load(std::memory_order_relaxed); }

    private:
        // Gentle loop (~7 s time constant) on a 2 s fill average: the measured
        // fill wobbles as the push/pull timing phases rotate, and a stiff loop
        // turns that into needless ratio wobble. Simulated at 44.1/48 kHz with
        // up to +-300 ppm drift: mean ratio exact, wobble about +-30 ppm, fill
        // within 0.5 ms of target.
        static constexpr double kProportionalGain = 3.0;  // ppm per frame of error
        static constexpr double kIntegralGain = 0.1;      // ppm per frame-second of error
        static constexpr float kGainStep = 1.0f / 64.0f;

        double computeTargetFill(int consumerFrames) const noexcept
        {
            const double push = static_cast<double>(maxPushFrames.load(std::memory_order_relaxed));
            const double target = push + static_cast<double>(consumerFrames) * nominalStep + marginFrames;
            return std::min(target, static_cast<double>(capacity) * 0.4);
        }

        float sampleAt(uint64_t frame, int channel) const noexcept
        {
            return ring[static_cast<size_t>(frame & mask) * static_cast<size_t>(channels) + static_cast<size_t>(channel)];
        }

        static float catmullRom(float y0, float y1, float y2, float y3, float t) noexcept
        {
            const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
            const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c = -0.5f * y0 + 0.5f * y2;
            return ((a * t + b) * t + c) * t + y1;
        }

        static void clearOutput(float* const* dst, int numDstChannels, int from, int to) noexcept
        {
            for (int c = 0; c < numDstChannels; ++c)
                if (dst[c] != nullptr)
                    std::fill(dst[c] + from, dst[c] + to, 0.0f);
        }

        void fadeTail(float* const* dst, int numDstChannels, int from, int to) noexcept
        {
            // Last produced samples ramp down; everything after is silence.
            const int fadeLen = std::min(from, 32);
            for (int c = 0; c < numDstChannels; ++c)
            {
                if (dst[c] == nullptr)
                    continue;
                for (int k = 0; k < fadeLen; ++k)
                    dst[c][from - fadeLen + k] *= 1.0f - static_cast<float>(k + 1) / static_cast<float>(fadeLen + 1);
            }
            clearOutput(dst, numDstChannels, from, to);
            outputGain = 0.0f;
        }

        // Shared
        std::vector<float> ring;
        int channels{2};
        int capacity{0};
        uint64_t mask{0};
        double nominalStep{1.0};
        double consumerSampleRate{48000.0};
        double producerSampleRate{48000.0};
        double minimumMarginFrames{96.0};

        std::atomic<uint64_t> writeCount{0};
        std::atomic<uint64_t> readTail{0};
        std::atomic<int> maxPushFrames{0};
        std::atomic<uint64_t> underruns{0};
        std::atomic<uint64_t> overrunFrames{0};
        std::atomic<double> correctionPpmSnapshot{0.0};
        std::atomic<double> fillSnapshot{0.0};
        std::atomic<double> targetSnapshot{0.0};

        // Consumer-owned
        double readPos{0.0};
        double primeFloor{0.0}; // input position below which frames were already consumed
        bool primed{false};
        float outputGain{0.0f};
        double integratorPpm{0.0};
        double correctionPpm{0.0};
        double fillAverage{-1.0};
        double marginFrames{96.0};
    };
} // namespace dsd
