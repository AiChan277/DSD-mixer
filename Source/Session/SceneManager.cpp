#include "Session/SceneManager.h"
#include "Audio/AudioEngine.h"

namespace dsd
{
    SceneManager& SceneManager::getInstance()
    {
        static SceneManager instance;
        return instance;
    }

    SceneManager::SceneManager()
    {
        for (int i = 0; i < kNumScenes; ++i)
            slots[static_cast<size_t>(i)].name = "Scene " + juce::String(i + 1);
    }

    void SceneManager::notify() const
    {
        if (onScenesChanged)
            onScenesChanged();
    }

    // =========================================================================
    // Codec
    // =========================================================================
    juce::var SceneManager::encodeDuckRules(const AutoDucker& ducker)
    {
        juce::Array<juce::var> arr;
        for (int i = 0; i < AutoDucker::kMaxRules; ++i)
        {
            const auto& r = ducker.getRule(i);
            auto* o = new juce::DynamicObject();
            o->setProperty("enabled", r.enabled.load());
            o->setProperty("trigger", r.triggerChannel.load());
            o->setProperty("targets", static_cast<juce::int64>(r.targetMask.load()));
            o->setProperty("thresholdDb", r.thresholdDb.load());
            o->setProperty("depthDb", r.depthDb.load());
            o->setProperty("attackMs", r.attackMs.load());
            o->setProperty("holdMs", r.holdMs.load());
            o->setProperty("releaseMs", r.releaseMs.load());
            arr.add(juce::var(o));
        }
        return arr;
    }

    void SceneManager::decodeDuckRules(AutoDucker& ducker, const juce::var& rules)
    {
        const auto* arr = rules.getArray();
        if (arr == nullptr)
            return;

        for (int i = 0; i < AutoDucker::kMaxRules && i < arr->size(); ++i)
        {
            const auto& item = (*arr)[i];
            if (!item.isObject())
                continue;
            auto& r = ducker.getRule(i);
            r.triggerChannel.store(static_cast<int>(item.getProperty("trigger", -1)));
            r.targetMask.store(static_cast<uint32_t>(static_cast<juce::int64>(item.getProperty("targets", 0))));
            r.thresholdDb.store(static_cast<float>(item.getProperty("thresholdDb", -35.0)));
            r.depthDb.store(static_cast<float>(item.getProperty("depthDb", -12.0)));
            r.attackMs.store(static_cast<float>(item.getProperty("attackMs", 15.0)));
            r.holdMs.store(static_cast<float>(item.getProperty("holdMs", 250.0)));
            r.releaseMs.store(static_cast<float>(item.getProperty("releaseMs", 500.0)));
            r.enabled.store(static_cast<bool>(item.getProperty("enabled", false)));
        }
    }

    juce::var SceneManager::captureMixState(const AudioEngine& engine)
    {
        auto* root = new juce::DynamicObject();

        juce::Array<juce::var> channels;
        const auto& chMgr = engine.getChannelManager();
        for (int i = 0; i < chMgr.getNumChannels(); ++i)
        {
            const auto* ch = chMgr.getChannel(i);
            if (ch == nullptr)
                continue;
            auto* o = new juce::DynamicObject();
            o->setProperty("gainDb", ch->getGainDb());
            o->setProperty("faderDb", ch->getFaderDb());
            o->setProperty("pan", ch->getPan());
            o->setProperty("mute", ch->getMute());
            o->setProperty("solo", ch->getSolo());
            o->setProperty("disableOutput", ch->getDisableOutput());
            o->setProperty("hpf", ch->getHpfEnabled());
            o->setProperty("pad", ch->getPadEnabled());
            o->setProperty("phaseInvert", ch->getPhaseInvert());
            o->setProperty("forceMono", ch->getForceMono());
            o->setProperty("directMonitor", ch->getDirectMonitor());
            o->setProperty("pfl", ch->getPfl());

            juce::Array<juce::var> bypass;
            const auto& rack = ch->getPluginRack();
            for (int p = 0; p < rack.getNumPlugins(); ++p)
            {
                if (const auto* slot = rack.getSlot(p))
                {
                    auto* b = new juce::DynamicObject();
                    b->setProperty("instanceId", juce::String(static_cast<juce::int64>(slot->instanceId)));
                    b->setProperty("bypassed", slot->bypassed.load());
                    bypass.add(juce::var(b));
                }
            }
            o->setProperty("pluginBypass", bypass);
            channels.add(juce::var(o));
        }
        root->setProperty("channels", channels);

        juce::Array<juce::var> routes;
        const auto& router = engine.getRoutingEngine();
        for (int c = 0; c < NUM_CHANNELS_LEVEL1; ++c)
        {
            for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty("ch", c);
                o->setProperty("out", out);
                o->setProperty("enabled", router.isRouteEnabled(c, out));
                o->setProperty("gainDb", router.getRouteGainDb(c, out));
                o->setProperty("preFader", router.isRoutePreFader(c, out));
                routes.add(juce::var(o));
            }
        }
        root->setProperty("routes", routes);

        juce::Array<juce::var> outputs;
        const auto& outMgr = engine.getOutputManager();
        for (int i = 0; i < outMgr.getNumOutputs(); ++i)
        {
            const auto* bus = outMgr.getOutput(i);
            if (bus == nullptr)
                continue;
            auto* o = new juce::DynamicObject();
            o->setProperty("faderDb", bus->getFaderDb());
            o->setProperty("mute", bus->getMute());
            o->setProperty("limiter", bus->getLimiter().isEnabled());
            o->setProperty("ceilingDb", bus->getLimiter().getCeilingDb());
            o->setProperty("limiterReleaseMs", bus->getLimiter().getReleaseMs());
            outputs.add(juce::var(o));
        }
        root->setProperty("outputs", outputs);

        root->setProperty("autoDuck", encodeDuckRules(engine.getAutoDucker()));
        root->setProperty("pdcEnabled", engine.isPluginDelayCompensationEnabled());
        return juce::var(root);
    }

    void SceneManager::applyMixState(AudioEngine& engine, const juce::var& state)
    {
        if (!state.isObject())
            return;

        if (const auto* channels = state["channels"].getArray())
        {
            auto& chMgr = engine.getChannelManager();
            for (int i = 0; i < channels->size() && i < chMgr.getNumChannels(); ++i)
            {
                const auto& item = (*channels)[i];
                auto* ch = chMgr.getChannel(i);
                if (ch == nullptr || !item.isObject())
                    continue;

                ch->setGainDb(static_cast<float>(item.getProperty("gainDb", ch->getGainDb())));
                ch->setFaderDb(static_cast<float>(item.getProperty("faderDb", ch->getFaderDb())));
                ch->setPan(static_cast<float>(item.getProperty("pan", ch->getPan())));
                ch->setMute(static_cast<bool>(item.getProperty("mute", ch->getMute())));
                ch->setSolo(static_cast<bool>(item.getProperty("solo", ch->getSolo())));
                ch->setDisableOutput(static_cast<bool>(item.getProperty("disableOutput", ch->getDisableOutput())));
                ch->setHpfEnabled(static_cast<bool>(item.getProperty("hpf", ch->getHpfEnabled())));
                ch->setPadEnabled(static_cast<bool>(item.getProperty("pad", ch->getPadEnabled())));
                ch->setPhaseInvert(static_cast<bool>(item.getProperty("phaseInvert", ch->getPhaseInvert())));
                ch->setForceMono(static_cast<bool>(item.getProperty("forceMono", ch->getForceMono())));
                // DM and PFL are mutually exclusive; apply the one that is on last.
                const bool dm = static_cast<bool>(item.getProperty("directMonitor", false));
                const bool pfl = static_cast<bool>(item.getProperty("pfl", false));
                ch->setDirectMonitor(dm);
                ch->setPfl(pfl);

                if (const auto* bypass = item["pluginBypass"].getArray())
                {
                    auto& rack = ch->getPluginRack();
                    for (const auto& b : *bypass)
                    {
                        const auto id = static_cast<PluginInstanceID>(b["instanceId"].toString().getLargeIntValue());
                        if (id != 0 && rack.getSlotById(id) != nullptr)
                            rack.setBypassById(id, static_cast<bool>(b["bypassed"]));
                    }
                }
            }
        }

        if (const auto* routes = state["routes"].getArray())
        {
            auto& router = engine.getRoutingEngine();
            for (const auto& r : *routes)
            {
                const int c = static_cast<int>(r["ch"]);
                const int out = static_cast<int>(r["out"]);
                router.setRouteEnabled(c, out, static_cast<bool>(r["enabled"]));
                router.setRouteGainDb(c, out, static_cast<float>(r["gainDb"]));
                router.setRoutePreFader(c, out, static_cast<bool>(r.getProperty("preFader", false)));
            }
        }

        if (const auto* outputs = state["outputs"].getArray())
        {
            auto& outMgr = engine.getOutputManager();
            for (int i = 0; i < outputs->size() && i < outMgr.getNumOutputs(); ++i)
            {
                const auto& item = (*outputs)[i];
                auto* bus = outMgr.getOutput(i);
                if (bus == nullptr || !item.isObject())
                    continue;
                bus->setFaderDb(static_cast<float>(item.getProperty("faderDb", bus->getFaderDb())));
                bus->setMute(static_cast<bool>(item.getProperty("mute", bus->getMute())));
                bus->getLimiter().setCeilingDb(static_cast<float>(item.getProperty("ceilingDb", -1.0)));
                bus->getLimiter().setReleaseMs(static_cast<float>(item.getProperty("limiterReleaseMs", 100.0)));
                bus->getLimiter().setEnabled(static_cast<bool>(item.getProperty("limiter", false)));
            }
        }

        if (state.hasProperty("autoDuck"))
            decodeDuckRules(engine.getAutoDucker(), state["autoDuck"]);
        if (state.hasProperty("pdcEnabled"))
            engine.setPluginDelayCompensationEnabled(static_cast<bool>(state["pdcEnabled"]));
    }

    // =========================================================================
    // Slots
    // =========================================================================
    void SceneManager::store(int slot, const AudioEngine& engine)
    {
        if (slot < 0 || slot >= kNumScenes)
            return;
        auto& s = slots[static_cast<size_t>(slot)];
        s.data = captureMixState(engine);
        s.used = true;
        s.stored = juce::Time::getCurrentTime();
        autoSave();
        notify();
    }

    bool SceneManager::recall(int slot, AudioEngine& engine) const
    {
        if (slot < 0 || slot >= kNumScenes || !slots[static_cast<size_t>(slot)].used)
            return false;
        applyMixState(engine, slots[static_cast<size_t>(slot)].data);
        lastRecalled = slot;
        notify();
        return true;
    }

    void SceneManager::clear(int slot)
    {
        if (slot < 0 || slot >= kNumScenes)
            return;
        auto& s = slots[static_cast<size_t>(slot)];
        s.used = false;
        s.data = juce::var();
        s.name = "Scene " + juce::String(slot + 1);
        if (lastRecalled == slot)
            lastRecalled = -1;
        autoSave();
        notify();
    }

    bool SceneManager::isUsed(int slot) const
    {
        return slot >= 0 && slot < kNumScenes && slots[static_cast<size_t>(slot)].used;
    }

    juce::String SceneManager::getName(int slot) const
    {
        return (slot >= 0 && slot < kNumScenes) ? slots[static_cast<size_t>(slot)].name : juce::String();
    }

    void SceneManager::setName(int slot, const juce::String& name)
    {
        if (slot < 0 || slot >= kNumScenes)
            return;
        slots[static_cast<size_t>(slot)].name = name.trim().isEmpty() ? "Scene " + juce::String(slot + 1) : name.trim().substring(0, 40);
        autoSave();
        notify();
    }

    juce::Time SceneManager::getStoredTime(int slot) const
    {
        return (slot >= 0 && slot < kNumScenes) ? slots[static_cast<size_t>(slot)].stored : juce::Time();
    }

    juce::var SceneManager::toVar() const
    {
        juce::Array<juce::var> arr;
        for (int i = 0; i < kNumScenes; ++i)
        {
            const auto& s = slots[static_cast<size_t>(i)];
            auto* o = new juce::DynamicObject();
            o->setProperty("slot", i);
            o->setProperty("name", s.name);
            o->setProperty("used", s.used);
            o->setProperty("stored", s.stored.toMilliseconds());
            if (s.used)
                o->setProperty("state", s.data);
            arr.add(juce::var(o));
        }
        return arr;
    }

    void SceneManager::fromVar(const juce::var& v)
    {
        const auto* arr = v.getArray();
        if (arr == nullptr)
            return;
        for (const auto& item : *arr)
        {
            const int i = static_cast<int>(item.getProperty("slot", -1));
            if (i < 0 || i >= kNumScenes)
                continue;
            auto& s = slots[static_cast<size_t>(i)];
            s.name = item.getProperty("name", "Scene " + juce::String(i + 1)).toString();
            s.used = static_cast<bool>(item.getProperty("used", false)) && item["state"].isObject();
            s.stored = juce::Time(static_cast<juce::int64>(item.getProperty("stored", 0)));
            s.data = s.used ? item["state"] : juce::var();
        }
        notify();
    }

    bool SceneManager::saveToFile(const juce::File& file) const
    {
        file.getParentDirectory().createDirectory();
        return file.replaceWithText(juce::JSON::toString(toVar(), false));
    }

    bool SceneManager::loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;
        const auto v = juce::JSON::parse(file);
        if (!v.isArray())
            return false;
        fromVar(v);
        return true;
    }

    juce::File SceneManager::getDefaultFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("DSDMixer").getChildFile("scenes.json");
    }

    void SceneManager::autoSave() const
    {
        saveToFile(getDefaultFile());
    }
} // namespace dsd
