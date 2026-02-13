// =============================================================================
// TFT Display — initialisation and notification text
// =============================================================================

#include <Arduino.h>

#include "pin_config.h"
#include "tft_config.h"
#include "display.h"
#include "status_icons.h"

TFT_eSPI tft = TFT_eSPI();

void initDisplay()
{
    tft.init();
    tft.setRotation(0); // 0 = portrait, connector at bottom
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

void drawNotification(const char *text)
{
    // Clear notification zone
    tft.fillRect(0, NOTIFY_Y, TFT_WIDTH, NOTIFY_H, COL_BG);

    tft.setTextColor(COL_NOTIFY, COL_BG);
    tft.setTextDatum(MC_DATUM); // middle-centre
    tft.setTextSize(2);
    tft.drawString(text, TFT_WIDTH / 2, NOTIFY_Y + NOTIFY_H / 2);
}
