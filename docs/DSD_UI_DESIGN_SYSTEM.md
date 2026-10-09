# DSD Mixer — UI/UX Design System Specification

## 1. Design Philosophy

DSD Mixer utilizes a professional hardware-digital hybrid console design system:
- **High legibility in low-light studio environments**: Matte dark charcoal backgrounds with high-contrast functional illumination.
- **Strict Visual Hierarchy**: Signal flow reads naturally from top to bottom (Input Selector $\rightarrow$ Stage Meter $\rightarrow$ Gain Rotary $\rightarrow$ Utility Buttons $\rightarrow$ Long-Throw Fader $\rightarrow$ Channel Strip Identity).
- **Console Feedback Consistency**: Active states have dedicated, unambiguous indicator colors (Green = Signal/Normal, Cyan = VST/Active, Red = Mute/Danger, Amber = Pre-fader/Warning/Bypass, Yellow = Solo).
- **Physical Precision**: Double-click to default, Shift+drag for fine adjustment, clear numeric overlays.

---

## 2. Color Palette & Semantic Tokens

```
Surface Base:          #121417 (Deep Matte Studio Black)
Surface Elevated:      #1A1D21 (Channel & Module Enclosures)
Surface Card/Slot:     #22262B (Plugin Rows, Control Pockets)
Surface Border:        #2E343D (Subtle 1px Enclosure Dividers)

Text Primary:          #E2E8F0 (High Contrast White-Gray)
Text Secondary:        #94A3B8 (Labels, Units, Inactive Badges)
Text Dimmed:           #64748B (Disabled Text, Slot Borders)

Accent Green (ON):     #10B981 (Channel Active, Normal Metering)
Accent Cyan (FX/GUI):  #06B6D4 (VST Rack, Plug-in Active)
Accent Red (MUTE/CLIP):#EF4444 (Mute Engaged, True-Peak Clip)
Accent Amber (BYPASS): #F59E0B (VST Bypass, Warning)
Accent Yellow (SOLO):  #EAB308 (Solo Engaged)
Accent Blue (MON/DM):  #3B82F6 (Monitor Bus, Direct Monitor PFL)
```

---

## 3. Typography & Badges

- **Console Monospace / Technical Readout**: Clear numeric typography for dB readouts, buffer latencies, and stage diagnostics.
- **Hardware Button Badges**:
  - Font: Bold Condensed Sans-Serif, all caps.
  - Height: 24–28 px.
  - Corner Radius: 3.0 px (machined beveled hardware appearance).
  - Normal State: Dark card `#22262B`, light gray text `#94A3B8`, 1px border `#2E343D`.
  - Active State: Vibrant background, high-contrast text, subtle glow border.

---

## 4. Reusable Core UI Components

### 4.1. `DSDGainControl`
- Precision rotary dial designed specifically for audio console input gain.
- **Range**: -60.0 dB to +24.0 dB (center detent at 0.0 dB).
- **Interactions**:
  - Vertical drag or rotary drag with linear sensitivity.
  - Shift + drag: Fine adjustment ($0.1\text{ dB}$ increments).
  - Double-click: Instantly resets to $0.0\text{ dB}$.
  - Live numeric overlay: Displays current gain value (e.g. `+3.5 dB`).

### 4.2. `MeterComponent`
- Multi-segment LED peak and True-Peak display.
- **Source Binding**: Directly bound to Stage A Pre-Gain (`ch.getMeterPreGain()`) on input strips to display true physical headroom independent of fader/gain manipulation.
- **Color Gradients**:
  - $-60\text{ dBFS}$ to $-18\text{ dBFS}$: Studio Green
  - $-18\text{ dBFS}$ to $-3\text{ dBFS}$: Warm Yellow/Amber
  - $-3\text{ dBFS}$ to $0\text{ dBFS}$: Caution Orange
  - $\ge 0\text{ dBFS}$: Red Peak Latch Indicator with click-to-reset.

### 4.3. `PluginRackDialog` & `PluginRowView`
- **Reorder Handle (`⋮⋮`)**: High-contrast grip affordance initiating drag-and-drop.
- **Drop Target Indicator**: 2px cyan highlight line indicating target insertion position.
- **Bypass Toggle**: Dedicated square toggle button with amber backlighting when active.
- **GUI Trigger**: Direct button to launch editor window with safe single-instance tracking.
- **Slot Removal**: Clean `×` button with confirmation.

### 4.4. `ChannelStrip` Layout
- **Input Selector**: Compact input dropdown (WASAPI device or loopback application).
- **OLED Screen Card**: High-contrast dark display housing:
  - Channel index badge (`CH 01`–`CH 16`) and Fader dB readout.
  - Editable channel scribble-strip name label.
  - Stage A Pre-Gain LED meter and live numeric dBFS readout.
- **Gain Block**: Compact `DSDGainControl` rotary dial with amber numeric readout.
- **Utility Rows**:
  - Row 1: `VST` (plugin rack launcher with count badge) and `DM` (Direct Monitor / PFL pre-fader tap).
  - Row 2: `MUTE` (independent post-fader mute), `PH` (180° phase invert), and `MONO` (stereo-to-mono summer).
- **Fader Bay**: Long-throw vertical broadcast fader slider with Allen screws and dual-sided scale markings.
- **Broadcast Base Buttons**:
  - `ON`: Prominent broadcast button illuminated in Green (`#4CAF50` / `#2E7D32`) when channel is active in mix.
  - `OFF`: Prominent broadcast button illuminated in Red (`#D32F2F`) when channel is disabled from mix.
  - Mutually exclusive physical radio-button operation, completely decoupled from `MUTE`.

---

## 5. Layout & Responsiveness

- **Top Bar**: Fixed 44px height. Global controls pinned left (Presets, Session, Reset); Technical telemetry pinned right (CPU %, DSP Load %, Buffer, Latency).
- **Mixer Section**: Horizontally scrolling channel bay supporting 16 channels with fixed 110px width per strip for physical touch and mouse target accuracy.
- **Output Bay Panel**: Docked right with 4 master output buses (`OUT 01` to `OUT 04`), each equipped with Master Fader, True-Peak Meter, `MUTE`, and `MON` toggle.
