# DSD Mixer — VST3 Subsystem Architecture & Lifecycle

## 1. Overview & Core Tenet

In real-time digital mixing consoles, plugin chains require strict determinism, lock-free audio processing, predictable execution order, and resilient GUI lifecycle management.

Previous versions of DSD Mixer suffered from index fragility (plugins identified solely by transient array indices), non-functional bypass buttons, GUI editor memory leaks/crashes, and UI-only reordering that had zero effect on DSP processing.

The refactored VST3 architecture provides:
1. **Stable 64-bit Monotonic Instance Identity (`PluginInstanceID`)**: Every plugin instance maintains a permanent, unique 64-bit ID throughout its lifecycle and across session serialization.
2. **True DSP Reordering**: Reordering in the rack UI immediately modifies the underlying `slots` processing vector in lockstep.
3. **Independent Per-Plugin Bypass**: Individual atomic bypass flags ensure that bypassing slot $N$ does not alter or disable processing on slot $M$.
4. **Isolated Editor Window Management**: Single-instance `juce::DocumentWindow` lifecycle tracking with `juce::Component::SafePointer` preventing duplicate windows, and preserving DSP processing state when editor windows are closed.
5. **Real-time Latency Tracking**: Dynamic total delay calculation per rack for delay compensation telemetry.

---

## 2. Data Structures

### `PluginSlot`
```cpp
struct PluginSlot
{
    PluginInstanceID instanceId{0};
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String name;
    std::atomic<bool> bypassed{false};
    std::atomic<int> latencySamples{0};
    juce::Component::SafePointer<juce::DocumentWindow> activeEditorWindow;
};
```

- `instanceId`: Monotonically incrementing 64-bit integer, generated via `std::atomic<PluginInstanceID> nextUniqueId`.
- `instance`: Owning unique pointer to the JUCE plugin processor.
- `bypassed`: Thread-safe lock-free atomic flag read directly by `PluginRack::processBlock()`.
- `latencySamples`: Extracted from `instance->getLatencySamples()`.
- `activeEditorWindow`: Safe pointer to the floating GUI window; auto-nulls upon OS window closure without deleting the DSP instance.

---

## 3. Real-Time Audio Processing Pipeline

`PluginRack::processBlock` executes on high-priority audio threads:

```cpp
void PluginRack::processBlock(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages,
                              int numSamples)
{
    std::unique_lock<std::mutex> lock(rackMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        // Audio thread never blocks on UI mutations.
        // Passes audio through clean to prevent xruns/dropouts.
        return;
    }

    const int actualSamples = (numSamples > 0) ? numSamples : buffer.getNumSamples();

    for (auto& slot : slots)
    {
        if (slot == nullptr || slot->instance == nullptr)
            continue;

        // Atomic check: If bypassed, signal bypasses this plugin completely
        if (slot->bypassed.load(std::memory_order_relaxed))
            continue;

        slot->instance->processBlock(buffer, midiMessages);
    }
}
```

### Audio-Thread Safety Guarantee
- **Try-Lock Mutex**: Audio thread attempts `std::try_to_lock`. If UI thread is reallocating or reordering slots, the audio thread cleanly passes audio through for that single block rather than blocking or causing an audio glitch.
- **Zero Allocations**: Buffers are pre-allocated during `PluginRack::prepare()`.

---

## 4. Plugin Chain Reordering

Plugin reordering is driven by `PluginRack::movePluginById(PluginInstanceID id, int targetIndex)`:

```cpp
bool PluginRack::movePluginById(PluginInstanceID id, int targetIndex)
{
    std::lock_guard<std::mutex> lock(rackMutex);

    int currentIndex = findSlotIndex(id);
    if (currentIndex < 0 || targetIndex < 0 || targetIndex >= static_cast<int>(slots.size()))
        return false;

    if (currentIndex == targetIndex)
        return true;

    auto movingSlot = std::move(slots[currentIndex]);
    slots.erase(slots.begin() + currentIndex);
    slots.insert(slots.begin() + targetIndex, std::move(movingSlot));

    return true;
}
```

Because `processBlock()` iterates over `slots` sequentially (`for (auto& slot : slots)`), moving slot pointers in `slots` directly alters the serial DSP execution order:
$$\text{Output} = \text{Plugin}_n( \dots \text{Plugin}_1( \text{Plugin}_0( \text{Input} ) ) )$$

---

## 5. UI Rack Component & Drag-and-Drop Interaction

The rack UI (`PluginRackDialog`) provides hardware-console visual feedback:
- **`PluginRowView`**: Represents one plugin slot:
  - Drag handle `⋮⋮` with mouse drag listener initiating `juce::DragAndDropContainer::startDragging()`.
  - Slot sequence index badge (`01`, `02`, ...).
  - Plugin name label.
  - Latency indicator (`0.0 ms` or sample count).
  - `BYPASS` toggle button (illuminates amber when bypassed).
  - `GUI` button (launches plugin window or brings existing window to front).
  - `REMOVE` button (`×`) with safe teardown.
- **`PluginListContainer`**: Implements `juce::DragAndDropTarget`:
  - Visual insertion indicator bar during drag-over.
  - Dispatches `movePluginById(draggedId, targetIndex)` upon drop.
  - Repopulates UI rows while preserving active editor window references.

---

## 6. Session Persistence Compatibility

When serializing sessions into `.dsd` files:
1. Each plugin entry records its permanent `instanceId`, `name`, `bypassed` state, `fileOrIdentifier`, `format`, and base64-encoded plugin state block.
2. During deserialization, `PluginRack::addPlugin` preserves the saved `instanceId`, ensuring full backward and forward compatibility without ID drift.
