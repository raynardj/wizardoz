#pragma once

#include <Arduino.h>

// =============================================================================
// HTTP Audio Client — POST audio to /talkie, parse JSON response
// =============================================================================
//
// Sends WAV (or raw PCM) to the server's /talkie endpoint with an X-Button
// header identifying the pressed button.  The server forwards the audio to
// the backend configured for that button and returns the JSON response.
// The client extracts the value at response_key for display on the LCD.
//
// =============================================================================

/// POST audio to /talkie with the given button ID and extract text at response_key.
/// button: keypad button identifier (e.g. "A").
/// responseKey: JSON key to extract (e.g. "text"). Simple key only.
/// contentType: e.g. "audio/wav".
/// data, size: WAV buffer and length.
/// outText: receives the extracted string on success.
/// Returns true on success, false on error.
bool audioClientPost(const char *button,
                    const char *responseKey,
                    const char *contentType,
                    const uint8_t *data,
                    size_t size,
                    String &outText);

/// Set base URL (e.g. "http://192.168.1.1:8000") for server endpoints.
void audioClientSetBaseUrl(const String &host, uint16_t port);

/// Fetch button config from GET /api/button-config.
/// Fills responseKeyA, contentTypeA for button A.
/// Returns true on success; caller should fall back to compile-time defaults on false.
bool audioClientFetchConfig(String &responseKeyA, String &contentTypeA);
