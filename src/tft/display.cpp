// =============================================================================
// TFT Display — initialisation and notification text
// =============================================================================

#include <Arduino.h>

#include "pin_config.h"
#include "tft_config.h"
#include "display.h"
#include "status_icons.h"
#include "fonts/chinese_16.h" // PROGMEM smooth font (Latin + Chinese)

TFT_eSPI tft = TFT_eSPI();

void initDisplay()
{
    tft.init();
    tft.setRotation(0); // 0 = portrait, connector at bottom
    tft.fillScreen(COL_BG);

    // Backlight ON
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Load anti-aliased Unicode font from flash (supports Chinese + Latin)
    tft.loadFont(chinese_16);
    Serial.println("[TFT] Unicode font loaded from flash");
    Serial0.println("[TFT] Unicode font loaded from flash");

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

    // Clip to notification zone; pass UTF-8 directly (smooth font handles it)
    tft.setViewport(0, NOTIFY_Y, TFT_WIDTH, NOTIFY_H, true);
    tft.setTextWrap(true, false);
    tft.setTextColor(COL_NOTIFY, COL_BG);

    // Check for newline — if present, draw two centred lines
    String s(text);
    int nl = s.indexOf('\n');
    if (nl >= 0)
    {
        String line1 = s.substring(0, nl);
        String line2 = s.substring(nl + 1);
        tft.setTextDatum(BC_DATUM); // bottom-centre for line 1
        tft.drawString(line1.c_str(), TFT_WIDTH / 2, NOTIFY_H / 2 - 1);
        tft.setTextDatum(TC_DATUM); // top-centre for line 2
        tft.drawString(line2.c_str(), TFT_WIDTH / 2, NOTIFY_H / 2 + 1);
    }
    else
    {
        tft.setTextDatum(MC_DATUM);
        tft.drawString(text, TFT_WIDTH / 2, NOTIFY_H / 2);
    }

    tft.resetViewport();
}
