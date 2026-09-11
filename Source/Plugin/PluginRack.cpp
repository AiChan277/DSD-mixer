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
                slot->latencySamples.store(slot->instance->getLatencySamples(), std::memory_order_relaxed);
            }
        }
    }

    void PluginRack::releaseResources()
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (auto& slot : slots)
        {
            if (slot != nullptr)
            {
                if (slot->activeEditorWindow != nullptr)
                {
                    slot->activeEditorWindow->setVisible(false);
                    delete slot->activeEditorWindow.getComponent();
                }
                if (slot->instance != nullptr)
                    slot->instance->releaseResources();
            }
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
        slot->latencySamples.store(pluginInstance->getLatencySamples(), std::memory_order_relaxed);
        slot->instance = std::move(pluginInstance);

        slots.push_back(std::move(slot));
        return true;
    }

    bool PluginRack::removePlugin(int index)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
        {
            if (slots[index]->activeEditorWindow != nullptr)
            {
                slots[index]->activeEditorWindow->setVisible(false);
                delete slots[index]->activeEditorWindow.getComponent();
            }

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

    class PluginEditorWindow : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow(const juce::String& title, juce::AudioProcessorEditor* editor)
            : DocumentWindow(title, juce::Colour(0xff22252a), DocumentWindow::closeButton, true)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(editor, true);
            setResizable(editor->isResizable(), false);
            centreWithSize(editor->getWidth(), editor->getHeight());
            toFront(true);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            setVisible(false);
            delete this;
        }
    };

    void PluginRack::openPluginEditor(int index)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index < 0 || index >= static_cast<int>(slots.size()) || slots[index] == nullptr)
            return;

        auto* slot = slots[index].get();
        if (slot->instance == nullptr)
            return;

        if (slot->activeEditorWindow != nullptr)
        {
            slot->activeEditorWindow->toFront(true);
            slot->activeEditorWindow->grabKeyboardFocus();
            return;
        }

        auto* editor = slot->instance->createEditorIfNeeded();
        if (editor != nullptr)
        {
            auto* window = new PluginEditorWindow(slot->name, editor);
            slot->activeEditorWindow = window;
        }
    }
} // namespace dsd
