"""
OpenClaw Gateway Bridge

FastAPI server that acts as a bridge between the Cardputer ADV device
and the OpenClaw gateway. Handles WebSocket connections from devices,
STT (Speech-to-Text) processing, and message forwarding.
"""

import asyncio
import base64
import json
import logging
import os
import time
import uuid
from contextlib import asynccontextmanager
from dataclasses import dataclass, field
from typing import Dict, Optional, Set

import httpx
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, status
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


# =============================================================================
# Configuration
# =============================================================================

class Settings:
    """Application settings from environment variables."""
    WEBSOCKET_PORT: int = int(os.getenv("WEBSOCKET_PORT", "8765"))
    OPENCLAW_GATEWAY_URL: str = os.getenv("OPENCLAW_GATEWAY_URL", "http://localhost:8080")
    OPENCLAW_API_KEY: Optional[str] = os.getenv("OPENCLAW_API_KEY")
    WHISPER_MODEL: str = os.getenv("WHISPER_MODEL", "base")
    WHISPER_URL: Optional[str] = os.getenv("WHISPER_URL")  # External Whisper service
    LOG_LEVEL: str = os.getenv("LOG_LEVEL", "info")
    MAX_AUDIO_SIZE_MB: float = float(os.getenv("MAX_AUDIO_SIZE_MB", "10"))
    SESSION_TIMEOUT_SECONDS: int = int(os.getenv("SESSION_TIMEOUT_SECONDS", "300"))


settings = Settings()


# =============================================================================
# Data Models
# =============================================================================

class DeviceInfo(BaseModel):
    """Device information."""
    device_id: str
    device_name: str
    version: str


class AuthMessage(BaseModel):
    """Authentication message from device."""
    type: str = "auth"
    device_id: str
    device_name: str = "Unknown"
    version: str = "unknown"
    api_key: Optional[str] = None


class TextMessage(BaseModel):
    """Text message from device."""
    type: str = "text"
    payload: str
    device_id: str
    timestamp: int = Field(default_factory=lambda: int(time.time() * 1000))


class AudioMessage(BaseModel):
    """Audio message from device."""
    type: str = "audio"
    payload: str  # Base64 encoded audio
    is_final: bool = False
    device_id: str
    timestamp: int = Field(default_factory=lambda: int(time.time() * 1000))
    codec: str = "opus"


class ResponseMessage(BaseModel):
    """Response message to device."""
    type: str = "response"
    payload: str
    is_final: bool = True
    timestamp: int = Field(default_factory=lambda: int(time.time() * 1000))


class ErrorMessage(BaseModel):
    """Error message to device."""
    type: str = "error"
    payload: str
    timestamp: int = Field(default_factory=lambda: int(time.time() * 1000))


class StatusMessage(BaseModel):
    """Status message to device."""
    type: str = "status"
    payload: str
    timestamp: int = Field(default_factory=lambda: int(time.time() * 1000))


# =============================================================================
# Connection Manager
# =============================================================================

@dataclass
class DeviceConnection:
    """Represents a connected device."""
    websocket: WebSocket
    device_id: str
    device_name: str
    version: str
    connected_at: float = field(default_factory=time.time)
    last_activity: float = field(default_factory=time.time)
    authenticated: bool = False
    session_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    audio_buffer: bytes = field(default_factory=bytes)
    
    def touch(self):
        """Update last activity timestamp."""
        self.last_activity = time.time()


class ConnectionManager:
    """Manages WebSocket connections from devices."""
    
    def __init__(self):
        self.connections: Dict[str, DeviceConnection] = {}
        self.device_to_session: Dict[str, str] = {}
        
    async def connect(self, websocket: WebSocket, device_id: str) -> DeviceConnection:
        """Accept a new WebSocket connection."""
        await websocket.accept()
        
        conn = DeviceConnection(
            websocket=websocket,
            device_id=device_id,
            device_name="Unknown",
            version="unknown"
        )
        
        self.connections[conn.session_id] = conn
        logger.info(f"New connection: {device_id} (session: {conn.session_id})")
        return conn
    
    def disconnect(self, session_id: str):
        """Remove a connection."""
        if session_id in self.connections:
            conn = self.connections[session_id]
            logger.info(f"Disconnected: {conn.device_id} (session: {session_id})")
            del self.connections[session_id]
            
            # Clean up device mapping
            for device_id, sid in list(self.device_to_session.items()):
                if sid == session_id:
                    del self.device_to_session[device_id]
    
    def authenticate(self, session_id: str, device_info: DeviceInfo):
        """Mark a connection as authenticated."""
        if session_id in self.connections:
            conn = self.connections[session_id]
            conn.authenticated = True
            conn.device_name = device_info.device_name
            conn.version = device_info.version
            self.device_to_session[device_info.device_id] = session_id
            logger.info(f"Authenticated: {device_info.device_id} ({device_info.device_name})")
    
    async def send_to_device(self, session_id: str, message: dict):
        """Send a message to a specific device."""
        if session_id not in self.connections:
            return False
        
        conn = self.connections[session_id]
        try:
            await conn.websocket.send_json(message)
            conn.touch()
            return True
        except Exception as e:
            logger.error(f"Failed to send to {conn.device_id}: {e}")
            return False
    
    async def send_to_device_id(self, device_id: str, message: dict):
        """Send a message to a device by ID."""
        if device_id not in self.device_to_session:
            return False
        session_id = self.device_to_session[device_id]
        return await self.send_to_device(session_id, message)
    
    def get_connection(self, session_id: str) -> Optional[DeviceConnection]:
        """Get a connection by session ID."""
        return self.connections.get(session_id)
    
    def get_connection_by_device(self, device_id: str) -> Optional[DeviceConnection]:
        """Get a connection by device ID."""
        session_id = self.device_to_session.get(device_id)
        if session_id:
            return self.connections.get(session_id)
        return None
    
    def get_active_connections(self) -> list:
        """Get list of active authenticated connections."""
        return [
            conn for conn in self.connections.values()
            if conn.authenticated
        ]
    
    async def cleanup_stale(self):
        """Remove stale connections."""
        now = time.time()
        stale_sessions = [
            sid for sid, conn in self.connections.items()
            if now - conn.last_activity > settings.SESSION_TIMEOUT_SECONDS
        ]
        
        for session_id in stale_sessions:
            logger.info(f"Cleaning up stale session: {session_id}")
            self.disconnect(session_id)


manager = ConnectionManager()


# =============================================================================
# STT Service
# =============================================================================

class STTService:
    """Speech-to-Text service using Whisper or external provider."""
    
    def __init__(self):
        self.whisper_available = False
        self.model = None
        self._check_whisper()
    
    def _check_whisper(self):
        """Check if Whisper is available locally."""
        try:
            import whisper
            self.whisper_available = True
            logger.info(f"Loading Whisper model: {settings.WHISPER_MODEL}")
            self.model = whisper.load_model(settings.WHISPER_MODEL)
            logger.info("Whisper model loaded successfully")
        except ImportError:
            logger.info("Whisper not available locally, will use external service")
        except Exception as e:
            logger.error(f"Failed to load Whisper: {e}")
    
    async def transcribe(self, audio_data: bytes, codec: str = "opus") -> Optional[str]:
        """Transcribe audio data to text."""
        try:
            # Decode base64 if needed
            if isinstance(audio_data, str):
                audio_data = base64.b64decode(audio_data)
            
            # Convert to format Whisper expects if needed
            # For now, assume PCM 16-bit 16kHz mono
            
            if settings.WHISPER_URL:
                # Use external Whisper service
                return await self._transcribe_external(audio_data, codec)
            elif self.whisper_available:
                # Use local Whisper
                return await self._transcribe_local(audio_data)
            else:
                logger.error("No STT service available")
                return None
                
        except Exception as e:
            logger.error(f"Transcription error: {e}")
            return None
    
    async def _transcribe_local(self, audio_data: bytes) -> Optional[str]:
        """Transcribe using local Whisper model."""
        import tempfile
        import wave
        
        # Write to temporary WAV file
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            wav_path = f.name
            with wave.open(f, 'wb') as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(16000)
                wav.writeframes(audio_data)
        
        try:
            # Run transcription
            result = self.model.transcribe(wav_path)
            return result.get("text", "").strip()
        finally:
            os.unlink(wav_path)
    
    async def _transcribe_external(self, audio_data: bytes, codec: str) -> Optional[str]:
        """Transcribe using external Whisper service."""
        async with httpx.AsyncClient() as client:
            files = {"audio": ("audio.bin", audio_data, f"audio/{codec}")}
            response = await client.post(
                f"{settings.WHISPER_URL}/transcribe",
                files=files,
                timeout=30.0
            )
            response.raise_for_status()
            result = response.json()
            return result.get("text", "").strip()


stt_service = STTService()


# =============================================================================
# OpenClaw Gateway Client
# =============================================================================

class OpenClawGateway:
    """Client for the OpenClaw gateway."""
    
    def __init__(self):
        self.base_url = settings.OPENCLAW_GATEWAY_URL.rstrip('/')
        self.api_key = settings.OPENCLAW_API_KEY
        self.client = httpx.AsyncClient(timeout=60.0)
    
    async def send_message(self, device_id: str, message: str) -> Optional[str]:
        """Send a message to OpenClaw and get response."""
        try:
            headers = {}
            if self.api_key:
                headers["Authorization"] = f"Bearer {self.api_key}"
            
            payload = {
                "device_id": device_id,
                "message": message,
                "source": "cardputer",
                "timestamp": int(time.time() * 1000)
            }
            
            response = await self.client.post(
                f"{self.base_url}/api/v1/message",
                json=payload,
                headers=headers
            )
            response.raise_for_status()
            
            result = response.json()
            return result.get("response", "No response from gateway")
            
        except httpx.HTTPError as e:
            logger.error(f"Gateway HTTP error: {e}")
            return f"Gateway error: {str(e)}"
        except Exception as e:
            logger.error(f"Gateway error: {e}")
            return f"Error: {str(e)}"
    
    async def close(self):
        """Close the HTTP client."""
        await self.client.aclose()


gateway = OpenClawGateway()


# =============================================================================
# FastAPI Application
# =============================================================================

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager."""
    logger.info("Starting OpenClaw Gateway Bridge")
    
    # Start cleanup task
    cleanup_task = asyncio.create_task(cleanup_loop())
    
    yield
    
    # Cleanup
    logger.info("Shutting down OpenClaw Gateway Bridge")
    cleanup_task.cancel()
    await gateway.close()


app = FastAPI(
    title="OpenClaw Gateway Bridge",
    description="Bridge server for Cardputer ADV devices",
    version="1.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


async def cleanup_loop():
    """Background task to clean up stale connections."""
    while True:
        try:
            await asyncio.sleep(60)  # Run every minute
            await manager.cleanup_stale()
        except asyncio.CancelledError:
            break
        except Exception as e:
            logger.error(f"Cleanup error: {e}")


# =============================================================================
# WebSocket Endpoint
# =============================================================================

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for device connections."""
    device_id = "unknown"
    session_id = None
    
    try:
        # Wait for auth message before accepting
        await websocket.accept()
        
        # Receive auth message
        auth_data = await websocket.receive_json()
        auth_msg = AuthMessage(**auth_data)
        device_id = auth_msg.device_id
        
        # Create connection
        conn = await manager.connect(websocket, device_id)
        session_id = conn.session_id
        
        # Validate API key if required
        if settings.OPENCLAW_API_KEY:
            if auth_msg.api_key != settings.OPENCLAW_API_KEY:
                await websocket.send_json({
                    "type": "auth_response",
                    "success": False,
                    "error": "Invalid API key"
                })
                await websocket.close()
                return
        
        # Authenticate
        device_info = DeviceInfo(
            device_id=auth_msg.device_id,
            device_name=auth_msg.device_name,
            version=auth_msg.version
        )
        manager.authenticate(session_id, device_info)
        
        # Send auth success
        await websocket.send_json({
            "type": "auth_response",
            "success": True
        })
        
        # Main message loop
        while True:
            try:
                message = await websocket.receive_json()
                conn.touch()
                
                msg_type = message.get("type", "unknown")
                
                if msg_type == "text":
                    await handle_text_message(session_id, message)
                elif msg_type == "audio":
                    await handle_audio_message(session_id, message)
                elif msg_type == "ping":
                    await websocket.send_json({
                        "type": "pong",
                        "timestamp": int(time.time() * 1000)
                    })
                else:
                    logger.warning(f"Unknown message type: {msg_type}")
                    
            except WebSocketDisconnect:
                break
            except Exception as e:
                logger.error(f"Message handling error: {e}")
                await manager.send_to_device(session_id, {
                    "type": "error",
                    "payload": f"Message error: {str(e)}"
                })
                
    except WebSocketDisconnect:
        logger.info(f"WebSocket disconnected: {device_id}")
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
    finally:
        if session_id:
            manager.disconnect(session_id)


async def handle_text_message(session_id: str, message: dict):
    """Handle text message from device."""
    conn = manager.get_connection(session_id)
    if not conn:
        return
    
    text = message.get("payload", "").strip()
    if not text:
        return
    
    logger.info(f"Text from {conn.device_id}: {text[:50]}...")
    
    # Send to OpenClaw gateway
    response = await gateway.send_message(conn.device_id, text)
    
    # Send response back to device
    await manager.send_to_device(session_id, {
        "type": "response",
        "payload": response,
        "is_final": True
    })


async def handle_audio_message(session_id: str, message: dict):
    """Handle audio message from device."""
    conn = manager.get_connection(session_id)
    if not conn:
        return
    
    audio_b64 = message.get("payload", "")
    is_final = message.get("is_final", False)
    codec = message.get("codec", "opus")
    
    if audio_b64:
        # Decode and buffer audio
        try:
            audio_data = base64.b64decode(audio_b64)
            conn.audio_buffer += audio_data
        except Exception as e:
            logger.error(f"Audio decode error: {e}")
            return
    
    if is_final and conn.audio_buffer:
        logger.info(f"Processing audio from {conn.device_id} "
                   f"({len(conn.audio_buffer)} bytes)")
        
        # Send processing status
        await manager.send_to_device(session_id, {
            "type": "status",
            "payload": "Transcribing..."
        })
        
        # Transcribe audio
        text = await stt_service.transcribe(conn.audio_buffer, codec)
        
        # Clear buffer
        conn.audio_buffer = b""
        
        if text:
            logger.info(f"Transcribed: {text}")
            
            # Send transcription to device
            await manager.send_to_device(session_id, {
                "type": "response",
                "payload": f"[You said: {text}]",
                "is_final": False
            })
            
            # Send to OpenClaw
            response = await gateway.send_message(conn.device_id, text)
            
            # Send AI response
            await manager.send_to_device(session_id, {
                "type": "response",
                "payload": response,
                "is_final": True
            })
        else:
            await manager.send_to_device(session_id, {
                "type": "error",
                "payload": "Could not transcribe audio"
            })


# =============================================================================
# HTTP API Endpoints
# =============================================================================

@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {
        "status": "healthy",
        "version": "1.0.0",
        "connected_devices": len(manager.get_active_connections()),
        "whisper_available": stt_service.whisper_available
    }


@app.get("/devices")
async def list_devices():
    """List connected devices."""
    devices = [
        {
            "device_id": conn.device_id,
            "device_name": conn.device_name,
            "version": conn.version,
            "connected_at": conn.connected_at,
            "last_activity": conn.last_activity,
            "session_id": conn.session_id
        }
        for conn in manager.get_active_connections()
    ]
    return {"devices": devices}


@app.post("/devices/{device_id}/message")
async def send_to_device(device_id: str, message: dict):
    """Send a message to a specific device."""
    success = await manager.send_to_device_id(device_id, {
        "type": "response",
        "payload": message.get("text", ""),
        "is_final": True
    })
    
    if not success:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Device {device_id} not connected"
        )
    
    return {"success": True}


# =============================================================================
# Main Entry Point
# =============================================================================

if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=settings.WEBSOCKET_PORT,
        log_level=settings.LOG_LEVEL,
        reload=False
    )
