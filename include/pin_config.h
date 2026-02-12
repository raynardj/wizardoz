#pragma once

// =============================================================================
// Pin Configuration for Wave Visualizer (YD-ESP32-23 / ESP32-S3-N16R8)
// =============================================================================

// --- I2S Microphone (INMP441) -----------------------------------------------
// Protocol: I2S (receive mode)
#define I2S_MIC_PORT I2S_NUM_0
#define I2S_MIC_BCLK 4 // GPIO 4  -> SCK / BCLK
#define I2S_MIC_LRCK 5 // GPIO 5  -> WS  / LRCK
#define I2S_MIC_DATA 6 // GPIO 6  -> SD  (serial data out from mic)

#define I2S_MIC_SAMPLE_RATE 16000                 // 16 kHz
#define I2S_MIC_SAMPLE_BITS 16                    // 16-bit samples (INMP441 is 24-bit, we truncate)
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT // L/R pin tied to GND

// --- SPI TFT Display (ST7789 240x240) ----------------------------------------
// Protocol: SPI + control lines
#define TFT_BL_PIN  7    // GPIO 7  -> BLK (backlight, active high)
#define TFT_DC_PIN  8    // GPIO 8  -> DC  (data/command select)
#define TFT_RST_PIN 9    // GPIO 9  -> RES (hardware reset, active low)
#define TFT_CS_PIN  10   // GPIO 10 -> CS  (chip select, active low)
#define TFT_MOSI_PIN 11  // GPIO 11 -> SDA (SPI MOSI)
#define TFT_SCLK_PIN 12  // GPIO 12 -> SCL (SPI clock)
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// --- Onboard RGB LED (WS2812) -----------------------------------------------
#define LED_RGB_PIN 48

// --- WebSocket Configuration ------------------------------------------------
#define WS_SERVER_PORT 8000
#define WS_AUDIO_PATH "/ws/audio/"
