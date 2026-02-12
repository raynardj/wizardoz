# Wave Visualizer

Real-time audio wave visualizer: ESP32-S3 captures sound via an INMP441 I2S microphone, displays a bar-graph waveform on a 1602 LCD, and streams audio over WiFi to a browser-based oscilloscope.

---

## Wiring Diagram

```mermaid
graph LR
    subgraph ESP32_S3 ["ESP32-S3 — YD-ESP32-23"]
        P3V3["3V3"]
        P5V["5V"]
        PGND["GND"]
        G4["GPIO 4"]
        G5["GPIO 5"]
        G6["GPIO 6"]
        G8["GPIO 8"]
        G9["GPIO 9"]
    end

    subgraph INMP441 ["INMP441 Mic"]
        VDD["VDD"]
        MGND["GND"]
        SCK["SCK / BCLK"]
        WS["WS / LRCK"]
        SD["SD"]
        LR["L/R"]
    end

    subgraph LCD1602 ["LCD 1602 I2C"]
        VCC["VCC"]
        LGND["GND"]
        SDA["SDA"]
        SCL["SCL"]
    end

    P3V3 -->|"3.3 V"| VDD
    PGND -->|"GND"| MGND
    G4 -->|"I2S BCLK"| SCK
    G5 -->|"I2S LRCK"| WS
    G6 -->|"I2S DATA"| SD
    PGND -->|"Left Ch"| LR

    P5V -->|"5 V"| VCC
    PGND -->|"GND"| LGND
    G8 -->|"I2C Data"| SDA
    G9 -->|"I2C Clock"| SCL
```

### Pin Reference Table

| ESP32-S3 Pin | Connects To | Protocol | Notes |
|:------------:|:-----------:|:--------:|:------|
| **3V3** | INMP441 VDD | Power | 3.3 V supply |
| **GND** | INMP441 GND | Power | |
| **GPIO 4** | INMP441 SCK | I2S BCLK | Bit clock |
| **GPIO 5** | INMP441 WS | I2S LRCK | Word select / L-R clock |
| **GPIO 6** | INMP441 SD | I2S DATA | Serial data (mic output) |
| **GND** | INMP441 L/R | Config | Tied LOW = left channel |
| **5V** | LCD VCC | Power | LCD backpack needs 5 V |
| **GND** | LCD GND | Power | |
| **GPIO 8** | LCD SDA | I2C Data | PCF8574T at address `0x27` |
| **GPIO 9** | LCD SCL | I2C Clock | |

> **Note:** The LCD 1602's PCF8574T I2C backpack runs its logic at 5 V but is compatible with the ESP32's 3.3 V I2C signals — no level shifter required.

---

## System Architecture

```mermaid
sequenceDiagram
    participant Browser
    participant FastAPI
    participant ESP32

    Note over Browser,ESP32: Phase 1 — WiFi Provisioning via BLE
    Browser->>ESP32: Web Bluetooth scan + connect
    Browser->>ESP32: Write SSID characteristic
    Browser->>ESP32: Write Password characteristic
    ESP32->>ESP32: Connect to WiFi, store creds in NVS
    ESP32-->>Browser: Notify WiFi status + IP address

    Note over Browser,ESP32: Phase 2 — Audio Streaming
    ESP32->>ESP32: Read INMP441 via I2S
    ESP32->>ESP32: Compute amplitude bars for LCD
    ESP32->>ESP32: Display waveform on LCD 1602
    ESP32->>FastAPI: WebSocket stream PCM audio
    FastAPI->>Browser: WebSocket relay to visualizer page
```

---

## Data Flow

```mermaid
flowchart TD
    MIC["INMP441 Mic"] -->|"I2S 16-bit @ 16 kHz"| ESP["ESP32-S3"]
    ESP -->|"I2C"| LCD["LCD 1602"]
    ESP -->|"WebSocket (binary PCM)"| SRV["FastAPI Server"]
    SRV -->|"WebSocket relay"| VIZ["Browser Visualizer"]

    subgraph device ["ESP32 On-Device"]
        ESP
        LCD
    end

    subgraph lan ["Local Network"]
        SRV
        VIZ
    end
```

---

## Setup & Deploy

### Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| [PlatformIO](https://platformio.org/) | latest | Build & flash ESP32 firmware |
| [Python](https://python.org/) | >= 3.11 | Server runtime |
| [Poetry](https://python-poetry.org/) | >= 1.7 | Python dependency management |
| Chrome or Edge | latest | Web Bluetooth requires a Chromium browser |

### 1. Flash the Firmware

```bash
# From the project root
pio run -t upload          # compile & flash
pio device monitor         # open serial monitor (115200 baud)
```

On first boot the ESP32 will:
- Start BLE advertising as **"Wizardoz-Wave"**
- Begin reading the INMP441 microphone
- Display `Waiting for BLE` on the LCD

If WiFi credentials were previously saved, it will auto-connect.

### 2. Start the Server

```bash
cd server
poetry install             # install dependencies (first time)
poetry run serve           # start FastAPI on http://0.0.0.0:8000
```

The server binds to `0.0.0.0:8000` so any device on your LAN can reach it.

### 3. Configure WiFi via the Dashboard

1. Open **http://localhost:8000** in Chrome/Edge
2. Click **Scan for Devices** — the browser BLE picker will appear
3. Select your ESP32 (shows as "Wizardoz-Wave")
4. Enter your WiFi SSID and password, then click **Configure WiFi**
5. The status badge will update to **CONNECTED** with the device's IP address

The ESP32 saves the credentials in flash (NVS). On subsequent boots it will reconnect automatically — no need to re-provision.

### 4. View the Waveform

1. Navigate to **http://localhost:8000/visualizer**
2. Select your device from the dropdown and click **Connect**
3. The oscilloscope waveform, frequency spectrum, and level meter update in real time

Meanwhile, the 1602 LCD on the ESP32 itself shows a 16-column bar-graph visualizer.

---

## Project Structure

```
wizardoz/
├── include/
│   └── pin_config.h               # GPIO & I2C address definitions
├── lib/
│   └── WizardozConnect/           # Reusable BLE WiFi provisioning library
│       ├── WizardozConnect.h
│       └── WizardozConnect.cpp
├── src/
│   ├── main.cpp                   # Wave visualizer firmware
│   └── README.md                  # ← you are here
├── server/                        # FastAPI backend
│   ├── pyproject.toml
│   ├── README.md
│   └── wizardoz_server/
│       ├── main.py                # App + WebSocket relay
│       ├── static/css/style.css
│       ├── static/js/ble-connect.js
│       ├── static/js/visualizer.js
│       └── templates/
│           ├── base.html
│           ├── dashboard.html
│           └── visualizer.html
└── platformio.ini
```

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| BLE scan shows no devices | Make sure the ESP32 is powered and you see `BLE advertising` in serial monitor |
| WiFi status stuck on CONNECTING | Double-check SSID spelling and password; try moving closer to the access point |
| LCD shows nothing | Adjust the blue contrast potentiometer on the I2C backpack |
| Visualizer shows flat line | Confirm the device appears in `/api/devices`; check that the ESP32 serial log shows `[WS] Connection opened` |
| "Web Bluetooth not supported" | Switch to Chrome or Edge — Firefox and Safari do not support the Web Bluetooth API |
