#pragma once

// DSD Control Protocol — State Observer (Stage B)
//
// Periodically polls the AudioEngine's atomic mixer parameters via juce::Timer,
// detects state changes using dirty-checking diffs, and publishes delta events
// to registered listeners (such as the ControlServer in Stage C).
//
// Also provides:
//   - Full state snapshot on demand (for sync messages)
//   - Optional opt-in metering polling with rate limiting (min 50ms interval)
//   - Manual pollState() for deterministic testing without timer dependency

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "Control/DSDCPTypes.h"
#include "Control/MixerStateSnapshot.h"
#include <vector>
#include <algorithm>

namespace dsd
{
    class AudioEngine;
}

namespace dsd::control
{
    class StateObserver : public juce::Timer
    {
    public:
        class Listener
        {
        public:
            virtual ~Listener() = default;
            // Called on the message thread when one or more mixer parameters changed.
            virtual void onStateEvents(const std::vector<juce::String>& events) = 0;
            // Called on the message thread with metering readings if metering is enabled.
            virtual void onMeterEvent(const juce::String& meterJson) {}
        };

        explicit StateObserver(AudioEngine& engine, int timerHz = 30);
        ~StateObserver() override;

        // Timer lifecycle
        void start();
        void stop();
        void setTimerHz(int hz);
        int getTimerHz() const noexcept { return timerHz_; }

        // Listener management (must be called from message thread)
        void addListener(Listener* l);
        void removeListener(Listener* l);

        // Manual polling (safe to call from message thread or in tests)
        // Detects parameter changes and triggers onStateEvents() if deltas found.
        std::vector<juce::String> pollState();

        // Manual meter polling (generates meters event and triggers onMeterEvent)
        juce::String pollMeters();

        // Metering configuration
        void setMeteringEnabled(bool enabled) noexcept { meteringEnabled_ = enabled; }
        bool isMeteringEnabled() const noexcept { return meteringEnabled_; }

        void setMeterIntervalMs(int intervalMs) noexcept
        {
            meterIntervalMs_ = std::max(DSDCP_METER_MIN_INTERVAL_MS, intervalMs);
        }
        int getMeterIntervalMs() const noexcept { return meterIntervalMs_; }

        // Full state snapshot (e.g. for handshake "sync" message)
        juce::String getFullSyncJson() const;
        juce::var getFullSyncVar() const;

        const MixerStateSnapshot& getLastSnapshot() const noexcept { return lastSnapshot_; }

        // juce::Timer callback
        void timerCallback() override;

    private:
        AudioEngine& engine_;
        int timerHz_ { 30 };
        MixerStateSnapshot lastSnapshot_;

        std::vector<Listener*> listeners_;

        bool meteringEnabled_ { false };
        int meterIntervalMs_ { 100 }; // default 100ms
        juce::uint32 lastMeterTimeMs_ { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateObserver)
    };

} // namespace dsd::control
