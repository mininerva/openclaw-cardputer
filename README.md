# OpenClaw Cardputer ADV

A thin client firmware for the M5Stack Cardputer ADV that connects to an OpenClaw gateway, enabling voice and text interaction with AI assistants.

## Architecture

```
┌─────────────┐      WiFi      ┌──────────────────┐      ┌─────────────┐
│  Cardputer  │◄──────────────►│  Gateway Bridge  │◄────►│   OpenClaw  │
│    ADV      │                │   (FastAPI)      │      │   Gateway   │
│             │                │                  │      │             │
│ ┌─────────┐ │                │ ┌──────────────┐ │      │ ┌─────────┐ │
│ │   Mic   │ │                │ │  WebSocket   │ │      │ │Telegram │ │
│ │Keyboard │ │                │ │   Handler    │ │      │ │ Channel │ │
│ │ Display │ │                │ │  STT (Whisper)│ │      │ │         │ │
│ └─────────┘ │                │ └──────────────┘ │      │ └─────────┘ │
└─────────────┘                └──────────────────┘      └─────────────┘
```

## Quick Start

### Prerequisites

- M5Stack Cardputer ADV (ESP32-S3)
- PlatformIO CLI or VS Code extension
- Python 3.9+ (for gateway bridge)
- OpenClaw gateway running on your VPS

### Firmware Setup

1. **Install PlatformIO**:
   ```bash
   pip install platformio
   ```

2. **Configure WiFi and Gateway**:
   ```bash
   cd firmware/data
   # Edit config.json with your credentials
   nano config.json
   ```

3. **Upload config to device** (important!):
   ```bash
   cd firmware
   pio run --target uploadfs  # Uploads data/ folder to SPIFFS
   ```

4. **Build and flash the firmware**:
   ```bash
   pio run --target upload
   ```

5. **Monitor serial output**:
   ```bash
   pio run --target monitor
   ```

### Gateway Bridge Setup

1. **Install dependencies**:
   ```bash
   cd bridge
   pip install -r requirements.txt
   ```

2. **Configure environment**:
   ```bash
   cp .env.example .env
   # Edit .env with your settings
   ```

3. **Run the bridge**:
   ```bash
   uvicorn main:app --host 0.0.0.0 --port 8765
   ```

Or use Docker:
```bash
docker build -t openclaw-bridge .
docker run -p 8765:8765 --env-file .env openclaw-bridge
```

## Project Structure

```
openclaw-cardputer/
├── firmware/              # ESP32-S3 firmware
│   ├── src/              # Source files
│   ├── include/          # Header files
│   └── platformio.ini    # PlatformIO configuration
├── bridge/               # Gateway bridge server
│   ├── main.py           # FastAPI application
│   ├── requirements.txt  # Python dependencies
│   └── Dockerfile        # Container image
├── config/               # Configuration files
│   └── config.example.json
├── docs/                 # Documentation
└── README.md             # This file
```

## Hardware Features

- **Voice Input**: I2S microphone with real-time streaming
- **Text Input**: Cardputer built-in keyboard
- **Display**: 240x135 color LCD for responses
- **Connectivity**: WiFi to gateway bridge
- **Audio Codec**: Opus compression (or raw PCM fallback)

## Protocol

### Message Format (JSON over WebSocket)

```json
{
  "type": "audio|text|response|status",
  "payload": "...",
  "timestamp": 1234567890,
  "device_id": "cardputer-001"
}
```

### Message Types

- `audio`: Base64-encoded Opus audio frames
- `text`: Keyboard input text
- `response`: AI response text (markdown supported)
- `status`: Connection status updates
- `ping`/`pong`: Keepalive messages

## Configuration

### Configuration (`firmware/data/config.json`)

```json
{
  "wifi": {
    "ssid": "YourWiFi",
    "password": "YourPassword",
    "dhcp": true
  },
  "gateway": {
    "websocket_url": "ws://your-vps:8765/ws",
    "fallback_url": "http://your-vps:8765/api",
    "api_key": ""
  },
  "device": {
    "id": "cardputer-001",
    "name": "My Cardputer",
    "display_brightness": 128
  },
  "audio": {
    "sample_rate": 16000,
    "codec": "opus",
    "mic_gain": 64
  }
}
```

### Bridge (`.env`)

```
OPENCLAW_GATEWAY_URL=http://localhost:8080
OPENCLAW_API_KEY=your-api-key
WHISPER_MODEL=base
WEBSOCKET_PORT=8765
LOG_LEVEL=info
```

## Development

### Building

```bash
cd firmware
pio run
```

### Testing

```bash
cd firmware
pio test
```

### Debugging

```bash
pio run --target monitor
```

## License

MIT License - See LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## Troubleshooting

### WiFi Connection Issues

- Check credentials in `config.json`
- Ensure 2.4GHz network (ESP32-S3 limitation)
- Verify signal strength

### Gateway Connection Issues

- Check firewall rules on VPS
- Verify gateway bridge is running
- Check WebSocket URL format

### Audio Quality Issues

- Adjust microphone gain in settings
- Check for electrical interference
- Verify I2S wiring

## Support

For issues and questions, please open an issue on GitHub or contact the OpenClaw community.
