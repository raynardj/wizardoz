#pragma once

// =============================================================================
// TFT Status Icons — WiFi and Bluetooth (top corners)
// =============================================================================

/// Draw WiFi icon (top-left). Filled when connected, outline when disconnected.
void drawWiFiIcon(bool connected);

/// Draw Bluetooth icon (top-right). Filled when client connected, outline otherwise.
void drawBTIcon(bool clientConnected);
