// =============================================================================
// TFT Bar Visualizer — waveform bar graph from audio samples
// =============================================================================
//
// Splits audio into DISPLAY_BAR_COLS columns, computes RMS per column,
// applies smoothing (fast rise, slow decay), and renders coloured bars.
//
// =============================================================================

#include <math.h>

#include "tft_config.h"

static int barHeights[DISPLAY_BAR_COLS] = {0};

static uint16_t barColor(int height, int maxH)
{
    // Green for low, yellow for mid, red for high
    int pct = (height * 100) / maxH;
    if (pct > 80)
        return COL_BAR_HIGH;
    if (pct > 50)
        return COL_BAR_MID;
    return COL_BAR_LOW;
}

void computeBars(const int16_t *samples, int numSamples)
{
    int samplesPerBar = numSamples / DISPLAY_BAR_COLS;
    if (samplesPerBar < 1)
        samplesPerBar = 1;

    for (int col = 0; col < DISPLAY_BAR_COLS; col++)
    {
        int start = col * samplesPerBar;
        int end = start + samplesPerBar;
        if (end > numSamples)
            end = numSamples;

        // RMS amplitude for this column
        uint64_t sumSq = 0;
        for (int i = start; i < end; i++)
        {
            int32_t s = samples[i];
            sumSq += (uint64_t)(s * s);
        }
        double rms = sqrt((double)sumSq / (end - start));

        // Map RMS to 0 .. WAVE_HEIGHT  (tune the divisor to taste)
        int height = (int)(rms / 1024.0 * WAVE_HEIGHT);
        if (height > WAVE_HEIGHT)
            height = WAVE_HEIGHT;

        // Smooth: slow decay, fast rise
        if (height >= barHeights[col])
        {
            barHeights[col] = height;
        }
        else
        {
            barHeights[col] = barHeights[col] - 2;
            if (barHeights[col] < 0)
                barHeights[col] = 0;
        }
    }
}

void renderBars()
{
    for (int col = 0; col < DISPLAY_BAR_COLS; col++)
    {
        int h = barHeights[col]; // 0 .. WAVE_HEIGHT
        if (h < 0)
            h = 0;
        if (h > WAVE_HEIGHT)
            h = WAVE_HEIGHT;

        int x = WAVE_LEFT + col * BAR_STEP;

        // Clear the bar column first (draw background)
        if (h < WAVE_HEIGHT)
        {
            tft.fillRect(x, WAVE_TOP, BAR_WIDTH, WAVE_HEIGHT - h, COL_BG);
        }

        // Draw the filled bar (growing upward from bottom)
        if (h > 0)
        {
            uint16_t col16 = barColor(h, WAVE_HEIGHT);
            tft.fillRect(x, WAVE_BOTTOM - h, BAR_WIDTH, h, col16);
        }
    }
}

void clearBars()
{
    for (int col = 0; col < DISPLAY_BAR_COLS; col++)
    {
        barHeights[col] = 0;
    }
    int waveWidth = DISPLAY_BAR_COLS * BAR_STEP - BAR_GAP;
    tft.fillRect(WAVE_LEFT, WAVE_TOP, waveWidth, WAVE_HEIGHT, COL_BG);
}
