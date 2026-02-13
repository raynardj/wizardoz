#pragma once

// =============================================================================
// Audio Recorder — buffer PCM from I2S, output as WAV
// =============================================================================
//
// Uses PSRAM for the buffer. 16-bit mono @ 16 kHz.
// ~32 KB/s; default capacity ~30 s (~960 KB).
//
// =============================================================================

/// Maximum recording duration in seconds.
#define RECORDER_MAX_SECONDS 30

/// Start recording; clears any previous data.
void recorderStart();

/// Append samples to the buffer. Call while recording.
void recorderFeedSamples(const int16_t *samples, int numSamples);

/// Stop recording and finalize WAV header.
void recorderStop();

/// Return true if currently recording.
bool recorderIsRecording();

/// Return pointer to WAV buffer (header + PCM). Valid after stopRecording().
const uint8_t *recorderGetWavBuffer();

/// Return total WAV size in bytes (header + PCM).
size_t recorderGetWavSize();
