#include "Session/SessionManager.h"
#include <juce_data_structures/juce_data_structures.h>

namespace dsd
{
    bool SessionManager::saveSessionToFile(const juce::File& file,
                                          const AudioEngine& engine,
                                          const AudioDeviceManager& devManager)
    {
        auto rootObj = std::make_unique<juce::DynamicObject>();
        rootObj->setProperty("product", "DSD Mixer Level 1");
        rootObj->setProperty("version", "1.0");
        rootObj->setProperty("sampleRate", devManager.getCurrentSampleRate());
        rootObj->setProperty("bufferSize", devManager.getCurrentBufferSize());

        // 1. Serialize 16 Channels
        juce::Array<juce::var> channelsArray;
        const auto& chMgr = engine.getChannelManager();
        for (int i = 0; i < chMgr.getNumChannels(); ++i)
        {
            const auto* ch = chMgr.getChannel(i);
            if (ch == nullptr) continue;

            auto chObj = std::make_unique<juce::DynamicObject>();
            chObj->setProperty("id", ch->getChannelID());
            chObj->setProperty("name", juce::String(ch->getName()));
            chObj->setProperty("gainDb", ch->getGainDb());
            chObj->setProperty("faderDb", ch->getFaderDb());
            chObj->setProperty("pan", ch->getPan());
            chObj->setProperty("mute", ch->getMute());
            chObj->setProperty("solo", ch->getSolo());
            chObj->setProperty("directMonitor", ch->getDirectMonitor());
            chObj->setProperty("disableOutput", ch->getDisableOutput());
            chObj->setProperty("phaseInvert", ch->getPhaseInvert());
            chObj->setProperty("forceMono", ch->getForceMono());

            channelsArray.add(juce::var(chObj.release()));
        }
        rootObj->setProperty("channels", channelsArray);

        // 2. Serialize 16x4 Routing Matrix
        juce::Array<juce::var> matrixArray;
        const auto& router = engine.getRoutingEngine();
        for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
        {
            for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                auto routeObj = std::make_unique<juce::DynamicObject>();
                routeObj->setProperty("channelIdx", ch);
                routeObj->setProperty("outputIdx", out);
                routeObj->setProperty("enabled", router.isRouteEnabled(ch, out));
                routeObj->setProperty("sendGainDb", router.getRouteGainDb(ch, out));
                routeObj->setProperty("sendPan", router.getRoutePan(ch, out));
                matrixArray.add(juce::var(routeObj.release()));
            }
        }
        rootObj->setProperty("routingMatrix", matrixArray);

        // 3. Serialize 4 Output Buses
        juce::Array<juce::var> outputsArray;
        const auto& outMgr = engine.getOutputManager();
        for (int i = 0; i < outMgr.getNumOutputs(); ++i)
        {
            const auto* out = outMgr.getOutput(i);
            if (out == nullptr) continue;

            auto outObj = std::make_unique<juce::DynamicObject>();
            outObj->setProperty("id", out->getBusID());
            outObj->setProperty("name", juce::String(out->getName()));
            outObj->setProperty("faderDb", out->getFaderDb());
            outObj->setProperty("mute", out->getMute());
            outObj->setProperty("monitor", out->getMonitor());
            outObj->setProperty("deviceChannelOffset", out->getDeviceChannelOffset());
            outputsArray.add(juce::var(outObj.release()));
        }
        rootObj->setProperty("outputs", outputsArray);

        // Write to file as formatted JSON
        juce::var rootVar(rootObj.release());
        juce::String jsonString = juce::JSON::toString(rootVar, true);
        return file.replaceWithText(jsonString);
    }

    bool SessionManager::loadSessionFromFile(const juce::File& file,
                                            AudioEngine& engine,
                                            AudioDeviceManager& /*devManager*/)
    {
        if (!file.existsAsFile())
            return false;

        juce::var rootVar = juce::JSON::parse(file);
        if (!rootVar.isObject())
            return false;

        auto* rootObj = rootVar.getDynamicObject();
        if (rootObj == nullptr)
            return false;

        // 1. Restore Channels
        if (rootObj->hasProperty("channels"))
        {
            const auto* channelsVar = rootObj->getProperty("channels").getArray();
            if (channelsVar != nullptr)
            {
                auto& chMgr = engine.getChannelManager();
                for (int i = 0; i < channelsVar->size() && i < chMgr.getNumChannels(); ++i)
                {
                    const auto& item = (*channelsVar)[i];
                    if (!item.isObject()) continue;
                    auto* ch = chMgr.getChannel(i);
                    if (ch == nullptr) continue;

                    ch->setName(item["name"].toString().toStdString());
                    ch->setGainDb(static_cast<float>(item["gainDb"]));
                    ch->setFaderDb(static_cast<float>(item["faderDb"]));
                    ch->setPan(static_cast<float>(item["pan"]));
                    ch->setMute(static_cast<bool>(item["mute"]));
                    ch->setSolo(static_cast<bool>(item["solo"]));
                    ch->setDirectMonitor(static_cast<bool>(item["directMonitor"]));
                    ch->setDisableOutput(static_cast<bool>(item["disableOutput"]));
                    ch->setPhaseInvert(static_cast<bool>(item["phaseInvert"]));
                    ch->setForceMono(static_cast<bool>(item["forceMono"]));
                }
            }
        }

        // 2. Restore 16x4 Routing Matrix
        if (rootObj->hasProperty("routingMatrix"))
        {
            const auto* matrixVar = rootObj->getProperty("routingMatrix").getArray();
            if (matrixVar != nullptr)
            {
                auto& router = engine.getRoutingEngine();
                for (const auto& item : *matrixVar)
                {
                    if (!item.isObject()) continue;
                    int ch = static_cast<int>(item["channelIdx"]);
                    int out = static_cast<int>(item["outputIdx"]);
                    router.setRouteEnabled(ch, out, static_cast<bool>(item["enabled"]));
                    router.setRouteGainDb(ch, out, static_cast<float>(item["sendGainDb"]));
                    router.setRoutePan(ch, out, static_cast<float>(item["sendPan"]));
                }
            }
        }

        // 3. Restore 4 Output Buses
        if (rootObj->hasProperty("outputs"))
        {
            const auto* outputsVar = rootObj->getProperty("outputs").getArray();
            if (outputsVar != nullptr)
            {
                auto& outMgr = engine.getOutputManager();
                for (int i = 0; i < outputsVar->size() && i < outMgr.getNumOutputs(); ++i)
                {
                    const auto& item = (*outputsVar)[i];
                    if (!item.isObject()) continue;
                    auto* out = outMgr.getOutput(i);
                    if (out == nullptr) continue;

                    out->setName(item["name"].toString().toStdString());
                    out->setFaderDb(static_cast<float>(item["faderDb"]));
                    out->setMute(static_cast<bool>(item["mute"]));
                    out->setMonitor(static_cast<bool>(item["monitor"]));
                    out->setDeviceChannelOffset(static_cast<int>(item["deviceChannelOffset"]));
                }
            }
        }

        return true;
    }
} // namespace dsd
