// =============================================================================
// TFT Status Icons — WiFi and Bluetooth (top corners)
// =============================================================================
//
// WiFi: three concentric arcs representing a signal fan.
// BT: simplified Bluetooth rune (ᛒ shape).
//
// =============================================================================

#include "tft_config.h"

void drawWiFiIcon(bool connected)
{
    int cx = ICON_MARGIN + ICON_SIZE / 2; // centre X
    int cy = ICON_MARGIN + ICON_SIZE;     // base Y of fan
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
                tft.drawPixel(px, py - 1, col); // thicken
            }
            else
            {
                tft.drawPixel(px, py, col);
            }
        }
    }
}

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
    int left = cx - 5;

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
