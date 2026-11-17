#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace dsd
{
    class ChannelManager;

    // Automatic ducking ("music dips under the mic").
    //
    // Up to kMaxRules rules. Each rule watches one trigger channel's post-fader
    // signal (so a muted / switched-off mic never ducks anything) and lowers
    // a set of target channels by `depthDb` while the trigger is above
    // `thresholdDb`, with hold, attack and release. Parameters are atomics
    // written by the UI; process() runs on the audio thread between channel
    // DSP and routing and never allocates or locks.
    class AutoDucker
    {
    public:
        static constexpr int kMaxRules = 4;

        struct Rule
        {
            std::atomic<bool> enabled{false};
            std::atomic<int> triggerChannel{-1};
            std::atomic<uint32_t> targetMask{0};   // bit n = channel n is ducked
            std::atomic<float> thresholdDb{-35.0f};
            std::atomic<float> depthDb{-12.0f};
            std::atomic<float> attackMs{15.0f};
            std::atomic<float> holdMs{250.0f};
            std::atomic<float> releaseMs{500.0f};

            // Live gain applied by this rule (dB, <= 0), for the UI.
            std::atomic<float> currentGainDb{0.0f};
            std::atomic<bool> triggered{false};

            // Audio-thread state
            float gainDb{0.0f};
            int holdRemaining{0};
        };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;
        void process(ChannelManager& channels, int numSamples) noexcept;

        Rule& getRule(int index) noexcept { return rules[static_cast<size_t>(index)]; }
        const Rule& getRule(int index) const noexcept { return rules[static_cast<size_t>(index)]; }

        // Combined live ducking gain on a channel (dB, <= 0), for the UI.
        float getChannelDuckDb(int channel) const noexcept;

    private:
        static constexpr int kMaxChannels = 32;

        double sampleRate{48000.0};
        std::array<Rule, kMaxRules> rules;
        std::array<float, kMaxChannels> lastChannelDb{};          // audio thread
        std::array<std::atomic<float>, kMaxChannels> channelDbUi{}; // UI readout
    };
} // namespace dsd
