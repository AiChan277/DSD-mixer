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

        // Pre-allocate scratch buffer (up to 8 channels for stereo + sidechains / surround)
        scratchBuffer.setSize(8, std::max(128, maxBlockSize), false, true, true);

        for (auto& slot : slots)
        {
            if (slot != nullptr && slot->instance != nullptr)
            {
                try
                {
                    slot->instance->setPlayConfigDetails(2, 2, sampleRate, maxBlockSize);
                    slot->instance->prepareToPlay(sampleRate, maxBlockSize);
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
            if (slot != nullptr && slot->instance != nullptr)
            {
                try
                {
                    slot->instance->releaseResources();
                }
                catch (...) {}
            }
        }
        scratchBuffer.setSize(0, 0);
    }

    void PluginRack::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/, int numSamples)
    {
        // If numSamples not specified, use buffer sample count
        const int actualSamples = (numSamples > 0) ? numSamples : buffer.getNumSamples();
        if (actualSamples <= 0)
            return;

        // Use try_lock to avoid blocking real-time audio thread if UI is modifying the rack
        std::unique_lock<std::mutex> lock(rackMutex, std::try_to_lock);
        if (!lock.owns_lock())
            return; // Skip plugin processing safely for this single block if locked

        const int bufferChannels = buffer.getNumChannels();

        for (auto& slot : slots)
        {
            if (slot == nullptr || slot->instance == nullptr)
                continue;

            if (slot->bypassed.load(std::memory_order_relaxed))
                continue;

            auto* plugin = slot->instance.get();
            const int totalIns = plugin->getTotalNumInputChannels();
            const int totalOuts = plugin->getTotalNumOutputChannels();
            const int neededChannels = std::max({ bufferChannels, totalIns, totalOuts });

            // Isolated MIDI buffer per plugin slot to prevent MIDI corruption cascades
            juce::MidiBuffer slotMidi;

            try
            {
                if (neededChannels > bufferChannels)
                {
                    // Plugin requires extra channels (e.g. 4 channels for sidechain or surround)
                    const int allocChans = std::max(8, neededChannels);
                    const int allocSamples = std::max(currentBlockSize, actualSamples);
                    if (scratchBuffer.getNumChannels() < allocChans || scratchBuffer.getNumSamples() < allocSamples)
                    {
                        scratchBuffer.setSize(allocChans, allocSamples, false, true, true);
                    }

                    // Copy input channels to scratch buffer
                    for (int ch = 0; ch < bufferChannels; ++ch)
                        scratchBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), actualSamples);

                    // Clear extra channels (sidechain / aux)
                    for (int ch = bufferChannels; ch < neededChannels; ++ch)
                        scratchBuffer.clear(ch, 0, actualSamples);

                    juce::AudioBuffer<float> procBuf(scratchBuffer.getArrayOfWritePointers(), neededChannels, actualSamples);
                    plugin->processBlock(procBuf, slotMidi);

                    // Copy processed stereo output back
                    for (int ch = 0; ch < bufferChannels; ++ch)
                        buffer.copyFrom(ch, 0, procBuf.getReadPointer(ch), actualSamples);
                }
                else
                {
                    // Direct in-place processing with exact actualSamples proxy
                    juce::AudioBuffer<float> procBuf(buffer.getArrayOfWritePointers(), bufferChannels, actualSamples);
                    plugin->processBlock(procBuf, slotMidi);
                }

                // Sanitize audio buffer to prevent NaN / Inf propagation between chained plugins
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
                // Silently bypass crashing plugin to maintain console audio stability
                slot->bypassed.store(true, std::memory_order_relaxed);
            }
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

        // 1. Configure buses for Stereo In / Stereo Out, disabling non-main buses (sidechain/aux)
        pluginInstance->setPlayConfigDetails(2, 2, currentSampleRate, currentBlockSize);

        try
        {
            pluginInstance->prepareToPlay(currentSampleRate, currentBlockSize);
        }
        catch (...)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(rackMutex);

        auto slot = std::make_unique<PluginSlot>();
        slot->name = name.isNotEmpty() ? name : pluginInstance->getName();
        slot->latencySamples.store(std::max(0, pluginInstance->getLatencySamples()), std::memory_order_relaxed);
        slot->instance = std::move(pluginInstance);

        slots.push_back(std::move(slot));
        return true;
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

            const int w = std::max(200, editor->getWidth());
            const int h = std::max(150, editor->getHeight());
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
