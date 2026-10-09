#include "Control/MixerStateSnapshot.h"
#include "Audio/AudioEngine.h"
#include "Channel/ChannelManager.h"
#include "Channel/AudioChannel.h"
#include "Output/OutputManager.h"
#include "Output/OutputBus.h"
#include "Routing/RoutingEngine.h"

namespace dsd::control
{
    MixerStateSnapshot captureSnapshot(const AudioEngine& engine)
    {
        MixerStateSnapshot snap;

        // 1. Capture Channels
        const auto& chMgr = engine.getChannelManager();
        for (int i = 0; i < NUM_CHANNELS_LEVEL1; ++i)
        {
            auto& chSnap = snap.channels[static_cast<size_t>(i)];
            chSnap.index = i;

            const auto* ch = chMgr.getChannel(i);
            if (ch != nullptr)
            {
                chSnap.name    = ch->getName();
                chSnap.mute    = ch->getMute();
                chSnap.gainDb  = ch->getGainDb();
                chSnap.faderDb = ch->getFaderDb();
                chSnap.pan     = ch->getPan();
                chSnap.pfl     = ch->getPfl();
                chSnap.dm      = ch->getDirectMonitor();
                chSnap.solo    = ch->getSolo();
                chSnap.on      = !ch->getDisableOutput();
                chSnap.phase   = ch->getPhaseInvert();
                chSnap.mono    = ch->getForceMono();
                chSnap.hpf     = ch->getHpfEnabled();
                chSnap.pad     = ch->getPadEnabled();
            }
        }

        // 2. Capture Outputs
        const auto& outMgr = engine.getOutputManager();
        for (int i = 0; i < NUM_OUTPUT_BUSES_LEVEL1; ++i)
        {
            auto& outSnap = snap.outputs[static_cast<size_t>(i)];
            outSnap.index = i;

            const auto* out = outMgr.getOutput(i);
            if (out != nullptr)
            {
                outSnap.name    = out->getName();
                outSnap.mute    = out->getMute();
                outSnap.faderDb = out->getFaderDb();
                outSnap.monitor = out->getMonitor();
            }
        }

        // 3. Capture Routing
        const auto& router = engine.getRoutingEngine();
        for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
        {
            for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                auto& rSnap = snap.routing[static_cast<size_t>(ch)][static_cast<size_t>(out)];
                rSnap.channel    = ch;
                rSnap.output     = out;
                rSnap.enabled    = router.isRouteEnabled(ch, out);
                rSnap.sendGainDb = router.getRouteGainDb(ch, out);
                rSnap.sendPan    = router.getRoutePan(ch, out);
            }
        }

        return snap;
    }

    juce::String makeChannelStateEventJson(const ChannelSnapshot& ch)
    {
        auto stateObj = std::make_unique<juce::DynamicObject>();
        stateObj->setProperty("mute",  ch.mute);
        stateObj->setProperty("gain",  ch.gainDb);
        stateObj->setProperty("fader", ch.faderDb);
        stateObj->setProperty("pan",   ch.pan);
        stateObj->setProperty("pfl",   ch.pfl);
        stateObj->setProperty("dm",    ch.dm);
        stateObj->setProperty("solo",  ch.solo);
        stateObj->setProperty("on",    ch.on);
        stateObj->setProperty("phase", ch.phase);
        stateObj->setProperty("mono",  ch.mono);
        stateObj->setProperty("hpf",   ch.hpf);
        stateObj->setProperty("pad",   ch.pad);
        stateObj->setProperty("name",  juce::String(ch.name));

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type",    "event");
        root->setProperty("event",   "channel.state");
        root->setProperty("channel", ch.index);
        root->setProperty("state",   juce::var(stateObj.release()));

        return juce::JSON::toString(juce::var(root.release()), false);
    }

    juce::String makeOutputStateEventJson(const OutputSnapshot& out)
    {
        auto stateObj = std::make_unique<juce::DynamicObject>();
        stateObj->setProperty("mute",    out.mute);
        stateObj->setProperty("fader",   out.faderDb);
        stateObj->setProperty("monitor", out.monitor);
        stateObj->setProperty("name",    juce::String(out.name));

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type",   "event");
        root->setProperty("event",  "output.state");
        root->setProperty("output", out.index);
        root->setProperty("state",  juce::var(stateObj.release()));

        return juce::JSON::toString(juce::var(root.release()), false);
    }

    juce::String makeRouteStateEventJson(const RouteSnapshot& route)
    {
        auto stateObj = std::make_unique<juce::DynamicObject>();
        stateObj->setProperty("enabled", route.enabled);
        stateObj->setProperty("gain",    route.sendGainDb);
        stateObj->setProperty("pan",     route.sendPan);

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type",    "event");
        root->setProperty("event",   "route.state");
        root->setProperty("channel", route.channel);
        root->setProperty("output",  route.output);
        root->setProperty("state",   juce::var(stateObj.release()));

        return juce::JSON::toString(juce::var(root.release()), false);
    }

    std::vector<juce::String> diffSnapshots(const MixerStateSnapshot& previous,
                                            const MixerStateSnapshot& current)
    {
        std::vector<juce::String> events;

        // 1. Check Channels
        for (size_t i = 0; i < NUM_CHANNELS_LEVEL1; ++i)
        {
            if (previous.channels[i] != current.channels[i])
            {
                events.push_back(makeChannelStateEventJson(current.channels[i]));
            }
        }

        // 2. Check Outputs
        for (size_t i = 0; i < NUM_OUTPUT_BUSES_LEVEL1; ++i)
        {
            if (previous.outputs[i] != current.outputs[i])
            {
                events.push_back(makeOutputStateEventJson(current.outputs[i]));
            }
        }

        // 3. Check Routing
        for (size_t ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
        {
            for (size_t out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                if (previous.routing[ch][out] != current.routing[ch][out])
                {
                    events.push_back(makeRouteStateEventJson(current.routing[ch][out]));
                }
            }
        }

        return events;
    }

    juce::var makeSyncVar(const MixerStateSnapshot& snapshot)
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type", "sync");

        // Channels
        juce::Array<juce::var> chArray;
        for (size_t i = 0; i < NUM_CHANNELS_LEVEL1; ++i)
        {
            const auto& ch = snapshot.channels[i];
            auto chObj = std::make_unique<juce::DynamicObject>();
            chObj->setProperty("index", ch.index);
            chObj->setProperty("name",  juce::String(ch.name));
            chObj->setProperty("mute",  ch.mute);
            chObj->setProperty("gain",  ch.gainDb);
            chObj->setProperty("fader", ch.faderDb);
            chObj->setProperty("pan",   ch.pan);
            chObj->setProperty("pfl",   ch.pfl);
            chObj->setProperty("dm",    ch.dm);
            chObj->setProperty("solo",  ch.solo);
            chObj->setProperty("on",    ch.on);
            chObj->setProperty("phase", ch.phase);
            chObj->setProperty("mono",  ch.mono);
            chObj->setProperty("hpf",   ch.hpf);
            chObj->setProperty("pad",   ch.pad);
            chArray.add(juce::var(chObj.release()));
        }
        root->setProperty("channels", chArray);

        // Outputs
        juce::Array<juce::var> outArray;
        for (size_t i = 0; i < NUM_OUTPUT_BUSES_LEVEL1; ++i)
        {
            const auto& out = snapshot.outputs[i];
            auto outObj = std::make_unique<juce::DynamicObject>();
            outObj->setProperty("index",   out.index);
            outObj->setProperty("name",    juce::String(out.name));
            outObj->setProperty("mute",    out.mute);
            outObj->setProperty("fader",   out.faderDb);
            outObj->setProperty("monitor", out.monitor);
            outArray.add(juce::var(outObj.release()));
        }
        root->setProperty("outputs", outArray);

        // Routing
        juce::Array<juce::var> routeArray;
        for (size_t ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
        {
            for (size_t out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                const auto& r = snapshot.routing[ch][out];
                auto rObj = std::make_unique<juce::DynamicObject>();
                rObj->setProperty("channel", r.channel);
                rObj->setProperty("output",  r.output);
                rObj->setProperty("enabled", r.enabled);
                rObj->setProperty("gain",    r.sendGainDb);
                rObj->setProperty("pan",     r.sendPan);
                routeArray.add(juce::var(rObj.release()));
            }
        }
        root->setProperty("routing", routeArray);

        return juce::var(root.release());
    }

    juce::String makeSyncJson(const MixerStateSnapshot& snapshot)
    {
        return juce::JSON::toString(makeSyncVar(snapshot), false);
    }

    juce::String makeMetersEventJson(const AudioEngine& engine)
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("type",  "event");
        root->setProperty("event", "meters");

        // Channel Meters (Post-fader)
        juce::Array<juce::var> chMeters;
        const auto& chMgr = engine.getChannelManager();
        for (int i = 0; i < NUM_CHANNELS_LEVEL1; ++i)
        {
            const auto* ch = chMgr.getChannel(i);
            if (ch != nullptr)
            {
                const auto& mv = ch->getMeterValues();
                auto item = std::make_unique<juce::DynamicObject>();
                item->setProperty("ch",    i);
                item->setProperty("peakL", mv.peakL.load(std::memory_order_relaxed));
                item->setProperty("peakR", mv.peakR.load(std::memory_order_relaxed));
                item->setProperty("clip",  mv.clipped.load(std::memory_order_relaxed));
                chMeters.add(juce::var(item.release()));
            }
        }
        root->setProperty("channels", chMeters);

        // Output Meters
        juce::Array<juce::var> outMeters;
        const auto& outMgr = engine.getOutputManager();
        for (int i = 0; i < NUM_OUTPUT_BUSES_LEVEL1; ++i)
        {
            const auto* out = outMgr.getOutput(i);
            if (out != nullptr)
            {
                const auto& mv = out->getMeterValues();
                auto item = std::make_unique<juce::DynamicObject>();
                item->setProperty("out",   i);
                item->setProperty("peakL", mv.peakL.load(std::memory_order_relaxed));
                item->setProperty("peakR", mv.peakR.load(std::memory_order_relaxed));
                item->setProperty("clip",  mv.clipped.load(std::memory_order_relaxed));
                outMeters.add(juce::var(item.release()));
            }
        }
        root->setProperty("outputs", outMeters);

        return juce::JSON::toString(juce::var(root.release()), false);
    }

} // namespace dsd::control
