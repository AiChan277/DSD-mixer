# DSD Mixer — Automated Test Plan & Verification Report

## 1. Executive Summary

This test suite provides automated, headless verification of the DSD Mixer audio engine, multicore scheduler, routing engine, VST3 subsystem, and session serialization.

All tests are implemented in `Tests/AudioEngineTests.cpp` and compiled into the `AudioEngineTests.exe` binary.

---

## 2. Test Matrix & Verification Results

| # | Test Name | Target Subsystem | Verification Criteria | Status |
| :--- | :--- | :--- | :--- | :--- |
| **01** | `testDeviceEnumeration` | Audio Hardware | Enumerate WASAPI devices, open default device at 48 kHz / 128 buffer. | **PASS** |
| **02** | `testGainProcessor` | DSP Core | Accurate dB-to-linear conversion, -60 dB silence floor. | **PASS** |
| **03** | `testPanProcessor` | DSP Core | Constant-power pan curve ($L^2 + R^2 = 1.0$), hard left/right. | **PASS** |
| **04** | `testMeterProcessor` | DSP Core | Peak detection, true-peak tracking, clipping latch. | **PASS** |
| **05** | `test16x4RoutingAndOutputs` | Routing Engine | Multi-bus matrix routing, individual send gains. | **PASS** |
| **06** | `testMulticoreScheduler` | Multicore Thread Pool | Fast attack scale up under high load, slow decay scale down, zero thread churn. | **PASS** |
| **07** | `testPerformanceTelemetry` | Audio Diagnostics | Deadline miss counter, true peak DSP load, xrun tracking. | **PASS** |
| **08** | `testLowLatencyCaptureFifo` | Clock Drift Steering | Bounded buffer occupancy, drift rate ppm, burst flush. | **PASS** |
| **09** | `testWindowAudioCapture` | Loopback Engine | OBS process loopback capture, PID resolution, ring buffer latency. | **PASS** |
| **10** | `testSessionSaveLoad` | Persistence Subsystem | Full `.dsd` serialization of channel controls, plugins, routing matrix, and restoration. | **PASS** |
| **11** | `testVstBypassAndReorder` | VST3 Subsystem | Independent per-plugin bypass, real-time DSP execution reordering, slot removal by ID. | **PASS** |
| **12** | `testPreGainMetering` | Stage A Metering | Raw input level read pre-gain, unaffected by +12 dB boost or -60 dB cut. | **PASS** |
| **13** | `testGainAndFaderControls`| DSP Control Law | Gain rotary (-60 dB to +24 dB) and Fader (-60 dB to +12 dB) real-time gain multipliers. | **PASS** |
| **14** | `testOnOffVsMuteSemantics`| Control Decoupling | Channel ON/OFF (`disableOutput`) gates mix without touching MUTE; MUTE zeros fader without touching ON/OFF. | **PASS** |
| **15** | `testDirectMonitorRouting`| PFL / Monitor Bus | DM routes pre-fader PFL buffer to monitor buses while program mix remains unaffected. | **PASS** |
| **16** | `testPhaseAndMono` | DSP Utilities | Polarity flip (180°) and stereo-to-mono summing ($0.5 \times (L+R)$). | **PASS** |

---

## 3. Detailed Subsystem Verification

### 3.1. VST3 Bypass & Reordering Verification (`testVstBypassAndReorder`)
- **Plugin A**: $y = 2x + 1$
- **Plugin B**: $y = 3x$
- **Order A $\rightarrow$ B**: Input $1.0 \rightarrow A(1.0) = 3.0 \rightarrow B(3.0) = 9.0$. (Verified: Output = 9.0)
- **Bypass A**: Input $1.0 \rightarrow B(1.0) = 3.0$. (Verified: Output = 3.0)
- **Bypass Both**: Input $1.0 \rightarrow 1.0$. (Verified: Output = 1.0)
- **Reorder B $\rightarrow$ A**: Input $1.0 \rightarrow B(1.0) = 3.0 \rightarrow A(3.0) = 7.0$. (Verified: Output = 7.0)
- **Remove B**: Input $1.0 \rightarrow A(1.0) = 3.0$. (Verified: Output = 3.0)

### 3.2. Pre-Gain Metering Verification (`testPreGainMetering`)
- Input signal: Sine wave at $0.12589\text{ FS}$ ($-18\text{ dBFS}$).
- Gain applied: $+12.0\text{ dB}$ ($\approx 3.98\times$).
- **Stage A Pre-Gain Meter**: Measured $0.12589\text{ FS}$ ($-18\text{ dBFS}$, zero deviation from input).
- **Stage B Post-Gain Meter**: Measured $0.501\text{ FS}$ ($-6\text{ dBFS}$, reflecting $+12\text{ dB}$ boost).
- Gain reduced to $-60.0\text{ dB}$:
- **Stage A Pre-Gain Meter**: Still measured $0.12589\text{ FS}$ (headroom accurately monitored).
- **Stage B Post-Gain Meter**: $0.0\text{ FS}$ ($-\infty\text{ dBFS}$).

### 3.3. Direct Monitor / PFL Verification (`testDirectMonitorRouting`)
- Channel 0: Signal $0.8\text{ FS}$, Fader down to $-60\text{ dB}$, Direct Monitor = ON.
- Channel 1: Signal $0.4\text{ FS}$, Fader at $0\text{ dB}$, Direct Monitor = OFF.
- **Main Mix Output (Non-Monitor)**: Output level $= 0.4\text{ FS}$ (Channel 0 silenced in main mix).
- **Monitor Bus Output (Monitor = true)**: Output level $= 0.8\text{ FS}$ (Channel 0 pre-fader PFL tapped directly to headphones).
- When Direct Monitor is turned OFF: Monitor bus output immediately returns to $0.4\text{ FS}$.

---

## 4. Performance & Telemetry Results

Under full 16-channel processing at $48\text{ kHz}$ with 480 samples buffer size ($10.0\text{ ms}$ deadline):
- **DSP Processing Time**: $0.046\text{ ms}$ (Average), $1.6699\text{ ms}$ (Peak under initial thread spin-up).
- **DSP Load**: $0.46\%$ (Average), $16.7\%$ (Peak).
- **Headroom Margin**: $9.954\text{ ms}$ (out of $10.0\text{ ms}$).
- **Deadline Misses / XRuns**: $0$.
- **Process CPU Tracker**: $0\%$ jitter.
