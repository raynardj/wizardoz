#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <WizardozConnect.h>

#include "pin_config.h"
#include "tft/game_display.h"
#include "game/game_engine.h"

static const int DMA_BUF_COUNT = 4;
static const int DMA_BUF_LEN = 256;
static const int AUDIO_BUF_LEN = 512;
static const uint32_t FRAME_MS = 100;

WizardozConnect connector("Wizardoz-Jump");
int16_t audioBuf[AUDIO_BUF_LEN];

GameState game;

bool prevWiFiState = false;
bool currWiFiState = false;
bool prevBLEState = false;
bool currBLEState = false;

unsigned long lastFrame = 0;
int lastPrincessX = -1;
int lastPrincessY = -1;
int lastDistance = -1;

void initI2S()
{
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_MIC_CHANNEL,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num = I2S_MIC_BCLK,
        .ws_io_num = I2S_MIC_LRCK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_DATA,
    };

    esp_err_t err;
    err = i2s_driver_install(I2S_MIC_PORT, &i2sConfig, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] Driver install failed: %d\n", err);
    }
    err = i2s_set_pin(I2S_MIC_PORT, &pinConfig);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] Set pin failed: %d\n", err);
    }
    i2s_zero_dma_buffer(I2S_MIC_PORT);
    Serial.println("[I2S] Microphone initialised");
}

void onWiFiReady(const String &ip)
{
    Serial.printf("[Main] WiFi ready - IP: %s\n", ip.c_str());
    currWiFiState = true;
    drawWiFiIcon(true);
}

void onWiFiLost()
{
    Serial.println("[Main] WiFi lost");
    currWiFiState = false;
    drawWiFiIcon(false);
}

void onBLEClient(bool connected)
{
    currBLEState = connected;
    drawBTIcon(connected);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Voice Jump Game ===");

    initDisplay();
    initI2S();
    gameInit(&game);

    drawWiFiIcon(false);
    drawBTIcon(false);
    drawDistance(0);

    connector.onWiFiReady(onWiFiReady);
    connector.onWiFiLost(onWiFiLost);
    connector.onBLEClient(onBLEClient);
    connector.begin();

    Serial.println("[Main] Setup complete");
}

void loop()
{
    connector.loop();

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(
        I2S_MIC_PORT,
        audioBuf,
        sizeof(audioBuf),
        &bytesRead,
        portMAX_DELAY);

    int samplesRead = 0;
    if (err == ESP_OK && bytesRead > 0)
    {
        samplesRead = bytesRead / sizeof(int16_t);
    }

    if (samplesRead > 0 && isVoiceTriggered(audioBuf, samplesRead))
    {
        gameJump(&game);
        Serial.println("[Game] Jump!");
    }

    unsigned long now = millis();
    if (now - lastFrame >= FRAME_MS)
    {
        lastFrame = now;

        gameUpdate(&game);

        if (game.gameOver)
        {
            if (lastPrincessX >= 0)
            {
                drawPrincess(lastPrincessX, lastPrincessY, true);
            }
            return;
        }

        if (currWiFiState != prevWiFiState)
        {
            drawWiFiIcon(currWiFiState);
            prevWiFiState = currWiFiState;
        }
        if (currBLEState != prevBLEState)
        {
            drawBTIcon(currBLEState);
            prevBLEState = currBLEState;
        }

        if (game.distance != lastDistance)
        {
            drawDistance(game.distance);
            lastDistance = game.distance;
        }

        clearGameArea();
        drawGround(game.scrollOffset);

        for (int i = 0; i < MAX_OBSTACLES; i++)
        {
            if (game.obstacles[i].active)
            {
                if (game.obstacles[i].type == OBSTACLE_BLOCK)
                {
                    drawBlock(game.obstacles[i].x, game.obstacles[i].y,
                             game.obstacles[i].w, game.obstacles[i].h);
                }
                else if (game.obstacles[i].type == OBSTACLE_PIT)
                {
                    drawPit(game.obstacles[i].x, game.obstacles[i].y,
                           game.obstacles[i].w);
                }
            }
        }

        drawPrincess(game.princess.x, game.princess.y, false);
        lastPrincessX = game.princess.x;
        lastPrincessY = game.princess.y;
    }
}
