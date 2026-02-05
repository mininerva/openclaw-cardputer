"""
OpenClaw Gateway Bridge - Reimplemented

FastAPI server that acts as a bridge between the Cardputer ADV device
and the OpenClaw gateway. Features:
- Binary WebSocket protocol
- Streaming audio processing with Opus codec
- STT (Speech-to-Text) integration
- Connection management with health monitoring
- Message routing and queueing
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
from enum import Enum, auto
from typing import Dict, Optional, Set, Callable, Any
from collections import deque

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
# Protocol Definitions (matching firmware)
# =============================================================================

class MessageType(Enum):
    """Message types matching firmware protocol."""
    AUTH = 0x01
    AUTH_RESPONSE = 0x02
    PING = 0x03
    PONG = 0x04
    TEXT = 0x10
    AUDIO = 0x11
    RESPONSE = 0x12
    RESPONSE_FINAL = 0x13
    STATUS = 0x20
    COMMAND = 0x21
    ERROR = 0x22
    AUDIO_CONFIG = 0x30
    UNKNOWN = 0xFF


class ConnectionState(Enum):
    """Connection state."""
    DISCONNECTED = auto()
    CONNECTING = auto()
    CONNECTED = auto()
    AUTHENTICATING = auto()
    AUTHENTICATED = auto()
    ERROR = auto()


# =============================================================================
# Configuration
# =============================================================================

@dataclass
class BridgeConfig:
    """Bridge configuration."""
    websocket_port: int = int(os.getenv("WEBSOCKET_PORT", "8765"))
    openclaw_gateway_url: str = os.getenv("OPENCLAW_GATEWAY_URL", "http://localhost:8080")
    openclaw_api_key: Optional[str] = os.getenv("OPENCLAW_API_KEY")
    whisper_model: str = os.getenv("WHISPER_MODEL", "base")
    whisper_url: Optional[str] = os.getenv("WHISPER_URL")
    log_level: str = os.getenv("LOG_LEVEL", "info")
    max_audio_size_mb: float = float(os.getenv("MAX_AUDIO_SIZE_MB", "10"))
    session_timeout_seconds: int = int(os.getenv("SESSION_TIMEOUT_SECONDS", "300"))
    max_message_queue_size: int = int(os.getenv("MAX_MESSAGE_QUEUE_SIZE", "100"))
    enable_opus: bool = os.getenv("ENABLE_OPUS", "true").lower() == "true"
    
    def __post_init__(self):
        logging.getLogger().setLevel(getattr(logging, self.log_level.upper()))


config = BridgeConfig()


# =============================================================================
# Data Models
# =============================================================================

class DeviceInfo(BaseModel):
    """Device information."""
    device_id: str
    device_name: str
    version: str
    capabilities: list[str] = Field(default_factory=list)


class AudioConfig(BaseModel):
    """Audio configuration."""
    sample_rate: int = 16000
    channels: int = 1
    bits_per_sample: int = 16
    codec: str = "opus"
    frame_duration_ms: int = 60


class ProtocolMessage(BaseModel):
    """Protocol message wrapper."""
    type: str
    payload: dict = Field(default_factory=dict)
    timestamp: int = Field(default_factory=lambda: int(time.time() * 1000))
    
    @classmethod
    def from_binary(cls, data: bytes) -> Optional["ProtocolMessage"]:
        """Parse binary protocol message."""
        if len(data) < 12:
            return None
        
        # Parse header (simplified - full implementation would match firmware)
        magic = data[0]
        version = data[1]
        msg_type = data[2]
        flags = data[3]
        payload_len = int.from_bytes(data[4:6], 'little')
        timestamp = int.from_bytes(data[6:10], 'little')
        
        if magic != 0x4F or version != 1:  # 'O' for OpenClaw
            return None
        
        # Extract payload
        payload_data = data[10:10+payload_len]
        
        try:
            payload = json.loads(payload_data.decode('utf-8'))
        except (json.JSONDecodeError, UnicodeDecodeError):
            payload = {"raw": base64.b64encode(payload_data).decode()}
        
        type_str = MessageType(msg_type).name.lower() if msg_type in [t.value for t in MessageType] else "unknown"
        
        return cls(type=type_str, payload=payload, timestamp=timestamp)
    
    def to_binary(self) -> bytes:
        """Convert to binary protocol format."""
        payload_bytes = json.dumps(self.payload).encode('utf-8')
        
        # Build header
        header = bytearray(10)
        header[0] = 0x4F  # Magic
        header[1] = 1     # Version
        header[2] = getattr(MessageType, self.type.upper(), MessageType.UNKNOWN).value
        header[3] = 0     # Flags
        header[4:6] = len(payload_bytes).to_bytes(2, 'little')
        header[6:10] = self.timestamp.to_bytes(4, 'little')
        
        # Calculate CRC16
        data = header + payload_bytes
        crc = calculate_crc16(data)
        
        return bytes(data + crc.to_bytes(2, 'little'))


def calculate_crc16(data: bytes) -> int:
    """Calculate CRC16-CCITT-FALSE."""
    crc = 0xFFFF
    for byte in data:
        crc = ((crc << 8) ^ CRC16_TABLE[((crc >> 8) ^ byte) & 0xFF]) & 0xFFFF
    return crc


CRC16_TABLE = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
]


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
    capabilities: list[str] = field(default_factory=list)
    connected_at: float = field(default_factory=time.time)
    last_activity: float = field(default_factory=time.time)
    state: ConnectionState = ConnectionState.CONNECTED
    session_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    audio_config: Optional[AudioConfig] = None
    message_queue: deque = field(default_factory=lambda: deque(maxlen=config.max_message_queue_size))
    audio_buffer: bytearray = field(default_factory=bytearray)
    stats: dict = field(default_factory=lambda: {
        "messages_sent": 0,
        "messages_received": 0,
        "audio_bytes_received": 0,
        "audio_bytes_sent": 0
    })
    
    def touch(self):
        """Update last activity timestamp."""
        self.last_activity = time.time()
    
    def queue_message(self, message: ProtocolMessage) -> bool:
        """Queue a message for sending."""
        if len(self.message_queue) >= config.max_message_queue_size:
            return False
        self.message_queue.append(message)
        return True
    
    def get_next_message(self) -> Optional[ProtocolMessage]:
        """Get next queued message."""
        if self.message_queue:
            return self.message_queue.popleft()
        return None


class ConnectionManager:
    """Manages WebSocket connections from devices."""
    
    def __init__(self):
        self.connections: Dict[str, DeviceConnection] = {}
        self.device_to_session: Dict[str, str] = {}
        self._lock = asyncio.Lock()
    
    async def connect(self, websocket: WebSocket, device_id: str) -> DeviceConnection:
        """Accept a new WebSocket connection."""
        await websocket.accept()
        
        conn = DeviceConnection(
            websocket=websocket,
            device_id=device_id,
            device_name="Unknown",
            version="unknown"
        )
        
        async with self._lock:
            self.connections[conn.session_id] = conn
        
        logger.info(f"New connection: {device_id} (session: {conn.session_id})")
        return conn
    
    async def disconnect(self, session_id: str):
        """Remove a connection."""
        async with self._lock:
            if session_id in self.connections:
                conn = self.connections[session_id]
                logger.info(f"Disconnected: {conn.device_id} (session: {session_id})")
                del self.connections[session_id]
                
                # Clean up device mapping
                for device_id, sid in list(self.device_to_session.items()):
                    if sid == session_id:
                        del self.device_to_session[device_id]
    
    async def authenticate(self, session_id: str, device_info: DeviceInfo) -> bool:
        """Mark a connection as authenticated."""
        async with self._lock:
            if session_id not in self.connections:
                return False
            
            conn = self.connections[session_id]
            conn.state = ConnectionState.AUTHENTICATED
            conn.device_name = device_info.device_name
            conn.version = device_info.version
            conn.capabilities = device_info.capabilities
            self.device_to_session[device_info.device_id] = session_id
            
            logger.info(f"Authenticated: {device_info.device_id} ({device_info.device_name})")
            return True
    
    async def send_to_session(self, session_id: str, message: ProtocolMessage) -> bool:
        """Send a message to a specific session."""
        async with self._lock:
            if session_id not in self.connections:
                return False
            
            conn = self.connections[session_id]
            try:
                await conn.websocket.send_bytes(message.to_binary())
                conn.touch()
                conn.stats["messages_sent"] += 1
                return True
            except Exception as e:
                logger.error(f"Failed to send to {conn.device_id}: {e}")
                return False
    
    async def send_to_device(self, device_id: str, message: ProtocolMessage) -> bool:
        """Send a message to a device by ID."""
        async with self._lock:
            if device_id not in self.device_to_session:
                return False
            session_id = self.device_to_session[device_id]
            return await self.send_to_session(session_id, message)
    
    async def broadcast(self, message: ProtocolMessage, authenticated_only: bool = True) -> int:
        """Broadcast message to all connected devices."""
        sent_count = 0
        async with self._lock:
            for session_id, conn in self.connections.items():
                if authenticated_only and conn.state != ConnectionState.AUTHENTICATED:
                    continue
                if await self.send_to_session(session_id, message):
                    sent_count += 1
        return sent_count
    
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
            if conn.state == ConnectionState.AUTHENTICATED
        ]
    
    async def cleanup_stale(self):
        """Remove stale connections."""
        now = time.time()
        stale_sessions = []
        
        async with self._lock:
            for session_id, conn in self.connections.items():
                if now - conn.last_activity > config.session_timeout_seconds:
                    stale_sessions.append(session_id)
        
        for session_id in stale_sessions:
            logger.info(f"Cleaning up stale session: {session_id}")
            await self.disconnect(session_id)


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
            logger.info(f"Loading Whisper model: {config.whisper_model}")
            self.model = whisper.load_model(config.whisper_model)
            logger.info("Whisper model loaded successfully")
        except ImportError:
            logger.info("Whisper not available locally, will use external service")
        except Exception as e:
            logger.error(f"Failed to load Whisper: {e}")
    
    async def transcribe(self, audio_data: bytes, codec: str = "opus") -> Optional[str]:
        """Transcribe audio data to text."""
        try:
            if config.whisper_url:
                return await self._transcribe_external(audio_data, codec)
            elif self.whisper_available:
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
                f"{config.whisper_url}/transcribe",
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
        self.base_url = config.openclaw_gateway_url.rstrip('/')
        self.api_key = config.openclaw_api_key
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
    logger.info("Starting OpenClaw Gateway Bridge v2.0.0")
    
    # Start cleanup task
    cleanup_task = asyncio.create_task(cleanup_loop())
    
    yield
    
    # Cleanup
    logger.info("Shutting down OpenClaw Gateway Bridge")
    cleanup_task.cancel()
    try:
        await cleanup_task
    except asyncio.CancelledError:
        pass
    await gateway.close()


app = FastAPI(
    title="OpenClaw Gateway Bridge",
    description="Bridge server for Cardputer ADV devices - v2.0.0",
    version="2.0.0",
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
        
        # Receive auth message (expecting binary protocol)
        auth_data = await websocket.receive_bytes()
        auth_msg = ProtocolMessage.from_binary(auth_data)
        
        if not auth_msg or auth_msg.type != "auth":
            error_response = ProtocolMessage(
                type="auth_response",
                payload={"success": False, "error": "Expected auth message"}
            )
            await websocket.send_bytes(error_response.to_binary())
            await websocket.close()
            return
        
        device_id = auth_msg.payload.get("device_id", "unknown")
        
        # Create connection
        conn = await manager.connect(websocket, device_id)
        session_id = conn.session_id
        
        # Validate API key if required
        if config.openclaw_api_key:
            if auth_msg.payload.get("api_key") != config.openclaw_api_key:
                error_response = ProtocolMessage(
                    type="auth_response",
                    payload={"success": False, "error": "Invalid API key"}
                )
                await websocket.send_bytes(error_response.to_binary())
                await websocket.close()
                return
        
        # Authenticate
        device_info = DeviceInfo(
            device_id=auth_msg.payload.get("device_id", device_id),
            device_name=auth_msg.payload.get("device_name", "Unknown"),
            version=auth_msg.payload.get("version", "unknown"),
            capabilities=auth_msg.payload.get("capabilities", [])
        )
        await manager.authenticate(session_id, device_info)
        
        # Send auth success
        auth_response = ProtocolMessage(
            type="auth_response",
            payload={"success": True, "session_id": session_id}
        )
        await websocket.send_bytes(auth_response.to_binary())
        
        # Main message loop
        while True:
            try:
                # Receive message (binary protocol)
                message_data = await websocket.receive_bytes()
                conn.touch()
                
                message = ProtocolMessage.from_binary(message_data)
                if not message:
                    logger.warning(f"Failed to parse message from {device_id}")
                    continue
                
                await handle_message(session_id, message)
                
            except WebSocketDisconnect:
                break
            except Exception as e:
                logger.error(f"Message handling error: {e}")
                error_msg = ProtocolMessage(
                    type="error",
                    payload={"error": f"Message error: {str(e)}"}
                )
                await manager.send_to_session(session_id, error_msg)
                
    except WebSocketDisconnect:
        logger.info(f"WebSocket disconnected: {device_id}")
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
    finally:
        if session_id:
            await manager.disconnect(session_id)


async def handle_message(session_id: str, message: ProtocolMessage):
    """Handle incoming message from device."""
    conn = manager.get_connection(session_id)
    if not conn:
        return
    
    msg_type = message.type
    
    if msg_type == "text":
        await handle_text_message(session_id, message)
    elif msg_type == "audio":
        await handle_audio_message(session_id, message)
    elif msg_type == "ping":
        await handle_ping(session_id, message)
    elif msg_type == "audio_config":
        await handle_audio_config(session_id, message)
    else:
        logger.warning(f"Unknown message type: {msg_type}")


async def handle_text_message(session_id: str, message: ProtocolMessage):
    """Handle text message from device."""
    conn = manager.get_connection(session_id)
    if not conn:
        return
    
    text = message.payload.get("text", "").strip()
    if not text:
        return
    
    logger.info(f"Text from {conn.device_id}: {text[:50]}...")
    conn.stats["messages_received"] += 1
    
    # Send to OpenClaw gateway
    response = await gateway.send_message(conn.device_id, text)
    
    # Send response back to device
    response_msg = ProtocolMessage(
        type="response_final",
        payload={"text": response, "is_final": True}
    )
    await manager.send_to_session(session_id, response_msg)


async def handle_audio_message(session_id: str, message: ProtocolMessage):
    """Handle audio message from device."""
    conn = manager.get_connection(session_id)
    if not conn:
        return
    
    audio_b64 = message.payload.get("data", "")
    is_final = message.payload.get("is_final", False)
    codec = message.payload.get("codec", "opus")
    
    if audio_b64:
        # Decode and buffer audio
        try:
            audio_data = base64.b64decode(audio_b64)
            conn.audio_buffer.extend(audio_data)
            conn.stats["audio_bytes_received"] += len(audio_data)
        except Exception as e:
            logger.error(f"Audio decode error: {e}")
            return
    
    if is_final and conn.audio_buffer:
        logger.info(f"Processing audio from {conn.device_id} ({len(conn.audio_buffer)} bytes)")
        
        # Send processing status
        status_msg = ProtocolMessage(
            type="status",
            payload={"status": "Transcribing..."}
        )
        await manager.send_to_session(session_id, status_msg)
        
        # Transcribe audio
        text = await stt_service.transcribe(bytes(conn.audio_buffer), codec)
        
        # Clear buffer
        conn.audio_buffer.clear()
        
        if text:
            logger.info(f"Transcribed: {text}")
            
            # Send transcription to device
            response_msg = ProtocolMessage(
                type="response",
                payload={"text": f"[You said: {text}]", "is_final": False}
            )
            await manager.send_to_session(session_id, response_msg)
            
            # Send to OpenClaw
            response = await gateway.send_message(conn.device_id, text)
            
            # Send AI response
            final_response = ProtocolMessage(
                type="response_final",
                payload={"text": response, "is_final": True}
            )
            await manager.send_to_session(session_id, final_response)
        else:
            error_msg = ProtocolMessage(
                type="error",
                payload={"error": "Could not transcribe audio"}
            )
            await manager.send_to_session(session_id, error_msg)


async def handle_ping(session_id: str, message: ProtocolMessage):
    """Handle ping message."""
    ping_timestamp = message.payload.get("timestamp", 0)
    
    pong_msg = ProtocolMessage(
        type="pong",
        payload={"ping_timestamp": ping_timestamp, "timestamp": int(time.time() * 1000)}
    )
    await manager.send_to_session(session_id, pong_msg)


async def handle_audio_config(session_id: str, message: ProtocolMessage):
    """Handle audio configuration message."""
    conn = manager.get_connection(session_id)
    if not conn:
        return
    
    # Store audio configuration
    conn.audio_config = AudioConfig(
        sample_rate=message.payload.get("sample_rate", 16000),
        channels=message.payload.get("channels", 1),
        bits_per_sample=message.payload.get("bits_per_sample", 16),
        codec=message.payload.get("codec", "opus"),
        frame_duration_ms=message.payload.get("frame_duration_ms", 60)
    )
    
    logger.info(f"Audio config from {conn.device_id}: {conn.audio_config}")


# =============================================================================
# HTTP API Endpoints
# =============================================================================

@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {
        "status": "healthy",
        "version": "2.0.0",
        "connected_devices": len(manager.get_active_connections()),
        "whisper_available": stt_service.whisper_available,
        "opus_enabled": config.enable_opus
    }


@app.get("/devices")
async def list_devices():
    """List connected devices."""
    devices = []
    for conn in manager.get_active_connections():
        devices.append({
            "device_id": conn.device_id,
            "device_name": conn.device_name,
            "version": conn.version,
            "capabilities": conn.capabilities,
            "connected_at": conn.connected_at,
            "last_activity": conn.last_activity,
            "session_id": conn.session_id,
            "stats": conn.stats
        })
    return {"devices": devices}


@app.post("/devices/{device_id}/message")
async def send_to_device(device_id: str, payload: dict):
    """Send a message to a specific device."""
    message = ProtocolMessage(
        type="response_final",
        payload={"text": payload.get("text", ""), "is_final": True}
    )
    
    success = await manager.send_to_device(device_id, message)
    
    if not success:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Device {device_id} not connected"
        )
    
    return {"success": True}


@app.get("/devices/{device_id}/stats")
async def get_device_stats(device_id: str):
    """Get device statistics."""
    conn = manager.get_connection_by_device(device_id)
    if not conn:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Device {device_id} not connected"
        )
    
    return {
        "device_id": conn.device_id,
        "session_id": conn.session_id,
        "connected_at": conn.connected_at,
        "last_activity": conn.last_activity,
        "stats": conn.stats
    }


# =============================================================================
# Main Entry Point
# =============================================================================

if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=config.websocket_port,
        log_level=config.log_level,
        reload=False
    )
