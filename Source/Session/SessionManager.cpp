#include "Session/SessionManager.h"
#include "Audio/AudioInputSource.h"
#include "Audio/MultiDeviceManager.h"
#include "Audio/WindowAudioCapture.h"
#include "Plugin/PluginManager.h"
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
            chObj->setProperty("inputDeviceName", juce::String(ch->getInputDeviceName()));
            chObj->setProperty("inputChannelIndex", ch->getInputChannelIndex());

            // Serialize Plugins in Channel PluginRack
            juce::Array<juce::var> pluginsArray;
            const auto& rack = ch->getPluginRack();
            const int numPlugins = rack.getNumPlugins();
            for (int p = 0; p < numPlugins; ++p)
            {
                const auto* slot = rack.getSlot(p);
                if (slot != nullptr && slot->instance != nullptr)
                {
                    auto plugObj = std::make_unique<juce::DynamicObject>();
                    plugObj->setProperty("name", slot->name);
                    plugObj->setProperty("bypassed", slot->bypassed.load(std::memory_order_relaxed));

                    auto desc = slot->instance->getPluginDescription();
                    plugObj->setProperty("fileOrIdentifier", desc.fileOrIdentifier);
                    plugObj->setProperty("format", desc.pluginFormatName);

                    auto descXml = desc.createXml();
                    if (descXml != nullptr)
                        plugObj->setProperty("descXml", descXml->toString());

                    juce::MemoryBlock memBlock;
                    slot->instance->getStateInformation(memBlock);
                    plugObj->setProperty("state", memBlock.toBase64Encoding());

                    pluginsArray.add(juce::var(plugObj.release()));
                }
            }
            chObj->setProperty("plugins", pluginsArray);

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
            outObj->setProperty("outputDeviceName", juce::String(out->getOutputDeviceName()));
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

        // Query running applications once for restoring app audio capture
        auto runningApps = WindowAudioCapture::getRunningApplications();
        auto winInputs = MultiDeviceManager::getInstance().getAvailableInputDevices();

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

                    // Restore Input Device & Source
                    juce::String devName = item.hasProperty("inputDeviceName") ? item["inputDeviceName"].toString() : "None";
                    int chIdx = item.hasProperty("inputChannelIndex") ? static_cast<int>(item["inputChannelIndex"]) : -1;
                    ch->setInputDeviceName(devName.toStdString());
                    ch->setInputChannelIndex(chIdx);

                    if (devName.isEmpty() || devName == "None")
                    {
                        ch->setInputSource(std::make_unique<NullInputSource>());
                    }
                    else if (devName.containsIgnoreCase("Primary In 1") || devName.containsIgnoreCase("Mic / L"))
                    {
                        ch->setInputSource(std::make_unique<HardwareInputSource>(0, -1));
                    }
                    else if (devName.containsIgnoreCase("Primary In 2"))
                    {
                        ch->setInputSource(std::make_unique<HardwareInputSource>(1, -1));
                    }
                    else if (devName.containsIgnoreCase("Stereo") && devName.containsIgnoreCase("Primary"))
                    {
                        ch->setInputSource(std::make_unique<HardwareInputSource>(0, 1));
                    }
                    else if (devName.containsIgnoreCase("Sine"))
                    {
                        auto s = std::make_unique<SineInputSource>(1000.0f);
                        s->setEnabled(true);
                        ch->setInputSource(std::move(s));
                    }
                    else if (devName.containsIgnoreCase("Noise"))
                    {
                        ch->setInputSource(std::make_unique<NoiseInputSource>());
                    }
                    else
                    {
                        // Check if it's an application (Window Audio Capture)
                        bool appFound = false;
                        for (const auto& app : runningApps)
                        {
                            if (app.appName.equalsIgnoreCase(devName))
                            {
                                ch->setInputSource(std::make_unique<WindowAudioCapture>(app.processId, app.appName));
                                appFound = true;
                                break;
                            }
                        }

                        if (!appFound)
                        {
                            // Check if it's a Windows device
                            bool devFound = false;
                            for (const auto& wDev : winInputs)
                            {
                                if (wDev.equalsIgnoreCase(devName))
                                {
                                    auto src = MultiDeviceManager::getInstance().createInputSourceFor(wDev);
                                    ch->setInputSource(std::move(src));
                                    devFound = true;
                                    break;
                                }
                            }
                            if (!devFound)
                            {
                                ch->setInputSource(std::make_unique<NullInputSource>());
                            }
                        }
                    }

                    // Restore VST Plugins in PluginRack
                    auto& rack = ch->getPluginRack();
                    rack.clear();

                    if (item.hasProperty("plugins"))
                    {
                        const auto* plugArr = item["plugins"].getArray();
                        if (plugArr != nullptr)
                        {
                            for (const auto& pItem : *plugArr)
                            {
                                if (!pItem.isObject()) continue;
                                juce::String plugName = pItem["name"].toString();
                                bool bypassed = static_cast<bool>(pItem["bypassed"]);
                                juce::String fileOrId = pItem["fileOrIdentifier"].toString();

                                juce::PluginDescription desc;
                                bool haveDesc = false;
                                if (pItem.hasProperty("descXml"))
                                {
                                    auto xml = juce::XmlDocument::parse(pItem["descXml"].toString());
                                    if (xml != nullptr)
                                        haveDesc = desc.loadFromXml(*xml);
                                }

                                if (!haveDesc && fileOrId.isNotEmpty())
                                {
                                    desc.fileOrIdentifier = fileOrId;
                                    desc.name = plugName;
                                    desc.pluginFormatName = pItem.hasProperty("format") ? pItem["format"].toString() : "VST3";
                                    haveDesc = true;
                                }

                                juce::String loadErr;
                                std::unique_ptr<juce::AudioPluginInstance> instance;
                                if (haveDesc)
                                    instance = PluginManager::getInstance().loadPlugin(desc, loadErr);
                                if (instance == nullptr && fileOrId.isNotEmpty())
                                    instance = PluginManager::getInstance().loadPluginFromFile(juce::File(fileOrId), loadErr);

                                if (instance != nullptr)
                                {
                                    if (pItem.hasProperty("state"))
                                    {
                                        juce::MemoryBlock mem;
                                        if (mem.fromBase64Encoding(pItem["state"].toString()))
                                        {
                                            instance->setStateInformation(mem.getData(), static_cast<int>(mem.getSize()));
                                        }
                                    }
                                    rack.addPlugin(std::move(instance), plugName);
                                    rack.setBypass(rack.getNumPlugins() - 1, bypassed);
                                }
                            }
                        }
                    }
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

                    if (item.hasProperty("outputDeviceName"))
                    {
                        juce::String outDev = item["outputDeviceName"].toString();
                        out->setOutputDeviceName(outDev.toStdString());
                    }
                }
            }
        }

        return true;
    }
} // namespace dsd
