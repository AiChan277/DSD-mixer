# DSD Mixer — Signal Flow & Audio Architecture

## 1. Complete Channel Strip Signal Flow

Each input channel in DSD Mixer processes audio through four discrete stages (Stages A through D) with dedicated metering and tap points:

```
                      [ Audio Input Source ]
                 (Hardware In / Loopback / Sine)
                                |
                                v
                   +------------------------+
                   |  STAGE A: RAW INPUT    |
                   |  meterPreGainValues    | <--- Input Meter Tap (Raw dBFS)
                   +------------------------+
                                |
                                v
                   +------------------------+
                   |  POLARITY / MONO SUM   |
                   |  (Phase Invert / Mono) |
                   +------------------------+
                                |
                                v
                   +------------------------+
                   |  STAGE B: INPUT GAIN   |
                   |  (-60 dB to +24 dB)    |
                   |  meterPostGainValues   |
                   +------------------------+
                                |
                                v
                   +------------------------+
                   |  STAGE C: VST3 RACK    |
                   |  Plugin 1 -> ... -> N  |
                   |  (Independent Bypass)  |
                   |  meterPostVSTValues    |
                   +------------------------+
                                |
                   +------------+------------+
                   |                         |
                   v (PFL Tap)               v
       +-----------------------+     +------------------------+
       |   DIRECT MONITOR      |     |  STAGE D: FADER & MUTE |
       |   (pflBuffer)         |     |  (-60 dB to +12 dB)    |
       +-----------------------+     |  (Mute = 0.0x Gain)    |
                   |                 |  meterValues           |
                   |                 +------------------------+
                   |                             |
                   |                             v (Program Tap)
                   |                 +------------------------+
                   |                 |  CHANNEL ON / OFF GATE |
                   |                 |  (disableOutput flag)  |
                   |                 +------------------------+
                   |                             |
                   |                             v
        [ Monitor Outputs ]             [ Program Outputs ]
        (outBus->getMonitor)            (outBus->!getMonitor)
```

---

## 2. Metering Stages & Semantics

| Meter Stage | Accessor | Tap Location | Semantics & Purpose |
| :--- | :--- | :--- | :--- |
| **Stage A** | `AudioChannel::getMeterPreGain()` | Raw Input (`readBlock`) | Measures input transducer/capture headroom before any DSP manipulation. Channel strip meter component binds directly to this stage. |
| **Stage B** | `AudioChannel::getMeterPostGain()` | Post-Gain & Polarity | Measures signal level entering the VST3 plugin chain to verify gain staging. |
| **Stage C** | `AudioChannel::getMeterPostVST()` | Post-VST3 Rack | Measures signal level exiting plugins to detect VST output clipping or attenuation. |
| **Stage D** | `AudioChannel::getMeterPostFader()` | Post-Fader & Mute | Measures actual channel output level being sent into the routing matrix. |

---

## 3. Decoupling: Channel ON/OFF vs. MUTE

DSD Mixer strictly preserves the functional separation of console channel ON/OFF from channel MUTE:

### Channel ON/OFF (`AudioChannel::disableOutput`)
- **Location**: Post-Fader Routing Gate in `RoutingEngine::routeChannelsToOutputs`.
- **Behavior**:
  - When OFF (`disableOutput = true`), the channel is decoupled from program mix buses (`outBus->getMonitor() == false`).
  - Channel DSP continues executing; Stage A, B, C, D meters remain active and visible.
  - Direct Monitoring (PFL) remains operational if enabled.
  - Does **not** mutate the channel's `mute` state.

### Channel MUTE (`AudioChannel::mute`)
- **Location**: Pre-Routing DSP Attenuation in `GainProcessor::processLinear` / `AudioChannel::processBlock`.
- **Behavior**:
  - When MUTED (`mute = true`), audio is ramped to zero amplitude at the fader stage.
  - Stage A (pre-gain) and Stage B (post-gain) meters remain fully active.
  - Stage D (post-fader) meter drops to zero (-$\infty$ dBFS).
  - Does **not** mutate the channel's `disableOutput` state.

---

## 4. Direct Monitor (DM / PFL) Bus Routing

Direct Monitoring allows zero-latency / pre-fader headphone cueing:

1. **Pre-Fader PFL Buffer**:
   Immediately following Stage C (VST rack), audio is tapped into `AudioChannel::pflBuffer`:
   $$\text{pflBuffer} = \text{VST}(\text{Gain}(\text{Input}))$$
2. **Matrix Routing Evaluation**:
   When any channel has `directMonitor == true` (`ChannelManager::hasAnyDirectMonitorChannel() == true`):
   - **Monitor Output Buses** (`OutputBus::getMonitor() == true`):
     Monitor buses switch from the master mix to PFL summing. Channels with `directMonitor == true` route their `pflBuffer` directly into the monitor bus.
   - **Program Output Buses** (`OutputBus::getMonitor() == false`):
     Program buses continue receiving normal post-fader program audio (`channelBuffer`), gated by `disableOutput`.
3. **Restoration**:
   When Direct Monitor is deactivated on all channels, monitor buses seamlessly return to summing the program mix.

---

## 5. Output Bus Processing & Master True-Peak Limiting

Each output bus in `OutputManager`:
1. Collects summed float audio from `RoutingEngine`.
2. Applies bus Master Fader (-60 dB to +12 dB) and bus Mute.
3. Computes 4x oversampled True-Peak metering and detects sample inter-sample peaks exceeding 0.0 dBFS.
4. Outputs audio to the target WASAPI / ASIO physical device channel pair.
