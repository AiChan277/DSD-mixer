#pragma once

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

        // pan in range [-1.0f, +1.0f]
        void setPan(float panVal) noexcept
        {
            currentPan = std::clamp(panVal, -1.0f, 1.0f);
            // Constant-power angle: theta in [0, pi/2]
            constexpr float piOverFour = 0.7853981633974483f; // pi / 4
            const float angle = (currentPan + 1.0f) * piOverFour;

            gainL = std::cos(angle);
            gainR = std::sin(angle);
        }

        float getPan() const noexcept { return currentPan; }
        float getGainL() const noexcept { return gainL; }
        float getGainR() const noexcept { return gainR; }

        inline void processStereo(float* leftChannel, float* rightChannel, int numSamples) const noexcept
        {
            // If pan is centered (gainL == gainR ~= 0.7071), we still scale to maintain power law
            for (int i = 0; i < numSamples; ++i)
            {
                leftChannel[i]  *= gainL;
                rightChannel[i] *= gainR;
            }
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
    };
} // namespace dsd
