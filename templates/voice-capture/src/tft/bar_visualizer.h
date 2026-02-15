#pragma once

// =============================================================================
// TFT Bar Visualizer — waveform bar graph from audio samples
// =============================================================================

/// Compute bar heights from audio samples (RMS per column, with smoothing).
/// Call before renderBars().
void computeBars(const int16_t *samples, int numSamples);

/// Render the bar graph to the TFT.
void renderBars();

/// Clear the waveform area and reset bar heights.
void clearBars();
