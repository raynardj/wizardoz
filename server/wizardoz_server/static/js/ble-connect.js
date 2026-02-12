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

const CMD_CONNECT = 'CONNECT';
const CMD_DISCONNECT = 'DISCONNECT';
const CMD_CLEAR = 'CLEAR';

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
            if (this.onDisconnect) this.onDisconnect();
        });

        this.server = await this.bleDevice.gatt.connect();
        this.service = await this.server.getPrimaryService(SERVICE_UUID);

        // Grab all characteristics
        this.charSSID = await this.service.getCharacteristic(CHAR_SSID_UUID);
        this.charPassword = await this.service.getCharacteristic(CHAR_PASSWORD_UUID);
        this.charCommand = await this.service.getCharacteristic(CHAR_COMMAND_UUID);
        this.charStatus = await this.service.getCharacteristic(CHAR_STATUS_UUID);
        this.charDevName = await this.service.getCharacteristic(CHAR_DEVICE_NAME_UUID);

        // Read device name
        const nameValue = await this.charDevName.readValue();
        this.deviceName = new TextDecoder().decode(nameValue);

        // Subscribe to status notifications
        await this.charStatus.startNotifications();
        this.charStatus.addEventListener('characteristicvaluechanged', (event) => {
            const raw = new TextDecoder().decode(event.target.value);
            this.status = raw;
            console.log(`[BLE] Status (${this.deviceName}): ${raw}`);
            if (this.onStatusChange) this.onStatusChange(raw);
        });

        // Read initial status
        const initStatus = await this.charStatus.readValue();
        this.status = new TextDecoder().decode(initStatus);
        if (this.onStatusChange) this.onStatusChange(this.status);

        console.log(`[BLE] Connected to: ${this.deviceName}`);
        return this;
    }

    /** Write WiFi credentials and send CONNECT command. */
    async configureWiFi(ssid, password) {
        if (!this.server || !this.server.connected) {
            throw new Error('Device not connected');
        }
        const encoder = new TextEncoder();
        await this.charSSID.writeValue(encoder.encode(ssid));
        await this.charPassword.writeValue(encoder.encode(password));
        await this.charCommand.writeValue(encoder.encode(CMD_CONNECT));
        console.log(`[BLE] Sent WiFi config to ${this.deviceName}: SSID="${ssid}"`);
    }

    /** Send DISCONNECT command. */
    async disconnectWiFi() {
        if (!this.server || !this.server.connected) return;
        const encoder = new TextEncoder();
        await this.charCommand.writeValue(encoder.encode(CMD_DISCONNECT));
    }

    /** Send CLEAR command to wipe stored credentials. */
    async clearCredentials() {
        if (!this.server || !this.server.connected) return;
        const encoder = new TextEncoder();
        await this.charCommand.writeValue(encoder.encode(CMD_CLEAR));
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
