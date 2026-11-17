#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace dsd
{
    using PluginInstanceID = uint64_t;

    struct PluginSlot
    {
        PluginInstanceID instanceId{0};
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::String name;
        std::atomic<bool> bypassed{false};
        std::atomic<int> latencySamples{0};
        juce::Component::SafePointer<juce::DocumentWindow> activeEditorWindow;

        // ---- Audio-thread-owned bypass transition state (never touched by UI) ----
        // Zero-latency plugins: wet/dry crossfade position (1 = fully wet).
        float mixWet{1.0f};
        // Latency-bearing plugins: fade out, switch path at silence, fade in.
        // Summing a delayed wet signal with an undelayed dry one would comb-filter.
        enum class Stage : uint8_t { Steady, FadeOut, FadeIn };
        Stage stage{Stage::Steady};
        bool pathIsWet{true};
        float pathGain{1.0f};
        // Dry copy used while a transition is in progress (preallocated).
        juce::AudioBuffer<float> dryBuffer;
    };

    class PluginRack
    {
    public:
        PluginRack();
        ~PluginRack();

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int numSamples = -1);

        int getNumPlugins() const;

        // Index-based access
        const PluginSlot* getSlot(int index) const;
        PluginSlot* getSlot(int index);

        // Stable ID-based access
        const PluginSlot* getSlotById(PluginInstanceID id) const;
        PluginSlot* getSlotById(PluginInstanceID id);
        int findSlotIndex(PluginInstanceID id) const;

        // Lifecycle & Mutations
        PluginInstanceID addPlugin(std::unique_ptr<juce::AudioPluginInstance> pluginInstance,
                                  const juce::String& name,
                                  PluginInstanceID specificId = 0);
        bool removePlugin(int index);
        bool removePluginById(PluginInstanceID id);
        void clear();

        // Reordering
        void movePlugin(int fromIndex, int toIndex);
        bool movePluginById(PluginInstanceID id, int targetIndex);

        // Bypass (Independent per-instance)
        void setBypass(int index, bool bypassed);
        void setBypassById(PluginInstanceID id, bool bypassed);
        bool isBypassed(int index) const;
        bool isBypassedById(PluginInstanceID id) const;

        // Latency
        int getTotalLatencySamples() const noexcept;

        // Latency of the non-bypassed plugins as last seen by the audio thread.
        // Lock-free; safe to read from any thread (used for delay compensation).
        int getProcessingLatencySamples() const noexcept { return processingLatency.load(std::memory_order_relaxed); }

        // True while any plugin is mid-way through a bypass crossfade.
        bool isBypassTransitionActive() const noexcept { return transitionActive.load(std::memory_order_relaxed); }

        // Length of the zero-latency bypass crossfade.
        static constexpr double kBypassCrossfadeSeconds = 0.010;
        // Each half (out / in) of the latency-plugin bypass switch.
        static constexpr double kBypassDipSeconds = 0.005;
        int getPluginLatencySamples(PluginInstanceID id) const noexcept;

        // Editor GUI
        void openPluginEditor(int index);
        void openPluginEditorById(PluginInstanceID id);
        void closePluginEditorById(PluginInstanceID id);

    private:
        // Immutable list the audio thread walks. A new list is published on every
        // structural change; the previous one is freed only after the audio thread
        // has left any processBlock() pass that may have loaded it.
        struct ActiveList
        {
            std::vector<PluginSlot*> slots;
        };

        void publishActiveListLocked(); // call with rackMutex held
        bool waitForAudioQuiescence() const noexcept;
        void processSlot(PluginSlot& slot, juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
        bool runPlugin(PluginSlot& slot, juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
        void prepareSlotBuffers(PluginSlot& slot) const;
        static void destroySlot(std::unique_ptr<PluginSlot> slot);

        std::vector<std::unique_ptr<PluginSlot>> slots;
        mutable std::mutex rackMutex; // Serialises UI/control mutations; never taken by the audio thread
        std::atomic<int> pluginCount{0}; // Mirrors slots.size() for lock-free UI reads

        std::atomic<const ActiveList*> activeList{nullptr};
        std::unique_ptr<ActiveList> ownedActiveList;
        // Odd while the audio thread is inside processBlock().
        std::atomic<uint64_t> audioPassCounter{0};

        std::atomic<int> processingLatency{0};
        std::atomic<bool> transitionActive{false};

        double currentSampleRate{48000.0};
        int currentBlockSize{128};
        float crossfadeStep{1.0f / 480.0f};
        float dipStep{1.0f / 240.0f};

        juce::AudioBuffer<float> scratchBuffer;

        static std::atomic<PluginInstanceID> nextUniqueId;
    };
} // namespace dsd
