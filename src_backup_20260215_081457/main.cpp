// =============================================================================
// ESP32-S3 Base Firmware — BLE WiFi Provisioning + TFT Status Display
// =============================================================================
//
// A minimal starting point for ESP32-S3 projects that provides:
//   - BLE-based WiFi provisioning (via WizardozConnect library)
//   - TFT display with WiFi/BT status icons
//   - Foundation for building custom applications
//
// Hardware:
//   - YD-ESP32-23 (ESP32-S3-N16R8)
//   - ST7789 240x240 IPS TFT (SPI)
//
// Features:
//   - BLE advertising for Web Bluetooth provisioning
//   - WiFi connection with credentials stored in NVS
//   - Visual status feedback on TFT (WiFi + Bluetooth icons)
//
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WizardozConnect.h>

#include "tft/display.h"
#include "tft/status_icons.h"

// =============================================================================
// Configuration
// =============================================================================

// BLE device name (shown in Web Bluetooth picker)
static const char *BLE_DEVICE_NAME = "Wizardoz-Device";

// =============================================================================
// Globals
// =============================================================================

WizardozConnect connector(BLE_DEVICE_NAME);

// State tracking for icons to avoid unnecessary redraws
bool currWiFiState = false;
bool currBLEState = false;

// =============================================================================
// WiFi ready / lost callbacks
// =============================================================================

void onWiFiReady(const String &ip)
{
    Serial.printf("[Main] WiFi ready - IP: %s\n", ip.c_str());

    currWiFiState = true;
    drawWiFiIcon(true);

    String msg = "WiFi OK " + ip.substring(ip.lastIndexOf('.') + 1);
    drawNotification(msg.c_str());

    Serial.printf("[Main] Connected to: %s\n", WiFi.SSID().c_str());
    delay(500);
}

void onWiFiLost()
{
    Serial.println("[Main] WiFi lost");

    currWiFiState = false;
    drawWiFiIcon(false);
    drawNotification("WiFi lost...");
}

// =============================================================================
// BLE client callback — update Bluetooth icon
// =============================================================================

void onBLEClient(bool connected)
{
    currBLEState = connected;
    drawBTIcon(connected);
    
    if (connected)
    {
        Serial.println("[Main] BLE client connected");
        drawNotification("BLE connected");
    }
    else
    {
        Serial.println("[Main] BLE client disconnected");
        if (!currWiFiState)
        {
            drawNotification("Waiting for BLE");
        }
    }
}

// =============================================================================
// Setup
// =============================================================================

void setup()
{
    Serial.begin(115200);  // USB CDC (OTG port)
    Serial0.begin(115200); // UART0 (FTDI/COM port)
    delay(2000);           // give USB CDC time to enumerate
    
    Serial.println("\n=== Wizardoz Base Firmware ===");
    Serial.println("BLE WiFi Provisioning + TFT Status Display");
    Serial0.println("\n=== Wizardoz Base Firmware ===");

    // Initialize TFT display
    initDisplay();

    // Set up WizardozConnect callbacks
    connector.onWiFiReady(onWiFiReady);
    connector.onWiFiLost(onWiFiLost);
    connector.onBLEClient(onBLEClient);
    
    // Start BLE advertising and WiFi management
    connector.begin();

    Serial.println("[Main] Setup complete — waiting for BLE provisioning or saved WiFi");
    Serial0.println("[Main] Setup complete — waiting for BLE provisioning or saved WiFi");
}

// =============================================================================
// Loop
// =============================================================================

void loop()
{
    // Process BLE events and WiFi state machine
    connector.loop();

    // Add your application logic here
    // Examples:
    // - Read sensors
    // - Control actuators  
    // - HTTP requests to backend
    // - MQTT publishing
    // - etc.
    
    delay(10); // Small delay to prevent watchdog issues
}
