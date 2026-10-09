#include "Control/DSDCPSerializer.h"

namespace dsd::control
{
    // =========================================================================
    // Parsing helpers
    // =========================================================================

    ParseResult parseFrame(const juce::String& jsonText)
    {
        ParseResult result;

        juce::var parsed = juce::JSON::parse(jsonText);
        if (parsed.isVoid() || !parsed.isObject())
        {
            result.parseError = "Frame is not a valid JSON object";
            return result;
        }

        result.root = parsed;

        const juce::var& typeVar = parsed["type"];
        if (!typeVar.isString())
        {
            result.parseError = "Missing or non-string 'type' field";
            return result;
        }

        result.type = messageTypeFromString(typeVar.toString());
        return result;
    }

    std::optional<DSDCPResponse> extractCommand(const ParseResult& frame,
                                                DSDCPCommand&      outCommand)
    {
        // Extract command id — use empty string as fallback so callers can
        // always return a response even if id parsing itself failed.
        juce::String cmdId;
        if (frame.root.hasProperty("id"))
            cmdId = frame.root["id"].toString();

        // Validate method field
        if (!frame.root.hasProperty("method"))
            return DSDCPResponse::fail(cmdId, ErrorCode::MissingParameter,
                                       "Command message missing 'method' field");

        const juce::String methodStr = frame.root["method"].toString();
        const CommandMethod method   = commandMethodFromString(methodStr);

        if (method == CommandMethod::Unknown)
            return DSDCPResponse::fail(cmdId, ErrorCode::UnknownMethod,
                                       "Unknown command method: " + methodStr);

        // params is optional at the envelope level; individual command handlers
        // validate what they actually need.
        juce::var params;
        if (frame.root.hasProperty("params"))
            params = frame.root["params"];
        else
            params = juce::var(new juce::DynamicObject()); // empty object

        outCommand.id     = cmdId;
        outCommand.method = method;
        outCommand.params = params;
        return std::nullopt; // success
    }

    // =========================================================================
    // Serialisation helpers
    // =========================================================================

    juce::String serialiseResponse(const DSDCPResponse& response)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("type", "response");
        obj->setProperty("id",   response.id);
        obj->setProperty("success", response.success);

        if (!response.success)
        {
            auto errObj = std::make_unique<juce::DynamicObject>();
            errObj->setProperty("code",    static_cast<int>(response.errorCode));
            errObj->setProperty("message", response.errorMessage);
            obj->setProperty("error", juce::var(errObj.release()));
        }

        return juce::JSON::toString(juce::var(obj.release()), false);
    }

    juce::String serialiseWelcome(const juce::String& clientId,
                                  const juce::var&    capabilities)
    {
        auto serverObj = std::make_unique<juce::DynamicObject>();
        serverObj->setProperty("name",    "DSD Mixer");
        serverObj->setProperty("version", "1.0.0");

        auto sessionObj = std::make_unique<juce::DynamicObject>();
        sessionObj->setProperty("clientId",     clientId);
        sessionObj->setProperty("capabilities", capabilities);

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type",     "welcome");
        root->setProperty("protocol", "DSDCP");
        root->setProperty("version",  DSDCP_VERSION);
        root->setProperty("server",   juce::var(serverObj.release()));
        root->setProperty("session",  juce::var(sessionObj.release()));

        return juce::JSON::toString(juce::var(root.release()), false);
    }

    juce::String serialisePing(juce::int64 timestampMs)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("type",      "ping");
        obj->setProperty("timestamp", timestampMs);
        return juce::JSON::toString(juce::var(obj.release()), false);
    }

    juce::String serialisePong(juce::int64 timestampMs)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("type",      "pong");
        obj->setProperty("timestamp", timestampMs);
        return juce::JSON::toString(juce::var(obj.release()), false);
    }

    juce::String serialiseGoodbye(const juce::String& reason)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("type", "goodbye");
        if (reason.isNotEmpty())
            obj->setProperty("reason", reason);
        return juce::JSON::toString(juce::var(obj.release()), false);
    }

    juce::String serialiseError(ErrorCode code, const juce::String& message)
    {
        auto errObj = std::make_unique<juce::DynamicObject>();
        errObj->setProperty("code",    static_cast<int>(code));
        errObj->setProperty("message", message);

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type",  "error");
        root->setProperty("error", juce::var(errObj.release()));

        return juce::JSON::toString(juce::var(root.release()), false);
    }

} // namespace dsd::control
