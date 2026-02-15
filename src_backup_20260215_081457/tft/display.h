#pragma once

// =============================================================================
// TFT Display — initialisation and notification text
// =============================================================================

/// Initialise TFT, backlight, and draw initial UI (icons + "Waiting for BLE").
void initDisplay();

/// Draw centred status text in the notification zone.
void drawNotification(const char *text);
