/**
 * USB CDC "Hello World" Test
 *
 * This script verifies that the USB CDC serial connection is working
 * correctly on your ESP32-S3 with the -DARDUINO_USB_CDC_ON_BOOT=1 flag.
 *
 * Expected behavior:
 * 1. The built-in LED will blink to show the board is running
 * 2. Messages will appear in the Serial Monitor every 2 seconds
 * 3. You can type messages and they will be echoed back
 */

#include <Arduino.h>

// Built-in LED on most ESP32-S3 dev boards (check your board's pinout)
#ifndef LED_BUILTIN
#define LED_BUILTIN 48 // Common for ESP32-S3 DevKitM-1
#endif

unsigned long messageCount = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastMessageTime = 0;
bool ledState = false;

void setup()
{
    // Initialize the built-in LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Initialize USB CDC Serial
    // With ARDUINO_USB_CDC_ON_BOOT=1, Serial uses the native USB port
    Serial.begin(115200);

    // Wait for USB CDC to be ready (important for native USB)
    // This gives time for the host computer to enumerate the device
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime) < 5000)
    {
        // Blink LED rapidly while waiting for connection
        digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
        delay(10);
    }

    // Turn LED on solid briefly to indicate we're starting
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);

    // Print startup banner
    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32-S3 USB CDC Hello World Test");
    Serial.println("========================================");
    Serial.println();
    Serial.println("USB CDC Serial connection established!");
    Serial.println();
    Serial.print("CPU Frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB");

    // Check PSRAM
    if (psramFound())
    {
        Serial.print("PSRAM Size: ");
        Serial.print(ESP.getPsramSize() / (1024 * 1024));
        Serial.println(" MB");
        Serial.print("Free PSRAM: ");
        Serial.print(ESP.getFreePsram() / (1024 * 1024));
        Serial.println(" MB");
    }
    else
    {
        Serial.println("PSRAM: Not detected");
    }

    Serial.println();
    Serial.println("Type a message and press Enter to echo it back.");
    Serial.println("LED will blink every 500ms to show the board is running.");
    Serial.println();
}

void loop()
{
    unsigned long currentTime = millis();

    // Blink LED every 500ms to show we're running
    if (currentTime - lastBlinkTime >= 500)
    {
        lastBlinkTime = currentTime;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }

    // Send a periodic message every 2 seconds
    if (currentTime - lastMessageTime >= 2000)
    {
        lastMessageTime = currentTime;
        messageCount++;

        Serial.print("[");
        Serial.print(currentTime / 1000);
        Serial.print("s] Hello from ESP32-S3! Message #");
        Serial.println(messageCount);
    }

    // Echo any received characters back to the sender
    if (Serial.available())
    {
        Serial.print("You typed: ");
        while (Serial.available())
        {
            char c = Serial.read();
            Serial.print(c);
        }
        Serial.println();
    }

    // Small delay to prevent watchdog issues
    delay(10);
}
