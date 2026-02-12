"""
Wizardoz Server — FastAPI application.

Serves the BLE WiFi provisioning dashboard and real-time audio wave visualiser.
Relays audio data from ESP32 WebSocket clients to browser WebSocket viewers.
"""

from __future__ import annotations

import asyncio
import logging
from collections import defaultdict
from pathlib import Path

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import RedirectResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from starlette.requests import Request

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"
TEMPLATE_DIR = BASE_DIR / "templates"

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("wizardoz")

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
app = FastAPI(title="Wizardoz Server", version="0.1.0")
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")
templates = Jinja2Templates(directory=str(TEMPLATE_DIR))

# ---------------------------------------------------------------------------
# WebSocket connection registries
#   device_id -> set of WebSocket connections
# ---------------------------------------------------------------------------
# ESP32 audio producers (normally one per device)
audio_producers: dict[str, set[WebSocket]] = defaultdict(set)
# Browser visualiser consumers
visualizer_consumers: dict[str, set[WebSocket]] = defaultdict(set)
# Track known device IDs for the API
known_devices: set[str] = set()

# ---------------------------------------------------------------------------
# Routes — Pages
# ---------------------------------------------------------------------------

@app.get("/")
async def index():
    """Redirect root to dashboard."""
    return RedirectResponse(url="/dashboard")


@app.get("/dashboard")
async def dashboard(request: Request):
    """BLE WiFi provisioning dashboard."""
    return templates.TemplateResponse("dashboard.html", {"request": request})


@app.get("/visualizer")
async def visualizer(request: Request):
    """Real-time audio waveform visualiser."""
    return templates.TemplateResponse("visualizer.html", {"request": request})


# ---------------------------------------------------------------------------
# Routes — REST API
# ---------------------------------------------------------------------------

@app.get("/api/devices")
async def list_devices():
    """Return list of known device IDs (devices that have connected via WS)."""
    return {"devices": sorted(known_devices)}


@app.get("/api/server-ip")
async def server_ip(request: Request):
    """Return the server's LAN IP so the ESP32 can connect to the WebSocket.
    Uses the Host header when available (e.g. 192.168.1.100), else detects
    from network interfaces."""
    import socket

    host = request.headers.get("host", "").split(":")[0]
    # If accessed via localhost/127.0.0.1, try to find LAN IP
    if host in ("localhost", "127.0.0.1", ""):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            host = s.getsockname()[0]
            s.close()
        except Exception:
            host = "127.0.0.1"
    return {"host": host}


# ---------------------------------------------------------------------------
# WebSocket — Audio producer (ESP32 -> Server)
# ---------------------------------------------------------------------------

@app.websocket("/ws/audio/{device_id}")
async def ws_audio(websocket: WebSocket, device_id: str):
    """
    ESP32 devices connect here and send raw PCM audio as binary frames.
    The server relays each frame to every browser on /ws/visualizer/{device_id}.
    """
    await websocket.accept()
    audio_producers[device_id].add(websocket)
    known_devices.add(device_id)
    logger.info("Audio producer connected: %s (total: %d)", device_id, len(audio_producers[device_id]))

    try:
        while True:
            data = await websocket.receive_bytes()
            # Fan-out to all visualiser consumers for this device
            consumers = list(visualizer_consumers.get(device_id, set()))
            if consumers:
                send_tasks = [_safe_send(ws, data) for ws in consumers]
                await asyncio.gather(*send_tasks)
    except WebSocketDisconnect:
        logger.info("Audio producer disconnected: %s", device_id)
    except Exception as exc:
        logger.warning("Audio producer error (%s): %s", device_id, exc)
    finally:
        audio_producers[device_id].discard(websocket)
        if not audio_producers[device_id]:
            del audio_producers[device_id]


# ---------------------------------------------------------------------------
# WebSocket — Visualiser consumer (Server -> Browser)
# ---------------------------------------------------------------------------

@app.websocket("/ws/visualizer/{device_id}")
async def ws_visualizer(websocket: WebSocket, device_id: str):
    """
    Browser clients connect here to receive relayed audio frames for a device.
    """
    await websocket.accept()
    visualizer_consumers[device_id].add(websocket)
    logger.info("Visualizer consumer connected: %s (total: %d)", device_id, len(visualizer_consumers[device_id]))

    try:
        # Keep the connection open; we only send data (no reads expected).
        # We still read to detect disconnection / pongs.
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        logger.info("Visualizer consumer disconnected: %s", device_id)
    except Exception as exc:
        logger.warning("Visualizer consumer error (%s): %s", device_id, exc)
    finally:
        visualizer_consumers[device_id].discard(websocket)
        if not visualizer_consumers[device_id]:
            del visualizer_consumers[device_id]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

async def _safe_send(ws: WebSocket, data: bytes) -> None:
    """Send bytes to a WebSocket, ignoring errors from stale connections."""
    try:
        await ws.send_bytes(data)
    except Exception:
        pass  # will be cleaned up on next receive


# ---------------------------------------------------------------------------
# Entry-point (used by `poetry run serve`)
# ---------------------------------------------------------------------------

def run() -> None:
    """Launch the server with Uvicorn."""
    uvicorn.run(
        "wizardoz_server.main:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info",
    )


if __name__ == "__main__":
    run()
