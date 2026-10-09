/**
 * DSD Control Protocol (DSDCP v1) WebSocket Client Library
 * Dedicated remote control client for DSD Mixer & Samsung Galaxy A9
 */

class DSDCPClient {
    constructor() {
        this.ws = null;
        this.url = '';
        this.pin = '123456';
        this.clientName = 'Samsung Galaxy A9';
        this.clientType = 'control_surface';
        this.clientId = null;
        this.capabilities = null;

        this.isConnected = false;
        this.isAuthenticated = false;
        this.reconnectTimer = null;
        this.autoReconnect = true;
        this.reconnectIntervalMs = 2500;

        this.pingTimer = null;
        this.lastPingTime = 0;
        this.latencyMs = 0;

        this.nextCmdId = 1;
        this.pendingCommands = new Map(); // id -> { resolve, reject, timeout }

        // Event callbacks
        this.onConnect = null;
        this.onDisconnect = null;
        this.onError = null;
        this.onWelcome = null;
        this.onSync = null;
        this.onChannelState = null;
        this.onOutputState = null;
        this.onRouteState = null;
        this.onMeters = null;
        this.onLatency = null;
    }

    connect(url, pin = '123456', clientName = 'Samsung Galaxy A9') {
        this.url = url;
        this.pin = pin;
        this.clientName = clientName;
        this.autoReconnect = true;

        this._cleanupWs();

        try {
            this.ws = new WebSocket(this.url);
            this.ws.binaryType = 'blob';

            this.ws.onopen = () => {
                this.isConnected = true;
                this._sendHello();
                this._startHeartbeat();
                if (this.onConnect) this.onConnect();
            };

            this.ws.onmessage = (event) => {
                this._handleMessage(event.data);
            };

            this.ws.onclose = (event) => {
                this._handleClose();
            };

            this.ws.onerror = (err) => {
                if (this.onError) this.onError(err);
            };
        } catch (e) {
            this._handleClose();
        }
    }

    disconnect() {
        this.autoReconnect = false;
        if (this.isConnected && this.ws) {
            this._sendGoodbye();
        }
        this._cleanupWs();
        if (this.onDisconnect) this.onDisconnect();
    }

    _cleanupWs() {
        this.isConnected = false;
        this.isAuthenticated = false;
        this._stopHeartbeat();

        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }

        for (const [id, req] of this.pendingCommands.entries()) {
            clearTimeout(req.timeout);
            req.reject(new Error('Connection closed'));
        }
        this.pendingCommands.clear();

        if (this.ws) {
            this.ws.onopen = null;
            this.ws.onmessage = null;
            this.ws.onclose = null;
            this.ws.onerror = null;
            try { this.ws.close(); } catch (_) {}
            this.ws = null;
        }
    }

    _handleClose() {
        const wasConnected = this.isConnected;
        this.isConnected = false;
        this.isAuthenticated = false;
        this._stopHeartbeat();

        if (wasConnected && this.onDisconnect) {
            this.onDisconnect();
        }

        if (this.autoReconnect && !this.reconnectTimer) {
            this.reconnectTimer = setTimeout(() => {
                this.reconnectTimer = null;
                if (this.autoReconnect) {
                    this.connect(this.url, this.pin, this.clientName);
                }
            }, this.reconnectIntervalMs);
        }
    }

    _sendHello() {
        const helloMsg = {
            type: 'hello',
            protocol: 'DSDCP',
            version: 1,
            client: {
                name: this.clientName,
                type: this.clientType,
                id: 'a9_' + Math.random().toString(36).substr(2, 6)
            },
            auth: {
                pin: this.pin
            }
        };
        this._send(helloMsg);
    }

    _sendGoodbye() {
        const msg = {
            type: 'goodbye',
            reason: 'User disconnected'
        };
        this._send(msg);
    }

    _startHeartbeat() {
        this._stopHeartbeat();
        this.pingTimer = setInterval(() => {
            if (this.isConnected) {
                this.lastPingTime = performance.now();
                this._send({
                    type: 'ping',
                    timestamp: Date.now()
                });
            }
        }, 5000);
    }

    _stopHeartbeat() {
        if (this.pingTimer) {
            clearInterval(this.pingTimer);
            this.pingTimer = null;
        }
    }

    _send(obj) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(obj));
        }
    }

    _handleMessage(data) {
        let msg;
        try {
            msg = JSON.parse(data);
        } catch (e) {
            console.error('Invalid JSON received:', data);
            return;
        }

        switch (msg.type) {
            case 'welcome':
                this.isAuthenticated = true;
                this.clientId = msg.session?.clientId;
                this.capabilities = msg.session?.capabilities;
                if (this.onWelcome) this.onWelcome(msg);
                // Subscribe to state events and high-speed metering
                this.subscribe(['channel.state', 'output.state', 'route.state'], true, 50);
                break;

            case 'sync':
                if (this.onSync) this.onSync(msg);
                break;

            case 'event':
                this._handleEvent(msg);
                break;

            case 'response':
                this._handleResponse(msg);
                break;

            case 'pong':
                if (this.lastPingTime > 0) {
                    this.latencyMs = Math.round(performance.now() - this.lastPingTime);
                    if (this.onLatency) this.onLatency(this.latencyMs);
                }
                break;

            case 'error':
                console.error('DSDCP Server Error:', msg);
                if (this.onError) this.onError(msg);
                break;

            default:
                break;
        }
    }

    _handleEvent(msg) {
        if (msg.event === 'channel.state' && this.onChannelState) {
            this.onChannelState(msg.channel, msg.state);
        } else if (msg.event === 'output.state' && this.onOutputState) {
            this.onOutputState(msg.output, msg.state);
        } else if (msg.event === 'route.state' && this.onRouteState) {
            this.onRouteState(msg.channel, msg.output, msg.state);
        } else if (msg.event === 'meters' && this.onMeters) {
            this.onMeters(msg.channels, msg.outputs);
        }
    }

    _handleResponse(msg) {
        const id = msg.id;
        if (id && this.pendingCommands.has(id)) {
            const req = this.pendingCommands.get(id);
            this.pendingCommands.delete(id);
            clearTimeout(req.timeout);
            if (msg.success) {
                req.resolve(msg);
            } else {
                req.reject(new Error(msg.error?.message || 'Command failed'));
            }
        }
    }

    // =========================================================================
    // Protocol Commands API
    // =========================================================================

    sendCommand(method, params = {}) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected || !this.isAuthenticated) {
                reject(new Error('Not connected or not authenticated'));
                return;
            }

            const id = 'cmd_' + (this.nextCmdId++);
            const cmd = {
                type: 'command',
                id,
                method,
                params
            };

            const timeout = setTimeout(() => {
                if (this.pendingCommands.has(id)) {
                    this.pendingCommands.delete(id);
                    reject(new Error('Command timeout (method: ' + method + ')'));
                }
            }, 3000);

            this.pendingCommands.set(id, { resolve, reject, timeout });
            this._send(cmd);
        });
    }

    subscribe(events = ['channel.state', 'output.state', 'route.state'], meteringEnabled = true, intervalMs = 50) {
        const msg = {
            type: 'subscribe',
            events,
            meters: {
                enabled: meteringEnabled,
                intervalMs: Math.max(50, intervalMs)
            }
        };
        this._send(msg);
    }

    requestSync() {
        return this.sendCommand('sync.request', {});
    }

    // Channel Control Helpers
    setChannelMute(channel, value) {
        return this.sendCommand('channel.mute', { channel, value: Boolean(value) });
    }

    setChannelGain(channel, value) {
        return this.sendCommand('channel.gain', { channel, value: Number(value) });
    }

    setChannelFader(channel, value) {
        return this.sendCommand('channel.fader', { channel, value: Number(value) });
    }

    setChannelPan(channel, value) {
        return this.sendCommand('channel.pan', { channel, value: Number(value) });
    }

    setChannelPfl(channel, value) {
        return this.sendCommand('channel.pfl', { channel, value: Boolean(value) });
    }

    setChannelDm(channel, value) {
        return this.sendCommand('channel.dm', { channel, value: Boolean(value) });
    }

    setChannelSolo(channel, value) {
        return this.sendCommand('channel.solo', { channel, value: Boolean(value) });
    }

    setChannelOn(channel, value) {
        return this.sendCommand('channel.on', { channel, value: Boolean(value) });
    }

    setChannelPhase(channel, value) {
        return this.sendCommand('channel.phase', { channel, value: Boolean(value) });
    }

    setChannelMono(channel, value) {
        return this.sendCommand('channel.mono', { channel, value: Boolean(value) });
    }

    setChannelHpf(channel, value) {
        return this.sendCommand('channel.hpf', { channel, value: Boolean(value) });
    }

    setChannelPad(channel, value) {
        return this.sendCommand('channel.pad', { channel, value: Boolean(value) });
    }

    setChannelName(channel, value) {
        return this.sendCommand('channel.name', { channel, value: String(value) });
    }

    // Output Control Helpers
    setOutputMute(output, value) {
        return this.sendCommand('output.mute', { output, value: Boolean(value) });
    }

    setOutputFader(output, value) {
        return this.sendCommand('output.fader', { output, value: Number(value) });
    }

    setOutputMonitor(output, value) {
        return this.sendCommand('output.monitor', { output, value: Boolean(value) });
    }

    setOutputName(output, value) {
        return this.sendCommand('output.name', { output, value: String(value) });
    }

    // Routing Control Helpers
    setRouteEnable(channel, output, value) {
        return this.sendCommand('route.enable', { channel, output, value: Boolean(value) });
    }

    setRouteGain(channel, output, value) {
        return this.sendCommand('route.gain', { channel, output, value: Number(value) });
    }

    setRoutePan(channel, output, value) {
        return this.sendCommand('route.pan', { channel, output, value: Number(value) });
    }
}
