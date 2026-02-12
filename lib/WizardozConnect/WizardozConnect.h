#pragma once
// =============================================================================
// WizardozConnect — BLE WiFi Provisioning Library for ESP32
// =============================================================================
//
// A modular, reusable library that lets a Web Bluetooth browser page configure
// WiFi credentials on any ESP32-S3 device.  Credentials are persisted in NVS
// so the device reconnects automatically on reboot.
//
// Usage:
//   #include <WizardozConnect.h>
//
//   WizardozConnect connector("MyDevice");
//   connector.onWiFiReady([](const String& ip) { Serial.println(ip); });
//   connector.begin();           // call once in setup()
//   connector.loop();            // call every iteration of loop()
//
// =============================================================================

#include <Arduino.h>
#include <functional>

// ---------------------------------------------------------------------------
// BLE UUIDs  (custom, generated once — keep in sync with ble-connect.js)
// ---------------------------------------------------------------------------
#define WIZARDOZ_SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WIZARDOZ_CHAR_SSID_UUID         "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define WIZARDOZ_CHAR_PASSWORD_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define WIZARDOZ_CHAR_COMMAND_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define WIZARDOZ_CHAR_STATUS_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define WIZARDOZ_CHAR_DEVICE_NAME_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26ac"

// Commands written to the Command characteristic
#define CMD_CONNECT     "CONNECT"
#define CMD_DISCONNECT  "DISCONNECT"
#define CMD_CLEAR       "CLEAR"

// Status strings sent via the Status characteristic
#define STATUS_IDLE             "IDLE"
#define STATUS_CONNECTING       "CONNECTING"
#define STATUS_CONNECTED        "CONNECTED"
#define STATUS_FAILED           "FAILED"
#define STATUS_DISCONNECTED     "DISCONNECTED"

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------
using WiFiReadyCallback   = std::function<void(const String& localIP)>;
using WiFiLostCallback    = std::function<void()>;
using BLEClientCallback   = std::function<void(bool connected)>;

// ---------------------------------------------------------------------------
// WizardozConnect class
// ---------------------------------------------------------------------------
class WizardozConnect {
public:
    /// @param deviceName  Friendly name advertised over BLE and shown in the
    ///                    browser device picker.
    explicit WizardozConnect(const String& deviceName = "Wizardoz");

    // -- Lifecycle -----------------------------------------------------------
    /// Initialise BLE, WiFi and NVS.  Call once in setup().
    void begin();

    /// Poll WiFi state & BLE events.  Call every loop() iteration.
    void loop();

    // -- Callbacks -----------------------------------------------------------
    void onWiFiReady(WiFiReadyCallback cb)    { _onWiFiReady = cb; }
    void onWiFiLost(WiFiLostCallback cb)      { _onWiFiLost  = cb; }
    void onBLEClient(BLEClientCallback cb)    { _onBLEClient = cb; }

    // -- State queries -------------------------------------------------------
    bool isWiFiConnected() const;
    String getLocalIP() const;
    String getDeviceName() const { return _deviceName; }

private:
    // -- Internal helpers ----------------------------------------------------
    void _initBLE();
    void _initWiFiFromNVS();
    void _connectWiFi(const String& ssid, const String& password);
    void _disconnectWiFi();
    void _clearCredentials();
    void _updateStatus(const char* status);
    void _updateStatusWithIP(const char* status, const String& ip);

    // -- BLE characteristic write handlers (invoked via friend callbacks) -----
    void _handleSSIDWrite(const String& value);
    void _handlePasswordWrite(const String& value);
    void _handleCommandWrite(const String& value);

    // -- Data ----------------------------------------------------------------
    String _deviceName;
    String _pendingSSID;
    String _pendingPassword;

    bool   _wifiWasConnected = false;
    unsigned long _wifiConnectStart = 0;
    bool   _wifiConnecting = false;
    static const unsigned long WIFI_TIMEOUT_MS = 15000;

    // -- Callbacks -----------------------------------------------------------
    WiFiReadyCallback  _onWiFiReady  = nullptr;
    WiFiLostCallback   _onWiFiLost   = nullptr;
    BLEClientCallback  _onBLEClient  = nullptr;

    // Allow the NimBLE callback classes to reach private handlers.
    friend class _WCServerCallbacks;
    friend class _WCSSIDCallback;
    friend class _WCPasswordCallback;
    friend class _WCCommandCallback;
};
