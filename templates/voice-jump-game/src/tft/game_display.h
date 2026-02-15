#pragma once

#include <TFT_eSPI.h>

extern TFT_eSPI tft;

static const int TFT_WIDTH = 240;
static const int TFT_HEIGHT = 240;

static const int STATUS_BAR_H = 24;
static const int ICON_SIZE = 16;
static const int ICON_MARGIN = 6;

static const int GAME_AREA_TOP = 30;
static const int GAME_AREA_BOTTOM = 230;
static const int GAME_AREA_HEIGHT = GAME_AREA_BOTTOM - GAME_AREA_TOP;

static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_ICON_ON = TFT_CYAN;
static const uint16_t COL_ICON_OFF = 0x4208;
static const uint16_t COL_TEXT = TFT_WHITE;
static const uint16_t COL_PRINCESS = 0xF81F;
static const uint16_t COL_PRINCESS_DRESS = 0xF800;
static const uint16_t COL_BLOCK = 0x8C51;
static const uint16_t COL_PIT = TFT_BLACK;
static const uint16_t COL_GROUND = 0x6540;
static const uint16_t COL_SKY = 0x10A2;
static const uint16_t COL_SAD = 0x632C;

void initDisplay();
void drawWiFiIcon(bool connected);
void drawBTIcon(bool connected);
void drawDistance(int distance);
void clearGameArea();
void drawPrincess(int x, int y, bool sad);
void drawBlock(int x, int y, int w, int h);
void drawPit(int x, int y, int w);
void drawGround(int scrollOffset);
