#pragma once

#include <algorithm>
#include <vector>

namespace dsd
{
    // Stereo delay line used for plugin delay compensation (PDC).
    //
    // Storage is allocated in prepare(); process() never allocates or locks.
    // When the requested delay changes, the output fades to silence, the read
    // position jumps, and the output fades back in, so changing the amount of
    // compensation (adding/removing/bypassing a latency plugin anywhere in the
    // mixer) does not click.
    class CompensationDelay
    {
    public:
        static constexpr int kMaxDelaySamples = 16383;

        void prepare(double sampleRate)
        {
            ring.assign(static_cast<size_t>(kRingFrames) * 2, 0.0f);
            writePos = 0;
            currentDelay = 0;
            gain = 1.0f;
            stage = Stage::Steady;
            const double fadeSamples = std::max(16.0, sampleRate * 0.003); // 3 ms each way
            fadeStep = static_cast<float>(1.0 / fadeSamples);
        }

        int getCurrentDelay() const noexcept { return currentDelay; }
        bool isTransitioning() const noexcept { return stage != Stage::Steady; }

        void process(float* left, float* right, int numSamples, int targetDelay) noexcept
        {
            if (ring.empty() || left == nullptr || right == nullptr)
                return;

            targetDelay = std::clamp(targetDelay, 0, kMaxDelaySamples);

            if (stage == Stage::Steady && targetDelay != currentDelay)
                stage = Stage::FadeOut;
            else if (stage == Stage::FadeIn && targetDelay != currentDelay)
                stage = Stage::FadeOut;

            for (int i = 0; i < numSamples; ++i)
            {
                // Always record the input so a later delay increase reads real audio.
                const size_t w = static_cast<size_t>(writePos) * 2;
                ring[w] = left[i];
                ring[w + 1] = right[i];

                if (stage == Stage::FadeOut)
                {
                    gain -= fadeStep;
                    if (gain <= 0.0f)
                    {
                        gain = 0.0f;
                        currentDelay = targetDelay;
                        stage = Stage::FadeIn;
                    }
                }
                else if (stage == Stage::FadeIn)
                {
                    gain += fadeStep;
                    if (gain >= 1.0f)
                    {
                        gain = 1.0f;
                        stage = Stage::Steady;
                    }
                }

                const size_t r = static_cast<size_t>((writePos - currentDelay) & kRingMask) * 2;
                left[i] = ring[r] * gain;
                right[i] = ring[r + 1] * gain;

                writePos = (writePos + 1) & kRingMask;
            }
        }

    private:
        static constexpr int kRingFrames = 16384; // power of two > kMaxDelaySamples
        static constexpr int kRingMask = kRingFrames - 1;

        enum class Stage { Steady, FadeOut, FadeIn };

        std::vector<float> ring;
        int writePos{0};
        int currentDelay{0};
        float gain{1.0f};
        float fadeStep{1.0f / 144.0f};
        Stage stage{Stage::Steady};
    };
} // namespace dsd
