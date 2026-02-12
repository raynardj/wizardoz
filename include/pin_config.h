#pragma once

// =============================================================================
// Pin Configuration for Wave Visualizer (YD-ESP32-23 / ESP32-S3-N16R8)
// =============================================================================

// --- I2S Microphone (INMP441) -----------------------------------------------
// Protocol: I2S (receive mode)
#define I2S_MIC_PORT        I2S_NUM_0
#define I2S_MIC_BCLK        4   // GPIO 4  -> SCK / BCLK
#define I2S_MIC_LRCK        5   // GPIO 5  -> WS  / LRCK
#define I2S_MIC_DATA        6   // GPIO 6  -> SD  (serial data out from mic)

#define I2S_MIC_SAMPLE_RATE 16000   // 16 kHz
#define I2S_MIC_SAMPLE_BITS 16      // 16-bit samples (INMP441 is 24-bit, we truncate)
#define I2S_MIC_CHANNEL     I2S_CHANNEL_FMT_ONLY_LEFT  // L/R pin tied to GND

// --- I2C LCD 1602 -----------------------------------------------------------
// Protocol: I2C via PCF8574T backpack
#define LCD_SDA             8       // GPIO 8  -> SDA
#define LCD_SCL             9       // GPIO 9  -> SCL
#define LCD_I2C_ADDR        0x27    // Default address (all jumpers open)
#define LCD_COLS            16
#define LCD_ROWS            2

// --- Onboard RGB LED (WS2812) -----------------------------------------------
#define LED_RGB_PIN         48

// --- WebSocket Configuration ------------------------------------------------
#define WS_SERVER_PORT      8000
#define WS_AUDIO_PATH       "/ws/audio/"
