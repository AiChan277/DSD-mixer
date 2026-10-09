# DSD Mixer — Level 1 Architecture Audit & Signal Flow Specification

**Document Version:** 1.0.0  
**Status:** Audit Complete (Phase 0)  
**Author:** Senior Audio Software Engineer + Desktop UI/UX Engineer  

---

## 1. Executive Summary

This document captures the **current implementation** of DSD Mixer based on source code inspection of all engine, channel, DSP, plugin, routing, output, and UI components. It serves as the baseline and source of truth before performing the UI/UX refactoring and audio functionality repair.

---

## 2. Audio Engine Architecture

The audio engine (`AudioEngine`) implements JUCE's `AudioIODeviceCallback` and executes on Windows Audio (WASAPI Exclusive/Shared, DirectSound, or ASIO) at 48000 Hz, 128 samples buffer size.

### Engine Components:
- **AudioEngine**: Master coordinator of the real-time audio callback `audioDeviceIOCallbackWithContext()`.
- **ChannelManager**: Manages 16 input channels (`AudioChannel` instances 0..15).
- **DSPScheduler**: Multi-worker thread pool (8 threads, 2–8 dynamically scaled) executing parallel channel DSP via `processChannelsParallel()`.
- **RoutingEngine**: 16x4 routing cross-point matrix with independent send gain (-60 dB to +12 dB), send pan, and enable toggles.
- **OutputManager**: Manages 4 output buses (`OutputBus` instances 0..3):
  - OUT 1: Studio Monitor (routes directly to primary hardware DAC channels 1-2).
  - OUT 2: Headphones.
  - OUT 3: OBS / Stream.
  - OUT 4: Recording.
- **EnginePerformanceStats**: Lock-free atomic telemetry tracking DSP execution time, deadline margin, headroom, and overrun/xrun detection.

---

## 3. Current Audio Signal Flow Diagram

Based on the actual implementation in `AudioEngine::audioDeviceIOCallbackWithContext`, `AudioChannel::processBlock`, `RoutingEngine::routeChannelsToOutputs`, and `OutputBus::processBlock`:

```
========================================================================================
                                 INPUT CHANNEL SIGNAL FLOW
========================================================================================

    [ Physical Audio In / WASAPI / Process Loopback / Test Tone ]
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 1. Input Acquisition (AudioInputSource::readBlock)     │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ├─────────────────────────────► [ Meter Stage A: Raw Input ]
                              │                               (Pre-Gain, Pre-Phase, Pre-Mono)
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 2. Polarity / Phase Invert (applyGain(-1.0f))          │
    │    [Controlled by atomic<bool> phaseInvert]            │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 3. Force Mono Summing ((L + R) * 0.5f)                 │
    │    [Controlled by atomic<bool> forceMono]              │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 4. Pre-Fader Channel Gain Stage (GainProcessor)        │
    │    [Range: -60 dB to +24 dB, atomic<float> gainDb]     │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ├─────────────────────────────► [ Meter Stage B: Post-Gain ]
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 5. VST3 Plugin Rack (PluginRack::processBlock)         │
    │    [Sequential chain of AudioPluginInstances]          │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ├─────────────────────────────► [ Meter Stage C: Post-VST ]
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 6. Constant-Power Panning (PanProcessor)               │
    │    [Range: -1.0 (Left) to +1.0 (Right)]                │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 7. Post-Fader Gain & Mute (GainProcessor)              │
    │    [Range: -60 dB to +12 dB, atomic<float> faderDb]    │
    │    [If atomic<bool> mute == true, target = -60.0 dB]   │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ├─────────────────────────────► [ Meter Stage D: Post-Fader ]
                              │                               (Currently read by ChannelStrip!)
                              ▼
    [ Channel Output Buffer: AudioChannel::channelBuffer ]

========================================================================================
                             ROUTING MATRIX & OUTPUT BAY FLOW
========================================================================================

    [ Channel Output Buffer (Post-Fader) ]
                  │
                  ▼
    ┌────────────────────────────────────────────────────────┐
    │ 8. ON/OFF Gate Check (RoutingEngine)                   │
    │    if (channel->getDisableOutput()) ---> [ SKIP ]      │
    │    if (hasAnySolo && !channel->getSolo()) -> [ SKIP ]  │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 9. 16x4 Routing Matrix Crosspoint Accumulation         │
    │    For each OutputBus (0..3):                          │
    │      if (matrix[ch][out].enabled):                     │
    │        outBuffer += chBuffer * sendGain                │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 10. Output Bus Processing (OutputBus::processBlock)    │
    │     - Post-fader Gain & Mute (-60 dB to +12 dB)        │
    │     - Output MeterProcessor (L/R Peak & Clip)          │
    │     - Optional dedicated WindowsDeviceOutputSink write │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────┐
    │ 11. DAC Output Write & Limiter (OutputManager)         │
    │     - Sum active buses to physical DAC channels        │
    │     - Transparent tanh saturation ceiling at 0.988     │
    └─────────────────────────┬──────────────────────────────┘
                              │
                              ▼
    [ Physical Audio Output / Headphones / Stream / Recording ]
```

---

## 4. Current Control Semantics (Source of Truth)

| Control | C++ Parameter / Member | DSP Location | Existing Behavior in Engine | Existing UI Binding in `ChannelStrip` | Audit Findings & Issues |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ON / OFF** | `atomic<bool> disableOutput` | `RoutingEngine.cpp` L115 | If `true`, the channel is omitted from all routing sends into output buses. Does not cut pre-fader processing or meters. | Bound to both `onBtn` and `muteBtn` simultaneously! `onBtn` set both `disableOutput` and `mute`. | **Bug:** ON/OFF was conflated with MUTE. Should be a dedicated channel ON switch controlling `disableOutput`. |
| **MUTE** | `atomic<bool> mute` | `AudioChannel.cpp` L154 | If `true`, sets `faderProcessor` target gain to -60 dB (smooth ramped silence). | Conflated with `muteBtn` ("OFF") which also toggled `disableOutput`. | **Bug:** Mute was not independent of ON/OFF. Must be decoupled. |
| **GAIN** | `atomic<float> gainDb` | `AudioChannel.cpp` L128 | Pre-fader input gain applied by `gainProcessor` (-60 dB to +24 dB). Default: 0 dB. | **NONE!** Not exposed as an interactive control on `ChannelStrip`. The readout showed `faderDb` instead! | **Major Missing Feature:** No UI control for channel GAIN; readout was misleadingly bound to fader. |
| **FADER** | `atomic<float> faderDb` | `AudioChannel.cpp` L155 | Post-fader level applied by `faderProcessor` (-60 dB to +12 dB). Default: 0 dB. | Bound to `fader` (FaderComponent). | Functional, but visual design and integration need modernization. |
| **DM** | `atomic<bool> directMonitor` | `AudioChannel.h` L96 | Stored on channel, serialized in `.DSD`. | `directMonitorBtn{"DM"}` toggles state with tooltip `"Direct Monitor (Pre-fader listen)"`. | **Missing Audio Link:** The engine does not tap pre-fader audio to the monitor bus when DM is active. |
| **PH** | `atomic<bool> phaseInvert` | `AudioChannel.cpp` L108 | Inverts polarity of L/R audio channels (`applyGain(-1.0f)`). | `phaseInvertBtn{"PH"}` toggles state. | Functional DSP. UI needs clearer styling and tooltip. |
| **MONO** | `atomic<bool> forceMono` | `AudioChannel.cpp` L115 | Sums L and R: `(l + r) * 0.5f` into both channels. | `monoBtn{"MONO"}` toggles state. | Functional DSP. |
| **MON** | `atomic<bool> monitor` | `OutputBus.h` L62 | Stored on OutputBus, default true. | `monitorBtn{"MON"}` on `OutputBayStrip`. | Functional flag, but monitor routing tap link needs completion. |
| **VST** | `PluginRack pluginRack` | `AudioChannel.cpp` L139 | Per-channel plugin rack processing stereo buffer between gain and pan. | `vstRackBtn{"VST"}` opens `PluginRackWindow`. | Bypassing bugs, no reordering, debug-style UI, index-only identification. |

---

## 5. Current VST3 Subsystem Audit

### Data Structure (`PluginRack.h` / `PluginRack.cpp`):
```cpp
struct PluginSlot {
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String name;
    std::atomic<bool> bypassed{false};
    std::atomic<int> latencySamples{0};
    juce::Component::SafePointer<juce::DocumentWindow> activeEditorWindow;
};
```

### Critical VST3 Deficiencies Identified:
1. **No Stable Plugin Identity**:
   - `PluginSlot` has no instance ID. Slots are addressed strictly by `int index` (0..N-1).
   - In `PluginRackDialog.cpp`, button callbacks capture `i` by value:
     ```cpp
     row.bypassBtn->onClick = [this, i, btn = row.bypassBtn.get()]() {
         channelRef.getPluginRack().setBypass(i, btn->getToggleState());
     };
     ```
   - When any plugin is removed, reordered, or added, indices shift, causing clicks on bypass, GUI, or remove to affect the WRONG plugin instance!
2. **Bypass State & Latency Recalculation**:
   - Toggling bypass in `PluginRackDialog` does not update `totalLatencyLabel`.
   - VST3 host bypass notification (`instance->getBypassParameter()`) is not integrated.
3. **No Reorder UI**:
   - `movePlugin(fromIndex, toIndex)` exists in `PluginRack.cpp` but has ZERO user interface in `PluginRackDialog`. Users have no way to reorder plugins.
4. **Editor Lifecycle**:
   - `PluginEditorWindow` uses `setContentOwned(editor, true)`, which deletes the editor on close. Reopening calls `createEditorIfNeeded()`. If instances shift due to index-based tracking, the wrong editor is displayed.
5. **Debug-Style Dialog**:
   - The rack dialog looks like an unstyled developer debug window with raw buttons and labels.

---

## 6. Current Metering System Audit

### Channel Metering:
- `AudioChannel` defines four stages:
  - `meterInputValues` (Stage A: Raw input before gain/phase/mono).
  - `meterPostGainValues` (Stage B: Post-gain).
  - `meterPostVSTValues` (Stage C: Post-VST).
  - `meterValues` (Stage D: Post-fader).
- **The Bug:** `ChannelStrip::updateMeterFromAudio()` reads `channelRef.getMeterValues()` (Stage D - Post-Fader).
  - When the fader is lowered to -inf, the meter falls to -inf.
  - When gain is applied, the meter shows post-gain level.
  - **Required fix:** The channel meter must read `meterInputValues` (Stage A / Pre-Gain), measuring incoming signal independent of gain and fader.

---

## 7. Current Persistence (`SessionManager.cpp`) Audit

- Saved as JSON (`.dsd`).
- Saves: `sampleRate`, `bufferSize`, 16 channels (`gainDb`, `faderDb`, `pan`, `mute`, `solo`, `directMonitor`, `disableOutput`, `phaseInvert`, `forceMono`, `inputDeviceName`, `inputChannelIndex`, `plugins` array), 16x4 `routingMatrix`, 4 `outputs`.
- Plugin array is serialized sequentially.
- **Requirement:** Stable plugin identity and exact chain order must be preserved during save and reload.
