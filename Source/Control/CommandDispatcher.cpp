#include "Control/CommandDispatcher.h"
#include "Channel/ChannelManager.h"
#include "Output/OutputManager.h"
#include "Routing/RoutingEngine.h"
#include "Channel/AudioChannel.h"
#include "Output/OutputBus.h"

namespace dsd::control
{
    CommandDispatcher::CommandDispatcher(AudioEngine& engine) noexcept
        : engine_(engine)
    {
    }

    // =========================================================================
    // Main dispatch
    // =========================================================================

    DSDCPResponse CommandDispatcher::execute(const DSDCPCommand& command)
    {
        switch (command.method)
        {
            // --- Channel ---
            case CommandMethod::ChannelMute:    return doChannelMute   (command);
            case CommandMethod::ChannelGain:    return doChannelGain   (command);
            case CommandMethod::ChannelFader:   return doChannelFader  (command);
            case CommandMethod::ChannelPan:     return doChannelPan    (command);
            case CommandMethod::ChannelPfl:     return doChannelPfl    (command);
            case CommandMethod::ChannelDm:      return doChannelDm     (command);
            case CommandMethod::ChannelSolo:    return doChannelSolo   (command);
            case CommandMethod::ChannelOn:      return doChannelOn     (command);
            case CommandMethod::ChannelPhase:   return doChannelPhase  (command);
            case CommandMethod::ChannelMono:    return doChannelMono   (command);
            case CommandMethod::ChannelHpf:     return doChannelHpf    (command);
            case CommandMethod::ChannelPad:     return doChannelPad    (command);
            case CommandMethod::ChannelName:    return doChannelName   (command);

            // --- Output ---
            case CommandMethod::OutputMute:     return doOutputMute    (command);
            case CommandMethod::OutputFader:    return doOutputFader   (command);
            case CommandMethod::OutputMonitor:  return doOutputMonitor (command);
            case CommandMethod::OutputName:     return doOutputName    (command);

            // --- Route ---
            case CommandMethod::RouteEnable:    return doRouteEnable   (command);
            case CommandMethod::RouteGain:      return doRouteGain     (command);
            case CommandMethod::RoutePan:       return doRoutePan      (command);

            // --- Utility ---
            case CommandMethod::SyncRequest:    return doSyncRequest   (command);

            case CommandMethod::Unknown:
            default:
                return DSDCPResponse::fail(command.id, ErrorCode::UnknownMethod,
                                           "Unknown command method");
        }
    }

    // =========================================================================
    // Pointer resolution helpers
    // =========================================================================

    AudioChannel* CommandDispatcher::resolveChannel(int idx,
                                                    const juce::String& cmdId,
                                                    DSDCPResponse& outErr)
    {
        AudioChannel* ch = engine_.getChannelManager().getChannel(idx);
        if (ch == nullptr)
        {
            outErr = DSDCPResponse::fail(cmdId, ErrorCode::InternalError,
                                         juce::String("Channel ") + juce::String(idx) + " not available");
        }
        return ch;
    }

    OutputBus* CommandDispatcher::resolveOutput(int idx,
                                                const juce::String& cmdId,
                                                DSDCPResponse& outErr)
    {
        OutputBus* out = engine_.getOutputManager().getOutput(idx);
        if (out == nullptr)
        {
            outErr = DSDCPResponse::fail(cmdId, ErrorCode::InternalError,
                                         juce::String("Output ") + juce::String(idx) + " not available");
        }
        return out;
    }

    // =========================================================================
    // Channel command handlers
    // =========================================================================

    DSDCPResponse CommandDispatcher::doChannelMute(const DSDCPCommand& cmd)
    {
        int  idx  = 0;
        bool val  = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx))  return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val))  return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setMute(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelGain(const DSDCPCommand& cmd)
    {
        int   idx = 0;
        float val = 0.0f;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx))                       return *e;
        if (auto e = requireFloatValue  (cmd.params, cmd.id, MIN_GAIN_DB, MAX_GAIN_DB, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setGainDb(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelFader(const DSDCPCommand& cmd)
    {
        int   idx = 0;
        float val = 0.0f;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx))                         return *e;
        if (auto e = requireFloatValue  (cmd.params, cmd.id, MIN_FADER_DB, MAX_FADER_DB, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setFaderDb(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelPan(const DSDCPCommand& cmd)
    {
        int   idx = 0;
        float val = 0.0f;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx))               return *e;
        if (auto e = requireFloatValue  (cmd.params, cmd.id, -1.0f, 1.0f, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setPan(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelPfl(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        // setPfl() already clears directMonitor when enabling — mutual exclusivity
        // is fully handled by the existing AudioChannel setter.
        ch->setPfl(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelDm(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        // setDirectMonitor() already clears pfl when enabling.
        ch->setDirectMonitor(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelSolo(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setSolo(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelOn(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        // "on" = true means the channel is active → disableOutput = false.
        ch->setDisableOutput(!val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelPhase(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setPhaseInvert(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelMono(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setForceMono(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelHpf(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setHpfEnabled(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelPad(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue   (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setPadEnabled(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doChannelName(const DSDCPCommand& cmd)
    {
        int          idx = 0;
        juce::String val;
        if (auto e = requireChannelIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireStringValue (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* ch = resolveChannel(idx, cmd.id, err);
        if (!ch) return err;

        ch->setName(val.toStdString());
        return DSDCPResponse::ok(cmd.id);
    }

    // =========================================================================
    // Output command handlers
    // =========================================================================

    DSDCPResponse CommandDispatcher::doOutputMute(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireOutputIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue  (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* out = resolveOutput(idx, cmd.id, err);
        if (!out) return err;

        out->setMute(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doOutputFader(const DSDCPCommand& cmd)
    {
        int   idx = 0;
        float val = 0.0f;
        if (auto e = requireOutputIndex(cmd.params, cmd.id, idx))                          return *e;
        if (auto e = requireFloatValue (cmd.params, cmd.id, MIN_FADER_DB, MAX_FADER_DB, val)) return *e;

        DSDCPResponse err;
        auto* out = resolveOutput(idx, cmd.id, err);
        if (!out) return err;

        out->setFaderDb(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doOutputMonitor(const DSDCPCommand& cmd)
    {
        int  idx = 0;
        bool val = false;
        if (auto e = requireOutputIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireBoolValue  (cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* out = resolveOutput(idx, cmd.id, err);
        if (!out) return err;

        out->setMonitor(val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doOutputName(const DSDCPCommand& cmd)
    {
        int          idx = 0;
        juce::String val;
        if (auto e = requireOutputIndex(cmd.params, cmd.id, idx)) return *e;
        if (auto e = requireStringValue(cmd.params, cmd.id, val)) return *e;

        DSDCPResponse err;
        auto* out = resolveOutput(idx, cmd.id, err);
        if (!out) return err;

        out->setName(val.toStdString());
        return DSDCPResponse::ok(cmd.id);
    }

    // =========================================================================
    // Route command handlers
    // =========================================================================

    // Helper: extract validated channel + output indices for route commands.
    // Returns true if both are valid; false and fills outErr otherwise.
    static bool requireRouteIndices(const juce::var& params, const juce::String& cmdId,
                                    int& outCh, int& outOut, DSDCPResponse& outErr)
    {
        if (auto e = requireChannelIndex(params, cmdId, outCh)) { outErr = *e; return false; }

        // Route commands use "output" for the bus index.
        if (auto e = requireOutputIndex(params, cmdId, outOut))  { outErr = *e; return false; }

        return true;
    }

    DSDCPResponse CommandDispatcher::doRouteEnable(const DSDCPCommand& cmd)
    {
        int  ch = 0, out = 0;
        bool val = false;
        DSDCPResponse err;
        if (!requireRouteIndices(cmd.params, cmd.id, ch, out, err)) return err;
        if (auto e = requireBoolValue(cmd.params, cmd.id, val))     return *e;

        engine_.getRoutingEngine().setRouteEnabled(ch, out, val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doRouteGain(const DSDCPCommand& cmd)
    {
        int   ch = 0, out = 0;
        float val = 0.0f;
        DSDCPResponse err;
        if (!requireRouteIndices(cmd.params, cmd.id, ch, out, err))                       return err;
        if (auto e = requireFloatValue(cmd.params, cmd.id, MIN_GAIN_DB, MAX_GAIN_DB, val)) return *e;

        engine_.getRoutingEngine().setRouteGainDb(ch, out, val);
        return DSDCPResponse::ok(cmd.id);
    }

    DSDCPResponse CommandDispatcher::doRoutePan(const DSDCPCommand& cmd)
    {
        int   ch = 0, out = 0;
        float val = 0.0f;
        DSDCPResponse err;
        if (!requireRouteIndices(cmd.params, cmd.id, ch, out, err))               return err;
        if (auto e = requireFloatValue(cmd.params, cmd.id, -1.0f, 1.0f, val))    return *e;

        engine_.getRoutingEngine().setRoutePan(ch, out, val);
        return DSDCPResponse::ok(cmd.id);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    DSDCPResponse CommandDispatcher::doSyncRequest(const DSDCPCommand& cmd)
    {
        // sync.request has no parameters.
        // The actual state snapshot is built by StateObserver (Stage B).
        // At Stage A the response simply acknowledges receipt; the caller
        // (ControlServer, Stage C) is responsible for pushing the sync payload.
        return DSDCPResponse::ok(cmd.id);
    }

} // namespace dsd::control
