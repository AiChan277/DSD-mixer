#pragma once

// DSD Control Protocol — Command Dispatcher  (Stage A)
//
// CommandDispatcher is the single entry point for all DSDCP control commands.
// It validates parameters and calls the existing public setters on
// AudioChannel, OutputBus, and RoutingEngine — exactly the same setters the
// desktop UI uses.  No audio-engine internals are touched; no second state
// model is introduced.
//
// Threading: execute() must be called from the JUCE message thread.
// The setters it calls are all lock-free atomic stores, matching what the
// existing UI already does from the message thread.

#include "Control/DSDCPTypes.h"
#include "Audio/AudioEngine.h"

namespace dsd::control
{
    class CommandDispatcher
    {
    public:
        // engine must outlive this dispatcher.
        explicit CommandDispatcher(AudioEngine& engine) noexcept;

        // Execute a validated command.
        // Must be called on the JUCE message thread.
        // Returns a DSDCPResponse whose id mirrors command.id.
        DSDCPResponse execute(const DSDCPCommand& command);

    private:
        AudioEngine& engine_;

        // --- Channel command handlers ---
        DSDCPResponse doChannelMute   (const DSDCPCommand& cmd);
        DSDCPResponse doChannelGain   (const DSDCPCommand& cmd);
        DSDCPResponse doChannelFader  (const DSDCPCommand& cmd);
        DSDCPResponse doChannelPan    (const DSDCPCommand& cmd);
        DSDCPResponse doChannelPfl    (const DSDCPCommand& cmd);
        DSDCPResponse doChannelDm     (const DSDCPCommand& cmd);
        DSDCPResponse doChannelSolo   (const DSDCPCommand& cmd);
        DSDCPResponse doChannelOn     (const DSDCPCommand& cmd);
        DSDCPResponse doChannelPhase  (const DSDCPCommand& cmd);
        DSDCPResponse doChannelMono   (const DSDCPCommand& cmd);
        DSDCPResponse doChannelHpf    (const DSDCPCommand& cmd);
        DSDCPResponse doChannelPad    (const DSDCPCommand& cmd);
        DSDCPResponse doChannelName   (const DSDCPCommand& cmd);

        // --- Output command handlers ---
        DSDCPResponse doOutputMute    (const DSDCPCommand& cmd);
        DSDCPResponse doOutputFader   (const DSDCPCommand& cmd);
        DSDCPResponse doOutputMonitor (const DSDCPCommand& cmd);
        DSDCPResponse doOutputName    (const DSDCPCommand& cmd);

        // --- Route command handlers ---
        DSDCPResponse doRouteEnable   (const DSDCPCommand& cmd);
        DSDCPResponse doRouteGain     (const DSDCPCommand& cmd);
        DSDCPResponse doRoutePan      (const DSDCPCommand& cmd);

        // --- Utility ---
        DSDCPResponse doSyncRequest   (const DSDCPCommand& cmd);

        // Helper: resolve a channel pointer and return an error response if it
        // is null (index already validated at a higher level, but defensive).
        AudioChannel* resolveChannel(int idx, const juce::String& cmdId,
                                     DSDCPResponse& outErr);

        OutputBus*    resolveOutput  (int idx, const juce::String& cmdId,
                                     DSDCPResponse& outErr);
    };

} // namespace dsd::control
