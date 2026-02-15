// =============================================================================
// HTTP Audio Client — POST to /talkie + JSON parse
// =============================================================================

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "http/audio_client.h"

static String baseUrl;

void audioClientSetBaseUrl(const String &host, uint16_t port)
{
    baseUrl = "http://" + host + ":" + String(port);
}

bool audioClientFetchConfig(String &responseKeyA, String &contentTypeA)
{
    responseKeyA = "";
    contentTypeA = "";

    if (baseUrl.length() == 0)
    {
        Serial.println("[AudioClient] No base URL for config fetch");
        return false;
    }

    String url = baseUrl + "/api/button-config";
    HTTPClient http;
    http.begin(url);

    int code = http.GET();

    if (code <= 0)
    {
        Serial.printf("[AudioClient] Config fetch error: %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (code != 200)
    {
        Serial.printf("[AudioClient] Config HTTP %d\n", code);
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err)
    {
        Serial.printf("[AudioClient] Config JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonObject buttons = doc["buttons"];
    if (buttons.isNull())
    {
        Serial.println("[AudioClient] No 'buttons' in config");
        return false;
    }

    JsonObject btnA = buttons["A"];
    if (btnA.isNull())
    {
        Serial.println("[AudioClient] No 'A' in buttons");
        return false;
    }

    if (btnA["response_key"].is<const char *>())
        responseKeyA = btnA["response_key"].as<const char *>();
    if (btnA["content_type"].is<const char *>())
        contentTypeA = btnA["content_type"].as<const char *>();

    Serial.printf("[AudioClient] Config loaded: key=%s ct=%s\n",
                  responseKeyA.c_str(), contentTypeA.c_str());
    return true;
}

bool audioClientPost(const char *button,
                     const char *responseKey,
                     const char *contentType,
                     const uint8_t *data,
                     size_t size,
                     String &outText)
{
    outText = "";

    if (data == nullptr || size == 0)
    {
        Serial.println("[AudioClient] No data to send");
        return false;
    }

    if (baseUrl.length() == 0)
    {
        Serial.println("[AudioClient] No base URL set");
        return false;
    }

    String url = baseUrl + "/talkie";

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", contentType);
    http.addHeader("X-Button", button);

    int code = http.POST(const_cast<uint8_t *>(data), size);

    if (code <= 0)
    {
        Serial.printf("[AudioClient] HTTP error: %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (code != 200)
    {
        Serial.printf("[AudioClient] HTTP %d: %s\n", code, payload.c_str());
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err)
    {
        Serial.printf("[AudioClient] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonVariant val = doc[responseKey];
    if (val.isNull() || !val.is<const char *>())
    {
        Serial.printf("[AudioClient] Key '%s' not found or not string\n", responseKey);
        return false;
    }

    outText = val.as<const char *>();
    return true;
}
