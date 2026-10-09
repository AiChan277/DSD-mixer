# DSD Control Surface — Web Client for Samsung Galaxy A9

Dedicated AMOLED touchscreen control surface for the DSD Mixer application, built using HTML5, CSS3, and the DSD Control Protocol (`DSDCP v1`).

---

## Features

- **Pure AMOLED Pitch-Black Theme (`#000000`):** Minimizes battery drain and eliminates OLED burn-in on Samsung Galaxy A9.
- **Dual Transport Support:** Connect wirelessly via local **Wi-Fi** or low-latency **USB (ADB Reverse)**.
- **High-Precision Non-Linear Touch Faders:** Decibel curve matching broadcast console response with 0.0 dB snap detent and double-tap reset.
- **Dual Peak/RMS LED Metering:** 50ms rate-limited meter telemetry with peak-hold decay and clip warnings.
- **Console Toggles:** Glowing MUTE (Amber/Red), SOLO (Yellow), PFL (Amber), Direct Monitor (Cyan), and ON/OFF (Green) with mutual exclusion safety.
- **Channel Detail Inspector:** HPF (80 Hz Butterworth), PAD (-20 dB), Phase Invert, Force Mono, and 16x4 Output Bus Sends.
- **Bank Navigation:** Quick switching between Bank 1 (CH 1–8), Bank 2 (CH 9–16), Master Outputs, and All 16 Channels.
- **Bi-Directional State Synchronization:** Automatic state diffing ensures real-time parity between desktop UI and phone without feedback loops.
- **Security & PIN Pairing:** Localhost and USB connections enforce the exact same 6-digit PIN pairing model (localhost is never assumed trusted).

---

## Connection Modes

### Mode 1: USB / ADB Connection (Low Latency & Always Powered)

Using physical USB keeps the Samsung Galaxy A9 powered and active as a dedicated mixing deck with minimal latency (<2ms).

#### Step-by-Step Setup:

1. **Enable USB Debugging on Samsung Galaxy A9:**
   - Go to `Settings` → `About phone` → `Software information`.
   - Tap `Build number` 7 times to enable Developer options.
   - Go back to `Settings` → `Developer options` → toggle **USB debugging** to **ON**.
2. **Connect USB Cable:**
   - Plug the Samsung Galaxy A9 into your PC via USB.
   - When prompted on the phone, tap **"Always allow from this computer"** and click **Allow**.
3. **Verify ADB Connection on PC:**
   ```cmd
   adb devices
   ```
   *(Ensure your device appears with `device` status).*
4. **Establish ADB Reverse Port Forwarding:**
   ```cmd
   adb reverse tcp:9100 tcp:9100
   adb reverse tcp:8080 tcp:8080
   ```
5. **Start the Local Web Server for WebClient (on PC):**
   ```powershell
   cd d:\Project\DSD-mixer\WebClient
   python serve.py
   ```
6. **Open the Control Surface on Samsung A9:**
   - Open Chrome or Samsung Internet on the phone.
   - Navigate to:
     ```
     http://127.0.0.1:8080
     ```
7. **Pair with DSD Mixer:**
   - In the connection modal, select **USB (ADB Reverse)**.
   - The WebSocket endpoint automatically defaults to `ws://127.0.0.1:9100`.
   - Enter the 6-digit pairing PIN shown in the PC desktop DSD Mixer **`REMOTE`** dialog.
   - Tap **CONNECT & SYNC**.
8. **Full-Screen Deck:**
   - Tap the fullscreen button (⛶) or choose **"Add to Home Screen"** in browser menu for a bezel-to-bezel console.

#### Removing Reverse Port Forwarding:
```cmd
adb reverse --remove tcp:9100
adb reverse --remove tcp:8080
```
Or remove all reverse rules at once:
```cmd
adb reverse --remove-all
```

---

### Mode 2: Local Wi-Fi (Wireless)

1. Launch **DSD Mixer** on your computer.
2. In the top bar of DSD Mixer, click **`REMOTE`** to open the Control Surface Setup dialog.
3. Note your computer's local IP address (e.g. `192.168.1.150`) and the 6-digit PIN code (e.g. `123456`).
4. On your Samsung Galaxy A9:
   - Ensure the phone is connected to the same Wi-Fi network.
   - Start the local HTTP server or open `http://<YOUR_PC_IP>:8080`.
   - Select **Wi-Fi (LAN)** transport.
   - Enter `ws://<YOUR_PC_IP>:9100` and the 6-digit pairing PIN.
   - Tap **CONNECT & SYNC**.
