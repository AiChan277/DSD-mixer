#pragma once

#include <algorithm>
#include <cmath>

namespace dsd
{
    class PanProcessor
    {
    public:
        PanProcessor()
        {
            setPan(0.0f);
        }

        // pan in range [-1.0f, +1.0f]. The new gains are reached by a linear
        // ramp across the next processed block, so moving the pan knob does not
        // produce zipper noise. getGainL()/getGainR() report the target gains.
        void setPan(float panVal) noexcept
        {
            currentPan = std::clamp(panVal, -1.0f, 1.0f);
            // Constant-power angle: theta in [0, pi/2]
            constexpr float piOverFour = 0.7853981633974483f; // pi / 4
            const float angle = (currentPan + 1.0f) * piOverFour;

            gainL = std::cos(angle);
            gainR = std::sin(angle);
        }

        // Jump straight to the target gains (no ramp on the next block).
        void snapToTarget() noexcept
        {
            appliedL = gainL;
            appliedR = gainR;
        }

        float getPan() const noexcept { return currentPan; }
        float getGainL() const noexcept { return gainL; }
        float getGainR() const noexcept { return gainR; }

        inline void processStereo(float* leftChannel, float* rightChannel, int numSamples) noexcept
        {
            if (numSamples <= 0)
                return;

            if (appliedL == gainL && appliedR == gainR)
            {
                // If pan is centered (gainL == gainR ~= 0.7071), we still scale to maintain power law
                for (int i = 0; i < numSamples; ++i)
                {
                    leftChannel[i]  *= gainL;
                    rightChannel[i] *= gainR;
                }
                return;
            }

            const float stepL = (gainL - appliedL) / static_cast<float>(numSamples);
            const float stepR = (gainR - appliedR) / static_cast<float>(numSamples);
            for (int i = 0; i < numSamples; ++i)
            {
                const float t = static_cast<float>(i + 1);
                leftChannel[i]  *= appliedL + stepL * t;
                rightChannel[i] *= appliedR + stepR * t;
            }
            snapToTarget();
        }

        inline void processMonoToStereo(const float* monoIn, float* leftOut, float* rightOut, int numSamples) const noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float s = monoIn[i];
                leftOut[i]  = s * gainL;
                rightOut[i] = s * gainR;
            }
        }

    private:
        float currentPan{0.0f};
        float gainL{0.70710678f};
        float gainR{0.70710678f};
        float appliedL{0.70710678f}; // gains actually applied at the end of the last block
        float appliedR{0.70710678f};
    };
} // namespace dsd
