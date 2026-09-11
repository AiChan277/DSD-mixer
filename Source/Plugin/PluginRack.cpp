#include "Plugin/PluginRack.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsd
{
    PluginRack::PluginRack()
    {
    }

    PluginRack::~PluginRack()
    {
        releaseResources();
    }

    void PluginRack::prepare(double sampleRate, int maxBlockSize)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        currentSampleRate = sampleRate;
        currentBlockSize = maxBlockSize;

        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instance != nullptr)
            {
                slot->instance->prepareToPlay(sampleRate, maxBlockSize);
                slot->latencySamples.store(slot->instance->getLatencyInSamples(), std::memory_order_relaxed);
            }
        }
    }

    void PluginRack::releaseResources()
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instance != nullptr)
                slot->instance->releaseResources();
        }
    }

    void PluginRack::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        // Use try_lock to avoid blocking real-time audio thread if UI is modifying the rack
        std::unique_lock<std::mutex> lock(rackMutex, std::try_to_lock);
        if (!lock.owns_lock())
            return; // Skip plugin processing safely for this single block if locked

        for (auto& slot : slots)
        {
            if (slot == nullptr || slot->instance == nullptr)
                continue;

            if (slot->bypassed.load(std::memory_order_relaxed))
                continue;

            slot->instance->processBlock(buffer, midiMessages);
        }
    }

    int PluginRack::getNumPlugins() const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        return static_cast<int>(slots.size());
    }

    const PluginSlot* PluginRack::getSlot(int index) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
            return slots[index].get();
        return nullptr;
    }

    PluginSlot* PluginRack::getSlot(int index)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
            return slots[index].get();
        return nullptr;
    }

    bool PluginRack::addPlugin(std::unique_ptr<juce::AudioPluginInstance> pluginInstance, const juce::String& name)
    {
        if (pluginInstance == nullptr)
            return false;

        std::lock_guard<std::mutex> lock(rackMutex);
        pluginInstance->prepareToPlay(currentSampleRate, currentBlockSize);

        auto slot = std::make_unique<PluginSlot>();
        slot->name = name.isNotEmpty() ? name : pluginInstance->getName();
        slot->latencySamples.store(pluginInstance->getLatencyInSamples(), std::memory_order_relaxed);
        slot->instance = std::move(pluginInstance);

        slots.push_back(std::move(slot));
        return true;
    }

    bool PluginRack::removePlugin(int index)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
        {
            if (slots[index]->instance != nullptr)
                slots[index]->instance->releaseResources();

            slots.erase(slots.begin() + index);
            return true;
        }
        return false;
    }

    void PluginRack::movePlugin(int fromIndex, int toIndex)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        const int count = static_cast<int>(slots.size());
        if (fromIndex >= 0 && fromIndex < count && toIndex >= 0 && toIndex < count && fromIndex != toIndex)
        {
            auto item = std::move(slots[fromIndex]);
            slots.erase(slots.begin() + fromIndex);
            slots.insert(slots.begin() + toIndex, std::move(item));
        }
    }

    void PluginRack::setBypass(int index, bool bypassed)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
        {
            slots[index]->bypassed.store(bypassed, std::memory_order_relaxed);
        }
    }

    bool PluginRack::isBypassed(int index) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
        {
            return slots[index]->bypassed.load(std::memory_order_relaxed);
        }
        return false;
    }

    int PluginRack::getTotalLatencySamples() const noexcept
    {
        int total = 0;
        // Non-locking atomic read
        for (const auto& slot : slots)
        {
            if (slot != nullptr && !slot->bypassed.load(std::memory_order_relaxed))
            {
                total += slot->latencySamples.load(std::memory_order_relaxed);
            }
        }
        return total;
    }

    void PluginRack::openPluginEditor(int index)
    {
        juce::AudioPluginInstance* instance = nullptr;
        juce::String pluginName;
        {
            std::lock_guard<std::mutex> lock(rackMutex);
            if (index >= 0 && index < static_cast<int>(slots.size()) && slots[index]->instance != nullptr)
            {
                instance = slots[index]->instance.get();
                pluginName = slots[index]->name;
            }
        }

        if (instance == nullptr)
            return;

        // If plugin has custom editor GUI, open it in a DocumentWindow
        auto* editor = instance->createEditorIfNeeded();
        if (editor != nullptr)
        {
            auto* window = new juce::DocumentWindow(pluginName,
                                                    juce::Colour(0xff22252a),
                                                    juce::DocumentWindow::closeButton,
                                                    true);
            window->setContentOwned(editor, true);
            window->setResizable(editor->isResizable(), false);
            window->setUsingNativeTitleBar(true);
            window->centreWithSize(editor->getWidth(), editor->getHeight());
            window->setVisible(true);
        }
    }
} // namespace dsd
