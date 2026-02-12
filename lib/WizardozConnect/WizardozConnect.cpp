// =============================================================================
// WizardozConnect — Implementation
// =============================================================================

#include "WizardozConnect.h"

#include <WiFi.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

// ---------------------------------------------------------------------------
// NVS namespace & keys
// ---------------------------------------------------------------------------
static const char *NVS_NAMESPACE = "wz_wifi";
static const char *NVS_KEY_SSID = "ssid";
static const char *NVS_KEY_PASS = "pass";

// ---------------------------------------------------------------------------
// Forward-declare BLE callback classes
// ---------------------------------------------------------------------------
static NimBLECharacteristic *g_charStatus = nullptr;
static NimBLECharacteristic *g_charDevName = nullptr;

// ============================================================================
// BLE Callback Classes
// ============================================================================

class _WCServerCallbacks : public NimBLEServerCallbacks
{
public:
    explicit _WCServerCallbacks(WizardozConnect *owner) : _owner(owner) {}

    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        Serial.println("[WC] BLE client connected");
        // Allow multiple connections
        NimBLEDevice::startAdvertising();
        if (_owner->_onBLEClient)
            _owner->_onBLEClient(true);
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        Serial.println("[WC] BLE client disconnected");
        NimBLEDevice::startAdvertising();
        if (_owner->_onBLEClient)
            _owner->_onBLEClient(false);
    }

private:
    WizardozConnect *_owner;
};

class _WCSSIDCallback : public NimBLECharacteristicCallbacks
{
public:
    explicit _WCSSIDCallback(WizardozConnect *owner) : _owner(owner) {}
    void onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo) override
    {
        _owner->_handleSSIDWrite(String(pChar->getValue().c_str()));
    }

private:
    WizardozConnect *_owner;
};

class _WCPasswordCallback : public NimBLECharacteristicCallbacks
{
public:
    explicit _WCPasswordCallback(WizardozConnect *owner) : _owner(owner) {}
    void onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo) override
    {
        _owner->_handlePasswordWrite(String(pChar->getValue().c_str()));
    }

private:
    WizardozConnect *_owner;
};

class _WCCommandCallback : public NimBLECharacteristicCallbacks
{
public:
    explicit _WCCommandCallback(WizardozConnect *owner) : _owner(owner) {}
    void onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo) override
    {
        _owner->_handleCommandWrite(String(pChar->getValue().c_str()));
    }

private:
    WizardozConnect *_owner;
};

// ============================================================================
// Constructor
// ============================================================================

WizardozConnect::WizardozConnect(const String &deviceName)
    : _deviceName(deviceName) {}

// ============================================================================
// Public — Lifecycle
// ============================================================================

void WizardozConnect::begin()
{
    Serial.println("[WC] Initialising WizardozConnect...");

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    _initBLE();
    _initWiFiFromNVS();
}

void WizardozConnect::loop()
{
    // --- Monitor WiFi connection attempts -----------------------------------
    if (_wifiConnecting)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            _wifiConnecting = false;
            String ip = WiFi.localIP().toString();
            Serial.printf("[WC] WiFi connected — IP: %s\n", ip.c_str());
            _updateStatusWithIP(STATUS_CONNECTED, ip);
            _wifiWasConnected = true;
            if (_onWiFiReady)
                _onWiFiReady(ip);
        }
        else if (millis() - _wifiConnectStart > WIFI_TIMEOUT_MS)
        {
            _wifiConnecting = false;
            Serial.println("[WC] WiFi connection timed out");
            _updateStatus(STATUS_FAILED);
        }
        return;
    }

    // --- Detect WiFi drop ---------------------------------------------------
    if (_wifiWasConnected && WiFi.status() != WL_CONNECTED)
    {
        _wifiWasConnected = false;
        Serial.println("[WC] WiFi connection lost");
        _updateStatus(STATUS_DISCONNECTED);
        if (_onWiFiLost)
            _onWiFiLost();
    }
}

// ============================================================================
// Public — State queries
// ============================================================================

bool WizardozConnect::isWiFiConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

String WizardozConnect::getLocalIP() const
{
    return WiFi.localIP().toString();
}

// ============================================================================
// Private — BLE initialisation
// ============================================================================

void WizardozConnect::_initBLE()
{
    NimBLEDevice::init(_deviceName.c_str());
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new _WCServerCallbacks(this));

    NimBLEService *pService = pServer->createService(WIZARDOZ_SERVICE_UUID);

    // SSID (write)
    NimBLECharacteristic *charSSID = pService->createCharacteristic(
        WIZARDOZ_CHAR_SSID_UUID,
        NIMBLE_PROPERTY::WRITE);
    charSSID->setCallbacks(new _WCSSIDCallback(this));

    // Password (write)
    NimBLECharacteristic *charPass = pService->createCharacteristic(
        WIZARDOZ_CHAR_PASSWORD_UUID,
        NIMBLE_PROPERTY::WRITE);
    charPass->setCallbacks(new _WCPasswordCallback(this));

    // Command (write)
    NimBLECharacteristic *charCmd = pService->createCharacteristic(
        WIZARDOZ_CHAR_COMMAND_UUID,
        NIMBLE_PROPERTY::WRITE);
    charCmd->setCallbacks(new _WCCommandCallback(this));

    // Status (read + notify)
    g_charStatus = pService->createCharacteristic(
        WIZARDOZ_CHAR_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    g_charStatus->setValue(STATUS_IDLE);

    // Device name (read)
    g_charDevName = pService->createCharacteristic(
        WIZARDOZ_CHAR_DEVICE_NAME_UUID,
        NIMBLE_PROPERTY::READ);
    g_charDevName->setValue(_deviceName.c_str());

    pService->start();

    // Advertising
    NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(WIZARDOZ_SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->start();

    Serial.printf("[WC] BLE advertising as \"%s\"\n", _deviceName.c_str());
}

// ============================================================================
// Private — WiFi from NVS
// ============================================================================

void WizardozConnect::_initWiFiFromNVS()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true); // read-only

    String ssid = prefs.getString(NVS_KEY_SSID, "");
    String pass = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();

    if (ssid.length() > 0)
    {
        Serial.printf("[WC] Found saved WiFi: \"%s\" — connecting...\n", ssid.c_str());
        _connectWiFi(ssid, pass);
    }
    else
    {
        Serial.println("[WC] No saved WiFi credentials");
    }
}

// ============================================================================
// Private — WiFi actions
// ============================================================================

void WizardozConnect::_connectWiFi(const String &ssid, const String &password)
{
    _updateStatus(STATUS_CONNECTING);
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());
    _wifiConnecting = true;
    _wifiConnectStart = millis();

    // Persist credentials
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(NVS_KEY_SSID, ssid);
    prefs.putString(NVS_KEY_PASS, password);
    prefs.end();

    Serial.printf("[WC] Attempting WiFi: \"%s\"\n", ssid.c_str());
}

void WizardozConnect::_disconnectWiFi()
{
    WiFi.disconnect(true);
    _wifiConnecting = false;
    _wifiWasConnected = false;
    _updateStatus(STATUS_DISCONNECTED);
    Serial.println("[WC] WiFi disconnected");
}

void WizardozConnect::_clearCredentials()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    _disconnectWiFi();
    _updateStatus(STATUS_IDLE);
    Serial.println("[WC] Credentials cleared");
}

// ============================================================================
// Private — Status helpers
// ============================================================================

void WizardozConnect::_updateStatus(const char *status)
{
    if (g_charStatus)
    {
        g_charStatus->setValue(status);
        g_charStatus->notify();
    }
}

void WizardozConnect::_updateStatusWithIP(const char *status, const String &ip)
{
    if (g_charStatus)
    {
        String payload = String(status) + ":" + ip;
        g_charStatus->setValue(payload.c_str());
        g_charStatus->notify();
    }
}

// ============================================================================
// Private — BLE write handlers
// ============================================================================

void WizardozConnect::_handleSSIDWrite(const String &value)
{
    _pendingSSID = value;
    Serial.printf("[WC] SSID received: \"%s\"\n", value.c_str());
}

void WizardozConnect::_handlePasswordWrite(const String &value)
{
    _pendingPassword = value;
    Serial.println("[WC] Password received (hidden)");
}

void WizardozConnect::_handleCommandWrite(const String &value)
{
    Serial.printf("[WC] Command: %s\n", value.c_str());

    if (value == CMD_CONNECT)
    {
        if (_pendingSSID.length() > 0)
        {
            _connectWiFi(_pendingSSID, _pendingPassword);
        }
        else
        {
            Serial.println("[WC] Cannot connect — no SSID provided");
            _updateStatus(STATUS_FAILED);
        }
    }
    else if (value == CMD_DISCONNECT)
    {
        _disconnectWiFi();
    }
    else if (value == CMD_CLEAR)
    {
        _clearCredentials();
    }
    else
    {
        Serial.printf("[WC] Unknown command: %s\n", value.c_str());
    }
}
