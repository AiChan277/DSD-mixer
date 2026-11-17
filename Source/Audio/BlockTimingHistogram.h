#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace dsd
{
    // Distribution of audio-callback processing time as a fraction of the
    // block deadline. Averages hide the rare slow block that causes a click;
    // this shows how often each load band occurs and where the p95/p99 lie.
    //
    // record() is called by the single audio thread: lock-free, allocation-free.
    // snapshot()/reset() may be called from the UI thread; the snapshot is
    // built from relaxed loads, so it can be off by a block or two while the
    // audio thread is writing, which is fine for display.
    class BlockTimingHistogram
    {
    public:
        // Upper edge of each bucket as a fraction of the deadline. The final
        // bucket (index kNumBuckets - 1) collects every block over 100%.
        static constexpr std::array<float, 10> kUpperEdges{0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                                                           0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        static constexpr int kNumBuckets = static_cast<int>(kUpperEdges.size()) + 1;

        struct Snapshot
        {
            std::array<uint64_t, kNumBuckets> counts{};
            uint64_t total{0};

            // Upper edge (fraction of deadline) of the bucket holding the given
            // percentile; values above 1.0 mean "over the deadline". 0 if empty.
            float percentileUpperEdge(double percentile) const noexcept
            {
                if (total == 0)
                    return 0.0f;

                const double target = std::ceil(static_cast<double>(total) * percentile / 100.0);
                uint64_t cumulative = 0;
                for (int i = 0; i < kNumBuckets; ++i)
                {
                    cumulative += counts[static_cast<size_t>(i)];
                    if (static_cast<double>(cumulative) >= target)
                        return i < static_cast<int>(kUpperEdges.size()) ? kUpperEdges[static_cast<size_t>(i)] : 1.5f;
                }
                return 1.5f;
            }
        };

        static constexpr int bucketFor(double fractionOfDeadline) noexcept
        {
            for (int i = 0; i < static_cast<int>(kUpperEdges.size()); ++i)
            {
                if (fractionOfDeadline <= static_cast<double>(kUpperEdges[static_cast<size_t>(i)]))
                    return i;
            }
            return kNumBuckets - 1;
        }

        void record(double elapsedSec, double deadlineSec) noexcept
        {
            if (!(deadlineSec > 0.0) || !std::isfinite(elapsedSec) || elapsedSec < 0.0)
                return;

            const int bucket = bucketFor(elapsedSec / deadlineSec);
            counts[static_cast<size_t>(bucket)].fetch_add(1, std::memory_order_relaxed);
        }

        Snapshot snapshot() const noexcept
        {
            Snapshot s;
            for (int i = 0; i < kNumBuckets; ++i)
            {
                s.counts[static_cast<size_t>(i)] = counts[static_cast<size_t>(i)].load(std::memory_order_relaxed);
                s.total += s.counts[static_cast<size_t>(i)];
            }
            return s;
        }

        void reset() noexcept
        {
            for (auto& c : counts)
                c.store(0, std::memory_order_relaxed);
        }

    private:
        std::array<std::atomic<uint64_t>, kNumBuckets> counts{};
    };
} // namespace dsd
