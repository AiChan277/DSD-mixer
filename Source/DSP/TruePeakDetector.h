#pragma once

#include <array>
#include <cmath>

namespace dsd
{
    // 4x oversampled peak estimator (ITU-R BS.1770 style true-peak).
    // Polyphase windowed-sinc interpolation of the three in-between phases.
    class TruePeakDetector
    {
    public:
        static constexpr int kTaps = 12;
        static constexpr int kPhases = 4;

        TruePeakDetector()
        {
            const double pi = 3.14159265358979323846;
            for (int p = 1; p < kPhases; ++p)
            {
                const double frac = static_cast<double>(p) / kPhases;
                double sum = 0.0;
                for (int k = 0; k < kTaps; ++k)
                {
                    const double t = static_cast<double>(k - kTaps / 2 + 1) - frac;
                    const double sinc = std::abs(t) < 1e-12 ? 1.0 : std::sin(pi * t) / (pi * t);
                    const double w = 0.42 - 0.5 * std::cos(2.0 * pi * (k + 0.5) / kTaps)
                                   + 0.08 * std::cos(4.0 * pi * (k + 0.5) / kTaps);
                    coeffs[static_cast<size_t>(p)][static_cast<size_t>(k)] = static_cast<float>(sinc * w);
                    sum += sinc * w;
                }
                for (int k = 0; k < kTaps; ++k)
                    coeffs[static_cast<size_t>(p)][static_cast<size_t>(k)] /= static_cast<float>(sum);
            }
            reset();
        }

        void reset() noexcept
        {
            history.fill(0.0f);
            pos = 0;
        }

        // Feeds one sample, returns the true-peak magnitude around the sample
        // that is now kTaps/2 - 1 samples old (constant detector delay).
        inline float push(float x) noexcept
        {
            history[static_cast<size_t>(pos)] = x;
            history[static_cast<size_t>(pos + kTaps)] = x; // mirrored for contiguous reads
            pos = (pos + 1) % kTaps;

            const float* h = &history[static_cast<size_t>(pos)]; // oldest .. newest
            float peak = std::abs(h[kTaps / 2 - 1]);
            for (int p = 1; p < kPhases; ++p)
            {
                const auto& c = coeffs[static_cast<size_t>(p)];
                float acc = 0.0f;
                for (int k = 0; k < kTaps; ++k)
                    acc += c[static_cast<size_t>(k)] * h[k];
                peak = std::max(peak, std::abs(acc));
            }
            return peak;
        }

        static constexpr int latency() noexcept { return kTaps / 2 - 1; }

    private:
        std::array<std::array<float, kTaps>, kPhases> coeffs{};
        std::array<float, kTaps * 2> history{};
        int pos{0};
    };
} // namespace dsd
