#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace dsd
{
    struct PluginSlot
    {
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
        const PluginSlot* getSlot(int index) const;
        PluginSlot* getSlot(int index);

        bool addPlugin(std::unique_ptr<juce::AudioPluginInstance> pluginInstance, const juce::String& name);
        bool removePlugin(int index);
        void movePlugin(int fromIndex, int toIndex);
        void setBypass(int index, bool bypassed);
        bool isBypassed(int index) const;

        int getTotalLatencySamples() const noexcept;

        void openPluginEditor(int index);

    private:
        std::vector<std::unique_ptr<PluginSlot>> slots;
        mutable std::mutex rackMutex; // Used only for UI modification, try_lock on audio

        double currentSampleRate{48000.0};
        int currentBlockSize{128};

        juce::AudioBuffer<float> scratchBuffer;
    };
} // namespace dsd
