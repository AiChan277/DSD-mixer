#pragma once

#include <cmath>

namespace dsd
{
    // 2nd-order Butterworth High-Pass Filter (Direct Form II Transposed)
    // Default cutoff: 80 Hz. Stereo processing.
    class BiquadFilter
    {
    public:
        BiquadFilter() = default;

        void prepare(double sampleRate) noexcept
        {
            currentSampleRate = sampleRate;
            recalculateCoefficients();
            reset();
        }

        void setCutoffHz(double hz) noexcept
        {
            if (hz != cutoffHz)
            {
                cutoffHz = hz;
                recalculateCoefficients();
            }
        }

        double getCutoffHz() const noexcept { return cutoffHz; }

        void setBypassed(bool bypass) noexcept { bypassed = bypass; }
        bool isBypassed() const noexcept { return bypassed; }

        void reset() noexcept
        {
            z1L = z2L = z1R = z2R = 0.0;
        }

        void processBlock(float* left, float* right, int numSamples) noexcept
        {
            if (bypassed)
                return;

            for (int i = 0; i < numSamples; ++i)
            {
                // Left channel
                {
                    double x = static_cast<double>(left[i]);
                    double y = b0 * x + z1L;
                    z1L = b1 * x - a1 * y + z2L;
                    z2L = b2 * x - a2 * y;
                    left[i] = static_cast<float>(y);
                }
                // Right channel
                {
                    double x = static_cast<double>(right[i]);
                    double y = b0 * x + z1R;
                    z1R = b1 * x - a1 * y + z2R;
                    z2R = b2 * x - a2 * y;
                    right[i] = static_cast<float>(y);
                }
            }
        }

    private:
        void recalculateCoefficients() noexcept
        {
            // Butterworth 2nd-order HPF via bilinear transform
            const double pi = 3.14159265358979323846;
            const double w0 = 2.0 * pi * cutoffHz / currentSampleRate;
            const double cosw0 = std::cos(w0);
            const double sinw0 = std::sin(w0);
            // Q = 1/sqrt(2) for Butterworth
            const double alpha = sinw0 / (2.0 * 0.7071067811865476);

            const double a0_inv = 1.0 / (1.0 + alpha);

            b0 = ((1.0 + cosw0) / 2.0) * a0_inv;
            b1 = (-(1.0 + cosw0)) * a0_inv;
            b2 = ((1.0 + cosw0) / 2.0) * a0_inv;
            a1 = (-2.0 * cosw0) * a0_inv;
            a2 = (1.0 - alpha) * a0_inv;
        }

        double currentSampleRate{48000.0};
        double cutoffHz{80.0};
        bool bypassed{true};

        // Coefficients
        double b0{1.0}, b1{0.0}, b2{0.0};
        double a1{0.0}, a2{0.0};

        // State (Direct Form II Transposed) — per channel
        double z1L{0.0}, z2L{0.0};
        double z1R{0.0}, z2R{0.0};
    };
} // namespace dsd
