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
        int getPluginLatencySamples(PluginInstanceID id) const noexcept;

        // Editor GUI
        void openPluginEditor(int index);
        void openPluginEditorById(PluginInstanceID id);
        void closePluginEditorById(PluginInstanceID id);

    private:
        std::vector<std::unique_ptr<PluginSlot>> slots;
        mutable std::mutex rackMutex; // Lock for UI mutations; audio thread uses try_lock
        std::atomic<int> pluginCount{0}; // Mirrors slots.size() for lock-free UI reads

        double currentSampleRate{48000.0};
        int currentBlockSize{128};

        juce::AudioBuffer<float> scratchBuffer;

        static std::atomic<PluginInstanceID> nextUniqueId;
    };
} // namespace dsd
