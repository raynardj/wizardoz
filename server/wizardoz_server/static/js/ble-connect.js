/**
 * Wizardoz BLE Connect — Web Bluetooth API module
 *
 * Handles scanning, connecting, and configuring WiFi on ESP32 devices
 * running the WizardozConnect BLE firmware library.
 */

// BLE UUIDs — must match WizardozConnect.h
const SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const CHAR_SSID_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';
const CHAR_PASSWORD_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a9';
const CHAR_COMMAND_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26aa';
const CHAR_STATUS_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26ab';
const CHAR_DEVICE_NAME_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26ac';
const CHAR_SERVER_HOST_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26ad';

const CMD_CONNECT = 'CONNECT';
const CMD_DISCONNECT = 'DISCONNECT';
const CMD_CLEAR = 'CLEAR';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Dump a DataView as a hex string (e.g. "43 4f 4e 4e 45 43 54") for debug. */
function _bleHex(dataView) {
    const bytes = new Uint8Array(dataView.buffer, dataView.byteOffset, dataView.byteLength);
    return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join(' ');
}

/** Decode a DataView to a clean UTF-8 string, stripping nulls / control
 *  chars / U+FFFD replacement characters that BLE reads sometimes produce
 *  (macOS GATT cache, MTU clipping, uninitialised buffers, …). */
function _bleDecode(dataView) {
    const raw = new TextDecoder().decode(dataView);
    return raw.replace(/[\x00-\x1f\uFFFD]/g, '').trim();
}

/** Acknowledged BLE write (Write Request).  Falls back to the deprecated
 *  writeValue() for browsers that lack writeValueWithResponse. */
function _bleWrite(char, data) {
    return char.writeValueWithResponse
        ? char.writeValueWithResponse(data)
        : char.writeValue(data);
}

/**
 * Represents a single BLE connection to a Wizardoz ESP32 device.
 */
class WizardozDevice {
    constructor() {
        this.bleDevice = null;
        this.server = null;
        this.service = null;
        this.charSSID = null;
        this.charPassword = null;
        this.charCommand = null;
        this.charServerHost = null;
        this.charStatus = null;
        this.charDevName = null;
        this.deviceName = '(unknown)';
        this.status = 'IDLE';
        this.onStatusChange = null;    // callback(status)
        this.onDisconnect = null;      // callback()
    }

    /** Request & connect to one ESP32 device via the browser BLE picker. */
    async connect() {
        if (!navigator.bluetooth) {
            throw new Error('Web Bluetooth is not supported in this browser.');
        }

        // Filter by name prefix — more reliable on macOS/Chrome than filtering
        // by 128-bit service UUID, which may not fit in the 31-byte advertisement.
        this.bleDevice = await navigator.bluetooth.requestDevice({
            filters: [{ namePrefix: 'Wizardoz' }],
            optionalServices: [SERVICE_UUID],
        });

        this.bleDevice.addEventListener('gattserverdisconnected', () => {
            console.log(`[BLE] Device disconnected: ${this.deviceName}`);
            this.status = 'DISCONNECTED';
            if (this.onStatusChange) this.onStatusChange('DISCONNECTED');
            if (this.onDisconnect) this.onDisconnect();
        });

        this.server = await this.bleDevice.gatt.connect();
        this.service = await this.server.getPrimaryService(SERVICE_UUID);

        // Grab all characteristics
        this.charSSID = await this.service.getCharacteristic(CHAR_SSID_UUID);
        this.charPassword = await this.service.getCharacteristic(CHAR_PASSWORD_UUID);
        this.charCommand = await this.service.getCharacteristic(CHAR_COMMAND_UUID);
        this.charServerHost = await this.service.getCharacteristic(CHAR_SERVER_HOST_UUID);
        this.charStatus = await this.service.getCharacteristic(CHAR_STATUS_UUID);
        this.charDevName = await this.service.getCharacteristic(CHAR_DEVICE_NAME_UUID);

        // Device name — prefer the advertisement name (decoded natively by
        // the browser & never garbled by GATT caching) over a characteristic
        // read which macOS may return stale / uninitialised bytes for.
        if (this.bleDevice.name) {
            this.deviceName = this.bleDevice.name;
        } else {
            const nameValue = await this.charDevName.readValue();
            this.deviceName = _bleDecode(nameValue) || '(unknown)';
        }

        // Subscribe to status notifications (or indications — startNotifications
        // handles both).
        await this.charStatus.startNotifications();
        this.charStatus.addEventListener('characteristicvaluechanged', (event) => {
            const hex = _bleHex(event.target.value);
            const clean = _bleDecode(event.target.value);
            console.log(`[BLE] Status raw [${hex}]  decoded "${clean}"`);
            if (!clean) return;  // skip empty / fully-garbled frames
            this.status = clean;
            if (this.onStatusChange) this.onStatusChange(clean, hex);
        });

        // Read initial status — sanitise because macOS may return cached junk.
        // Fall back to "IDLE" when the read is unreadable.
        try {
            const initStatus = await this.charStatus.readValue();
            const hex = _bleHex(initStatus);
            const decoded = _bleDecode(initStatus);
            console.log(`[BLE] Initial status raw [${hex}]  decoded "${decoded}"`);
            this.status = decoded || 'IDLE';
        } catch (e) {
            console.warn('[BLE] Could not read initial status, defaulting to IDLE', e);
            this.status = 'IDLE';
        }
        if (this.onStatusChange) this.onStatusChange(this.status, null);

        console.log(`[BLE] Connected to: ${this.deviceName}`);
        return this;
    }

    /** Write WiFi credentials and send CONNECT command.
     *  @param {string} ssid - WiFi network name
     *  @param {string} password - WiFi password
     *  @param {string} [serverHost] - Optional server IP for WebSocket
     *  @param {function} [onProgress] - Optional callback(message) for step-by-step feedback
     */
    async configureWiFi(ssid, password, serverHost, onProgress) {
        const log = onProgress || (() => {});

        if (!this.server || !this.server.connected) {
            throw new Error('BLE device not connected — please re-scan.');
        }
        const encoder = new TextEncoder();

        log(`Writing SSID "${ssid}" (${ssid.length} chars)…`);
        await _bleWrite(this.charSSID, encoder.encode(ssid));
        log('SSID written OK');

        log('Writing password…');
        await _bleWrite(this.charPassword, encoder.encode(password));
        log('Password written OK');

        if (serverHost) {
            log(`Writing server host: ${serverHost}…`);
            await _bleWrite(this.charServerHost, encoder.encode(serverHost));
            log('Server host written OK');
        }

        log('Sending CONNECT command…');
        await _bleWrite(this.charCommand, encoder.encode(CMD_CONNECT));
        log('CONNECT sent — waiting for device to join WiFi…');
    }

    /** Send DISCONNECT command. */
    async disconnectWiFi() {
        if (!this.server || !this.server.connected) return;
        await _bleWrite(this.charCommand, new TextEncoder().encode(CMD_DISCONNECT));
    }

    /** Send CLEAR command to wipe stored credentials. */
    async clearCredentials() {
        if (!this.server || !this.server.connected) return;
        await _bleWrite(this.charCommand, new TextEncoder().encode(CMD_CLEAR));
    }

    /** Disconnect BLE GATT. */
    disconnect() {
        if (this.bleDevice && this.bleDevice.gatt.connected) {
            this.bleDevice.gatt.disconnect();
        }
    }

    get isConnected() {
        return this.bleDevice && this.bleDevice.gatt.connected;
    }
}

/**
 * Manager that keeps track of multiple WizardozDevice instances.
 */
class WizardozDeviceManager {
    constructor() {
        /** @type {WizardozDevice[]} */
        this.devices = [];
        this.onDeviceAdded = null;     // callback(device)
        this.onDeviceRemoved = null;   // callback(device)
    }

    /** Scan for and connect a new device. Returns the WizardozDevice. */
    async addDevice() {
        const device = new WizardozDevice();

        device.onDisconnect = () => {
            this.devices = this.devices.filter(d => d !== device);
            if (this.onDeviceRemoved) this.onDeviceRemoved(device);
        };

        await device.connect();
        this.devices.push(device);
        if (this.onDeviceAdded) this.onDeviceAdded(device);
        return device;
    }

    /** Disconnect all devices. */
    disconnectAll() {
        for (const d of [...this.devices]) {
            d.disconnect();
        }
        this.devices = [];
    }
}

// Export to window for inline usage
window.WizardozDevice = WizardozDevice;
window.WizardozDeviceManager = WizardozDeviceManager;
