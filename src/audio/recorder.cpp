// =============================================================================
// Audio Recorder — PSRAM buffer, WAV output
// =============================================================================

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "pin_config.h"
#include "audio/recorder.h"

static const int SAMPLE_RATE = I2S_MIC_SAMPLE_RATE;
static const int BYTES_PER_SAMPLE = 2;  // 16-bit
static const int CHANNELS = 1;
static const size_t MAX_PCM_BYTES = (size_t)RECORDER_MAX_SECONDS * SAMPLE_RATE * BYTES_PER_SAMPLE * CHANNELS;
static const size_t WAV_HEADER_SIZE = 44;

static uint8_t *pcmBuffer = nullptr;
static size_t pcmLength = 0;
static bool recording = false;

static void writeWavHeader(uint8_t *buf, size_t dataSize)
{
    // RIFF header
    buf[0] = 'R';
    buf[1] = 'I';
    buf[2] = 'F';
    buf[3] = 'F';
    uint32_t fileSize = (uint32_t)(dataSize + 36);
    buf[4] = (uint8_t)(fileSize);
    buf[5] = (uint8_t)(fileSize >> 8);
    buf[6] = (uint8_t)(fileSize >> 16);
    buf[7] = (uint8_t)(fileSize >> 24);
    buf[8] = 'W';
    buf[9] = 'A';
    buf[10] = 'V';
    buf[11] = 'E';

    // fmt subchunk
    buf[12] = 'f';
    buf[13] = 'm';
    buf[14] = 't';
    buf[15] = ' ';
    buf[16] = 16;  // subchunk size
    buf[17] = 0;
    buf[18] = 0;
    buf[19] = 0;
    buf[20] = 1;   // PCM
    buf[21] = 0;
    buf[22] = (uint8_t)CHANNELS;
    buf[23] = 0;
    uint32_t sr = (uint32_t)SAMPLE_RATE;
    buf[24] = (uint8_t)(sr);
    buf[25] = (uint8_t)(sr >> 8);
    buf[26] = (uint8_t)(sr >> 16);
    buf[27] = (uint8_t)(sr >> 24);
    uint32_t byteRate = (uint32_t)(SAMPLE_RATE * BYTES_PER_SAMPLE * CHANNELS);
    buf[28] = (uint8_t)(byteRate);
    buf[29] = (uint8_t)(byteRate >> 8);
    buf[30] = (uint8_t)(byteRate >> 16);
    buf[31] = (uint8_t)(byteRate >> 24);
    buf[32] = (uint8_t)(BYTES_PER_SAMPLE * CHANNELS);
    buf[33] = 0;
    buf[34] = (uint8_t)(BYTES_PER_SAMPLE * 8);
    buf[35] = 0;

    // data subchunk
    buf[36] = 'd';
    buf[37] = 'a';
    buf[38] = 't';
    buf[39] = 'a';
    buf[40] = (uint8_t)(dataSize);
    buf[41] = (uint8_t)(dataSize >> 8);
    buf[42] = (uint8_t)(dataSize >> 16);
    buf[43] = (uint8_t)(dataSize >> 24);
}

void recorderStart()
{
    if (pcmBuffer == nullptr)
    {
        pcmBuffer = (uint8_t *)heap_caps_malloc(MAX_PCM_BYTES + WAV_HEADER_SIZE, MALLOC_CAP_SPIRAM);
        if (pcmBuffer == nullptr)
        {
            Serial.println("[Recorder] PSRAM alloc failed, using internal RAM");
            pcmBuffer = (uint8_t *)malloc(MAX_PCM_BYTES + WAV_HEADER_SIZE);
        }
    }
    if (pcmBuffer == nullptr)
    {
        Serial.println("[Recorder] Buffer alloc failed");
        return;
    }
    pcmLength = 0;
    recording = true;
    Serial.println("[Recorder] Recording started");
}

void recorderFeedSamples(const int16_t *samples, int numSamples)
{
    if (!recording || pcmBuffer == nullptr)
        return;

    size_t toAdd = (size_t)numSamples * BYTES_PER_SAMPLE;
    if (pcmLength + toAdd > MAX_PCM_BYTES)
        toAdd = MAX_PCM_BYTES - pcmLength;

    memcpy(pcmBuffer + WAV_HEADER_SIZE + pcmLength, samples, toAdd);
    pcmLength += toAdd;
}

void recorderStop()
{
    if (!recording)
        return;
    recording = false;

    if (pcmBuffer != nullptr && pcmLength > 0)
    {
        writeWavHeader(pcmBuffer, pcmLength);
    }
    Serial.printf("[Recorder] Stopped, %u bytes PCM\n", (unsigned)pcmLength);
}

bool recorderIsRecording()
{
    return recording;
}

const uint8_t *recorderGetWavBuffer()
{
    return pcmBuffer;
}

size_t recorderGetWavSize()
{
    if (pcmBuffer == nullptr || pcmLength == 0)
        return 0;
    return WAV_HEADER_SIZE + pcmLength;
}
