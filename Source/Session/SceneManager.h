#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <functional>

namespace dsd
{
    class AudioEngine;
    class AutoDucker;

    // Scene memory: 8 slots holding a snapshot of the mix (levels, mutes,
    // channel switches, plugin bypass, routing incl. PRE/POST, output levels
    // and protection, ducking rules, PDC). Sources, devices and the set of
    // loaded plugins are deliberately NOT part of a scene, so recalling one
    // never re-patches hardware or reloads plugins mid-show.
    //
    // Scenes are saved in .dsd sessions and auto-saved to the user's app-data
    // folder so they survive a restart. All calls are message-thread only.
    class SceneManager
    {
    public:
        static constexpr int kNumScenes = 8;

        static SceneManager& getInstance();

        void store(int slot, const AudioEngine& engine);
        bool recall(int slot, AudioEngine& engine) const;
        void clear(int slot);

        bool isUsed(int slot) const;
        juce::String getName(int slot) const;
        void setName(int slot, const juce::String& name);
        juce::Time getStoredTime(int slot) const;
        int getLastRecalled() const noexcept { return lastRecalled; }

        juce::var toVar() const;
        void fromVar(const juce::var& v);

        bool saveToFile(const juce::File& file) const;
        bool loadFromFile(const juce::File& file);
        static juce::File getDefaultFile();
        void autoSave() const;

        std::function<void()> onScenesChanged;

        // Mix-state codec shared by scenes and sessions.
        static juce::var captureMixState(const AudioEngine& engine);
        static void applyMixState(AudioEngine& engine, const juce::var& state);
        static juce::var encodeDuckRules(const AutoDucker& ducker);
        static void decodeDuckRules(AutoDucker& ducker, const juce::var& rules);

    private:
        SceneManager();

        struct Slot
        {
            bool used{false};
            juce::String name;
            juce::Time stored;
            juce::var data;
        };

        std::array<Slot, kNumScenes> slots;
        mutable int lastRecalled{-1};
        void notify() const;
    };
} // namespace dsd
