#pragma once

// DSD Control Protocol — Serialiser / Deserialiser  (Stage A)
//
// Converts between DSDCP wire JSON (juce::String) and the structured types
// defined in DSDCPTypes.h.  Uses only juce::JSON / juce::var — the same
// serialisation layer already used by SessionManager.
//
// Thread-safety: all functions are stateless pure functions and are safe to
// call from any thread, but callers are responsible for ensuring that any
// juce::var objects passed in are not simultaneously mutated elsewhere.

#include "Control/DSDCPTypes.h"
#include <juce_core/juce_core.h>

namespace dsd::control
{
    // =========================================================================
    // Parsing — JSON → structured types
    // =========================================================================

    // Parse a raw JSON text frame into a type + raw var.
    // Returns MessageType::Unknown if the text is not valid DSDCP JSON.
    struct ParseResult
    {
        MessageType type { MessageType::Unknown };
        juce::var   root;           // the full parsed object
        juce::String parseError;    // non-empty if parsing failed
    };

    ParseResult parseFrame(const juce::String& jsonText);

    // Extract a DSDCPCommand from an already-parsed frame whose type == Command.
    // Returns nullopt on success and a failed DSDCPResponse on parse error.
    // The DSDCPResponse carries an empty id if the id field itself is malformed.
    std::optional<DSDCPResponse> extractCommand(const ParseResult& frame,
                                                DSDCPCommand&       outCommand);

    // =========================================================================
    // Serialisation — structured types → JSON text
    // =========================================================================

    // Serialise a DSDCPResponse into a JSON wire frame.
    juce::String serialiseResponse(const DSDCPResponse& response);

    // Serialise a server-sent "hello" acknowledgement / welcome message.
    // capabilities is a juce::var object (e.g. channels:16, outputs:4, …).
    juce::String serialiseWelcome(const juce::String& clientId,
                                  const juce::var&    capabilities);

    // Serialise a ping frame (timestamp = milliseconds since epoch).
    juce::String serialisePing(juce::int64 timestampMs);

    // Serialise a pong frame.
    juce::String serialisePong(juce::int64 timestampMs);

    // Serialise a goodbye frame.
    juce::String serialiseGoodbye(const juce::String& reason = "");

    // Serialise a protocol-level error frame (not tied to a command id).
    juce::String serialiseError(ErrorCode code, const juce::String& message);

} // namespace dsd::control
