#include "Plugin/PluginRack.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <chrono>

namespace dsd
{
    std::atomic<PluginInstanceID> PluginRack::nextUniqueId{1};

    PluginRack::PluginRack()
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        publishActiveListLocked();
    }

    PluginRack::~PluginRack()
    {
        releaseResources();
        activeList.store(nullptr, std::memory_order_seq_cst);
    }

    // =========================================================================
    // Active-list publication (control thread)
    // =========================================================================
    bool PluginRack::waitForAudioQuiescence() const noexcept
    {
        // Any audio pass that could have loaded the previous list started before
        // our publish (seq_cst ordering). Wait until that pass has finished.
        const uint64_t observed = audioPassCounter.load(std::memory_order_seq_cst);
        if ((observed & 1u) == 0)
            return true;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        int spins = 0;
        while (audioPassCounter.load(std::memory_order_seq_cst) == observed)
        {
            if (++spins < 64)
                std::this_thread::yield();
            else
                std::this_thread::sleep_for(std::chrono::microseconds(200));

            if (std::chrono::steady_clock::now() > deadline)
                return false; // audio thread stuck inside a plugin: do not free
        }
        return true;
    }

    void PluginRack::publishActiveListLocked()
    {
        auto next = std::make_unique<ActiveList>();
        next->slots.reserve(slots.size());
        for (auto& slot : slots)
            if (slot != nullptr)
                next->slots.push_back(slot.get());

        activeList.store(next.get(), std::memory_order_seq_cst);
        pluginCount.store(static_cast<int>(slots.size()), std::memory_order_release);

        auto previous = std::move(ownedActiveList);
        ownedActiveList = std::move(next);

        if (previous != nullptr && !waitForAudioQuiescence())
            (void)previous.release(); // leak rather than free memory still in use
    }

    void PluginRack::destroySlot(std::unique_ptr<PluginSlot> slot)
    {
        if (slot == nullptr)
            return;

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

    void PluginRack::prepareSlotBuffers(PluginSlot& slot) const
    {
        int channels = 2;
        if (slot.instance != nullptr)
            channels = std::max({ 2, slot.instance->getTotalNumInputChannels(), slot.instance->getTotalNumOutputChannels() });
        slot.dryBuffer.setSize(channels, std::max(128, currentBlockSize), false, true, false);
        slot.dryBuffer.clear();
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    void PluginRack::prepare(double sampleRate, int maxBlockSize)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        currentBlockSize = (maxBlockSize > 0) ? maxBlockSize : 128;

        crossfadeStep = static_cast<float>(1.0 / std::max(1.0, currentSampleRate * kBypassCrossfadeSeconds));
        dipStep = static_cast<float>(1.0 / std::max(1.0, currentSampleRate * kBypassDipSeconds));

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
                prepareSlotBuffers(*slot);
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

    // =========================================================================
    // Audio thread
    // =========================================================================
    bool PluginRack::runPlugin(PluginSlot& slot, juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        auto* plugin = slot.instance.get();
        if (plugin == nullptr)
            return false;

        const int bufferChannels = buffer.getNumChannels();
        const int neededChannels = std::max({ bufferChannels, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels() });
        juce::MidiBuffer slotMidi;

        try
        {
            if (neededChannels > bufferChannels)
            {
                if (scratchBuffer.getNumChannels() < neededChannels || scratchBuffer.getNumSamples() < numSamples)
                    scratchBuffer.setSize(std::max(8, neededChannels), std::max(currentBlockSize, numSamples), false, true, true);

                for (int ch = 0; ch < bufferChannels; ++ch)
                    scratchBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
                for (int ch = bufferChannels; ch < neededChannels; ++ch)
                    scratchBuffer.clear(ch, 0, numSamples);

                juce::AudioBuffer<float> procBuf(scratchBuffer.getArrayOfWritePointers(), neededChannels, numSamples);
                plugin->processBlock(procBuf, slotMidi);

                for (int ch = 0; ch < bufferChannels; ++ch)
                    buffer.copyFrom(ch, 0, procBuf.getReadPointer(ch), numSamples);
            }
            else
            {
                juce::AudioBuffer<float> procBuf(buffer.getArrayOfWritePointers(), bufferChannels, numSamples);
                plugin->processBlock(procBuf, slotMidi);
            }

            // Sanitize buffer against NaN / Inf propagation
            for (int ch = 0; ch < bufferChannels; ++ch)
            {
                float* channelData = buffer.getWritePointer(ch);
                for (int s = 0; s < numSamples; ++s)
                {
                    if (std::isnan(channelData[s]) || std::isinf(channelData[s]))
                        channelData[s] = 0.0f;
                }
            }
            return true;
        }
        catch (...)
        {
            // Silently bypass ONLY this single failing plugin to preserve entire mixer stability
            slot.bypassed.store(true, std::memory_order_relaxed);
            slot.mixWet = 0.0f;
            slot.pathIsWet = false;
            slot.pathGain = 1.0f;
            slot.stage = PluginSlot::Stage::Steady;
            return false;
        }
    }

    void PluginRack::processSlot(PluginSlot& slot, juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        if (slot.instance == nullptr)
            return;

        const bool wantWet = !slot.bypassed.load(std::memory_order_acquire);
        const int latency = slot.latencySamples.load(std::memory_order_relaxed);
        const int bufferChannels = buffer.getNumChannels();
        const bool canCrossfade = slot.dryBuffer.getNumSamples() >= numSamples
                               && slot.dryBuffer.getNumChannels() >= bufferChannels;

        if (latency <= 0)
        {
            // ---- Zero-latency plugin: equal-length wet/dry crossfade ----
            const float target = wantWet ? 1.0f : 0.0f;
            if (slot.mixWet == target || !canCrossfade)
            {
                slot.mixWet = target;
                if (wantWet)
                    runPlugin(slot, buffer, numSamples);
                return;
            }

            for (int ch = 0; ch < bufferChannels; ++ch)
                slot.dryBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);

            if (!runPlugin(slot, buffer, numSamples))
            {
                for (int ch = 0; ch < bufferChannels; ++ch)
                    buffer.copyFrom(ch, 0, slot.dryBuffer.getReadPointer(ch), numSamples);
                return;
            }

            const float start = slot.mixWet;
            const float step = (target > start) ? crossfadeStep : -crossfadeStep;
            float w = start;
            for (int ch = 0; ch < bufferChannels; ++ch)
            {
                const float* dry = slot.dryBuffer.getReadPointer(ch);
                float* out = buffer.getWritePointer(ch);
                w = start;
                for (int i = 0; i < numSamples; ++i)
                {
                    w = std::clamp(w + step, 0.0f, 1.0f);
                    out[i] = dry[i] + (out[i] - dry[i]) * w;
                }
            }
            slot.mixWet = w;
            transitionActive.store(true, std::memory_order_relaxed);
            return;
        }

        // ---- Latency-bearing plugin: fade out, switch at silence, fade in ----
        if (slot.stage == PluginSlot::Stage::Steady && slot.pathIsWet == wantWet)
        {
            if (wantWet)
                runPlugin(slot, buffer, numSamples);
            return;
        }

        if (!canCrossfade)
        {
            slot.stage = PluginSlot::Stage::Steady;
            slot.pathIsWet = wantWet;
            slot.pathGain = 1.0f;
            if (wantWet)
                runPlugin(slot, buffer, numSamples);
            return;
        }

        if (slot.stage == PluginSlot::Stage::Steady)
            slot.stage = PluginSlot::Stage::FadeOut;
        else if (slot.stage == PluginSlot::Stage::FadeIn && slot.pathIsWet != wantWet)
            slot.stage = PluginSlot::Stage::FadeOut;

        // Keep a dry copy, then run the plugin (it must keep running through the
        // transition so its internal delay line stays continuous).
        for (int ch = 0; ch < bufferChannels; ++ch)
            slot.dryBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
        const bool wetValid = runPlugin(slot, buffer, numSamples);

        auto stage = slot.stage;
        bool pathIsWet = slot.pathIsWet;
        float gain = slot.pathGain;

        // Same envelope for every channel: compute per channel from the saved start state.
        const auto startStage = stage;
        const bool startPath = pathIsWet;
        const float startGain = gain;

        for (int ch = 0; ch < bufferChannels; ++ch)
        {
            stage = startStage;
            pathIsWet = startPath;
            gain = startGain;
            const float* dry = slot.dryBuffer.getReadPointer(ch);
            float* out = buffer.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                if (stage == PluginSlot::Stage::FadeOut)
                {
                    gain -= dipStep;
                    if (gain <= 0.0f)
                    {
                        gain = 0.0f;
                        pathIsWet = wantWet && wetValid;
                        stage = PluginSlot::Stage::FadeIn;
                    }
                }
                else if (stage == PluginSlot::Stage::FadeIn)
                {
                    gain += dipStep;
                    if (gain >= 1.0f)
                    {
                        gain = 1.0f;
                        stage = PluginSlot::Stage::Steady;
                    }
                }

                const float src = (pathIsWet && wetValid) ? out[i] : dry[i];
                out[i] = src * gain;
            }
        }

        slot.stage = stage;
        slot.pathIsWet = pathIsWet;
        slot.pathGain = gain;
        transitionActive.store(true, std::memory_order_relaxed);
    }

    void PluginRack::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/, int numSamples)
    {
        const int actualSamples = (numSamples > 0) ? numSamples : buffer.getNumSamples();
        if (actualSamples <= 0)
            return;

        // Enter (counter becomes odd). seq_cst pairs with the publisher's store.
        audioPassCounter.fetch_add(1, std::memory_order_seq_cst);
        const ActiveList* list = activeList.load(std::memory_order_seq_cst);

        transitionActive.store(false, std::memory_order_relaxed);
        int latencySum = 0;

        if (list != nullptr)
        {
            // Process sequentially in strict visual/chain order
            for (PluginSlot* slot : list->slots)
            {
                if (slot == nullptr)
                    continue;

                processSlot(*slot, buffer, actualSamples);

                if (!slot->bypassed.load(std::memory_order_relaxed))
                    latencySum += slot->latencySamples.load(std::memory_order_relaxed);
            }
        }

        processingLatency.store(latencySum, std::memory_order_relaxed);

        // Leave (counter becomes even): the list may now be reclaimed.
        audioPassCounter.fetch_add(1, std::memory_order_seq_cst);
    }

    // =========================================================================
    // Control-thread accessors and mutations
    // =========================================================================
    int PluginRack::getNumPlugins() const
    {
        // Lock-free: the UI polls this for every strip on its refresh timer.
        return pluginCount.load(std::memory_order_acquire);
    }

    const PluginSlot* PluginRack::getSlot(int index) const
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
            return slots[static_cast<size_t>(index)].get();
        return nullptr;
    }

    PluginSlot* PluginRack::getSlot(int index)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        if (index >= 0 && index < static_cast<int>(slots.size()))
            return slots[static_cast<size_t>(index)].get();
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
            if (slots[static_cast<size_t>(i)] != nullptr && slots[static_cast<size_t>(i)]->instanceId == id)
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
        prepareSlotBuffers(*slot);

        const PluginInstanceID assignedId = slot->instanceId;
        slots.push_back(std::move(slot));
        publishActiveListLocked();
        return assignedId;
    }

    bool PluginRack::removePlugin(int index)
    {
        std::unique_ptr<PluginSlot> slotToDelete;
        {
            std::lock_guard<std::mutex> lock(rackMutex);
            if (index >= 0 && index < static_cast<int>(slots.size()))
            {
                slotToDelete = std::move(slots[static_cast<size_t>(index)]);
                slots.erase(slots.begin() + index);
                publishActiveListLocked(); // returns once the audio thread cannot see it
            }
        }

        if (slotToDelete != nullptr)
        {
            destroySlot(std::move(slotToDelete));
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
                    publishActiveListLocked();
                    break;
                }
            }
        }

        if (slotToDelete != nullptr)
        {
            destroySlot(std::move(slotToDelete));
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
            publishActiveListLocked();
        }

        for (auto& slot : toDelete)
            destroySlot(std::move(slot));
    }

    void PluginRack::movePlugin(int fromIndex, int toIndex)
    {
        std::lock_guard<std::mutex> lock(rackMutex);
        const int count = static_cast<int>(slots.size());
        if (fromIndex >= 0 && fromIndex < count && toIndex >= 0 && toIndex < count && fromIndex != toIndex)
        {
            auto item = std::move(slots[static_cast<size_t>(fromIndex)]);
            slots.erase(slots.begin() + fromIndex);
            slots.insert(slots.begin() + toIndex, std::move(item));
            publishActiveListLocked();
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
            if (slots[static_cast<size_t>(i)] != nullptr && slots[static_cast<size_t>(i)]->instanceId == id)
            {
                fromIndex = i;
                break;
            }
        }

        if (fromIndex >= 0 && fromIndex != targetIndex)
        {
            auto item = std::move(slots[static_cast<size_t>(fromIndex)]);
            slots.erase(slots.begin() + fromIndex);
            slots.insert(slots.begin() + targetIndex, std::move(item));
            publishActiveListLocked();
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
