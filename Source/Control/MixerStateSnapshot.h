#pragma once

// DSD Control Protocol — Mixer State Snapshot & Diffing  (Stage B)
//
// Represents an immutable snapshot of all controllable parameters in the mixer:
//   - 16 Channel strips
//   - 4 Output buses
//   - 16x4 Routing matrix
//
// Thread-safety: captureSnapshot() reads atomic values from AudioEngine subsystems.
// Snapshots themselves are value types and can be compared, copied, and serialized
// from any thread (typically the JUCE message thread).

#include <juce_core/juce_core.h>
#include "Audio/AudioTypes.h"
#include <array>
#include <string>
#include <vector>
#include <cmath>

namespace dsd
{
    class AudioEngine;
}

namespace dsd::control
{
    struct ChannelSnapshot
    {
        int index { 0 };
        std::string name;
        bool mute { false };
        float gainDb { 0.0f };
        float faderDb { 0.0f };
        float pan { 0.0f };
        bool pfl { false };
        bool dm { false };
        bool solo { false };
        bool on { true };       // true when output is NOT disabled (!disableOutput)
        bool phase { false };
        bool mono { false };
        bool hpf { false };
        bool pad { false };

        bool operator==(const ChannelSnapshot& o) const noexcept
        {
            return index == o.index &&
                   name == o.name &&
                   mute == o.mute &&
                   std::abs(gainDb - o.gainDb) < 0.001f &&
                   std::abs(faderDb - o.faderDb) < 0.001f &&
                   std::abs(pan - o.pan) < 0.001f &&
                   pfl == o.pfl &&
                   dm == o.dm &&
                   solo == o.solo &&
                   on == o.on &&
                   phase == o.phase &&
                   mono == o.mono &&
                   hpf == o.hpf &&
                   pad == o.pad;
        }

        bool operator!=(const ChannelSnapshot& o) const noexcept { return !(*this == o); }
    };

    struct OutputSnapshot
    {
        int index { 0 };
        std::string name;
        bool mute { false };
        float faderDb { 0.0f };
        bool monitor { false };

        bool operator==(const OutputSnapshot& o) const noexcept
        {
            return index == o.index &&
                   name == o.name &&
                   mute == o.mute &&
                   std::abs(faderDb - o.faderDb) < 0.001f &&
                   monitor == o.monitor;
        }

        bool operator!=(const OutputSnapshot& o) const noexcept { return !(*this == o); }
    };

    struct RouteSnapshot
    {
        int channel { 0 };
        int output { 0 };
        bool enabled { false };
        float sendGainDb { 0.0f };
        float sendPan { 0.0f };

        bool operator==(const RouteSnapshot& o) const noexcept
        {
            return channel == o.channel &&
                   output == o.output &&
                   enabled == o.enabled &&
                   std::abs(sendGainDb - o.sendGainDb) < 0.001f &&
                   std::abs(sendPan - o.sendPan) < 0.001f;
        }

        bool operator!=(const RouteSnapshot& o) const noexcept { return !(*this == o); }
    };

    struct MixerStateSnapshot
    {
        std::array<ChannelSnapshot, NUM_CHANNELS_LEVEL1> channels;
        std::array<OutputSnapshot, NUM_OUTPUT_BUSES_LEVEL1> outputs;
        std::array<std::array<RouteSnapshot, NUM_OUTPUT_BUSES_LEVEL1>, NUM_CHANNELS_LEVEL1> routing;
    };

    // Capture the current state of all engine subsystems into a snapshot.
    MixerStateSnapshot captureSnapshot(const AudioEngine& engine);

    // Compute delta events between two snapshots.
    // Returns a list of JSON-encoded DSDCP "event" strings ready for broadcast.
    std::vector<juce::String> diffSnapshots(const MixerStateSnapshot& previous,
                                            const MixerStateSnapshot& current);

    // Create a DSDCP "channel.state" event JSON string.
    juce::String makeChannelStateEventJson(const ChannelSnapshot& ch);

    // Create a DSDCP "output.state" event JSON string.
    juce::String makeOutputStateEventJson(const OutputSnapshot& out);

    // Create a DSDCP "route.state" event JSON string.
    juce::String makeRouteStateEventJson(const RouteSnapshot& route);

    // Create a full DSDCP "sync" message from a snapshot.
    juce::var makeSyncVar(const MixerStateSnapshot& snapshot);
    juce::String makeSyncJson(const MixerStateSnapshot& snapshot);

    // Create a DSDCP "meters" event JSON string from current engine readings.
    juce::String makeMetersEventJson(const AudioEngine& engine);

} // namespace dsd::control
