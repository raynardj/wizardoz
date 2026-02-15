// =============================================================================
// 4x4 Matrix Keypad — row-column scan implementation
// =============================================================================

#include <Arduino.h>

#include "pin_config.h"
#include "keypad/keypad.h"

static const int ROWS = 4;
static const int COLS = 4;
static const int DEBOUNCE_MS = 50;

static const int rowPins[ROWS] = KEYPAD_ROW_PINS;
static const int colPins[COLS] = KEYPAD_COL_PINS;

// Layout: keys[row][col]
static const char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

static char lastKey = '\0';
static unsigned long lastKeyTime = 0;
static char debouncedKey = '\0';

void keypadInit()
{
    for (int r = 0; r < ROWS; r++)
    {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH); // inactive
    }
    for (int c = 0; c < COLS; c++)
    {
        pinMode(colPins[c], INPUT_PULLUP);
    }
    Serial.println("[Keypad] Initialised (4x4 matrix)");
}

void keypadScan()
{
    char pressed = '\0';

    for (int r = 0; r < ROWS && pressed == '\0'; r++)
    {
        digitalWrite(rowPins[r], LOW);

        for (int c = 0; c < COLS; c++)
        {
            if (digitalRead(colPins[c]) == LOW)
            {
                pressed = keys[r][c];
                break;
            }
        }

        digitalWrite(rowPins[r], HIGH);
    }

    unsigned long now = millis();

    if (pressed != '\0')
    {
        if (pressed == lastKey && (now - lastKeyTime) >= (unsigned long)DEBOUNCE_MS)
        {
            debouncedKey = pressed;
        }
        else if (pressed != lastKey)
        {
            lastKey = pressed;
            lastKeyTime = now;
        }
    }
    else
    {
        lastKey = '\0';
        lastKeyTime = 0;
        debouncedKey = '\0';
    }
}

char keypadGetKey()
{
    return debouncedKey;
}

bool keypadIsKeyPressed(char key)
{
    return debouncedKey == key;
}
