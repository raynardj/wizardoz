// =============================================================================
// Wave Visualizer — ESP32-S3 Firmware
// =============================================================================
//
// Reads audio from an INMP441 I2S MEMS microphone, displays a real-time
// waveform on a 240x240 ST7789 SPI TFT with WiFi/BT status icons, and
// streams audio over WiFi (WebSocket) to a FastAPI backend for browser-based
// visualisation.
//
// Hardware:
//   - YD-ESP32-23 (ESP32-S3-N16R8)
//   - INMP441 microphone       (I2S — GPIO 4/5/6)
//   - ST7789 240x240 IPS TFT   (SPI — GPIO 7/8/9/10/11/12)
//
// =============================================================================

#include <Arduino.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoWebsockets.h>
#include <WizardozConnect.h>

#include "pin_config.h"

using namespace websockets;

// =============================================================================
// Layout Constants
// =============================================================================

// Status bar
static const int STATUS_BAR_H     = 24;
static const int ICON_SIZE         = 16;
static const int ICON_MARGIN       = 6;

// Notification zone (centred text area between status bar and waveform)
static const int NOTIFY_Y          = 40;
static const int NOTIFY_H          = 50;

// Waveform zone
static const int WAVE_TOP          = 100;
static const int WAVE_BOTTOM       = 235;
static const int WAVE_HEIGHT       = WAVE_BOTTOM - WAVE_TOP;    // 135 px
static const int BAR_WIDTH         = 6;
static const int BAR_GAP           = 2;
static const int BAR_STEP          = BAR_WIDTH + BAR_GAP;       // 8 px per bar
static const int DISPLAY_BAR_COLS  = 28;                        // 28 * 8 = 224 px
static const int WAVE_LEFT         = (TFT_WIDTH - DISPLAY_BAR_COLS * BAR_STEP + BAR_GAP) / 2;

// Audio / I2S
static const int DMA_BUF_COUNT    = 4;
static const int DMA_BUF_LEN      = 256;       // samples per DMA buffer
static const int AUDIO_BUF_LEN    = 512;       // samples per read cycle

// Timing
static const uint32_t TFT_UPDATE_MS = 50;      // ~20 fps display refresh
static const uint32_t WS_SEND_MS    = 50;      // 20 fps WebSocket send

// =============================================================================
// Colours
// =============================================================================

static const uint16_t COL_BG         = TFT_BLACK;
static const uint16_t COL_BAR_LOW    = TFT_GREEN;
static const uint16_t COL_BAR_MID    = TFT_YELLOW;
static const uint16_t COL_BAR_HIGH   = TFT_RED;
static const uint16_t COL_ICON_ON    = TFT_CYAN;
static const uint16_t COL_ICON_OFF   = 0x4208;   // dark grey
static const uint16_t COL_NOTIFY     = TFT_WHITE;

// =============================================================================
// Globals
// =============================================================================

TFT_eSPI tft = TFT_eSPI();
WizardozConnect connector("Wizardoz-Wave");
WebsocketsClient wsClient;

int16_t audioBuf[AUDIO_BUF_LEN];
int barHeights[DISPLAY_BAR_COLS] = {0};

bool wsConnected  = false;
String serverHost = "";
uint16_t serverPort = WS_SERVER_PORT;

// State tracking for icons to avoid unnecessary redraws
bool prevWiFiState = false;
bool currWiFiState = false;
bool prevBLEState  = false;
bool currBLEState  = false;

unsigned long lastTFTUpdate = 0;
unsigned long lastWSSend    = 0;

// =============================================================================
// I2S Setup
// =============================================================================

void initI2S()
{
    i2s_config_t i2sConfig = {
        .mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate       = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample   = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format    = I2S_MIC_CHANNEL,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags  = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count     = DMA_BUF_COUNT,
        .dma_buf_len       = DMA_BUF_LEN,
        .use_apll          = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk        = 0,
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num   = I2S_MIC_BCLK,
        .ws_io_num    = I2S_MIC_LRCK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_MIC_DATA,
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
// TFT Display — Initialisation
// =============================================================================

void drawWiFiIcon(bool connected);
void drawBTIcon(bool clientConnected);
void drawNotification(const char *text);

void initDisplay()
{
    tft.init();
    tft.setRotation(0);          // 0 = portrait, connector at bottom
    tft.fillScreen(COL_BG);

    // Backlight ON
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Draw initial UI
    drawWiFiIcon(false);
    drawBTIcon(false);
    drawNotification("Waiting for BLE");

    Serial.println("[TFT] Display initialised (ST7789 240x240)");
    Serial0.println("[TFT] Display initialised (ST7789 240x240)");
}

// =============================================================================
// TFT Display — WiFi Icon (top-left corner)
// =============================================================================
//
// Draws three concentric arcs to represent a WiFi signal fan.
// Filled arcs when connected, hollow outline when disconnected.

void drawWiFiIcon(bool connected)
{
    int cx = ICON_MARGIN + ICON_SIZE / 2;       // centre X
    int cy = ICON_MARGIN + ICON_SIZE;            // base Y of fan
    uint16_t col = connected ? COL_ICON_ON : COL_ICON_OFF;

    // Clear icon area
    tft.fillRect(ICON_MARGIN, ICON_MARGIN, ICON_SIZE + 4, ICON_SIZE + 4, COL_BG);

    // Centre dot
    tft.fillCircle(cx, cy, 2, col);

    // Arc radii (inner to outer)
    int radii[] = {6, 10, 14};
    for (int i = 0; i < 3; i++)
    {
        int r = radii[i];
        // Draw an arc from roughly -45 to +45 degrees above centre
        // by plotting a circle and masking to the upper-left quadrant fan
        for (int a = -45; a <= 45; a++)
        {
            float rad = a * PI / 180.0f;
            int px = cx + (int)(r * sin(rad));
            int py = cy - (int)(r * cos(rad));
            if (connected)
            {
                tft.drawPixel(px, py, col);
                tft.drawPixel(px, py - 1, col);   // thicken
            }
            else
            {
                tft.drawPixel(px, py, col);
            }
        }
    }
}

// =============================================================================
// TFT Display — Bluetooth Icon (top-right corner)
// =============================================================================
//
// Draws a simplified Bluetooth rune (the ᛒ shape).

void drawBTIcon(bool clientConnected)
{
    int x0 = TFT_WIDTH - ICON_MARGIN - ICON_SIZE; // left edge of icon area
    int y0 = ICON_MARGIN;
    uint16_t col = clientConnected ? COL_ICON_ON : COL_ICON_OFF;

    // Clear icon area
    tft.fillRect(x0 - 2, y0, ICON_SIZE + 4, ICON_SIZE + 4, COL_BG);

    int cx = x0 + ICON_SIZE / 2;
    int top = y0 + 1;
    int bot = y0 + ICON_SIZE - 1;
    int mid = (top + bot) / 2;
    int right = cx + 5;
    int left  = cx - 5;

    // Vertical line
    tft.drawLine(cx, top, cx, bot, col);

    // Top-right arrow: from centre-top to right-mid
    tft.drawLine(cx, top, right, mid, col);
    // Bottom-right arrow: from right-mid to centre-bottom
    tft.drawLine(right, mid, cx, bot, col);

    // Cross lines
    tft.drawLine(left, top + 3, right, bot - 3, col);
    tft.drawLine(left, bot - 3, right, top + 3, col);
}

// =============================================================================
// TFT Display — Notification Text (centred)
// =============================================================================

void drawNotification(const char *text)
{
    // Clear notification zone
    tft.fillRect(0, NOTIFY_Y, TFT_WIDTH, NOTIFY_H, COL_BG);

    tft.setTextColor(COL_NOTIFY, COL_BG);
    tft.setTextDatum(MC_DATUM);  // middle-centre
    tft.setTextSize(2);
    tft.drawString(text, TFT_WIDTH / 2, NOTIFY_Y + NOTIFY_H / 2);
}

// =============================================================================
// TFT Display — Waveform Bar Graph
// =============================================================================

static uint16_t barColor(int height, int maxH)
{
    // Green for low, yellow for mid, red for high
    int pct = (height * 100) / maxH;
    if (pct > 80) return COL_BAR_HIGH;
    if (pct > 50) return COL_BAR_MID;
    return COL_BAR_LOW;
}

void renderBars()
{
    for (int col = 0; col < DISPLAY_BAR_COLS; col++)
    {
        int h = barHeights[col]; // 0 .. WAVE_HEIGHT
        if (h < 0) h = 0;
        if (h > WAVE_HEIGHT) h = WAVE_HEIGHT;

        int x = WAVE_LEFT + col * BAR_STEP;

        // Clear the bar column first (draw background)
        if (h < WAVE_HEIGHT)
        {
            tft.fillRect(x, WAVE_TOP, BAR_WIDTH, WAVE_HEIGHT - h, COL_BG);
        }

        // Draw the filled bar (growing upward from bottom)
        if (h > 0)
        {
            uint16_t col16 = barColor(h, WAVE_HEIGHT);
            tft.fillRect(x, WAVE_BOTTOM - h, BAR_WIDTH, h, col16);
        }
    }
}

// =============================================================================
// Audio Processing — compute amplitude bars from the audio buffer
// =============================================================================

void computeBars(const int16_t *samples, int numSamples)
{
    int samplesPerBar = numSamples / DISPLAY_BAR_COLS;
    if (samplesPerBar < 1)
        samplesPerBar = 1;

    for (int col = 0; col < DISPLAY_BAR_COLS; col++)
    {
        int start = col * samplesPerBar;
        int end   = start + samplesPerBar;
        if (end > numSamples)
            end = numSamples;

        // RMS amplitude for this column
        uint64_t sumSq = 0;
        for (int i = start; i < end; i++)
        {
            int32_t s = samples[i];
            sumSq += (uint64_t)(s * s);
        }
        double rms = sqrt((double)sumSq / (end - start));

        // Map RMS to 0 .. WAVE_HEIGHT  (tune the divisor to taste)
        int height = (int)(rms / 1024.0 * WAVE_HEIGHT);
        if (height > WAVE_HEIGHT)
            height = WAVE_HEIGHT;

        // Smooth: slow decay, fast rise
        if (height >= barHeights[col])
        {
            barHeights[col] = height;
        }
        else
        {
            barHeights[col] = barHeights[col] - 2;
            if (barHeights[col] < 0)
                barHeights[col] = 0;
        }
    }
}

// =============================================================================
// WebSocket
// =============================================================================

void onWSMessage(WebsocketsMessage message)
{
    Serial.printf("[WS] Received: %s\n", message.data().c_str());
}

void onWSEvent(WebsocketsEvent event, String data)
{
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        Serial.println("[WS] Connection opened");
        wsConnected = true;
    }
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        Serial.println("[WS] Connection closed");
        wsConnected = false;
    }
    else if (event == WebsocketsEvent::GotPing)
    {
        wsClient.pong();
    }
}

void connectWebSocket()
{
    if (serverHost.length() == 0)
        return;

    String url = "ws://" + serverHost + ":" + String(serverPort) + WS_AUDIO_PATH + connector.getDeviceName();
    Serial.printf("[WS] Connecting to %s\n", url.c_str());
    wsClient.onMessage(onWSMessage);
    wsClient.onEvent(onWSEvent);
    wsClient.connect(url);
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

    delay(500);
    connectWebSocket();
}

void onWiFiLost()
{
    Serial.println("[Main] WiFi lost");
    wsConnected = false;

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

    // --- Update TFT waveform -----------------------------------------------
    if (samplesRead > 0)
    {
        computeBars(audioBuf, samplesRead);

        unsigned long now = millis();
        if (now - lastTFTUpdate >= TFT_UPDATE_MS)
        {
            lastTFTUpdate = now;
            renderBars();
        }
    }

    // --- Stream audio over WebSocket ---------------------------------------
    if (wsConnected && samplesRead > 0)
    {
        unsigned long now = millis();
        if (now - lastWSSend >= WS_SEND_MS)
        {
            lastWSSend = now;
            // Send raw PCM as binary frame
            wsClient.sendBinary((const char *)audioBuf, bytesRead);
        }
    }

    // --- Maintain WebSocket connection -------------------------------------
    if (wsConnected)
    {
        wsClient.poll();
    }

    // --- Reconnect WebSocket if WiFi up but WS dropped ---------------------
    if (connector.isWiFiConnected() && !wsConnected)
    {
        static unsigned long lastReconnect = 0;
        unsigned long now = millis();
        if (now - lastReconnect > 5000)
        {
            lastReconnect = now;
            connectWebSocket();
        }
    }
}
