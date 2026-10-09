/**
 * DSD A9 Remote — VFD Instrument Control Surface
 * 4×4 Grid, 16 channels. Green = ON. Red = MUTED. Black = everything else.
 */

// =============================================================================
// Global State
// =============================================================================
const state = {
    client: new DSDCPClient(),
    transportMode: 'wifi', // 'wifi' | 'usb'
    channels: [],
    dom: { grid: null, tiles: [] }
};

// Initialize 16 channel default state
for (let i = 0; i < 16; ++i) {
    state.channels.push({
        index: i,
        name: 'CH ' + String(i + 1).padStart(2, '0'),
        mute: false,
        on:   true,   // DSD channels default to ON (green active border)
        peakL: -60.0,
        peakR: -60.0,
        clip:  false,
    });
}

const CLIENT_VERSION = 'v0.1.2';

// =============================================================================
// Development Cache Purge (Service Worker + CacheStorage)
// =============================================================================
(function purgeDevCache() {
    if ('serviceWorker' in navigator) {
        navigator.serviceWorker.getRegistrations().then(regs => {
            regs.forEach(r => r.unregister().catch(() => {}));
        }).catch(() => {});
    }
    if ('caches' in window) {
        caches.keys().then(keys => {
            keys.forEach(k => caches.delete(k).catch(() => {}));
        }).catch(() => {});
    }
})();

// =============================================================================
// Startup
// =============================================================================
document.addEventListener('DOMContentLoaded', () => {
    console.log(`[DSD Remote] ${CLIENT_VERSION}`);
    const verBadge = document.getElementById('brand-version');
    if (verBadge) verBadge.textContent = CLIENT_VERSION;

    initDSDCP();
    renderDeckSurface();
    setupConnectionModal();
    setupFullscreenToggle();

    const savedTransport = localStorage.getItem('dsd_transport_mode') || 'wifi';
    const savedWifiUrl   = localStorage.getItem('dsd_wifi_url') || ('ws://' + (window.location.hostname || '127.0.0.1') + ':9100');
    const savedPin       = localStorage.getItem('dsd_pin') || '123456';

    setTransportMode(savedTransport, false);
    if (savedTransport === 'wifi') {
        const urlInput = document.getElementById('ws-url-input');
        if (urlInput) urlInput.value = savedWifiUrl;
    }
    setPinInputs(savedPin);

    const targetUrl = (savedTransport === 'usb') ? 'ws://127.0.0.1:9100' : savedWifiUrl;
    state.client.connect(targetUrl, savedPin, 'Samsung Galaxy A9');
});

// =============================================================================
// DSDCP Handlers — Bidirectional State Sync
// =============================================================================
function initDSDCP() {
    const client = state.client;

    client.onConnect = () => {
        document.getElementById('status-dot').className = 'status-dot';
        document.getElementById('status-text').textContent = 'PAIRING...';
    };

    client.onWelcome = () => {
        document.getElementById('status-dot').className = 'status-dot connected';
        document.getElementById('status-text').textContent = 'ONLINE';
        hideConnectionModal();
    };

    client.onDisconnect = () => {
        document.getElementById('status-dot').className = 'status-dot';
        document.getElementById('status-text').textContent = 'OFFLINE';
        document.getElementById('latency-label').textContent = '-- ms';
    };

    client.onLatency = (latencyMs) => {
        document.getElementById('latency-label').textContent = latencyMs + ' ms';
    };

    // Full snapshot sync
    client.onSync = (syncMsg) => {
        if (syncMsg.channels && Array.isArray(syncMsg.channels)) {
            syncMsg.channels.forEach(ch => {
                const s = state.channels[ch.index];
                if (!s) return;
                if (ch.name  !== undefined) s.name = ch.name;
                if (ch.mute  !== undefined) s.mute = Boolean(ch.mute);
                if (ch.on    !== undefined) s.on   = Boolean(ch.on);
            });
        }
        updateAllUI();
    };

    // Delta channel state (from desktop mixer or remote command feedback)
    client.onChannelState = (channelIndex, chState) => {
        const s = state.channels[channelIndex];
        if (!s) return;
        if (chState.name !== undefined) s.name = chState.name;
        if (chState.mute !== undefined) s.mute = chState.mute;
        if (chState.on   !== undefined) s.on   = chState.on;
        updateChannelUI(channelIndex);
    };

    // 50ms metering
    client.onMeters = (chMeters) => {
        if (!chMeters || !Array.isArray(chMeters)) return;
        for (let i = 0; i < chMeters.length; ++i) {
            const m = chMeters[i];
            updateTileMeterFast(m.ch, Math.max(m.peakL, m.peakR), m.clip);
        }
    };
}

// =============================================================================
// DOM — 4×4 Instrument Grid
// =============================================================================
function renderDeckSurface() {
    const grid = document.getElementById('deck-grid');
    grid.innerHTML = '';
    state.dom.tiles = [];
    for (let i = 0; i < 16; ++i) {
        grid.appendChild(createChannelTile(i));
    }
}

function createChannelTile(chIndex) {
    const ch   = state.channels[chIndex];
    const tile = document.createElement('div');

    tile.id        = 'channel-tile-' + chIndex;
    tile.className = tileClass(ch);

    // Channel number — zero-padded, matches instrument numbering
    const chNum = String(chIndex + 1).padStart(2, '0');

    tile.innerHTML = `
        <div class="tile-top-row">
            <span class="tile-ch-num" id="tile-chnum-${chIndex}">${chNum}</span>
            <span class="tile-status-dot" id="tile-dot-${chIndex}"></span>
        </div>

        <div class="tile-center">
            <span class="tile-name" id="tile-name-${chIndex}">${ch.name}</span>
        </div>

        <div class="tile-meter-vertical" id="tile-meter-${chIndex}">
            <div class="meter-seg green"  data-seg="0"></div>
            <div class="meter-seg green"  data-seg="1"></div>
            <div class="meter-seg green"  data-seg="2"></div>
            <div class="meter-seg green"  data-seg="3"></div>
            <div class="meter-seg green"  data-seg="4"></div>
            <div class="meter-seg yellow" data-seg="5"></div>
            <div class="meter-seg yellow" data-seg="6"></div>
            <div class="meter-seg red"    data-seg="7"></div>
        </div>

        <button type="button" class="tile-mute-btn" id="tile-mute-${chIndex}" title="Toggle MUTE">
            <span class="tile-mute-label">MUTE</span>
        </button>
    `;

    // Cache references for fast update
    state.dom.tiles[chIndex] = {
        tile,
        segments: tile.querySelectorAll('.meter-seg'),
        muteBtn:  tile.querySelector('#tile-mute-'  + chIndex),
        nameEl:   tile.querySelector('#tile-name-'  + chIndex),
    };

    // Tap anywhere on tile → toggle mute
    tile.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        toggleChannelMute(chIndex);
    });

    return tile;
}

// Build tile class string from channel state
function tileClass(ch) {
    let cls = 'deck-tile';
    if (ch.on)   cls += ' is-on';
    if (ch.mute) cls += ' is-muted';
    return cls;
}

// =============================================================================
// UI Synchronization
// =============================================================================
function updateAllUI() {
    for (let i = 0; i < 16; ++i) updateChannelUI(i);
}

function updateChannelUI(chIndex) {
    const ch  = state.channels[chIndex];
    const dom = state.dom.tiles[chIndex];
    if (!ch || !dom) return;

    // Update class: is-on / is-muted drive all visual state
    dom.tile.className = tileClass(ch);

    // Update name
    if (dom.nameEl) dom.nameEl.textContent = ch.name;
}

// =============================================================================
// High-performance Meter (no DOM recreation, no classList churn beyond 'lit')
// =============================================================================
function updateTileMeterFast(chIndex, peakDb, clip) {
    const dom = state.dom.tiles[chIndex];
    if (!dom || !dom.segments) return;

    // Segment thresholds (dBFS): 5 green, 2 amber, 1 red
    const thresholds = [-48, -36, -24, -18, -12, -6, -2, 0];
    const n = dom.segments.length;

    for (let s = 0; s < n; ++s) {
        const isLit = (peakDb >= thresholds[s]) || (s === 7 && clip);
        const seg   = dom.segments[s];
        if (isLit) {
            if (!seg.classList.contains('lit')) seg.classList.add('lit');
        } else {
            if (seg.classList.contains('lit'))  seg.classList.remove('lit');
        }
    }
}

// =============================================================================
// MUTE Toggle
// =============================================================================
function toggleChannelMute(chIndex) {
    const ch = state.channels[chIndex];
    ch.mute = !ch.mute;
    updateChannelUI(chIndex);
    state.client.setChannelMute(chIndex, ch.mute);
    if (navigator.vibrate) navigator.vibrate(10);
}

// =============================================================================
// Transport & Connection Modal
// =============================================================================
function setTransportMode(mode, save = true) {
    state.transportMode = mode;
    if (save) localStorage.setItem('dsd_transport_mode', mode);

    const wifiBtn  = document.getElementById('transport-wifi-btn');
    const usbBtn   = document.getElementById('transport-usb-btn');
    const urlInput = document.getElementById('ws-url-input');
    const urlHint  = document.getElementById('ws-url-hint');
    const badge    = document.getElementById('transport-badge');

    if (!wifiBtn || !usbBtn) return;

    if (mode === 'usb') {
        wifiBtn.classList.remove('active');
        usbBtn.classList.add('active');
        if (urlInput) { urlInput.value = 'ws://127.0.0.1:9100'; urlInput.disabled = true; urlInput.style.opacity = '0.5'; }
        if (urlHint)  { urlHint.textContent = 'USB mode: ADB reverse loopback to ws://127.0.0.1:9100'; urlHint.style.color = '#FFA000'; }
        if (badge)    { badge.textContent = 'USB'; badge.className = 'transport-badge'; }
    } else {
        usbBtn.classList.remove('active');
        wifiBtn.classList.add('active');
        const savedUrl = localStorage.getItem('dsd_wifi_url') || ('ws://' + (window.location.hostname || '127.0.0.1') + ':9100');
        if (urlInput) { urlInput.value = savedUrl; urlInput.disabled = false; urlInput.style.opacity = '1'; }
        if (urlHint)  { urlHint.textContent = "Enter PC's LAN IP shown in the DSD Mixer REMOTE dialog."; urlHint.style.color = ''; }
        if (badge)    { badge.textContent = 'Wi-Fi'; badge.className = 'transport-badge'; }
    }
}

function setupConnectionModal() {
    const connectBtn = document.getElementById('connect-btn');
    const pinInputs  = document.querySelectorAll('.pin-digit');

    pinInputs.forEach((input, i) => {
        input.addEventListener('input', () => {
            if (input.value.length === 1 && i < pinInputs.length - 1)
                pinInputs[i + 1].focus();
        });
        input.addEventListener('keydown', (e) => {
            if (e.key === 'Backspace' && input.value.length === 0 && i > 0)
                pinInputs[i - 1].focus();
        });
    });

    connectBtn.addEventListener('click', () => {
        const pin = getPinFromInputs();
        localStorage.setItem('dsd_pin', pin);

        let targetUrl = 'ws://127.0.0.1:9100';
        if (state.transportMode === 'wifi') {
            const raw = document.getElementById('ws-url-input').value.trim();
            targetUrl = raw.startsWith('ws://') ? raw : ('ws://' + raw);
            localStorage.setItem('dsd_wifi_url', targetUrl);
        }

        document.getElementById('status-text').textContent = 'CONNECTING...';
        state.client.connect(targetUrl, pin, 'Samsung Galaxy A9');
    });
}

function openConnectionModal()  { document.getElementById('connection-modal').classList.remove('hidden'); }
function hideConnectionModal()  { document.getElementById('connection-modal').classList.add('hidden'); }

function getPinFromInputs() {
    let pin = '';
    document.querySelectorAll('.pin-digit').forEach(i => pin += i.value);
    return pin;
}

function setPinInputs(pin) {
    const inputs = document.querySelectorAll('.pin-digit');
    for (let i = 0; i < inputs.length; ++i) inputs[i].value = pin[i] || '';
}

function setupFullscreenToggle() {
    const btn = document.getElementById('fullscreen-btn');
    btn.addEventListener('click', () => {
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen().catch(() => {});
        } else {
            document.exitFullscreen().catch(() => {});
        }
    });
}
