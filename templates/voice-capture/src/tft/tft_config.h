#pragma once

// =============================================================================
// TFT Shared Configuration — layout constants, colours, extern TFT instance
// =============================================================================

#include <TFT_eSPI.h>

// Global TFT instance (defined in display.cpp)
extern TFT_eSPI tft;

// -----------------------------------------------------------------------------
// Layout Constants
// -----------------------------------------------------------------------------

// Status bar
static const int STATUS_BAR_H = 24;
static const int ICON_SIZE = 16;
static const int ICON_MARGIN = 6;

// Notification zone (centred text area between status bar and waveform)
static const int NOTIFY_Y = 40;
static const int NOTIFY_H = 50;

// Waveform zone
static const int WAVE_TOP = 100;
static const int WAVE_BOTTOM = 235;
static const int WAVE_HEIGHT = WAVE_BOTTOM - WAVE_TOP; // 135 px
static const int BAR_WIDTH = 6;
static const int BAR_GAP = 2;
static const int BAR_STEP = BAR_WIDTH + BAR_GAP; // 8 px per bar
static const int DISPLAY_BAR_COLS = 28;          // 28 * 8 = 224 px
static const int WAVE_LEFT = (TFT_WIDTH - DISPLAY_BAR_COLS * BAR_STEP + BAR_GAP) / 2;

// -----------------------------------------------------------------------------
// Colours
// -----------------------------------------------------------------------------

static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_BAR_LOW = TFT_GREEN;
static const uint16_t COL_BAR_MID = TFT_YELLOW;
static const uint16_t COL_BAR_HIGH = TFT_RED;
static const uint16_t COL_ICON_ON = TFT_CYAN;
static const uint16_t COL_ICON_OFF = 0x4208; // dark grey
static const uint16_t COL_NOTIFY = TFT_WHITE;
