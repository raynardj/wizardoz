// =============================================================================
// Wave Visualizer — ESP32-S3 Firmware
// =============================================================================
//
// Reads audio from an INMP441 I2S MEMS microphone, displays a real-time
// bar-graph waveform on a 1602 I2C LCD, and streams audio data over WiFi
// (WebSocket) to a FastAPI backend for browser-based visualisation.
//
// Hardware:
//   - YD-ESP32-23 (ESP32-S3-N16R8)
//   - INMP441 microphone  (I2S — GPIO 4/5/6)
//   - 1602 LCD + PCF8574T (I2C — GPIO 8/9, addr 0x27)
//
// =============================================================================

#include <Arduino.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoWebsockets.h>
#include <WizardozConnect.h>

#include "pin_config.h"

using namespace websockets;

// =============================================================================
// Constants
// =============================================================================

static const int      DMA_BUF_COUNT    = 4;
static const int      DMA_BUF_LEN      = 256;     // samples per DMA buffer
static const int      AUDIO_BUF_LEN    = 512;     // samples per read cycle
static const uint32_t LCD_UPDATE_MS    = 60;       // ~16 fps LCD refresh
static const uint32_t WS_SEND_MS       = 50;       // 20 fps WebSocket send
static const int      LCD_BAR_COLS     = 16;
static const int      BAR_MAX_HEIGHT   = 16;       // 2 rows x 8 pixel rows

// Custom characters for vertical bars (bottom half, 1-8 rows filled)
static byte barChars[8][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},  // 1
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F},  // 2
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F},  // 3
    {0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F},  // 4
    {0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},  // 5
    {0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},  // 6
    {0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},  // 7
    {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},  // 8 (full)
};

// =============================================================================
// Globals
// =============================================================================

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
WizardozConnect   connector("Wizardoz-Wave");
WebsocketsClient  wsClient;

int16_t  audioBuf[AUDIO_BUF_LEN];
int      barHeights[LCD_BAR_COLS] = {0};

bool     wsConnected    = false;
String   serverHost     = "";
uint16_t serverPort     = WS_SERVER_PORT;

unsigned long lastLCDUpdate = 0;
unsigned long lastWSSend    = 0;

// =============================================================================
// I2S Setup
// =============================================================================

void initI2S() {
    i2s_config_t i2sConfig = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_MIC_CHANNEL,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num   = I2S_MIC_BCLK,
        .ws_io_num    = I2S_MIC_LRCK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_MIC_DATA,
    };

    esp_err_t err;
    err = i2s_driver_install(I2S_MIC_PORT, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Driver install failed: %d\n", err);
    }
    err = i2s_set_pin(I2S_MIC_PORT, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Set pin failed: %d\n", err);
    }
    i2s_zero_dma_buffer(I2S_MIC_PORT);
    Serial.println("[I2S] Microphone initialised");
}

// =============================================================================
// LCD Setup
// =============================================================================

void initLCD() {
    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();

    // Register 8 custom bar characters (indices 0-7)
    for (int i = 0; i < 8; i++) {
        lcd.createChar(i, barChars[i]);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wizardoz  Wave");
    lcd.setCursor(0, 1);
    lcd.print("Waiting for BLE");
    Serial.println("[LCD] Display initialised");
}

// =============================================================================
// LCD Bar Graph Rendering
// =============================================================================

void renderBars() {
    for (int col = 0; col < LCD_BAR_COLS; col++) {
        int h = barHeights[col];  // 0 .. 16
        if (h < 0)  h = 0;
        if (h > BAR_MAX_HEIGHT) h = BAR_MAX_HEIGHT;

        // --- Top row (row 0) — represents heights 9..16 ---
        int topH = h > 8 ? h - 8 : 0;
        lcd.setCursor(col, 0);
        if (topH == 0) {
            lcd.write(' ');
        } else {
            lcd.write((uint8_t)(topH - 1));  // char 0..7
        }

        // --- Bottom row (row 1) — represents heights 1..8 ---
        int botH = h > 8 ? 8 : h;
        lcd.setCursor(col, 1);
        if (botH == 0) {
            lcd.write(' ');
        } else {
            lcd.write((uint8_t)(botH - 1));  // char 0..7
        }
    }
}

// =============================================================================
// Audio Processing — compute 16 amplitude bars from the audio buffer
// =============================================================================

void computeBars(const int16_t* samples, int numSamples) {
    int samplesPerBar = numSamples / LCD_BAR_COLS;
    if (samplesPerBar < 1) samplesPerBar = 1;

    for (int col = 0; col < LCD_BAR_COLS; col++) {
        int start = col * samplesPerBar;
        int end   = start + samplesPerBar;
        if (end > numSamples) end = numSamples;

        // RMS amplitude for this column
        uint64_t sumSq = 0;
        for (int i = start; i < end; i++) {
            int32_t s = samples[i];
            sumSq += (uint64_t)(s * s);
        }
        double rms = sqrt((double)sumSq / (end - start));

        // Map RMS to 0..16  (tune the divisor to taste)
        int height = (int)(rms / 1024.0 * BAR_MAX_HEIGHT);
        if (height > BAR_MAX_HEIGHT) height = BAR_MAX_HEIGHT;

        // Smooth: slow decay, fast rise
        if (height >= barHeights[col]) {
            barHeights[col] = height;
        } else {
            barHeights[col] = barHeights[col] - 1;
            if (barHeights[col] < 0) barHeights[col] = 0;
        }
    }
}

// =============================================================================
// WebSocket
// =============================================================================

void onWSMessage(WebsocketsMessage message) {
    Serial.printf("[WS] Received: %s\n", message.data().c_str());
}

void onWSEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("[WS] Connection opened");
        wsConnected = true;
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("[WS] Connection closed");
        wsConnected = false;
    } else if (event == WebsocketsEvent::GotPing) {
        wsClient.pong();
    }
}

void connectWebSocket() {
    if (serverHost.length() == 0) return;

    String url = "ws://" + serverHost + ":" + String(serverPort)
               + WS_AUDIO_PATH + connector.getDeviceName();
    Serial.printf("[WS] Connecting to %s\n", url.c_str());
    wsClient.onMessage(onWSMessage);
    wsClient.onEvent(onWSEvent);
    wsClient.connect(url);
}

// =============================================================================
// WiFi ready / lost callbacks
// =============================================================================

void onWiFiReady(const String& ip) {
    Serial.printf("[Main] WiFi ready — IP: %s\n", ip.c_str());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK ");
    lcd.print(ip.substring(ip.lastIndexOf('.') + 1));  // last octet

    // Derive server IP from gateway (common LAN pattern) — user can override
    // For now, we expect the server URL to be set elsewhere or use gateway.
    // Default: assume server is at the gateway IP.
    IPAddress gw = WiFi.gatewayIP();
    serverHost = gw.toString();
    Serial.printf("[Main] Server host (gateway): %s\n", serverHost.c_str());

    delay(500);
    connectWebSocket();
}

void onWiFiLost() {
    Serial.println("[Main] WiFi lost");
    wsConnected = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi lost...");
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Wizardoz Wave Visualizer ===");

    initLCD();
    initI2S();

    connector.onWiFiReady(onWiFiReady);
    connector.onWiFiLost(onWiFiLost);
    connector.begin();

    Serial.println("[Main] Setup complete — waiting for BLE provisioning or saved WiFi");
}

// =============================================================================
// Loop
// =============================================================================

void loop() {
    connector.loop();

    // --- Read audio from INMP441 -------------------------------------------
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(
        I2S_MIC_PORT,
        audioBuf,
        sizeof(audioBuf),
        &bytesRead,
        portMAX_DELAY
    );

    int samplesRead = 0;
    if (err == ESP_OK && bytesRead > 0) {
        samplesRead = bytesRead / sizeof(int16_t);
    }

    // --- Update LCD bar graph ----------------------------------------------
    if (samplesRead > 0) {
        computeBars(audioBuf, samplesRead);

        unsigned long now = millis();
        if (now - lastLCDUpdate >= LCD_UPDATE_MS) {
            lastLCDUpdate = now;
            renderBars();
        }
    }

    // --- Stream audio over WebSocket ---------------------------------------
    if (wsConnected && samplesRead > 0) {
        unsigned long now = millis();
        if (now - lastWSSend >= WS_SEND_MS) {
            lastWSSend = now;
            // Send raw PCM as binary frame
            wsClient.sendBinary((const char*)audioBuf, bytesRead);
        }
    }

    // --- Maintain WebSocket connection -------------------------------------
    if (wsConnected) {
        wsClient.poll();
    }

    // --- Reconnect WebSocket if WiFi up but WS dropped ---------------------
    if (connector.isWiFiConnected() && !wsConnected) {
        static unsigned long lastReconnect = 0;
        unsigned long now = millis();
        if (now - lastReconnect > 5000) {
            lastReconnect = now;
            connectWebSocket();
        }
    }
}
