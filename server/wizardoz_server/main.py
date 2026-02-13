"""
Wizardoz Server — FastAPI application.

Serves the BLE WiFi provisioning dashboard and REST API for ESP32
push-to-talk audio transcription. Button and routing config is defined
in BUTTON_CONFIG below.
"""

from __future__ import annotations

import asyncio
import json
import logging
from collections import defaultdict
from pathlib import Path

import httpx
import uvicorn
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import Response
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


def music_identify_response_process(data) -> str:
    """
    return the title and the artist of the 1st match
    separated by newline so the LCD can display them on two lines.

    example response:
    """
    if "matches" in data:
        title = data['matches'][0]['metadata']['Title']
        artist = data['matches'][0]['metadata']['Artist']
        return f"{title}\n{artist}"
    return "No matches found"

BUTTON_CONFIG = {
    "buttons": {
        "A": {"endpoint": "http://localhost:8080/identify", "response_key": music_identify_response_process, "content_type": "audio/wav"},
        "B": {"endpoint": "", "response_key": "text", "content_type": "audio/wav"},
        "C": {"endpoint": "", "response_key": "text", "content_type": "audio/wav"},
        "D": {"endpoint": "", "response_key": "text", "content_type": "audio/wav"},
    },
}

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
    """Real-time audio waveform visualiser (disabled — use keypad A for push-to-talk)."""
    return templates.TemplateResponse("visualizer.html", {"request": request})


# ---------------------------------------------------------------------------
# Routes — REST API
# ---------------------------------------------------------------------------

@app.get("/api/devices")
async def list_devices():
    """Return list of known device IDs (devices that have connected via WS)."""
    return {"devices": sorted(known_devices)}


def _serialize_button_config() -> dict:
    """Return config for ESP32. Callables become response_key='text'."""
    buttons = {}
    for k, v in BUTTON_CONFIG["buttons"].items():
        rk = v.get("response_key", "text")
        serialized_key = "text" if callable(rk) else rk
        buttons[k] = {
            "endpoint": v.get("endpoint", ""),
            "response_key": serialized_key,
            "content_type": v.get("content_type", "audio/wav"),
        }
    return {"buttons": buttons}


@app.get("/api/button-config")
async def get_button_config():
    """Return current button-to-endpoint config for ESP32."""
    return _serialize_button_config()


@app.post("/talkie")
async def talkie(request: Request):
    """
    Proxy audio to a backend service configured per button.

    The device sends raw audio with an ``X-Button`` header (default ``A``).
    The server looks up that button's ``endpoint`` URL from BUTTON_CONFIG
    and forwards the audio as multipart/form-data.  When response_key is
    a callable, the backend JSON is transformed before returning.
    """
    button = request.headers.get("x-button", "A").upper()
    btn_cfg = BUTTON_CONFIG.get("buttons", {}).get(button)
    if not btn_cfg or not btn_cfg.get("endpoint", "").strip():
        raise HTTPException(status_code=404, detail=f"No endpoint configured for button {button}")

    backend_url = btn_cfg["endpoint"].strip()
    response_key = btn_cfg.get("response_key", "text")
    body = await request.body()
    if not body:
        raise HTTPException(status_code=400, detail="No audio data")

    content_type = request.headers.get("content-type", "audio/wav")
    files = {"audio": ("audio.wav", body, content_type)}
    try:
        async with httpx.AsyncClient(timeout=30.0) as client:
            r = await client.post(backend_url, files=files)
    except httpx.RequestError as e:
        logger.warning("Proxy request to %s failed: %s", backend_url, e)
        raise HTTPException(status_code=502, detail="Backend unavailable")

    # Apply response_key callback if callable and backend returned JSON.
    # Always return 200 so the ESP32 parses the body and displays the text.
    if callable(response_key):
        ct = r.headers.get("content-type", "")
        if "application/json" in ct:
            try:
                data = r.json()
                result = response_key(data)
                return Response(
                    content=json.dumps({"text": str(result)}),
                    status_code=200,
                    headers={"Content-Type": "application/json"},
                )
            except Exception as e:
                logger.warning("Callback response_key failed: %s", e)
                return Response(
                    content=json.dumps({"text": "Error: invalid response"}),
                    status_code=200,
                    headers={"Content-Type": "application/json"},
                )
        return Response(
            content=json.dumps({"text": "Error: invalid response"}),
            status_code=200,
            headers={"Content-Type": "application/json"},
        )

    return Response(
        content=r.content,
        status_code=r.status_code,
        headers=dict(r.headers),
    )


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
