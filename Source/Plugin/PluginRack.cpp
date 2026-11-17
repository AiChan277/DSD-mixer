#include "Plugin/PluginRack.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsd
{
    std::atomic<PluginInstanceID> PluginRack::nextUniqueId{1};

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
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        currentBlockSize = (maxBlockSize > 0) ? maxBlockSize : 128;

        // Pre-allocate scratch buffer (up to 8 channels for stereo + sidechains / surround)
        scratchBuffer.setSize(8, std::max(128, currentBlockSize), false, true, true);

        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instance != nullptr)
            {
                try
                {
                    slot->instance->setPlayConfigDetails(2, 2, currentSampleRate, currentBlockSize);
                    slot->instance->prepareToPlay(currentSampleRate, currentBlockSize);
                    slot->latencySamples.store(std::max(0, slot->instance->getLatencySamples()), std::memory_order_relaxed);
                }
                catch (...)
                {
                    slot->bypassed.store(true, std::memory_order_relaxed);
                }
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
                {
                    try { slot->instance->releaseResources(); } catch (...) {}
                }
            }
        }
        scratchBuffer.setSize(0, 0);
    }

    void PluginRack::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/, int numSamples)
    {
        const int actualSamples = (numSamples > 0) ? numSamples : buffer.getNumSamples();
        if (actualSamples <= 0)
            return;

        // Non-blocking try_lock avoids real-time audio thread starvation during UI mutations
        std::unique_lock<std::mutex> lock(rackMutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;

        const int bufferChannels = buffer.getNumChannels();

        // Process sequentially in strict visual/chain order
        for (auto& slot : slots)
        {
            if (slot == nullptr || slot->instance == nullptr)
                continue;

            // Independent per-plugin bypass check
            if (slot->bypassed.load(std::memory_order_relaxed))
                continue;

            auto* plugin = slot->instance.get();
            const int totalIns = plugin->getTotalNumInputChannels();
            const int totalOuts = plugin->getTotalNumOutputChannels();
            const int neededChannels = std::max({ bufferChannels, totalIns, totalOuts });

            juce::MidiBuffer slotMidi;

            try
            {
                if (neededChannels > bufferChannels)
                {
                    const int allocChans = std::max(8, neededChannels);
                    const int allocSamples = std::max(currentBlockSize, actualSamples);
                    if (scratchBuffer.getNumChannels() < allocChans || scratchBuffer.getNumSamples() < allocSamples)
                    {
                        scratchBuffer.setSize(allocChans, allocSamples, false, true, true);
                    }

                    for (int ch = 0; ch < bufferChannels; ++ch)
                        scratchBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), actualSamples);

                    for (int ch = bufferChannels; ch < neededChannels; ++ch)
                        scratchBuffer.clear(ch, 0, actualSamples);

                    juce::AudioBuffer<float> procBuf(scratchBuffer.getArrayOfWritePointers(), neededChannels, actualSamples);
                    plugin->processBlock(procBuf, slotMidi);

                    for (int ch = 0; ch < bufferChannels; ++ch)
                        buffer.copyFrom(ch, 0, procBuf.getReadPointer(ch), actualSamples);
                }
                else
                {
                    juce::AudioBuffer<float> procBuf(buffer.getArrayOfWritePointers(), bufferChannels, actualSamples);
                    plugin->processBlock(procBuf, slotMidi);
                }

                // Sanitize buffer against NaN / Inf propagation
                for (int ch = 0; ch < bufferChannels; ++ch)
                {
                    float* channelData = buffer.getWritePointer(ch);
                    for (int s = 0; s < actualSamples; ++s)
                    {
                        if (std::isnan(channelData[s]) || std::isinf(channelData[s]))
                            channelData[s] = 0.0f;
                    }
                }
            }
            catch (...)
            {
                // Silently bypass ONLY this single failing plugin to preserve entire mixer stability
                slot->bypassed.store(true, std::memory_order_relaxed);
            }
        }
    }

    int PluginRack::getNumPlugins() const
    {
        // Lock-free: the UI polls this for every strip on its refresh timer.
        // Taking rackMutex here made the audio thread's try_lock fail and
        // skip the whole plugin chain for that block (audible dry pop).
        return pluginCount.load(std::memory_order_acquire);
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

    const PluginSlot* PluginRack::getSlotById(PluginInstanceID id) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (const auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
                return slot.get();
        }
        return nullptr;
    }

    PluginSlot* PluginRack::getSlotById(PluginInstanceID id)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
                return slot.get();
        }
        return nullptr;
    }

    int PluginRack::findSlotIndex(PluginInstanceID id) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            if (slots[i] != nullptr && slots[i]->instanceId == id)
                return i;
        }
        return -1;
    }

    PluginInstanceID PluginRack::addPlugin(std::unique_ptr<juce::AudioPluginInstance> pluginInstance,
                                          const juce::String& name,
                                          PluginInstanceID specificId)
    {
        if (pluginInstance == nullptr)
            return 0;

        pluginInstance->setPlayConfigDetails(2, 2, currentSampleRate, currentBlockSize);

        try
        {
            pluginInstance->prepareToPlay(currentSampleRate, currentBlockSize);
        }
        catch (...)
        {
            return 0;
        }

        std::lock_guard<std::mutex> lock(rackMutex);

        auto slot = std::make_unique<PluginSlot>();
        slot->instanceId = (specificId > 0) ? specificId : nextUniqueId.fetch_add(1);
        if (specificId >= nextUniqueId.load())
            nextUniqueId.store(specificId + 1);

        slot->name = name.isNotEmpty() ? name : pluginInstance->getName();
        slot->latencySamples.store(std::max(0, pluginInstance->getLatencySamples()), std::memory_order_relaxed);
        slot->instance = std::move(pluginInstance);

        const PluginInstanceID assignedId = slot->instanceId;
        slots.push_back(std::move(slot));
        pluginCount.store(static_cast<int>(slots.size()), std::memory_order_release);
        return assignedId;
    }

    bool PluginRack::removePlugin(int index)
    {
        std::unique_ptr<PluginSlot> slotToDelete;
        {
            std::lock_guard<std::mutex> lock(rackMutex);
            if (index >= 0 && index < static_cast<int>(slots.size()))
            {
                slotToDelete = std::move(slots[index]);
                slots.erase(slots.begin() + index);
                pluginCount.store(static_cast<int>(slots.size()), std::memory_order_release);
            }
        }

        if (slotToDelete != nullptr)
        {
            if (slotToDelete->activeEditorWindow != nullptr)
            {
                slotToDelete->activeEditorWindow->setVisible(false);
                delete slotToDelete->activeEditorWindow.getComponent();
            }

            if (slotToDelete->instance != nullptr)
            {
                try { slotToDelete->instance->releaseResources(); } catch (...) {}
            }
            return true;
        }
        return false;
    }

    bool PluginRack::removePluginById(PluginInstanceID id)
    {
        std::unique_ptr<PluginSlot> slotToDelete;
        {
            std::lock_guard<std::mutex> lock(rackMutex);
            for (auto it = slots.begin(); it != slots.end(); ++it)
            {
                if (*it != nullptr && (*it)->instanceId == id)
                {
                    slotToDelete = std::move(*it);
                    slots.erase(it);
                    pluginCount.store(static_cast<int>(slots.size()), std::memory_order_release);
                    break;
                }
            }
        }

        if (slotToDelete != nullptr)
        {
            if (slotToDelete->activeEditorWindow != nullptr)
            {
                slotToDelete->activeEditorWindow->setVisible(false);
                delete slotToDelete->activeEditorWindow.getComponent();
            }

            if (slotToDelete->instance != nullptr)
            {
                try { slotToDelete->instance->releaseResources(); } catch (...) {}
            }
            return true;
        }
        return false;
    }

    void PluginRack::clear()
    {
        std::vector<std::unique_ptr<PluginSlot>> toDelete;
        {
            std::lock_guard<std::mutex> lock(rackMutex);
            toDelete = std::move(slots);
            slots.clear();
            pluginCount.store(0, std::memory_order_release);
        }

        for (auto& slot : toDelete)
        {
            if (slot != nullptr)
            {
                if (slot->activeEditorWindow != nullptr)
                {
                    slot->activeEditorWindow->setVisible(false);
                    delete slot->activeEditorWindow.getComponent();
                }
                if (slot->instance != nullptr)
                {
                    try { slot->instance->releaseResources(); } catch (...) {}
                }
            }
        }
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

    bool PluginRack::movePluginById(PluginInstanceID id, int targetIndex)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        const int count = static_cast<int>(slots.size());
        if (targetIndex < 0 || targetIndex >= count)
            return false;

        int fromIndex = -1;
        for (int i = 0; i < count; ++i)
        {
            if (slots[i] != nullptr && slots[i]->instanceId == id)
            {
                fromIndex = i;
                break;
            }
        }

        if (fromIndex >= 0 && fromIndex != targetIndex)
        {
            auto item = std::move(slots[fromIndex]);
            slots.erase(slots.begin() + fromIndex);
            slots.insert(slots.begin() + targetIndex, std::move(item));
            return true;
        }
        return false;
    }

    void PluginRack::setBypass(int index, bool bypassed)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()) && slots[index] != nullptr)
        {
            slots[index]->bypassed.store(bypassed, std::memory_order_release);
            if (slots[index]->instance != nullptr)
            {
                if (auto* bp = slots[index]->instance->getBypassParameter())
                    bp->setValueNotifyingHost(bypassed ? 1.0f : 0.0f);
            }
        }
    }

    void PluginRack::setBypassById(PluginInstanceID id, bool bypassed)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
            {
                slot->bypassed.store(bypassed, std::memory_order_release);
                if (slot->instance != nullptr)
                {
                    if (auto* bp = slot->instance->getBypassParameter())
                        bp->setValueNotifyingHost(bypassed ? 1.0f : 0.0f);
                }
                break;
            }
        }
    }

    bool PluginRack::isBypassed(int index) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()) && slots[index] != nullptr)
        {
            return slots[index]->bypassed.load(std::memory_order_relaxed);
        }
        return false;
    }

    bool PluginRack::isBypassedById(PluginInstanceID id) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (const auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
                return slot->bypassed.load(std::memory_order_relaxed);
        }
        return false;
    }

    int PluginRack::getTotalLatencySamples() const noexcept
    {
        int total = 0;
        for (const auto& slot : slots)
        {
            if (slot != nullptr && !slot->bypassed.load(std::memory_order_relaxed))
            {
                total += slot->latencySamples.load(std::memory_order_relaxed);
            }
        }
        return total;
    }

    int PluginRack::getPluginLatencySamples(PluginInstanceID id) const noexcept
    {
        for (const auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
                return slot->latencySamples.load(std::memory_order_relaxed);
        }
        return 0;
    }

    class PluginEditorWindow : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow(const juce::String& title, juce::AudioProcessorEditor* editor)
            : DocumentWindow(title, juce::Colour(0xff181a1f), DocumentWindow::closeButton, true)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(editor, true);
            setResizable(editor->isResizable(), false);

            const int w = std::max(240, editor->getWidth());
            const int h = std::max(180, editor->getHeight());
            centreWithSize(w, h);
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

        openPluginEditorById(slots[index]->instanceId);
    }

    void PluginRack::openPluginEditorById(PluginInstanceID id)
    {
        PluginSlot* targetSlot = nullptr;
        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
            {
                targetSlot = slot.get();
                break;
            }
        }

        if (targetSlot == nullptr || targetSlot->instance == nullptr)
            return;

        if (targetSlot->activeEditorWindow != nullptr)
        {
            targetSlot->activeEditorWindow->toFront(true);
            targetSlot->activeEditorWindow->grabKeyboardFocus();
            return;
        }

        auto* editor = targetSlot->instance->createEditorIfNeeded();
        if (editor != nullptr)
        {
            auto* window = new PluginEditorWindow(targetSlot->name, editor);
            targetSlot->activeEditorWindow = window;
        }
    }

    void PluginRack::closePluginEditorById(PluginInstanceID id)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instanceId == id)
            {
                if (slot->activeEditorWindow != nullptr)
                {
                    slot->activeEditorWindow->setVisible(false);
                    delete slot->activeEditorWindow.getComponent();
                }
                break;
            }
        }
    }
} // namespace dsd
