// ControlProtocolTests.cpp
// Stage A verification: DSDCPTypes, DSDCPSerializer, CommandDispatcher
//
// Test framework mirrors AudioEngineTests.cpp:
//   - Each test is a void function.
//   - Failures are reported via assert().
//   - Progress is reported via std::cout.
//   - main() runs all tests and prints a summary.

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>

// JUCE modules needed for juce::JSON, juce::var, juce::String
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Engine subsystems (needed to create an AudioEngine for CommandDispatcher)
#include "Audio/AudioTypes.h"
#include "Audio/AudioEngine.h"
#include "Channel/ChannelManager.h"
#include "Output/OutputManager.h"
#include "Routing/RoutingEngine.h"

// Stage A, B & C: control protocol
#include "Control/DSDCPTypes.h"
#include "Control/DSDCPSerializer.h"
#include "Control/CommandDispatcher.h"
#include "Control/MixerStateSnapshot.h"
#include "Control/StateObserver.h"
#include "Control/ClientManager.h"
#include "Control/WebSocketServer.h"
#include "Control/ControlServer.h"
#include <ixwebsocket/IXWebSocket.h>
#include <thread>
#include <chrono>

using namespace dsd;
using namespace dsd::control;

// ---------------------------------------------------------------------------
// Helper: build a minimal DSDCP command JSON string for use in round-trip
// tests.  Saves repetitive string construction in tests.
// ---------------------------------------------------------------------------
static juce::String makeCommandJson(const juce::String& id,
                                    const juce::String& method,
                                    const juce::String& paramsJson)
{
    return "{\"type\":\"command\",\"id\":\"" + id +
           "\",\"method\":\"" + method +
           "\",\"params\":" + paramsJson + "}";
}

// ---------------------------------------------------------------------------
// Helper: build a DSDCPCommand struct directly (bypasses JSON parse layer)
// so that CommandDispatcher tests are not dependent on DSDCPSerializer.
// ---------------------------------------------------------------------------
static DSDCPCommand makeCommand(const juce::String& id,
                                CommandMethod       method,
                                const juce::String& paramsJson)
{
    DSDCPCommand cmd;
    cmd.id     = id;
    cmd.method = method;
    cmd.params = juce::JSON::parse(paramsJson);
    return cmd;
}

// ===========================================================================
// DSDCPTypes tests
// ===========================================================================

void testMessageTypeRoundTrip()
{
    std::cout << "[TEST] MessageType string round-trip..." << std::endl;

    // Every named type must round-trip through toString / messageTypeFromString.
    const MessageType types[] = {
        MessageType::Hello, MessageType::Welcome, MessageType::Command,
        MessageType::Response, MessageType::Event, MessageType::Sync,
        MessageType::Subscribe, MessageType::Ping, MessageType::Pong,
        MessageType::Error, MessageType::Goodbye,
    };
    for (auto t : types)
    {
        assert(messageTypeFromString(toString(t)) == t);
    }

    // Unknown strings must return Unknown.
    assert(messageTypeFromString("nonsense")  == MessageType::Unknown);
    assert(messageTypeFromString("")          == MessageType::Unknown);

    std::cout << "[PASS] MessageType string round-trip." << std::endl;
}

void testCommandMethodRoundTrip()
{
    std::cout << "[TEST] CommandMethod string round-trip..." << std::endl;

    const CommandMethod methods[] = {
        CommandMethod::ChannelMute, CommandMethod::ChannelGain,
        CommandMethod::ChannelFader, CommandMethod::ChannelPan,
        CommandMethod::ChannelPfl, CommandMethod::ChannelDm,
        CommandMethod::ChannelSolo, CommandMethod::ChannelOn,
        CommandMethod::ChannelPhase, CommandMethod::ChannelMono,
        CommandMethod::ChannelHpf, CommandMethod::ChannelPad,
        CommandMethod::ChannelName,
        CommandMethod::OutputMute, CommandMethod::OutputFader,
        CommandMethod::OutputMonitor, CommandMethod::OutputName,
        CommandMethod::RouteEnable, CommandMethod::RouteGain,
        CommandMethod::RoutePan, CommandMethod::SyncRequest,
    };
    for (auto m : methods)
    {
        assert(commandMethodFromString(toString(m)) == m);
    }

    assert(commandMethodFromString("nonsense.command") == CommandMethod::Unknown);
    assert(commandMethodFromString("")                 == CommandMethod::Unknown);

    std::cout << "[PASS] CommandMethod string round-trip." << std::endl;
}

void testDSDCPResponseHelpers()
{
    std::cout << "[TEST] DSDCPResponse factory helpers..." << std::endl;

    auto ok = DSDCPResponse::ok("req1");
    assert(ok.id      == "req1");
    assert(ok.success == true);

    auto fail = DSDCPResponse::fail("req2", ErrorCode::ParameterOutOfRange, "bad value");
    assert(fail.id           == "req2");
    assert(fail.success      == false);
    assert(fail.errorCode    == ErrorCode::ParameterOutOfRange);
    assert(fail.errorMessage == "bad value");

    std::cout << "[PASS] DSDCPResponse factory helpers." << std::endl;
}

void testParameterValidationHelpers()
{
    std::cout << "[TEST] Parameter validation helpers..." << std::endl;

    // --- requireChannelIndex ---
    {
        // Valid
        auto params = juce::JSON::parse("{\"channel\":0}");
        int idx = -1;
        auto err = requireChannelIndex(params, "x", idx);
        assert(!err.has_value());
        assert(idx == 0);
    }
    {
        // Upper boundary (15)
        auto params = juce::JSON::parse("{\"channel\":15}");
        int idx = -1;
        auto err = requireChannelIndex(params, "x", idx);
        assert(!err.has_value());
        assert(idx == 15);
    }
    {
        // Too high
        auto params = juce::JSON::parse("{\"channel\":16}");
        int idx = -1;
        auto err = requireChannelIndex(params, "x", idx);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::ParameterOutOfRange);
    }
    {
        // Negative
        auto params = juce::JSON::parse("{\"channel\":-1}");
        int idx = -1;
        auto err = requireChannelIndex(params, "x", idx);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::ParameterOutOfRange);
    }
    {
        // Missing
        auto params = juce::JSON::parse("{}");
        int idx = -1;
        auto err = requireChannelIndex(params, "x", idx);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::MissingParameter);
    }
    {
        // Wrong type (string instead of int)
        auto params = juce::JSON::parse("{\"channel\":\"abc\"}");
        int idx = -1;
        auto err = requireChannelIndex(params, "x", idx);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::InvalidParameterType);
    }

    // --- requireOutputIndex ---
    {
        auto params = juce::JSON::parse("{\"output\":3}");
        int idx = -1;
        auto err = requireOutputIndex(params, "x", idx);
        assert(!err.has_value());
        assert(idx == 3);
    }
    {
        auto params = juce::JSON::parse("{\"output\":4}");
        int idx = -1;
        auto err = requireOutputIndex(params, "x", idx);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::ParameterOutOfRange);
    }
    {
        auto params = juce::JSON::parse("{\"output\":-1}");
        int idx = -1;
        auto err = requireOutputIndex(params, "x", idx);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::ParameterOutOfRange);
    }

    // --- requireBoolValue ---
    {
        auto params = juce::JSON::parse("{\"value\":true}");
        bool val = false;
        auto err = requireBoolValue(params, "x", val);
        assert(!err.has_value());
        assert(val == true);
    }
    {
        auto params = juce::JSON::parse("{\"value\":42}");
        bool val = false;
        auto err = requireBoolValue(params, "x", val);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::InvalidParameterType);
    }

    // --- requireFloatValue ---
    {
        auto params = juce::JSON::parse("{\"value\":0.0}");
        float val = 99.0f;
        auto err = requireFloatValue(params, "x", MIN_GAIN_DB, MAX_GAIN_DB, val);
        assert(!err.has_value());
        assert(std::abs(val - 0.0f) < 0.001f);
    }
    {
        // Above max gain
        auto params = juce::JSON::parse("{\"value\":25.0}");
        float val = 0.0f;
        auto err = requireFloatValue(params, "x", MIN_GAIN_DB, MAX_GAIN_DB, val);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::ParameterOutOfRange);
    }
    {
        // Below min gain
        auto params = juce::JSON::parse("{\"value\":-61.0}");
        float val = 0.0f;
        auto err = requireFloatValue(params, "x", MIN_GAIN_DB, MAX_GAIN_DB, val);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::ParameterOutOfRange);
    }
    {
        // Wrong type (string)
        auto params = juce::JSON::parse("{\"value\":\"loud\"}");
        float val = 0.0f;
        auto err = requireFloatValue(params, "x", -60.0f, 24.0f, val);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::InvalidParameterType);
    }

    std::cout << "[PASS] Parameter validation helpers." << std::endl;
}

// ===========================================================================
// DSDCPSerializer tests
// ===========================================================================

void testSerialiserParseFrame()
{
    std::cout << "[TEST] DSDCPSerializer::parseFrame..." << std::endl;

    // Valid command frame
    {
        auto res = parseFrame(makeCommandJson("42", "channel.mute",
                                             "{\"channel\":0,\"value\":true}"));
        assert(res.parseError.isEmpty());
        assert(res.type == MessageType::Command);
    }

    // Valid ping
    {
        auto res = parseFrame("{\"type\":\"ping\",\"timestamp\":12345}");
        assert(res.type == MessageType::Ping);
    }

    // Not JSON
    {
        auto res = parseFrame("not json at all");
        assert(res.type == MessageType::Unknown);
        assert(res.parseError.isNotEmpty());
    }

    // JSON but not an object
    {
        auto res = parseFrame("[1,2,3]");
        assert(res.type == MessageType::Unknown);
    }

    // JSON object but missing "type"
    {
        auto res = parseFrame("{\"method\":\"channel.mute\"}");
        assert(res.type == MessageType::Unknown);
        assert(res.parseError.isNotEmpty());
    }

    // Unknown type string
    {
        auto res = parseFrame("{\"type\":\"nonsense\"}");
        assert(res.type == MessageType::Unknown);
        // parseError may or may not be set — Unknown is the key signal
    }

    std::cout << "[PASS] DSDCPSerializer::parseFrame." << std::endl;
}

void testSerialiserExtractCommand()
{
    std::cout << "[TEST] DSDCPSerializer::extractCommand..." << std::endl;

    // Valid command
    {
        auto frame = parseFrame(makeCommandJson("cmd_1", "channel.mute",
                                               "{\"channel\":2,\"value\":false}"));
        DSDCPCommand cmd;
        auto err = extractCommand(frame, cmd);
        assert(!err.has_value());
        assert(cmd.id     == "cmd_1");
        assert(cmd.method == CommandMethod::ChannelMute);
        assert(static_cast<int>(cmd.params["channel"]) == 2);
    }

    // Missing method
    {
        auto frame = parseFrame("{\"type\":\"command\",\"id\":\"x\",\"params\":{}}");
        DSDCPCommand cmd;
        auto err = extractCommand(frame, cmd);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::MissingParameter);
    }

    // Unknown method
    {
        auto frame = parseFrame("{\"type\":\"command\",\"id\":\"x\","
                                "\"method\":\"magic.sparkles\",\"params\":{}}");
        DSDCPCommand cmd;
        auto err = extractCommand(frame, cmd);
        assert(err.has_value());
        assert(err->errorCode == ErrorCode::UnknownMethod);
    }

    // Command with no params field (should succeed — handlers validate params themselves)
    {
        auto frame = parseFrame("{\"type\":\"command\",\"id\":\"y\","
                                "\"method\":\"sync.request\"}");
        DSDCPCommand cmd;
        auto err = extractCommand(frame, cmd);
        assert(!err.has_value());
        assert(cmd.method == CommandMethod::SyncRequest);
    }

    std::cout << "[PASS] DSDCPSerializer::extractCommand." << std::endl;
}

void testSerialiserResponseRoundTrip()
{
    std::cout << "[TEST] DSDCPSerializer response serialisation round-trip..." << std::endl;

    // Success response
    {
        auto json = serialiseResponse(DSDCPResponse::ok("req_7"));
        auto parsed = juce::JSON::parse(json);
        assert(parsed["type"].toString()   == "response");
        assert(parsed["id"].toString()     == "req_7");
        assert(static_cast<bool>(parsed["success"]) == true);
        assert(!parsed.hasProperty("error"));
    }

    // Error response
    {
        auto json = serialiseResponse(DSDCPResponse::fail("req_8",
                                                          ErrorCode::ParameterOutOfRange,
                                                          "value too large"));
        auto parsed = juce::JSON::parse(json);
        assert(parsed["type"].toString()          == "response");
        assert(parsed["id"].toString()            == "req_8");
        assert(static_cast<bool>(parsed["success"]) == false);
        assert(static_cast<int>(parsed["error"]["code"]) ==
               static_cast<int>(ErrorCode::ParameterOutOfRange));
        assert(parsed["error"]["message"].toString() == "value too large");
    }

    // ping / pong
    {
        auto pingJson = serialisePing(999);
        auto p = juce::JSON::parse(pingJson);
        assert(p["type"].toString()        == "ping");
        assert(static_cast<int>(p["timestamp"]) == 999);

        auto pongJson = serialisePong(999);
        auto q = juce::JSON::parse(pongJson);
        assert(q["type"].toString()        == "pong");
        assert(static_cast<int>(q["timestamp"]) == 999);
    }

    // goodbye
    {
        auto g = juce::JSON::parse(serialiseGoodbye("test done"));
        assert(g["type"].toString()   == "goodbye");
        assert(g["reason"].toString() == "test done");
    }

    // error frame
    {
        auto e = juce::JSON::parse(
            serialiseError(ErrorCode::InternalError, "boom"));
        assert(e["type"].toString()               == "error");
        assert(static_cast<int>(e["error"]["code"]) ==
               static_cast<int>(ErrorCode::InternalError));
    }

    std::cout << "[PASS] DSDCPSerializer response serialisation round-trip." << std::endl;
}

// ===========================================================================
// CommandDispatcher tests
//
// We create a real AudioEngine (default-constructed, no audio hardware) so
// that CommandDispatcher tests exercise the actual engine setters.
// ===========================================================================

// Build an engine, initialise channels and outputs to a known state.
static std::unique_ptr<AudioEngine> makeTestEngine()
{
    auto engine = std::make_unique<AudioEngine>();
    engine->getChannelManager().initializeDefaultChannels(NUM_CHANNELS_LEVEL1);
    engine->getOutputManager().initializeDefaultOutputs(NUM_OUTPUT_BUSES_LEVEL1);
    engine->prepare(48000.0, 128, 2);
    return engine;
}

void testDispatcherChannelMute()
{
    std::cout << "[TEST] CommandDispatcher channel.mute..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Mute channel 0
    auto resp = disp.execute(makeCommand("m1", CommandMethod::ChannelMute,
                                         "{\"channel\":0,\"value\":true}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(0)->getMute() == true);

    // Unmute channel 0
    resp = disp.execute(makeCommand("m2", CommandMethod::ChannelMute,
                                    "{\"channel\":0,\"value\":false}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(0)->getMute() == false);

    // Last valid channel (15)
    resp = disp.execute(makeCommand("m3", CommandMethod::ChannelMute,
                                    "{\"channel\":15,\"value\":true}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(15)->getMute() == true);

    std::cout << "[PASS] CommandDispatcher channel.mute." << std::endl;
}

void testDispatcherInvalidChannelIndex()
{
    std::cout << "[TEST] CommandDispatcher invalid channel index..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Index too high (16)
    auto resp = disp.execute(makeCommand("e1", CommandMethod::ChannelMute,
                                          "{\"channel\":16,\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Negative index
    resp = disp.execute(makeCommand("e2", CommandMethod::ChannelMute,
                                    "{\"channel\":-1,\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    std::cout << "[PASS] CommandDispatcher invalid channel index." << std::endl;
}

void testDispatcherInvalidOutputIndex()
{
    std::cout << "[TEST] CommandDispatcher invalid output index..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    auto resp = disp.execute(makeCommand("o1", CommandMethod::OutputMute,
                                          "{\"output\":4,\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    resp = disp.execute(makeCommand("o2", CommandMethod::OutputMute,
                                    "{\"output\":-1,\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    std::cout << "[PASS] CommandDispatcher invalid output index." << std::endl;
}

void testDispatcherMissingParameter()
{
    std::cout << "[TEST] CommandDispatcher missing parameter..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Missing "channel"
    auto resp = disp.execute(makeCommand("mp1", CommandMethod::ChannelMute,
                                          "{\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::MissingParameter);

    // Missing "value"
    resp = disp.execute(makeCommand("mp2", CommandMethod::ChannelMute,
                                    "{\"channel\":0}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::MissingParameter);

    // Completely empty params
    resp = disp.execute(makeCommand("mp3", CommandMethod::ChannelMute, "{}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::MissingParameter);

    std::cout << "[PASS] CommandDispatcher missing parameter." << std::endl;
}

void testDispatcherInvalidParameterType()
{
    std::cout << "[TEST] CommandDispatcher invalid parameter type..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // channel is a string, should be int
    auto resp = disp.execute(makeCommand("t1", CommandMethod::ChannelMute,
                                          "{\"channel\":\"zero\",\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::InvalidParameterType);

    // value for mute is a number, should be bool
    resp = disp.execute(makeCommand("t2", CommandMethod::ChannelMute,
                                    "{\"channel\":0,\"value\":1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::InvalidParameterType);

    // value for gain is a string, should be number
    resp = disp.execute(makeCommand("t3", CommandMethod::ChannelGain,
                                    "{\"channel\":0,\"value\":\"loud\"}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::InvalidParameterType);

    std::cout << "[PASS] CommandDispatcher invalid parameter type." << std::endl;
}

void testDispatcherUnknownCommand()
{
    std::cout << "[TEST] CommandDispatcher unknown command..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Build a command whose method is Unknown (bypasses JSON parse)
    DSDCPCommand cmd;
    cmd.id     = "uk1";
    cmd.method = CommandMethod::Unknown;
    cmd.params = juce::JSON::parse("{}");

    auto resp = disp.execute(cmd);
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::UnknownMethod);

    std::cout << "[PASS] CommandDispatcher unknown command." << std::endl;
}

void testDispatcherGainRange()
{
    std::cout << "[TEST] CommandDispatcher gain range validation..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Valid: 0 dB
    auto resp = disp.execute(makeCommand("g1", CommandMethod::ChannelGain,
                                          "{\"channel\":0,\"value\":0.0}"));
    assert(resp.success);
    assert(std::abs(engine->getChannelManager().getChannel(0)->getGainDb() - 0.0f) < 0.001f);

    // Valid: boundary values
    resp = disp.execute(makeCommand("g2", CommandMethod::ChannelGain,
                                    "{\"channel\":0,\"value\":-60.0}"));
    assert(resp.success);

    resp = disp.execute(makeCommand("g3", CommandMethod::ChannelGain,
                                    "{\"channel\":0,\"value\":24.0}"));
    assert(resp.success);

    // Invalid: above max
    resp = disp.execute(makeCommand("g4", CommandMethod::ChannelGain,
                                    "{\"channel\":0,\"value\":24.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Invalid: below min
    resp = disp.execute(makeCommand("g5", CommandMethod::ChannelGain,
                                    "{\"channel\":0,\"value\":-60.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    std::cout << "[PASS] CommandDispatcher gain range validation." << std::endl;
}

void testDispatcherFaderRange()
{
    std::cout << "[TEST] CommandDispatcher fader range validation..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Valid boundaries
    auto resp = disp.execute(makeCommand("f1", CommandMethod::ChannelFader,
                                          "{\"channel\":0,\"value\":-60.0}"));
    assert(resp.success);

    resp = disp.execute(makeCommand("f2", CommandMethod::ChannelFader,
                                    "{\"channel\":0,\"value\":12.0}"));
    assert(resp.success);

    // Invalid: above max fader (12)
    resp = disp.execute(makeCommand("f3", CommandMethod::ChannelFader,
                                    "{\"channel\":0,\"value\":12.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Invalid: below min fader (-60)
    resp = disp.execute(makeCommand("f4", CommandMethod::ChannelFader,
                                    "{\"channel\":0,\"value\":-60.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Output fader same constraints
    resp = disp.execute(makeCommand("f5", CommandMethod::OutputFader,
                                    "{\"output\":0,\"value\":12.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    std::cout << "[PASS] CommandDispatcher fader range validation." << std::endl;
}

void testDispatcherPanRange()
{
    std::cout << "[TEST] CommandDispatcher pan range validation..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Valid: centre
    auto resp = disp.execute(makeCommand("p1", CommandMethod::ChannelPan,
                                          "{\"channel\":0,\"value\":0.0}"));
    assert(resp.success);

    // Valid: hard left / right
    resp = disp.execute(makeCommand("p2", CommandMethod::ChannelPan,
                                    "{\"channel\":0,\"value\":-1.0}"));
    assert(resp.success);

    resp = disp.execute(makeCommand("p3", CommandMethod::ChannelPan,
                                    "{\"channel\":0,\"value\":1.0}"));
    assert(resp.success);

    // Invalid: > 1.0
    resp = disp.execute(makeCommand("p4", CommandMethod::ChannelPan,
                                    "{\"channel\":0,\"value\":1.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Invalid: < -1.0
    resp = disp.execute(makeCommand("p5", CommandMethod::ChannelPan,
                                    "{\"channel\":0,\"value\":-1.1}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    std::cout << "[PASS] CommandDispatcher pan range validation." << std::endl;
}

void testDispatcherPflClearsDm()
{
    std::cout << "[TEST] PFL enabling clears DM..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Enable DM first
    auto resp = disp.execute(makeCommand("d1", CommandMethod::ChannelDm,
                                          "{\"channel\":0,\"value\":true}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(0)->getDirectMonitor() == true);
    assert(engine->getChannelManager().getChannel(0)->getPfl()           == false);

    // Now enable PFL — must clear DM
    resp = disp.execute(makeCommand("pfl1", CommandMethod::ChannelPfl,
                                    "{\"channel\":0,\"value\":true}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(0)->getPfl()           == true);
    assert(engine->getChannelManager().getChannel(0)->getDirectMonitor() == false);

    std::cout << "[PASS] PFL enabling clears DM." << std::endl;
}

void testDispatcherDmClearsPfl()
{
    std::cout << "[TEST] DM enabling clears PFL..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Enable PFL first
    auto resp = disp.execute(makeCommand("pfl2", CommandMethod::ChannelPfl,
                                          "{\"channel\":3,\"value\":true}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(3)->getPfl()           == true);
    assert(engine->getChannelManager().getChannel(3)->getDirectMonitor() == false);

    // Now enable DM — must clear PFL
    resp = disp.execute(makeCommand("d2", CommandMethod::ChannelDm,
                                    "{\"channel\":3,\"value\":true}"));
    assert(resp.success);
    assert(engine->getChannelManager().getChannel(3)->getDirectMonitor() == true);
    assert(engine->getChannelManager().getChannel(3)->getPfl()           == false);

    std::cout << "[PASS] DM enabling clears PFL." << std::endl;
}

void testDispatcherOutputCommands()
{
    std::cout << "[TEST] CommandDispatcher output commands..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // Mute output 1
    auto resp = disp.execute(makeCommand("om1", CommandMethod::OutputMute,
                                          "{\"output\":1,\"value\":true}"));
    assert(resp.success);
    assert(engine->getOutputManager().getOutput(1)->getMute() == true);

    // Unmute
    resp = disp.execute(makeCommand("om2", CommandMethod::OutputMute,
                                    "{\"output\":1,\"value\":false}"));
    assert(resp.success);
    assert(engine->getOutputManager().getOutput(1)->getMute() == false);

    // Fader
    resp = disp.execute(makeCommand("of1", CommandMethod::OutputFader,
                                    "{\"output\":0,\"value\":-6.0}"));
    assert(resp.success);
    assert(std::abs(engine->getOutputManager().getOutput(0)->getFaderDb() - (-6.0f)) < 0.001f);

    // Monitor
    resp = disp.execute(makeCommand("oM1", CommandMethod::OutputMonitor,
                                    "{\"output\":2,\"value\":true}"));
    assert(resp.success);
    assert(engine->getOutputManager().getOutput(2)->getMonitor() == true);

    std::cout << "[PASS] CommandDispatcher output commands." << std::endl;
}

void testDispatcherRouteCommands()
{
    std::cout << "[TEST] CommandDispatcher route commands..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);
    auto& router = engine->getRoutingEngine();

    // Enable route ch=2 out=1
    auto resp = disp.execute(makeCommand("r1", CommandMethod::RouteEnable,
                                          "{\"channel\":2,\"output\":1,\"value\":true}"));
    assert(resp.success);
    assert(router.isRouteEnabled(2, 1) == true);

    // Set route gain
    resp = disp.execute(makeCommand("r2", CommandMethod::RouteGain,
                                    "{\"channel\":2,\"output\":1,\"value\":-6.0}"));
    assert(resp.success);
    assert(std::abs(router.getRouteGainDb(2, 1) - (-6.0f)) < 0.001f);

    // Set route pan
    resp = disp.execute(makeCommand("r3", CommandMethod::RoutePan,
                                    "{\"channel\":2,\"output\":1,\"value\":0.5}"));
    assert(resp.success);
    assert(std::abs(router.getRoutePan(2, 1) - 0.5f) < 0.001f);

    // Disable route
    resp = disp.execute(makeCommand("r4", CommandMethod::RouteEnable,
                                    "{\"channel\":2,\"output\":1,\"value\":false}"));
    assert(resp.success);
    assert(router.isRouteEnabled(2, 1) == false);

    // Invalid route channel
    resp = disp.execute(makeCommand("r5", CommandMethod::RouteEnable,
                                    "{\"channel\":16,\"output\":0,\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Invalid route output
    resp = disp.execute(makeCommand("r6", CommandMethod::RouteEnable,
                                    "{\"channel\":0,\"output\":4,\"value\":true}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Route gain out of range
    resp = disp.execute(makeCommand("r7", CommandMethod::RouteGain,
                                    "{\"channel\":0,\"output\":0,\"value\":25.0}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    // Route pan out of range
    resp = disp.execute(makeCommand("r8", CommandMethod::RoutePan,
                                    "{\"channel\":0,\"output\":0,\"value\":2.0}"));
    assert(!resp.success);
    assert(resp.errorCode == ErrorCode::ParameterOutOfRange);

    std::cout << "[PASS] CommandDispatcher route commands." << std::endl;
}

void testDispatcherSyncRequest()
{
    std::cout << "[TEST] CommandDispatcher sync.request..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // sync.request takes no params — should always succeed
    auto resp = disp.execute(makeCommand("s1", CommandMethod::SyncRequest, "{}"));
    assert(resp.success);
    assert(resp.id == "s1");

    std::cout << "[PASS] CommandDispatcher sync.request." << std::endl;
}

void testDispatcherAllChannelBoolCommands()
{
    std::cout << "[TEST] CommandDispatcher all boolean channel commands..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);
    auto* ch = engine->getChannelManager().getChannel(5);

    struct BoolCase { CommandMethod method; std::function<bool()> getter; };
    BoolCase cases[] = {
        { CommandMethod::ChannelSolo,  [&]{ return ch->getSolo(); } },
        { CommandMethod::ChannelPhase, [&]{ return ch->getPhaseInvert(); } },
        { CommandMethod::ChannelMono,  [&]{ return ch->getForceMono(); } },
        { CommandMethod::ChannelHpf,   [&]{ return ch->getHpfEnabled(); } },
        { CommandMethod::ChannelPad,   [&]{ return ch->getPadEnabled(); } },
    };

    for (auto& c : cases)
    {
        auto on  = makeCommand("bc_on",  c.method, "{\"channel\":5,\"value\":true}");
        auto off = makeCommand("bc_off", c.method, "{\"channel\":5,\"value\":false}");

        auto r1 = disp.execute(on);
        assert(r1.success);
        assert(c.getter() == true);

        auto r2 = disp.execute(off);
        assert(r2.success);
        assert(c.getter() == false);
    }

    // channel.on maps to setDisableOutput(!val)
    auto ron = disp.execute(makeCommand("on1", CommandMethod::ChannelOn,
                                         "{\"channel\":5,\"value\":true}"));
    assert(ron.success);
    assert(ch->getDisableOutput() == false);

    auto roff = disp.execute(makeCommand("on2", CommandMethod::ChannelOn,
                                          "{\"channel\":5,\"value\":false}"));
    assert(roff.success);
    assert(ch->getDisableOutput() == true);

    std::cout << "[PASS] CommandDispatcher all boolean channel commands." << std::endl;
}

void testFullJsonRoundTrip()
{
    std::cout << "[TEST] Full JSON parse → dispatch → serialise round-trip..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher disp(*engine);

    // 1. Build a wire-format JSON command string
    const auto wireIn = makeCommandJson("rt1", "channel.fader",
                                        "{\"channel\":7,\"value\":-12.0}");

    // 2. Parse the frame
    auto frame = parseFrame(wireIn);
    assert(frame.type == MessageType::Command);
    assert(frame.parseError.isEmpty());

    // 3. Extract command
    DSDCPCommand cmd;
    auto extractErr = extractCommand(frame, cmd);
    assert(!extractErr.has_value());
    assert(cmd.method == CommandMethod::ChannelFader);

    // 4. Dispatch
    auto resp = disp.execute(cmd);
    assert(resp.success);
    assert(std::abs(engine->getChannelManager().getChannel(7)->getFaderDb()
                    - (-12.0f)) < 0.001f);

    // 5. Serialise the response
    auto wireOut = serialiseResponse(resp);
    auto parsedOut = juce::JSON::parse(wireOut);
    assert(parsedOut["type"].toString()              == "response");
    assert(parsedOut["id"].toString()                == "rt1");
    assert(static_cast<bool>(parsedOut["success"])   == true);

    std::cout << "[PASS] Full JSON parse → dispatch → serialise round-trip." << std::endl;
}

// ===========================================================================
// Stage B: MixerStateSnapshot and StateObserver tests
// ===========================================================================

class MockObserverListener : public StateObserver::Listener
{
public:
    std::vector<std::vector<juce::String>> eventBatches;
    std::vector<juce::String> meterEvents;

    void onStateEvents(const std::vector<juce::String>& events) override
    {
        eventBatches.push_back(events);
    }

    void onMeterEvent(const juce::String& meterJson) override
    {
        meterEvents.push_back(meterJson);
    }

    void reset()
    {
        eventBatches.clear();
        meterEvents.clear();
    }
};

void testSnapshotCaptureAndEquality()
{
    std::cout << "[TEST] MixerStateSnapshot capture and equality..." << std::endl;

    auto engine = makeTestEngine();
    auto snap1 = captureSnapshot(*engine);
    auto snap2 = captureSnapshot(*engine);

    assert(snap1.channels.size() == 16);
    assert(snap1.outputs.size() == 4);
    assert(snap1.routing.size() == 16);
    assert(snap1.routing[0].size() == 4);

    // Initial snapshots of same engine state should compare equal
    assert(snap1.channels[0] == snap2.channels[0]);
    assert(snap1.outputs[0] == snap2.outputs[0]);
    assert(snap1.routing[0][0] == snap2.routing[0][0]);

    // Modify a channel on engine and capture snap3
    engine->getChannelManager().getChannel(0)->setMute(true);
    auto snap3 = captureSnapshot(*engine);
    assert(snap1.channels[0] != snap3.channels[0]);

    // Modify an output on engine and capture snap4
    engine->getOutputManager().getOutput(1)->setFaderDb(-12.0f);
    auto snap4 = captureSnapshot(*engine);
    assert(snap3.outputs[1] != snap4.outputs[1]);

    // Modify a route on engine and capture snap5
    engine->getRoutingEngine().setRouteEnabled(1, 2, true);
    auto snap5 = captureSnapshot(*engine);
    assert(snap4.routing[1][2] != snap5.routing[1][2]);

    std::cout << "[PASS] MixerStateSnapshot capture and equality." << std::endl;
}

void testSnapshotDiffing()
{
    std::cout << "[TEST] MixerStateSnapshot diffing..." << std::endl;

    auto engine = makeTestEngine();
    auto initialSnap = captureSnapshot(*engine);

    // Identical snapshots produce 0 diffs
    auto noDiffs = diffSnapshots(initialSnap, initialSnap);
    assert(noDiffs.empty());

    // 1. Channel diff
    auto modSnap1 = initialSnap;
    modSnap1.channels[2].faderDb = -18.5f;
    modSnap1.channels[2].mute = true;
    auto diffs1 = diffSnapshots(initialSnap, modSnap1);
    assert(diffs1.size() == 1);
    auto p1 = juce::JSON::parse(diffs1[0]);
    assert(p1["type"].toString() == "event");
    assert(p1["event"].toString() == "channel.state");
    assert(static_cast<int>(p1["channel"]) == 2);
    assert(static_cast<bool>(p1["state"]["mute"]) == true);
    assert(std::abs(static_cast<float>(p1["state"]["fader"]) - (-18.5f)) < 0.001f);

    // 2. Output diff
    auto modSnap2 = initialSnap;
    modSnap2.outputs[3].monitor = true;
    auto diffs2 = diffSnapshots(initialSnap, modSnap2);
    assert(diffs2.size() == 1);
    auto p2 = juce::JSON::parse(diffs2[0]);
    assert(p2["type"].toString() == "event");
    assert(p2["event"].toString() == "output.state");
    assert(static_cast<int>(p2["output"]) == 3);
    assert(static_cast<bool>(p2["state"]["monitor"]) == true);

    // 3. Route diff
    auto modSnap3 = initialSnap;
    modSnap3.routing[4][1].enabled = true;
    modSnap3.routing[4][1].sendGainDb = -3.0f;
    auto diffs3 = diffSnapshots(initialSnap, modSnap3);
    assert(diffs3.size() == 1);
    auto p3 = juce::JSON::parse(diffs3[0]);
    assert(p3["type"].toString() == "event");
    assert(p3["event"].toString() == "route.state");
    assert(static_cast<int>(p3["channel"]) == 4);
    assert(static_cast<int>(p3["output"]) == 1);
    assert(static_cast<bool>(p3["state"]["enabled"]) == true);
    assert(std::abs(static_cast<float>(p3["state"]["gain"]) - (-3.0f)) < 0.001f);

    // 4. Combined diffs across channels, outputs, routes
    auto modSnap4 = initialSnap;
    modSnap4.channels[0].gainDb = 6.0f;
    modSnap4.channels[5].solo = true;
    modSnap4.outputs[0].faderDb = -6.0f;
    modSnap4.routing[0][0].sendPan = -0.5f;
    auto diffs4 = diffSnapshots(initialSnap, modSnap4);
    assert(diffs4.size() == 4);

    std::cout << "[PASS] MixerStateSnapshot diffing." << std::endl;
}

void testSyncMessageGeneration()
{
    std::cout << "[TEST] Sync message snapshot generation..." << std::endl;

    auto engine = makeTestEngine();
    auto snap = captureSnapshot(*engine);

    auto syncJson = makeSyncJson(snap);
    auto parsed = juce::JSON::parse(syncJson);

    assert(parsed["type"].toString() == "sync");
    assert(parsed["channels"].isArray());
    assert(parsed["channels"].size() == 16);
    assert(parsed["outputs"].isArray());
    assert(parsed["outputs"].size() == 4);
    assert(parsed["routing"].isArray());
    assert(parsed["routing"].size() == 16 * 4);

    // Check channel 0 fields
    const auto& ch0 = parsed["channels"][0];
    assert(ch0.hasProperty("index"));
    assert(ch0.hasProperty("name"));
    assert(ch0.hasProperty("mute"));
    assert(ch0.hasProperty("gain"));
    assert(ch0.hasProperty("fader"));
    assert(ch0.hasProperty("pan"));
    assert(ch0.hasProperty("pfl"));
    assert(ch0.hasProperty("dm"));
    assert(ch0.hasProperty("solo"));
    assert(ch0.hasProperty("on"));
    assert(ch0.hasProperty("phase"));
    assert(ch0.hasProperty("mono"));
    assert(ch0.hasProperty("hpf"));
    assert(ch0.hasProperty("pad"));

    // Check output 0 fields
    const auto& out0 = parsed["outputs"][0];
    assert(out0.hasProperty("index"));
    assert(out0.hasProperty("name"));
    assert(out0.hasProperty("mute"));
    assert(out0.hasProperty("fader"));
    assert(out0.hasProperty("monitor"));

    // Check route fields
    const auto& r0 = parsed["routing"][0];
    assert(r0.hasProperty("channel"));
    assert(r0.hasProperty("output"));
    assert(r0.hasProperty("enabled"));
    assert(r0.hasProperty("gain"));
    assert(r0.hasProperty("pan"));

    std::cout << "[PASS] Sync message snapshot generation." << std::endl;
}

void testStateObserverDirtyCheckingAndEvents()
{
    std::cout << "[TEST] StateObserver dirty-checking and listener events..." << std::endl;

    auto engine = makeTestEngine();
    StateObserver observer(*engine);
    MockObserverListener listener;
    observer.addListener(&listener);

    // Polling without changes yields zero events
    auto events0 = observer.pollState();
    assert(events0.empty());
    assert(listener.eventBatches.empty());

    // Modify channel 1 via engine
    engine->getChannelManager().getChannel(1)->setMute(true);
    auto events1 = observer.pollState();
    assert(events1.size() == 1);
    assert(listener.eventBatches.size() == 1);
    assert(listener.eventBatches[0].size() == 1);

    auto p1 = juce::JSON::parse(listener.eventBatches[0][0]);
    assert(p1["type"].toString() == "event");
    assert(p1["event"].toString() == "channel.state");
    assert(static_cast<int>(p1["channel"]) == 1);
    assert(static_cast<bool>(p1["state"]["mute"]) == true);

    // Second poll without changes should produce 0 events
    listener.reset();
    auto events2 = observer.pollState();
    assert(events2.empty());
    assert(listener.eventBatches.empty());

    // Modify multiple things: Output 2 fader and Route (3, 1) enabled
    engine->getOutputManager().getOutput(2)->setFaderDb(-24.0f);
    engine->getRoutingEngine().setRouteEnabled(3, 1, true);

    auto events3 = observer.pollState();
    assert(events3.size() == 2);
    assert(listener.eventBatches.size() == 1);
    assert(listener.eventBatches[0].size() == 2);

    observer.removeListener(&listener);
    std::cout << "[PASS] StateObserver dirty-checking and listener events." << std::endl;
}

void testStateObserverSyncAndMeters()
{
    std::cout << "[TEST] StateObserver sync and meters..." << std::endl;

    auto engine = makeTestEngine();
    StateObserver observer(*engine);
    MockObserverListener listener;
    observer.addListener(&listener);

    // Full sync from observer
    auto syncJson = observer.getFullSyncJson();
    auto pSync = juce::JSON::parse(syncJson);
    assert(pSync["type"].toString() == "sync");
    assert(pSync["channels"].size() == 16);

    // Metering disabled initially
    assert(!observer.isMeteringEnabled());
    observer.setMeteringEnabled(true);
    assert(observer.isMeteringEnabled());

    // Meter interval validation (clamp to min 50ms)
    observer.setMeterIntervalMs(20);
    assert(observer.getMeterIntervalMs() == DSDCP_METER_MIN_INTERVAL_MS);
    observer.setMeterIntervalMs(120);
    assert(observer.getMeterIntervalMs() == 120);

    // Poll meters manually
    auto meterJson = observer.pollMeters();
    assert(listener.meterEvents.size() == 1);
    auto pMeter = juce::JSON::parse(meterJson);
    assert(pMeter["type"].toString() == "event");
    assert(pMeter["event"].toString() == "meters");
    assert(pMeter["channels"].size() == 16);
    assert(pMeter["outputs"].size() == 4);

    observer.removeListener(&listener);
    std::cout << "[PASS] StateObserver sync and meters." << std::endl;
}

void testEndToEndBidirectionalSync()
{
    std::cout << "[TEST] End-to-end bidirectional sync (remote command & desktop UI)..." << std::endl;

    auto engine = makeTestEngine();
    CommandDispatcher dispatcher(*engine);
    StateObserver observer(*engine);
    MockObserverListener listener;
    observer.addListener(&listener);

    // 1. Remote command flow: Client -> CommandDispatcher -> Engine -> StateObserver -> Remote Event
    auto cmd = makeCommand("cmd_mute", CommandMethod::ChannelMute, "{\"channel\":7,\"value\":true}");
    auto resp = dispatcher.execute(cmd);
    assert(resp.success);

    // StateObserver polls engine
    auto deltas = observer.pollState();
    assert(deltas.size() == 1);
    assert(listener.eventBatches.size() == 1);
    auto eventRemote = juce::JSON::parse(listener.eventBatches[0][0]);
    assert(eventRemote["type"].toString() == "event");
    assert(eventRemote["event"].toString() == "channel.state");
    assert(static_cast<int>(eventRemote["channel"]) == 7);
    assert(static_cast<bool>(eventRemote["state"]["mute"]) == true);

    listener.reset();

    // 2. Desktop UI flow: UI directly sets atomic parameter on AudioEngine -> StateObserver -> Remote Event
    // User moves output 0 fader in main desktop UI:
    engine->getOutputManager().getOutput(0)->setFaderDb(-14.5f);

    deltas = observer.pollState();
    assert(deltas.size() == 1);
    assert(listener.eventBatches.size() == 1);
    auto eventUI = juce::JSON::parse(listener.eventBatches[0][0]);
    assert(eventUI["type"].toString() == "event");
    assert(eventUI["event"].toString() == "output.state");
    assert(static_cast<int>(eventUI["output"]) == 0);
    assert(std::abs(static_cast<float>(eventUI["state"]["fader"]) - (-14.5f)) < 0.001f);

    observer.removeListener(&listener);
    std::cout << "[PASS] End-to-end bidirectional sync (remote command & desktop UI)." << std::endl;
}

// ===========================================================================
// Stage C: ClientManager, WebSocketServer, and ControlServer tests
// ===========================================================================

void testClientManagerRegistrationAndAuth()
{
    std::cout << "[TEST] ClientManager registration and auth..." << std::endl;

    ClientManager mgr;
    mgr.setPin("987654");
    assert(mgr.getPin() == "987654");
    assert(mgr.verifyPin("987654") == true);
    assert(mgr.verifyPin("111111") == false);

    // Register with invalid PIN
    auto c1 = mgr.registerClient("conn_1", nullptr, "A9", "control_surface", "000000");
    assert(c1 != nullptr);
    assert(c1->authenticated == false);
    assert(mgr.getConnectedCount() == 1);
    assert(mgr.getAuthenticatedCount() == 0);

    // Register with valid PIN
    auto c2 = mgr.registerClient("conn_2", nullptr, "Web", "monitor", "987654");
    assert(c2 != nullptr);
    assert(c2->authenticated == true);
    assert(c2->clientId == "c_2");
    assert(mgr.getConnectedCount() == 2);
    assert(mgr.getAuthenticatedCount() == 1);

    // Check default subscriptions
    assert(c2->isSubscribedTo("channel.state"));
    assert(c2->isSubscribedTo("output.state"));
    assert(c2->isSubscribedTo("route.state"));
    assert(!c2->isSubscribedTo("custom.state"));
    assert(!c2->meteringSubscribed);

    // Update subscriptions
    mgr.updateSubscriptions("conn_2", {"output.state"}, true, 75);
    assert(!c2->isSubscribedTo("channel.state"));
    assert(c2->isSubscribedTo("output.state"));
    assert(c2->meteringSubscribed == true);
    assert(c2->meterIntervalMs == 75);
    assert(mgr.hasAnyMeteringSubscriber() == true);

    // Unregister
    mgr.unregisterClient("conn_1");
    assert(mgr.getConnectedCount() == 1);
    assert(mgr.getClient("conn_1") == nullptr);

    mgr.clear();
    assert(mgr.getConnectedCount() == 0);

    std::cout << "[PASS] ClientManager registration and auth." << std::endl;
}

void testControlServerLifecycleAndPort()
{
    std::cout << "[TEST] ControlServer lifecycle and port..." << std::endl;

    const int port = 9187;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);

    assert(!server.isRunning());
    assert(server.getConnectedClientCount() == 0);

    bool started = server.start(port);
    assert(started);
    assert(server.isRunning());
    assert(server.getPort() == port);

    server.stop();
    assert(!server.isRunning());

    std::cout << "[PASS] ControlServer lifecycle and port." << std::endl;
}

void testControlServerClientConnectAndHandshake()
{
    std::cout << "[TEST] ControlServer client connect and handshake..." << std::endl;

    const int port = 9188;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);
    server.setPin("654321");
    bool serverStarted = server.start(port);
    assert(serverStarted);

    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:9188");

    std::mutex clientMutex;
    std::vector<std::string> receivedMessages;
    std::atomic<bool> connected { false };

    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            connected = true;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(clientMutex);
            receivedMessages.push_back(msg->str);
        }
    });

    client.start();

    // Wait up to 2 seconds for connection
    for (int i = 0; i < 200 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(connected);

    // 1. Send hello with invalid PIN
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"TestPhone\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"000000\"}}");

    std::string errMsg;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (!receivedMessages.empty()) {
                errMsg = receivedMessages.back();
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!errMsg.empty());
    auto pErr = juce::JSON::parse(juce::String(errMsg));
    assert(pErr["type"].toString() == "error");
    assert(static_cast<int>(pErr["error"]["code"]) == static_cast<int>(ErrorCode::NotAuthenticated));

    // 2. Send hello with valid PIN
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"Samsung A9\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"654321\"}}");

    std::vector<std::string> msgs;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (receivedMessages.size() >= 2) {
                msgs = receivedMessages;
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(msgs.size() >= 2);
    auto pWelcome = juce::JSON::parse(juce::String(msgs[0]));
    assert(pWelcome["type"].toString() == "welcome");
    assert(pWelcome["protocol"].toString() == "DSDCP");
    assert(static_cast<int>(pWelcome["version"]) == 1);
    assert(static_cast<int>(pWelcome["session"]["capabilities"]["channels"]) == 16);

    auto pSync = juce::JSON::parse(juce::String(msgs[1]));
    assert(pSync["type"].toString() == "sync");
    assert(pSync["channels"].size() == 16);

    assert(server.getAuthenticatedClientCount() == 1);

    client.stop();
    server.stop();
    std::cout << "[PASS] ControlServer client connect and handshake." << std::endl;
}

void testControlServerCommandExecutionOverWebSocket()
{
    std::cout << "[TEST] ControlServer command execution over WebSocket..." << std::endl;

    const int port = 9189;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);
    server.setPin("123456");
    bool serverStarted = server.start(port);
    assert(serverStarted);

    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:9189");

    std::mutex clientMutex;
    std::vector<std::string> receivedMessages;
    std::atomic<bool> connected { false };

    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            connected = true;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(clientMutex);
            receivedMessages.push_back(msg->str);
        }
    });

    client.start();
    for (int i = 0; i < 200 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(connected);

    // Handshake
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"A9\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"123456\"}}");

    // Wait for welcome & sync
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (receivedMessages.size() >= 2) {
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Send control command: channel.fader
    client.sendText("{\"type\":\"command\",\"id\":\"c_fad_1\",\"method\":\"channel.fader\",\"params\":{\"channel\":3,\"value\":-9.5}}");

    // Wait for response and delta event
    std::string respStr;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            for (const auto& m : receivedMessages) {
                auto p = juce::JSON::parse(juce::String(m));
                if (p["type"].toString() == "response" && p["id"].toString() == "c_fad_1") {
                    respStr = m;
                    break;
                }
            }
            if (!respStr.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!respStr.empty());
    auto pResp = juce::JSON::parse(juce::String(respStr));
    assert(static_cast<bool>(pResp["success"]) == true);

    // Verify engine state
    assert(std::abs(engine->getChannelManager().getChannel(3)->getFaderDb() - (-9.5f)) < 0.001f);

    // Trigger state observer poll and verify event is broadcast
    server.getStateObserver().pollState();

    std::string evStr;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            for (const auto& m : receivedMessages) {
                auto p = juce::JSON::parse(juce::String(m));
                if (p["type"].toString() == "event" && p["event"].toString() == "channel.state") {
                    if (static_cast<int>(p["channel"]) == 3) {
                        evStr = m;
                        break;
                    }
                }
            }
            if (!evStr.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!evStr.empty());
    auto pEv = juce::JSON::parse(juce::String(evStr));
    assert(std::abs(static_cast<float>(pEv["state"]["fader"]) - (-9.5f)) < 0.001f);

    client.stop();
    server.stop();
    std::cout << "[PASS] ControlServer command execution over WebSocket." << std::endl;
}

void testControlServerRestrictionOnSessionOperations()
{
    std::cout << "[TEST] ControlServer rejection of remote session load/save..." << std::endl;

    const int port = 9190;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);
    server.setPin("123456");
    bool serverStarted = server.start(port);
    assert(serverStarted);

    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:9190");

    std::mutex clientMutex;
    std::vector<std::string> receivedMessages;
    std::atomic<bool> connected { false };

    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            connected = true;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(clientMutex);
            receivedMessages.push_back(msg->str);
        }
    });

    client.start();
    for (int i = 0; i < 200 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(connected);

    // Authenticate
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"A9\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"123456\"}}");
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (receivedMessages.size() >= 2) {
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Try session.load
    client.sendText("{\"type\":\"command\",\"id\":\"s_load_1\",\"method\":\"session.load\",\"params\":{\"path\":\"foo.dsd\"}}");

    std::string respStr;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (!receivedMessages.empty()) {
                respStr = receivedMessages.back();
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!respStr.empty());
    auto pResp = juce::JSON::parse(juce::String(respStr));
    assert(static_cast<bool>(pResp["success"]) == false);
    assert(static_cast<int>(pResp["error"]["code"]) == static_cast<int>(ErrorCode::PermissionDenied));

    // Try session.save
    client.sendText("{\"type\":\"command\",\"id\":\"s_save_1\",\"method\":\"session.save\",\"params\":{\"path\":\"foo.dsd\"}}");
    respStr.clear();
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (!receivedMessages.empty()) {
                respStr = receivedMessages.back();
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!respStr.empty());
    auto pSaveResp = juce::JSON::parse(juce::String(respStr));
    assert(static_cast<bool>(pSaveResp["success"]) == false);
    assert(static_cast<int>(pSaveResp["error"]["code"]) == static_cast<int>(ErrorCode::PermissionDenied));

    client.stop();
    server.stop();
    std::cout << "[PASS] ControlServer rejection of remote session load/save." << std::endl;
}

void testControlServerMeteringSubscription()
{
    std::cout << "[TEST] ControlServer metering subscription..." << std::endl;

    const int port = 9191;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);
    server.setPin("123456");
    bool serverStarted = server.start(port);
    assert(serverStarted);

    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:9191");

    std::mutex clientMutex;
    std::vector<std::string> receivedMessages;
    std::atomic<bool> connected { false };

    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            connected = true;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(clientMutex);
            receivedMessages.push_back(msg->str);
        }
    });

    client.start();
    for (int i = 0; i < 200 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(connected);

    // Authenticate
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"A9\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"123456\"}}");
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (receivedMessages.size() >= 2) {
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Subscribe to meters
    assert(!server.getStateObserver().isMeteringEnabled());
    client.sendText("{\"type\":\"subscribe\",\"events\":[\"channel.state\"],\"meters\":{\"enabled\":true,\"intervalMs\":60}}");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(server.getStateObserver().isMeteringEnabled());

    // Poll meters on observer
    server.getStateObserver().pollMeters();

    std::string meterMsg;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            for (const auto& m : receivedMessages) {
                auto p = juce::JSON::parse(juce::String(m));
                if (p["type"].toString() == "event" && p["event"].toString() == "meters") {
                    meterMsg = m;
                    break;
                }
            }
            if (!meterMsg.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!meterMsg.empty());
    auto pMeter = juce::JSON::parse(juce::String(meterMsg));
    assert(pMeter["channels"].size() == 16);
    assert(pMeter["outputs"].size() == 4);

    client.stop();
    server.stop();
    std::cout << "[PASS] ControlServer metering subscription." << std::endl;
}

void testControlServerDesktopIntegration()
{
    std::cout << "[TEST] ControlServer desktop integration & client querying (Stage D)..." << std::endl;

    const int port = 9192;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);

    // Default PIN check
    assert(server.getPin() == "123456");

    // Change PIN
    server.setPin("849201");
    assert(server.getPin() == "849201");

    // Start server on 0.0.0.0
    bool started = server.start(port, "0.0.0.0");
    assert(started);
    assert(server.isRunning());

    // Initially empty clients list
    auto allClients0 = server.getAllClients();
    assert(allClients0.empty());

    // Connect client
    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:9192");

    std::atomic<bool> connected { false };
    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            connected = true;
        }
    });

    client.start();
    for (int i = 0; i < 200 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(connected);

    // Send hello handshake
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"Samsung Galaxy A9\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"849201\"}}");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto allClients1 = server.getAllClients();
    assert(allClients1.size() == 1);
    assert(allClients1[0]->name == "Samsung Galaxy A9");
    assert(allClients1[0]->type == "control_surface");
    assert(allClients1[0]->authenticated == true);

    client.stop();
    server.stop();
    assert(!server.isRunning());

    std::cout << "[PASS] ControlServer desktop integration & client querying (Stage D)." << std::endl;
}

void testControlServerUsbAdbTransportEndpoint()
{
    std::cout << "[TEST] ControlServer USB/ADB loopback transport and security (Stage U)..." << std::endl;

    const int port = 9193;
    auto engine = makeTestEngine();
    ControlServer server(*engine, port);
    server.setPin("739281");

    // Start server binding to 0.0.0.0 so both LAN and loopback work
    bool started = server.start(port, "0.0.0.0");
    assert(started);
    assert(server.isRunning());

    // Connect client to loopback endpoint (ws://127.0.0.1:9193 as used by adb reverse)
    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:9193");

    std::mutex clientMutex;
    std::vector<std::string> receivedMessages;
    std::atomic<bool> connected { false };

    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            connected = true;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(clientMutex);
            receivedMessages.push_back(msg->str);
        }
    });

    client.start();
    for (int i = 0; i < 200 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(connected);

    // 1. Security check: Loopback/USB connection must NOT bypass PIN authentication
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"Samsung Galaxy A9 (USB)\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"000000\"}}");

    std::string errMsg;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (!receivedMessages.empty()) {
                errMsg = receivedMessages.back();
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!errMsg.empty());
    auto pErr = juce::JSON::parse(juce::String(errMsg));
    assert(pErr["type"].toString() == "error");
    assert(static_cast<int>(pErr["error"]["code"]) == static_cast<int>(ErrorCode::NotAuthenticated));
    assert(server.getAuthenticatedClientCount() == 0);

    // 2. Handshake with valid PIN over USB loopback
    client.sendText("{\"type\":\"hello\",\"protocol\":\"DSDCP\",\"version\":1,\"client\":{\"name\":\"Samsung Galaxy A9 (USB)\",\"type\":\"control_surface\"},\"auth\":{\"pin\":\"739281\"}}");

    std::vector<std::string> msgs;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (receivedMessages.size() >= 2) {
                msgs = receivedMessages;
                receivedMessages.clear();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(msgs.size() >= 2);
    auto pWelcome = juce::JSON::parse(juce::String(msgs[0]));
    assert(pWelcome["type"].toString() == "welcome");
    assert(server.getAuthenticatedClientCount() == 1);

    // 3. Command execution over USB loopback
    client.sendText("{\"type\":\"command\",\"id\":\"usb_cmd_1\",\"method\":\"channel.mute\",\"params\":{\"channel\":3,\"value\":true}}");

    std::string respStr;
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            for (const auto& m : receivedMessages) {
                auto p = juce::JSON::parse(juce::String(m));
                if (p["type"].toString() == "response" && p["id"].toString() == "usb_cmd_1") {
                    respStr = m;
                    break;
                }
            }
            if (!respStr.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!respStr.empty());
    auto pResp = juce::JSON::parse(juce::String(respStr));
    assert(static_cast<bool>(pResp["success"]) == true);
    assert(engine->getChannelManager().getChannel(3)->getMute() == true);

    client.stop();
    server.stop();
    std::cout << "[PASS] ControlServer USB/ADB loopback transport and security (Stage U)." << std::endl;
}

// ===========================================================================
// Entry point
// ===========================================================================

int main()
{
    ix::initNetSystem();

    std::cout << "=========================================================" << std::endl;
    std::cout << " DSD Control Protocol — Full Test Suite (Stages A..U)   " << std::endl;
    std::cout << "=========================================================" << std::endl;

    // --- DSDCPTypes ---
    testMessageTypeRoundTrip();
    testCommandMethodRoundTrip();
    testDSDCPResponseHelpers();
    testParameterValidationHelpers();

    // --- DSDCPSerializer ---
    testSerialiserParseFrame();
    testSerialiserExtractCommand();
    testSerialiserResponseRoundTrip();

    // --- CommandDispatcher (Stage A) ---
    testDispatcherChannelMute();
    testDispatcherInvalidChannelIndex();
    testDispatcherInvalidOutputIndex();
    testDispatcherMissingParameter();
    testDispatcherInvalidParameterType();
    testDispatcherUnknownCommand();
    testDispatcherGainRange();
    testDispatcherFaderRange();
    testDispatcherPanRange();
    testDispatcherPflClearsDm();
    testDispatcherDmClearsPfl();
    testDispatcherOutputCommands();
    testDispatcherRouteCommands();
    testDispatcherSyncRequest();
    testDispatcherAllChannelBoolCommands();
    testFullJsonRoundTrip();

    // --- StateObserver & MixerStateSnapshot (Stage B) ---
    testSnapshotCaptureAndEquality();
    testSnapshotDiffing();
    testSyncMessageGeneration();
    testStateObserverDirtyCheckingAndEvents();
    testStateObserverSyncAndMeters();
    testEndToEndBidirectionalSync();

    // --- ClientManager & ControlServer (Stage C) ---
    testClientManagerRegistrationAndAuth();
    testControlServerLifecycleAndPort();
    testControlServerClientConnectAndHandshake();
    testControlServerCommandExecutionOverWebSocket();
    testControlServerRestrictionOnSessionOperations();
    testControlServerMeteringSubscription();

    // --- Desktop Integration (Stage D) ---
    testControlServerDesktopIntegration();

    // --- USB / ADB Transport (Stage U) ---
    testControlServerUsbAdbTransportEndpoint();

    std::cout << "=========================================================" << std::endl;
    std::cout << " All Control Protocol & USB Transport Tests Passed!     " << std::endl;
    std::cout << "=========================================================" << std::endl;
    return 0;
}
