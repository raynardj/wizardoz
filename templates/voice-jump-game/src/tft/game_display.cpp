#include "game_display.h"

TFT_eSPI tft = TFT_eSPI();

static int lastDistance = -1;

void initDisplay()
{
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COL_BG);

    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    tft.setTextSize(1);
}

void drawWiFiIcon(bool connected)
{
    int cx = ICON_MARGIN + ICON_SIZE / 2;
    int cy = ICON_MARGIN + ICON_SIZE;
    uint16_t col = connected ? COL_ICON_ON : COL_ICON_OFF;

    tft.fillRect(ICON_MARGIN, ICON_MARGIN, ICON_SIZE + 4, ICON_SIZE + 4, COL_BG);

    tft.fillCircle(cx, cy, 2, col);

    int radii[] = {6, 10, 14};
    for (int i = 0; i < 3; i++)
    {
        int r = radii[i];
        for (int a = -45; a <= 45; a++)
        {
            float rad = a * PI / 180.0f;
            int px = cx + (int)(r * sin(rad));
            int py = cy - (int)(r * cos(rad));
            if (connected)
            {
                tft.drawPixel(px, py, col);
                tft.drawPixel(px, py - 1, col);
            }
            else
            {
                tft.drawPixel(px, py, col);
            }
        }
    }
}

void drawBTIcon(bool connected)
{
    int x0 = TFT_WIDTH - ICON_MARGIN - ICON_SIZE;
    int y0 = ICON_MARGIN;
    uint16_t col = connected ? COL_ICON_ON : COL_ICON_OFF;

    tft.fillRect(x0 - 2, y0, ICON_SIZE + 4, ICON_SIZE + 4, COL_BG);

    int cx = x0 + ICON_SIZE / 2;
    int top = y0 + 1;
    int bot = y0 + ICON_SIZE - 1;
    int mid = (top + bot) / 2;
    int right = cx + 5;
    int left = cx - 5;

    tft.drawLine(cx, top, cx, bot, col);
    tft.drawLine(cx, top, right, mid, col);
    tft.drawLine(right, mid, cx, bot, col);
    tft.drawLine(left, top + 3, right, bot - 3, col);
    tft.drawLine(left, bot - 3, right, top + 3, col);
}

void drawDistance(int distance)
{
    if (distance == lastDistance) return;
    lastDistance = distance;

    int x = ICON_MARGIN;
    int y = ICON_MARGIN + ICON_SIZE + 6;
    int w = 40;
    int h = 12;

    tft.fillRect(x, y, w, h, COL_BG);

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawNumber(distance, x, y);
}

void clearGameArea()
{
    tft.fillRect(0, GAME_AREA_TOP, TFT_WIDTH, GAME_AREA_HEIGHT, COL_BG);
}

void drawPrincess(int x, int y, bool sad)
{
    uint16_t dressColor = sad ? COL_SAD : COL_PRINCESS_DRESS;
    uint16_t skinColor = sad ? COL_SAD : 0xFF80;

    tft.fillRect(x - 6, y - 12, 12, 12, COL_BG);

    tft.fillCircle(x, y - 14, 4, sad ? COL_SAD : 0xFFE0);

    if (sad)
    {
        tft.drawPixel(x - 2, y - 15, COL_BG);
        tft.drawPixel(x + 2, y - 15, COL_BG);
        tft.drawLine(x - 2, y - 12, x + 2, y - 12, COL_BG);
    }
    else
    {
        tft.drawPixel(x - 2, y - 15, COL_BG);
        tft.drawPixel(x + 2, y - 15, COL_BG);
        tft.drawPixel(x - 1, y - 13, COL_BG);
        tft.drawPixel(x, y - 13, COL_BG);
        tft.drawPixel(x + 1, y - 13, COL_BG);
    }

    tft.fillTriangle(x - 6, y - 2, x + 6, y - 2, x, y + 6, dressColor);

    tft.fillRect(x - 5, y - 6, 3, 4, skinColor);
    tft.fillRect(x + 2, y - 6, 3, 4, skinColor);
}

void drawBlock(int x, int y, int w, int h)
{
    tft.fillRect(x, y, w, h, COL_BLOCK);
    tft.drawRect(x, y, w, h, 0xB5B6);
    for (int i = x + 4; i < x + w; i += 8)
    {
        for (int j = y + 4; j < y + h; j += 8)
        {
            tft.drawPixel(i, j, 0x9CF3);
        }
    }
}

void drawPit(int x, int y, int w)
{
    tft.fillRect(x, y, w, GAME_AREA_BOTTOM - y, COL_PIT);
}

void drawGround(int scrollOffset)
{
    int groundY = GAME_AREA_BOTTOM - 10;
    int patternW = 20;
    int offset = scrollOffset % patternW;

    for (int x = -offset; x < TFT_WIDTH; x += patternW)
    {
        tft.fillRect(x, groundY, patternW - 2, 10, COL_GROUND);
        tft.drawLine(x + patternW - 2, groundY, x + patternW - 2, groundY + 9, 0x4A69);
    }
}
