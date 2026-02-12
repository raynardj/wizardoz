# Wizardoz Server

FastAPI backend that serves:

1. **Dashboard** (`/dashboard`) — Use Web Bluetooth to discover and configure WiFi on ESP32 devices.
2. **Visualizer** (`/visualizer`) — Real-time audio waveform display from connected ESP32 devices streaming microphone data.

## Quick Start

```bash
cd server
poetry install
poetry run serve
```

Then open <http://localhost:8000> in **Chrome or Edge** (Web Bluetooth requires a Chromium browser).

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Redirect to dashboard |
| GET | `/dashboard` | BLE WiFi provisioning page |
| GET | `/visualizer` | Real-time audio waveform |
| WS | `/ws/audio/{device_id}` | ESP32 sends audio here |
| WS | `/ws/visualizer/{device_id}` | Browser receives audio here |
