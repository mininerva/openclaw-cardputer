# OpenClaw Cardputer ADV Documentation

## Table of Contents

1. [Overview](#overview)
2. [Hardware Setup](#hardware-setup)
3. [Firmware](#firmware)
4. [Gateway Bridge](#gateway-bridge)
5. [Protocol Specification](#protocol-specification)
6. [Troubleshooting](#troubleshooting)

## Overview

The OpenClaw Cardputer ADV is a thin client implementation that turns the M5Stack Cardputer ADV into a voice and text interface for AI assistants through an OpenClaw gateway.

### Features

- **Procedural Avatar**: Real-time rendered owl face with mood states and animations
- **Voice Input**: Real-time audio streaming with voice activity detection
- **Text Input**: Full keyboard support with special key combinations
- **Display**: Conversation history and status indicators
- **WebSocket**: Real-time bidirectional communication
- **Auto-reconnect**: Resilient connection handling

## Hardware Setup

### Required Hardware

- M5Stack Cardputer ADV (ESP32-S3)
- USB-C cable for programming
- WiFi network (2.4GHz)

### Pinout Reference

The Cardputer ADV uses the following pins:

| Function | GPIO |
|----------|------|
| PDM Clock | GPIO 42 |
| PDM Data | GPIO 41 |
| Display SPI | Default M5 pins |
| Keyboard I2C | Default M5 pins |

## Firmware

### Building

1. Install PlatformIO:
   ```bash
   pip install platformio
   ```

2. Configure WiFi and gateway:
   ```bash
   cp config/config.example.json config/config.json
   # Edit with your settings
   ```

3. Build:
   ```bash
   cd firmware
   pio run
   ```

4. Flash (optional):
   ```bash
   pio run --target upload
   ```

### Key Combinations

| Keys | Function |
|------|----------|
| Fn + V | Toggle voice input mode |
| Fn + S | Send message |
| Fn + A | Toggle Ancient Mode |
| Ctrl + A | Move cursor to start |
| Ctrl + E | Move cursor to end |
| Escape | Clear input |

## Procedural Avatar System

The firmware includes a real-time procedural owl avatar (Minerva) that responds to system state and user interaction.

### Mood States

| State | Trigger | Visual Behavior |
|-------|---------|-----------------|
| IDLE | Default | Slow breathing, occasional blink, drifting pupils |
| LISTENING | Voice input, connecting | Perked ears, focused pupils, slight head tilt |
| THINKING | Waiting for response | Narrowed eyes, pupil shimmer, faster breathing |
| SPEAKING | Receiving response | Beak chomping, expressive eyebrows |
| EXCITED | Special commands | Wide eyes, rapid blinking, feather fluff |
| JUDGING | Judgment command | Slow blink, side-eye, raised eyebrow |
| ERROR | Connection/audio error | Glitch shake, X-eyes flash |
| ANCIENT_MODE | Fn+A or trigger phrase | Sepia tint, runes, glowing pupils |

### Eye Tracking

The avatar's pupils track to different input sources:
- **Keyboard** (when typing): Slight down-left
- **Microphone** (when listening): Slight down-right  
- **User** (when responding): Slight up/center
- **Center** (idle): Center with subtle drift

### Ancient Mode Trigger Phrases

Say any of these to activate Ancient Mode:
- "ancient wisdom"
- "speak as minerva"
- "owl mode"
- "by the thirty-seven claws"

Press **Fn+A** to toggle Ancient Mode manually.

### Avatar API (for Developers)

```cpp
// Set mood
Avatar::g_avatar.setMood(Avatar::Mood::THINKING);

// Look at input source
Avatar::g_avatar.lookAt(Avatar::InputSource::MIC);

// Trigger speaking animation
Avatar::g_avatar.speak("Text to animate");

// Force blink
Avatar::g_avatar.blink(Avatar::BlinkType::SLOW);

// Trigger error animation
Avatar::g_avatar.triggerError();

// Toggle ancient mode
Avatar::g_avatar.setAncientMode(true);
```

## Gateway Bridge

### Installation

```bash
cd bridge
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Configuration

Create `.env` file:
```bash
cp .env.example .env
# Edit with your settings
```

### Running

```bash
uvicorn main:app --host 0.0.0.0 --port 8765
```

### Docker

```bash
docker build -t openclaw-bridge .
docker run -p 8765:8765 --env-file .env openclaw-bridge
```

## Protocol Specification

### WebSocket Message Format

All messages are JSON with the following structure:

```json
{
  "type": "message_type",
  "payload": "...",
  "timestamp": 1234567890,
  "device_id": "cardputer-001"
}
```

### Message Types

#### Device → Bridge

| Type | Description |
|------|-------------|
| `auth` | Authentication request |
| `text` | Text message |
| `audio` | Audio data (base64) |
| `ping` | Keepalive ping |

#### Bridge → Device

| Type | Description |
|------|-------------|
| `auth_response` | Authentication result |
| `response` | AI response text |
| `status` | Status update |
| `error` | Error message |
| `pong` | Keepalive pong |

### Authentication Flow

1. Device connects to WebSocket
2. Device sends `auth` message with device_id and api_key
3. Bridge validates and responds with `auth_response`
4. On success, device can send/receive messages

### Audio Streaming

Audio is streamed in chunks:
- Format: Opus or PCM 16-bit 16kHz mono
- Frame size: 60ms (960 samples at 16kHz)
- Encoding: Base64
- Final chunk marked with `is_final: true`

## Troubleshooting

### WiFi Connection Issues

**Problem**: Cannot connect to WiFi

**Solutions**:
- Verify SSID and password in config.json
- Ensure 2.4GHz network (ESP32-S3 doesn't support 5GHz)
- Check signal strength
- Try static IP if DHCP issues

### Gateway Connection Issues

**Problem**: Cannot connect to gateway

**Solutions**:
- Verify gateway URL in config.json
- Check firewall rules on VPS
- Ensure bridge server is running
- Check WebSocket URL format (ws:// or wss://)

### Audio Quality Issues

**Problem**: Poor audio quality or no transcription

**Solutions**:
- Adjust mic_gain in config (0-100)
- Check for electrical interference
- Verify microphone is not obstructed
- Try different codec (opus vs pcm)

### Build Errors

**Problem**: Firmware won't compile

**Solutions**:
- Update PlatformIO: `pio update`
- Clean build: `pio run --target clean`
- Check library dependencies in platformio.ini

## Development

### Adding New Features

1. Define message type in protocol
2. Implement in firmware (src/)
3. Implement in bridge (main.py)
4. Update documentation

### Testing

```bash
# Firmware unit tests
cd firmware
pio test

# Bridge tests
cd bridge
pytest
```

## License

MIT License - See LICENSE file
