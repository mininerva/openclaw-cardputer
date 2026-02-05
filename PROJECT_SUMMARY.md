# OpenClaw Cardputer ADV - Project Summary

## Overview
Complete thin client firmware implementation for M5Stack Cardputer ADV (ESP32-S3) that connects to an OpenClaw gateway for voice and text AI interaction. Features a real-time procedural owl avatar that responds to system state.

## Project Structure

```
openclaw-cardputer/
├── firmware/              # ESP32-S3 PlatformIO firmware
│   ├── include/          # Header files (.h)
│   │   ├── avatar/       # Procedural avatar system
│   │   │   ├── geometry.h      # Drawing primitives
│   │   │   ├── animation.h     # Easing, blink, breath controllers
│   │   │   ├── moods.h         # Mood states and transitions
│   │   │   └── procedural_avatar.h  # Main avatar class
│   │   ├── audio_capture.h
│   │   ├── config_manager.h
│   │   ├── display_manager.h
│   │   ├── gateway_client.h
│   │   └── keyboard_input.h
│   ├── src/              # Source files (.cpp)
│   │   ├── avatar/       # Avatar implementation
│   │   │   ├── geometry.cpp
│   │   │   └── procedural_avatar.cpp
│   │   ├── audio_capture.cpp
│   │   ├── config_manager.cpp
│   │   ├── display_manager.cpp
│   │   ├── gateway_client.cpp
│   │   ├── keyboard_input.cpp
│   │   └── main.cpp
│   ├── scripts/          # Build scripts
│   └── platformio.ini    # PlatformIO configuration
├── bridge/               # Python FastAPI gateway bridge
│   ├── main.py           # WebSocket server implementation
│   ├── requirements.txt  # Python dependencies
│   ├── Dockerfile        # Container build
│   └── .env.example      # Environment template
├── config/               # Configuration
│   └── config.example.json
├── docs/                 # Documentation
│   └── README.md
├── README.md             # Main project README
├── LICENSE               # MIT License
└── .gitignore            # Git ignore rules
```

## Firmware Components

### Core Modules
1. **config_manager** - JSON configuration with LittleFS storage
2. **audio_capture** - I2S microphone with voice activity detection
3. **keyboard_input** - Cardputer keyboard handling with special keys
4. **display_manager** - 240x135 LCD for conversation UI
5. **gateway_client** - WebSocket client with auto-reconnect

### Procedural Avatar System
- **geometry** - Procedural drawing primitives (circles, ellipses, bezier curves, feathers)
- **animation** - Easing functions, blink controller, breath controller, ruffle controller, beak animation
- **moods** - 9 mood states with parameters and smooth transitions
- **procedural_avatar** - Main renderer integrating all systems

### Avatar Features
- **Procedural Eyes**: Pupils track to input source (keyboard, mic, user, center)
- **Beak Animation**: Syllable-based lip-sync for speaking
- **Feather Ruffling**: Noise-based movement tied to activity level
- **Blink System**: Single, double, slow judgment, flutter, glitch types
- **Mood States**: IDLE, LISTENING, THINKING, TOOL_USE, SPEAKING, EXCITED, JUDGING, ERROR, ANCIENT_MODE
- **Ancient Mode**: Sepia tint, floating runes, scanlines, glowing amber pupils

### Key Combinations
| Keys | Function |
|------|----------|
| Fn + V | Toggle voice input mode |
| Fn + S | Send message |
| Fn + A | Toggle Ancient Mode |
| Ctrl + A | Move cursor to start |
| Ctrl + E | Move cursor to end |
| Escape | Clear input |

## Gateway Bridge

### Features
- FastAPI WebSocket server
- Device authentication
- STT integration (Whisper)
- OpenClaw gateway forwarding
- Docker deployment
- Health monitoring

### Protocol
- JSON message format
- WebSocket bidirectional
- Base64 audio encoding
- Automatic ping/pong

## Git Repository
- 4 commits with clean history
- 19,856 lines of firmware code
- Proper .gitignore for firmware builds
- MIT License

## Build Status
- PlatformIO: Dependencies downloaded, ready to build
- Python Bridge: Syntax verified
- Configuration: Templates provided

## Next Steps
1. Copy config/config.example.json to config/config.json and customize
2. Copy bridge/.env.example to bridge/.env and customize
3. Build firmware: cd firmware && pio run
4. Run bridge: cd bridge && uvicorn main:app --host 0.0.0.0 --port 8765

## Avatar API Quick Reference

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
