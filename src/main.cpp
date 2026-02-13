// =============================================================================
// Wave Visualizer — ESP32-S3 Firmware
// =============================================================================
//
// Reads audio from an INMP441 I2S MEMS microphone, displays a real-time
// waveform on a 240x240 ST7789 SPI TFT (only when key A is pressed), and
// POSTs recorded audio to a REST endpoint when A is released.
//
// Hardware:
//   - YD-ESP32-23 (ESP32-S3-N16R8)
//   - INMP441 microphone       (I2S — GPIO 4/5/6)
//   - ST7789 240x240 IPS TFT   (SPI — GPIO 7/8/9/10/11/12)
//   - 4x4 matrix keypad       (GPIO 13–20)
//
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <WizardozConnect.h>

#include "pin_config.h"
#include "button_config.h"
#include "tft/display.h"
#include "tft/status_icons.h"
#include "tft/bar_visualizer.h"
#include "keypad/keypad.h"
#include "audio/recorder.h"
#include "http/audio_client.h"

// =============================================================================
// Audio / I2S Constants
// =============================================================================

static const int DMA_BUF_COUNT = 4;
static const int DMA_BUF_LEN = 256;   // samples per DMA buffer
static const int AUDIO_BUF_LEN = 512; // samples per read cycle

// Timing
static const uint32_t TFT_UPDATE_MS = 50; // ~20 fps display refresh

// =============================================================================
// Globals
// =============================================================================

WizardozConnect connector("Wizardoz-Wave");

int16_t audioBuf[AUDIO_BUF_LEN];

String serverHost = "";
uint16_t serverPort = WS_SERVER_PORT;

// State tracking for icons to avoid unnecessary redraws
bool prevWiFiState = false;
bool currWiFiState = false;
bool prevBLEState = false;
bool currBLEState = false;

unsigned long lastTFTUpdate = 0;

// Keypad "A" — push-to-talk recording
static bool aWasPressed = false;

// Runtime config from GET /api/button-config (fallback to button_config.h if fetch fails)
static String configResponseKeyA;
static String configContentTypeA;

// =============================================================================
// I2S Setup
// =============================================================================

void initI2S()
{
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_MIC_CHANNEL,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num = I2S_MIC_BCLK,
        .ws_io_num = I2S_MIC_LRCK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_DATA,
    };

    esp_err_t err;
    err = i2s_driver_install(I2S_MIC_PORT, &i2sConfig, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] Driver install failed: %d\n", err);
    }
    err = i2s_set_pin(I2S_MIC_PORT, &pinConfig);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] Set pin failed: %d\n", err);
    }
    i2s_zero_dma_buffer(I2S_MIC_PORT);
    Serial.println("[I2S] Microphone initialised");
    Serial0.println("[I2S] Microphone initialised");
}

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

    // Use server host from BLE config if set, else gateway (router IP)
    String fromConnector = connector.getServerHost();
    if (fromConnector.length() > 0)
    {
        serverHost = fromConnector;
        Serial.printf("[Main] Server host (from config): %s\n", serverHost.c_str());
    }
    else
    {
        IPAddress gw = WiFi.gatewayIP();
        serverHost = gw.toString();
        Serial.printf("[Main] Server host (gateway): %s\n", serverHost.c_str());
    }

    audioClientSetBaseUrl(serverHost, serverPort);

    if (audioClientFetchConfig(configResponseKeyA, configContentTypeA))
    {
        Serial.println("[Main] Button config loaded from server");
    }
    else
    {
        configResponseKeyA = "";
        configContentTypeA = "";
        Serial.println("[Main] Using compile-time button config");
    }
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
}

// =============================================================================
// Setup
// =============================================================================

void setup()
{
    Serial.begin(115200);  // USB CDC (OTG port)
    Serial0.begin(115200); // UART0 (FTDI/COM port)
    delay(2000);           // give USB CDC time to enumerate
    Serial.println("\n=== Wizardoz Wave Visualizer ===");
    Serial0.println("\n=== Wizardoz Wave Visualizer ===");

    initDisplay();
    initI2S();
    keypadInit();

    connector.onWiFiReady(onWiFiReady);
    connector.onWiFiLost(onWiFiLost);
    connector.onBLEClient(onBLEClient);
    connector.begin();

    Serial.println("[Main] Setup complete — waiting for BLE provisioning or saved WiFi");
    Serial0.println("[Main] Setup complete — waiting for BLE provisioning or saved WiFi");
}

// =============================================================================
// Loop
// =============================================================================

void loop()
{
    connector.loop();

    // --- Keypad scan and "A" push-to-talk ----------------------------------
    keypadScan();
    bool aPressed = keypadIsKeyPressed('A');

    if (aPressed && !aWasPressed)
    {
        recorderStart();
        drawNotification("Recording...");
    }
    else if (!aPressed && aWasPressed)
    {
        clearBars();
        recorderStop();
        size_t wavSize = recorderGetWavSize();
        const uint8_t *wavBuf = recorderGetWavBuffer();

        if (wavSize > 0 && connector.isWiFiConnected())
        {
            drawNotification("Sending...");
            String outText;
            const char *key = configResponseKeyA.length() ? configResponseKeyA.c_str() : BUTTON_A_RESPONSE_KEY;
            const char *ct = configContentTypeA.length() ? configContentTypeA.c_str() : BUTTON_A_CONTENT_TYPE;
            bool ok = audioClientPost("A", key, ct, wavBuf, wavSize, outText);
            if (ok && outText.length() > 0)
            {
                drawNotification(outText.c_str());
            }
            else if (!ok)
            {
                drawNotification("Error: request failed");
            }
        }
        else if (!connector.isWiFiConnected())
        {
            drawNotification("Error: no WiFi");
        }
        else if (wavSize == 0)
        {
            drawNotification("Error: no audio");
        }
    }
    aWasPressed = aPressed;

    // --- Feed recorder while holding A -------------------------------------
    if (recorderIsRecording())
    {
        // Samples fed below after i2s_read
    }

    // --- Read audio from INMP441 -------------------------------------------
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(
        I2S_MIC_PORT,
        audioBuf,
        sizeof(audioBuf),
        &bytesRead,
        portMAX_DELAY);

    int samplesRead = 0;
    if (err == ESP_OK && bytesRead > 0)
    {
        samplesRead = bytesRead / sizeof(int16_t);
    }

    // --- Feed recorder if recording -----------------------------------------
    if (recorderIsRecording() && samplesRead > 0)
    {
        recorderFeedSamples(audioBuf, samplesRead);
    }

    // --- Update TFT waveform (only when recording) -------------------------
    if (recorderIsRecording() && samplesRead > 0)
    {
        computeBars(audioBuf, samplesRead);

        unsigned long now = millis();
        if (now - lastTFTUpdate >= TFT_UPDATE_MS)
        {
            lastTFTUpdate = now;
            renderBars();
        }
    }
}
