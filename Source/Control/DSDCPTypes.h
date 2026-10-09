#pragma once

// DSD Control Protocol — Type Definitions
// Version: 1
// Transport: WebSocket (Stage C)  —  this header is transport-independent.
//
// All wire encoding uses juce::var / juce::JSON (same infrastructure as the
// existing SessionManager).  No external networking library is referenced here.

#include <juce_core/juce_core.h>
#include "Audio/AudioTypes.h"   // NUM_CHANNELS_LEVEL1, NUM_OUTPUT_BUSES_LEVEL1, MIN/MAX constants

#include <optional>
#include <string>

namespace dsd::control
{
    // -------------------------------------------------------------------------
    // Protocol constants
    // -------------------------------------------------------------------------

    inline constexpr int  DSDCP_VERSION          = 1;
    inline constexpr int  DSDCP_DEFAULT_PORT      = 9100;
    inline constexpr int  DSDCP_HEARTBEAT_INTERVAL_MS = 5000;
    inline constexpr int  DSDCP_HEARTBEAT_TIMEOUT_MS  = 15000;
    inline constexpr int  DSDCP_METER_MIN_INTERVAL_MS = 50;

    // -------------------------------------------------------------------------
    // Message types  (wire: "type" field)
    // -------------------------------------------------------------------------

    enum class MessageType
    {
        Unknown = 0,
        Hello,      // client → server: handshake
        Welcome,    // server → client: handshake response
        Command,    // client → server: control command
        Response,   // server → client: command result
        Event,      // server → client: state-change notification
        Sync,       // server → client: full state snapshot
        Subscribe,  // client → server: event subscription preferences
        Ping,       // either → either: heartbeat
        Pong,       // either → either: heartbeat reply
        Error,      // server → client: protocol-level error
        Goodbye,    // either → either: graceful disconnect
    };

    inline juce::String toString(MessageType t)
    {
        switch (t)
        {
            case MessageType::Hello:     return "hello";
            case MessageType::Welcome:   return "welcome";
            case MessageType::Command:   return "command";
            case MessageType::Response:  return "response";
            case MessageType::Event:     return "event";
            case MessageType::Sync:      return "sync";
            case MessageType::Subscribe: return "subscribe";
            case MessageType::Ping:      return "ping";
            case MessageType::Pong:      return "pong";
            case MessageType::Error:     return "error";
            case MessageType::Goodbye:   return "goodbye";
            default:                     return "unknown";
        }
    }

    inline MessageType messageTypeFromString(const juce::String& s)
    {
        if (s == "hello")     return MessageType::Hello;
        if (s == "welcome")   return MessageType::Welcome;
        if (s == "command")   return MessageType::Command;
        if (s == "response")  return MessageType::Response;
        if (s == "event")     return MessageType::Event;
        if (s == "sync")      return MessageType::Sync;
        if (s == "subscribe") return MessageType::Subscribe;
        if (s == "ping")      return MessageType::Ping;
        if (s == "pong")      return MessageType::Pong;
        if (s == "error")     return MessageType::Error;
        if (s == "goodbye")   return MessageType::Goodbye;
        return MessageType::Unknown;
    }

    // -------------------------------------------------------------------------
    // Error codes
    // -------------------------------------------------------------------------

    enum class ErrorCode : int
    {
        // 4xxx — client errors
        UnknownMethod         = 4000,
        ParameterOutOfRange   = 4001,
        MissingParameter      = 4002,
        InvalidParameterType  = 4003,
        CommandFailed         = 4004,
        NotAuthenticated      = 4010,
        PermissionDenied      = 4011,
        VersionMismatch       = 4020,
        RateLimitExceeded     = 4030,

        // 5xxx — server errors
        InternalError         = 5000,
    };

    // -------------------------------------------------------------------------
    // Command methods  (wire: "method" field inside a Command message)
    // -------------------------------------------------------------------------

    enum class CommandMethod
    {
        Unknown = 0,

        // Channel controls
        ChannelMute,
        ChannelGain,
        ChannelFader,
        ChannelPan,
        ChannelPfl,
        ChannelDm,
        ChannelSolo,
        ChannelOn,
        ChannelPhase,
        ChannelMono,
        ChannelHpf,
        ChannelPad,
        ChannelName,

        // Output controls
        OutputMute,
        OutputFader,
        OutputMonitor,
        OutputName,

        // Routing
        RouteEnable,
        RouteGain,
        RoutePan,

        // Utility
        SyncRequest,
    };

    inline juce::String toString(CommandMethod m)
    {
        switch (m)
        {
            case CommandMethod::ChannelMute:    return "channel.mute";
            case CommandMethod::ChannelGain:    return "channel.gain";
            case CommandMethod::ChannelFader:   return "channel.fader";
            case CommandMethod::ChannelPan:     return "channel.pan";
            case CommandMethod::ChannelPfl:     return "channel.pfl";
            case CommandMethod::ChannelDm:      return "channel.dm";
            case CommandMethod::ChannelSolo:    return "channel.solo";
            case CommandMethod::ChannelOn:      return "channel.on";
            case CommandMethod::ChannelPhase:   return "channel.phase";
            case CommandMethod::ChannelMono:    return "channel.mono";
            case CommandMethod::ChannelHpf:     return "channel.hpf";
            case CommandMethod::ChannelPad:     return "channel.pad";
            case CommandMethod::ChannelName:    return "channel.name";
            case CommandMethod::OutputMute:     return "output.mute";
            case CommandMethod::OutputFader:    return "output.fader";
            case CommandMethod::OutputMonitor:  return "output.monitor";
            case CommandMethod::OutputName:     return "output.name";
            case CommandMethod::RouteEnable:    return "route.enable";
            case CommandMethod::RouteGain:      return "route.gain";
            case CommandMethod::RoutePan:       return "route.pan";
            case CommandMethod::SyncRequest:    return "sync.request";
            default:                            return "unknown";
        }
    }

    inline CommandMethod commandMethodFromString(const juce::String& s)
    {
        if (s == "channel.mute")   return CommandMethod::ChannelMute;
        if (s == "channel.gain")   return CommandMethod::ChannelGain;
        if (s == "channel.fader")  return CommandMethod::ChannelFader;
        if (s == "channel.pan")    return CommandMethod::ChannelPan;
        if (s == "channel.pfl")    return CommandMethod::ChannelPfl;
        if (s == "channel.dm")     return CommandMethod::ChannelDm;
        if (s == "channel.solo")   return CommandMethod::ChannelSolo;
        if (s == "channel.on")     return CommandMethod::ChannelOn;
        if (s == "channel.phase")  return CommandMethod::ChannelPhase;
        if (s == "channel.mono")   return CommandMethod::ChannelMono;
        if (s == "channel.hpf")    return CommandMethod::ChannelHpf;
        if (s == "channel.pad")    return CommandMethod::ChannelPad;
        if (s == "channel.name")   return CommandMethod::ChannelName;
        if (s == "output.mute")    return CommandMethod::OutputMute;
        if (s == "output.fader")   return CommandMethod::OutputFader;
        if (s == "output.monitor") return CommandMethod::OutputMonitor;
        if (s == "output.name")    return CommandMethod::OutputName;
        if (s == "route.enable")   return CommandMethod::RouteEnable;
        if (s == "route.gain")     return CommandMethod::RouteGain;
        if (s == "route.pan")      return CommandMethod::RoutePan;
        if (s == "sync.request")   return CommandMethod::SyncRequest;
        return CommandMethod::Unknown;
    }

    // -------------------------------------------------------------------------
    // Core message structures
    // -------------------------------------------------------------------------

    // A validated, parsed command extracted from an inbound JSON message.
    struct DSDCPCommand
    {
        juce::String  id;      // client-supplied correlation ID
        CommandMethod method { CommandMethod::Unknown };
        juce::var     params;  // raw params object for method-specific extraction
    };

    // Result returned by CommandDispatcher::execute().
    struct DSDCPResponse
    {
        juce::String id;         // mirrors DSDCPCommand::id
        bool         success { false };
        // Populated only on failure:
        ErrorCode    errorCode { ErrorCode::InternalError };
        juce::String errorMessage;

        static DSDCPResponse ok(const juce::String& commandId)
        {
            DSDCPResponse r;
            r.id      = commandId;
            r.success = true;
            return r;
        }

        static DSDCPResponse fail(const juce::String& commandId,
                                  ErrorCode           code,
                                  const juce::String& message)
        {
            DSDCPResponse r;
            r.id           = commandId;
            r.success      = false;
            r.errorCode    = code;
            r.errorMessage = message;
            return r;
        }
    };

    // -------------------------------------------------------------------------
    // Parameter validation helpers
    // -------------------------------------------------------------------------
    //
    // These return a populated DSDCPResponse on failure, or an empty optional
    // on success.  Using optional avoids exceptions in the dispatch path.

    /// Check that a required field exists in params.
    inline std::optional<DSDCPResponse>
    requireField(const juce::var& params, const char* field, const juce::String& cmdId)
    {
        if (!params.hasProperty(field))
            return DSDCPResponse::fail(cmdId,
                                       ErrorCode::MissingParameter,
                                       juce::String("Missing required parameter: ") + field);
        return std::nullopt;
    }

    /// Extract and validate a channel index (0 … NUM_CHANNELS_LEVEL1-1).
    inline std::optional<DSDCPResponse>
    requireChannelIndex(const juce::var& params, const juce::String& cmdId, int& outIndex)
    {
        if (auto err = requireField(params, "channel", cmdId)) return err;

        const juce::var& v = params["channel"];
        if (!v.isInt() && !v.isInt64())
            return DSDCPResponse::fail(cmdId, ErrorCode::InvalidParameterType,
                                       "Parameter 'channel' must be an integer");

        const int idx = static_cast<int>(v);
        if (idx < 0 || idx >= NUM_CHANNELS_LEVEL1)
            return DSDCPResponse::fail(cmdId, ErrorCode::ParameterOutOfRange,
                                       juce::String("Channel index ") + juce::String(idx) +
                                       " out of range [0, " +
                                       juce::String(NUM_CHANNELS_LEVEL1 - 1) + "]");
        outIndex = idx;
        return std::nullopt;
    }

    /// Extract and validate an output index (0 … NUM_OUTPUT_BUSES_LEVEL1-1).
    inline std::optional<DSDCPResponse>
    requireOutputIndex(const juce::var& params, const juce::String& cmdId, int& outIndex)
    {
        if (auto err = requireField(params, "output", cmdId)) return err;

        const juce::var& v = params["output"];
        if (!v.isInt() && !v.isInt64())
            return DSDCPResponse::fail(cmdId, ErrorCode::InvalidParameterType,
                                       "Parameter 'output' must be an integer");

        const int idx = static_cast<int>(v);
        if (idx < 0 || idx >= NUM_OUTPUT_BUSES_LEVEL1)
            return DSDCPResponse::fail(cmdId, ErrorCode::ParameterOutOfRange,
                                       juce::String("Output index ") + juce::String(idx) +
                                       " out of range [0, " +
                                       juce::String(NUM_OUTPUT_BUSES_LEVEL1 - 1) + "]");
        outIndex = idx;
        return std::nullopt;
    }

    /// Extract and validate the "value" field as a boolean.
    inline std::optional<DSDCPResponse>
    requireBoolValue(const juce::var& params, const juce::String& cmdId, bool& outValue)
    {
        if (auto err = requireField(params, "value", cmdId)) return err;

        const juce::var& v = params["value"];
        if (!v.isBool())
            return DSDCPResponse::fail(cmdId, ErrorCode::InvalidParameterType,
                                       "Parameter 'value' must be a boolean");
        outValue = static_cast<bool>(v);
        return std::nullopt;
    }

    /// Extract and validate the "value" field as a float within [minVal, maxVal].
    inline std::optional<DSDCPResponse>
    requireFloatValue(const juce::var& params, const juce::String& cmdId,
                      float minVal, float maxVal, float& outValue)
    {
        if (auto err = requireField(params, "value", cmdId)) return err;

        const juce::var& v = params["value"];
        if (!v.isDouble() && !v.isInt() && !v.isInt64())
            return DSDCPResponse::fail(cmdId, ErrorCode::InvalidParameterType,
                                       "Parameter 'value' must be a number");

        const float fv = static_cast<float>(static_cast<double>(v));
        if (fv < minVal || fv > maxVal)
            return DSDCPResponse::fail(cmdId, ErrorCode::ParameterOutOfRange,
                                       juce::String("Parameter 'value' ") + juce::String(fv) +
                                       " out of range [" + juce::String(minVal) + ", " + juce::String(maxVal) + "]");
        outValue = fv;
        return std::nullopt;
    }

    /// Extract and validate the "value" field as a string.
    inline std::optional<DSDCPResponse>
    requireStringValue(const juce::var& params, const juce::String& cmdId, juce::String& outValue)
    {
        if (auto err = requireField(params, "value", cmdId)) return err;

        const juce::var& v = params["value"];
        if (!v.isString())
            return DSDCPResponse::fail(cmdId, ErrorCode::InvalidParameterType,
                                       "Parameter 'value' must be a string");
        outValue = v.toString();
        return std::nullopt;
    }

} // namespace dsd::control
