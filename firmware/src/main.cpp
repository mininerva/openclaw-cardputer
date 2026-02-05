/**
 * @file main.cpp
 * @brief OpenClaw Cardputer ADV - Reimplemented Main Application
 * 
 * A clean, modern implementation using:
 * - Binary protocol for WebSocket communication
 * - Improved state machine architecture
 * - Modular component design
 * - Efficient audio streaming with Opus codec
 * - Robust error handling and reconnection
 * 
 * Firmware Version: 2.0.0
 */

#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// OpenClaw components
#include "protocol.h"
#include "websocket_client.h"
#include "audio_streamer.h"
#include "keyboard_handler.h"
#include "display_renderer.h"
#include "app_state_machine.h"

// Avatar disabled for now - needs fixes
// #include "avatar/procedural_avatar.h"
// #include "avatar/ancient_ritual.h"
// #include "avatar/easter_eggs.h"
// #include "avatar/voice_synthesis.h"

using namespace OpenClaw;

// =============================================================================
// Constants (defined in platformio.ini build flags)
// =============================================================================

// FIRMWARE_VERSION, FIRMWARE_NAME, FIRMWARE_CODENAME defined in build flags

// Timing constants
constexpr uint32_t MAIN_LOOP_DELAY_MS = 10;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 33;  // ~30 FPS
constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 1000;

// Display regions
constexpr int16_t AVATAR_HEIGHT = 64;
constexpr int16_t TEXT_AREA_Y = AVATAR_HEIGHT + 16;  // + status bar

// =============================================================================
// Global Application Context
// =============================================================================

struct Application {
    // Components
    WebSocketClient websocket;
    AudioStreamer audio;
    KeyboardHandler keyboard;
    DisplayRenderer display;
    AppStateMachine state_machine;
    AppContext context;
    
    // Configuration
    WebSocketConfig ws_config;
    AudioStreamerConfig audio_config;
    DisplayConfig display_config;
    
    // Runtime state
    bool initialized;
    uint32_t last_display_update;
    uint32_t last_status_update;
    uint32_t last_wifi_check;
    uint32_t wifi_connect_start;
    
    // Ancient mode
    bool ancient_mode_active;
    uint32_t ancient_mode_start;
    
    Application() : initialized(false), last_display_update(0),
                    last_status_update(0), last_wifi_check(0),
                    wifi_connect_start(0), ancient_mode_active(false),
                    ancient_mode_start(0) {}
};

static Application g_app;

// =============================================================================
// Forward Declarations
// =============================================================================

bool loadConfiguration();
void setupStateMachine();
void setupWebSocketCallbacks();
void setupAudioCallbacks();
void setupKeyboardCallbacks();
void setupDisplay();

void connectWiFi();
void updateWiFiStatus();
void handleWiFiConnected();
void handleWiFiDisconnected();

void onStateChange(AppState from, AppState to);
void handleSystemEvent(AppEvent event);

void processIncomingMessage(const ProtocolMessage& msg);
void sendTextToGateway(const char* text);
void sendAudioToGateway(const EncodedAudioPacket& packet);

void updateDisplay();
void updateStatusBar();
void renderAvatar();

void enterAncientMode();
void exitAncientMode();
bool checkAncientModeTrigger(const char* text);

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println(FIRMWARE_NAME);
    Serial.printf("Version: %s (%s)\n", FIRMWARE_VERSION, FIRMWARE_CODENAME);
    Serial.println("========================================\n");
    
    // Initialize M5Cardputer
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    
    // Initialize display early for boot screen
    g_app.display.begin(g_app.display_config);
    g_app.display.renderBootScreen(FIRMWARE_VERSION);
    
    // Load configuration
    if (!loadConfiguration()) {
        Serial.println("Failed to load configuration");
        g_app.display.renderErrorScreen("Config load failed");
        delay(5000);
        ESP.restart();
    }
    
    // Setup state machine
    setupStateMachine();
    
    // Initialize components
    if (!g_app.keyboard.begin()) {
        Serial.println("Keyboard init failed");
    }
    setupKeyboardCallbacks();
    
    if (!g_app.audio.begin(g_app.audio_config)) {
        Serial.println("Audio init failed - continuing without audio");
    }
    setupAudioCallbacks();
    
    if (!g_app.websocket.begin(g_app.ws_config)) {
        Serial.println("WebSocket init failed");
    }
    setupWebSocketCallbacks();
    
    // Start state machine
    g_app.state_machine.begin();
    g_app.initialized = true;
    
    Serial.println("Setup complete");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    uint32_t now = millis();
    
    // Update M5 systems
    M5Cardputer.update();
    
    // Update components
    g_app.keyboard.update();
    g_app.audio.update();
    g_app.websocket.update();
    g_app.state_machine.update();
    
    // Update WiFi status
    if (now - g_app.last_wifi_check >= 1000) {
        updateWiFiStatus();
        g_app.last_wifi_check = now;
    }
    
    // Update display at ~30 FPS
    if (now - g_app.last_display_update >= DISPLAY_UPDATE_INTERVAL_MS) {
        updateDisplay();
        g_app.last_display_update = now;
    }
    
    // Update status bar
    if (now - g_app.last_status_update >= STATUS_UPDATE_INTERVAL_MS) {
        updateStatusBar();
        g_app.last_status_update = now;
    }
    
    // Process incoming WebSocket messages
    ProtocolMessage msg;
    while (g_app.websocket.receive(msg)) {
        processIncomingMessage(msg);
    }
    
    // Process incoming audio packets
    EncodedAudioPacket audio_packet;
    while (g_app.audio.readEncodedPacket(audio_packet)) {
        sendAudioToGateway(audio_packet);
    }
    
    // Ancient mode timeout check
    if (g_app.ancient_mode_active && 
        (now - g_app.ancient_mode_start > 300000)) {  // 5 minutes
        exitAncientMode();
    }
    
    delay(MAIN_LOOP_DELAY_MS);
}

// =============================================================================
// Configuration
// =============================================================================

bool loadConfiguration() {
    // TODO: Load from SPIFFS/LittleFS config file
    // For now, use default/test configuration
    
    // WiFi configuration
    // These should come from config file
    strlcpy(g_app.context.config.wifi_ssid, "YOUR_WIFI_SSID", sizeof(g_app.context.config.wifi_ssid));
    strlcpy(g_app.context.config.wifi_password, "YOUR_WIFI_PASSWORD", sizeof(g_app.context.config.wifi_password));
    
    // Gateway configuration
    strlcpy(g_app.context.config.gateway_url, "ws://your-gateway:8765/ws", sizeof(g_app.context.config.gateway_url));
    strlcpy(g_app.context.config.device_id, "cardputer-001", sizeof(g_app.context.config.device_id));
    strlcpy(g_app.context.config.device_name, "OpenClaw Cardputer", sizeof(g_app.context.config.device_name));
    strlcpy(g_app.context.config.api_key, "", sizeof(g_app.context.config.api_key));
    
    // WebSocket configuration
    g_app.ws_config.host = "your-gateway";
    g_app.ws_config.port = 8765;
    g_app.ws_config.path = "/ws";
    g_app.ws_config.use_ssl = false;
    g_app.ws_config.device_id = g_app.context.config.device_id;
    g_app.ws_config.device_name = g_app.context.config.device_name;
    g_app.ws_config.firmware_version = FIRMWARE_VERSION;
    g_app.ws_config.api_key = g_app.context.config.api_key;
    
    // Audio configuration
    g_app.audio_config.sample_rate = 16000;
    g_app.audio_config.codec = AudioCodec::OPUS;
    g_app.audio_config.frame_duration_ms = 60;
    g_app.audio_config.vad_enabled = true;
    g_app.audio_config.vad_threshold = 500;
    
    // Display configuration
    g_app.display_config.brightness = 128;
    g_app.display_config.auto_scroll = true;
    
    return true;
}

// =============================================================================
// State Machine Setup
// =============================================================================

// Polyfill for std::make_unique (C++14 feature) for C++11 compatibility
#if __cplusplus < 201402L
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

void setupStateMachine() {
    // Create states
    auto boot_state = make_unique<State>(AppState::BOOT, "Boot");
    auto config_loading = make_unique<State>(AppState::CONFIG_LOADING, "ConfigLoading");
    auto wifi_connecting = make_unique<State>(AppState::WIFI_CONNECTING, "WiFiConnecting");
    auto gateway_connecting = make_unique<State>(AppState::GATEWAY_CONNECTING, "GatewayConnecting");
    auto authenticating = make_unique<State>(AppState::AUTHENTICATING, "Authenticating");
    auto ready = make_unique<State>(AppState::READY, "Ready");
    auto voice_input = make_unique<State>(AppState::VOICE_INPUT, "VoiceInput");
    auto ai_processing = make_unique<State>(AppState::AI_PROCESSING, "AIProcessing");
    auto ai_responding = make_unique<State>(AppState::AI_RESPONDING, "AIResponding");
    auto ancient_mode = make_unique<State>(AppState::ANCIENT_MODE, "AncientMode");
    auto error_state = make_unique<State>(AppState::ERROR_STATE, "Error");
    
    // Boot state transitions
    boot_state->addTransition(AppEvent::BOOT_COMPLETE, AppState::CONFIG_LOADING);
    boot_state->setTimeout(2000, AppState::CONFIG_LOADING);
    
    // Config loading transitions
    config_loading->addTransition(AppEvent::CONFIG_LOADED, AppState::WIFI_CONNECTING);
    config_loading->addTransition(AppEvent::CONFIG_ERROR, AppState::ERROR_STATE);
    
    // WiFi connecting transitions
    wifi_connecting->addTransition(AppEvent::WIFI_CONNECTED, AppState::GATEWAY_CONNECTING);
    wifi_connecting->addTransition(AppEvent::WIFI_ERROR, AppState::ERROR_STATE);
    wifi_connecting->setTimeout(WIFI_CONNECT_TIMEOUT_MS, AppState::ERROR_STATE);
    
    // Gateway connecting transitions
    gateway_connecting->addTransition(AppEvent::GATEWAY_CONNECTED, AppState::AUTHENTICATING);
    gateway_connecting->addTransition(AppEvent::GATEWAY_ERROR, AppState::ERROR_STATE);
    gateway_connecting->addTransition(AppEvent::WIFI_DISCONNECTED, AppState::WIFI_CONNECTING);
    
    // Authenticating transitions
    authenticating->addTransition(AppEvent::AUTHENTICATED, AppState::READY);
    authenticating->addTransition(AppEvent::AUTH_FAILED, AppState::ERROR_STATE);
    authenticating->setTimeout(10000, AppState::ERROR_STATE);
    
    // Ready state transitions
    ready->addTransition(AppEvent::VOICE_KEY_PRESSED, AppState::VOICE_INPUT);
    ready->addTransition(AppEvent::TEXT_SUBMITTED, AppState::AI_PROCESSING);
    ready->addTransition(AppEvent::ANCIENT_MODE_TRIGGER, AppState::ANCIENT_MODE);
    ready->addTransition(AppEvent::WIFI_DISCONNECTED, AppState::WIFI_CONNECTING);
    ready->addTransition(AppEvent::GATEWAY_DISCONNECTED, AppState::GATEWAY_CONNECTING);
    
    // Voice input transitions
    voice_input->addTransition(AppEvent::VOICE_STOPPED, AppState::AI_PROCESSING);
    voice_input->addTransition(AppEvent::VOICE_KEY_PRESSED, AppState::READY);
    voice_input->setTimeout(30000, AppState::READY);  // 30 second timeout
    
    // AI processing transitions
    ai_processing->addTransition(AppEvent::AI_RESPONSE_CHUNK, AppState::AI_RESPONDING);
    ai_processing->addTransition(AppEvent::AI_RESPONSE_COMPLETE, AppState::READY);
    ai_processing->addTransition(AppEvent::AI_ERROR, AppState::READY);
    ai_processing->setTimeout(60000, AppState::READY);  // 60 second timeout
    
    // AI responding transitions
    ai_responding->addTransition(AppEvent::AI_RESPONSE_COMPLETE, AppState::READY);
    ai_responding->addTransition(AppEvent::AI_RESPONSE_CHUNK, AppState::AI_RESPONDING);
    
    // Ancient mode transitions
    ancient_mode->addTransition(AppEvent::ANCIENT_MODE_TRIGGER, AppState::READY);
    ancient_mode->addTransition(AppEvent::TEXT_SUBMITTED, AppState::AI_PROCESSING);
    ancient_mode->setTimeout(300000, AppState::READY);  // 5 minute timeout
    
    // Error state transitions
    error_state->addTransition(AppEvent::ERROR_RECOVERED, AppState::WIFI_CONNECTING);
    error_state->addTransition(AppEvent::FORCE_RECONNECT, AppState::WIFI_CONNECTING);
    error_state->setTimeout(5000, AppState::WIFI_CONNECTING);  // Auto-retry after 5s
    
    // Set state entry/exit actions
    voice_input->setEntryAction([]() {
        g_app.display.addMessage("Listening...", DisplayMessageType::STATUS_MSG);
        g_app.display.setAudioStatus(AudioIndicator::LISTENING);
        g_app.audio.start();
    });
    
    voice_input->setExitAction([]() {
        g_app.audio.stop();
        g_app.display.setAudioStatus(AudioIndicator::IDLE);
    });
    
    ai_processing->setEntryAction([]() {
        g_app.display.setAudioStatus(AudioIndicator::PROCESSING);
    });
    
    ai_responding->setEntryAction([]() {
        g_app.display.setAudioStatus(AudioIndicator::SPEAKING);
    });
    
    ai_responding->setExitAction([]() {
        g_app.display.setAudioStatus(AudioIndicator::IDLE);
    });
    
    ancient_mode->setEntryAction([]() {
        enterAncientMode();
    });
    
    ancient_mode->setExitAction([]() {
        exitAncientMode();
    });
    
    // Add states to machine
    g_app.state_machine.addState(std::move(boot_state));
    g_app.state_machine.addState(std::move(config_loading));
    g_app.state_machine.addState(std::move(wifi_connecting));
    g_app.state_machine.addState(std::move(gateway_connecting));
    g_app.state_machine.addState(std::move(authenticating));
    g_app.state_machine.addState(std::move(ready));
    g_app.state_machine.addState(std::move(voice_input));
    g_app.state_machine.addState(std::move(ai_processing));
    g_app.state_machine.addState(std::move(ai_responding));
    g_app.state_machine.addState(std::move(ancient_mode));
    g_app.state_machine.addState(std::move(error_state));
    
    // Set state change callback
    g_app.state_machine.setOnStateChange(onStateChange);
    
    // Start with boot event
    g_app.state_machine.postEvent(AppEvent::BOOT_COMPLETE);
}

void onStateChange(AppState from, AppState to) {
    Serial.printf("State: %s -> %s\n", appStateToString(from), appStateToString(to));
    
    g_app.context.state.current_state = to;
    
    // Update display based on state
    switch (to) {
        case AppState::WIFI_CONNECTING:
            g_app.display.renderConnectionScreen(g_app.context.config.wifi_ssid);
            connectWiFi();
            break;
            
        case AppState::GATEWAY_CONNECTING:
            g_app.websocket.connect();
            break;
            
        case AppState::READY:
            g_app.display.clearInput();
            g_app.display.setConnectionStatus(ConnectionIndicator::CONNECTED);
            break;
            
        case AppState::ERROR_STATE:
            g_app.display.setConnectionStatus(ConnectionIndicator::ERROR);
            break;
            
        default:
            break;
    }
}

// =============================================================================
// WebSocket Callbacks
// =============================================================================

void setupWebSocketCallbacks() {
    g_app.websocket.onEvent([](WebSocketEvent event, const void* data) {
        switch (event) {
            case WebSocketEvent::CONNECTED:
                Serial.println("WebSocket connected");
                g_app.state_machine.postEvent(AppEvent::GATEWAY_CONNECTED);
                break;
                
            case WebSocketEvent::DISCONNECTED:
                Serial.println("WebSocket disconnected");
                g_app.state_machine.postEvent(AppEvent::GATEWAY_DISCONNECTED);
                break;
                
            case WebSocketEvent::AUTHENTICATED:
                Serial.println("Authenticated");
                g_app.context.state.authenticated = true;
                g_app.state_machine.postEvent(AppEvent::AUTHENTICATED);
                break;
                
            case WebSocketEvent::AUTH_FAILED:
                Serial.println("Auth failed");
                g_app.state_machine.postEvent(AppEvent::AUTH_FAILED);
                break;
                
            case WebSocketEvent::MESSAGE_RECEIVED: {
                auto* msg = static_cast<const ProtocolMessage*>(data);
                processIncomingMessage(*msg);
                break;
            }
                
            case WebSocketEvent::ERROR:
                Serial.println("WebSocket error");
                g_app.state_machine.postEvent(AppEvent::GATEWAY_ERROR);
                break;
                
            default:
                break;
        }
    });
}

void processIncomingMessage(const ProtocolMessage& msg) {
    switch (msg.getType()) {
        case MessageType::RESPONSE:
        case MessageType::RESPONSE_FINAL: {
            String text;
            if (msg.getJsonPayload(text)) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, text);
                if (!err) {
                    const char* response_text = doc["text"] | "";
                    bool is_final = doc["is_final"] | true;
                    
                    g_app.display.addMessage(response_text, 
                        is_final ? DisplayMessageType::AI_MSG : DisplayMessageType::STATUS_MSG);
                    
                    if (is_final) {
                        g_app.state_machine.postEvent(AppEvent::AI_RESPONSE_COMPLETE);
                    } else {
                        g_app.state_machine.postEvent(AppEvent::AI_RESPONSE_CHUNK);
                    }
                }
            }
            break;
        }
            
        case MessageType::STATUS: {
            String text;
            if (msg.getJsonPayload(text)) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, text);
                if (!err) {
                    const char* status = doc["status"] | "";
                    g_app.display.setStatusText(status);
                }
            }
            break;
        }
            
        case MessageType::ERROR: {
            String text;
            if (msg.getJsonPayload(text)) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, text);
                if (!err) {
                    const char* error = doc["error"] | "Unknown error";
                    g_app.display.addMessage(error, DisplayMessageType::ERROR_MSG);
                }
            }
            g_app.state_machine.postEvent(AppEvent::AI_ERROR);
            break;
        }
            
        default:
            break;
    }
}

void sendTextToGateway(const char* text) {
    if (!g_app.websocket.isAuthenticated()) {
        g_app.display.addMessage("Not connected", DisplayMessageType::ERROR_MSG);
        return;
    }
    
    if (!g_app.websocket.sendText(text)) {
        g_app.display.addMessage("Failed to send", DisplayMessageType::ERROR_MSG);
    } else {
        g_app.context.stats.messages_sent++;
    }
}

void sendAudioToGateway(const EncodedAudioPacket& packet) {
    if (!g_app.websocket.isAuthenticated()) return;
    
    // Send audio packet
    g_app.websocket.sendAudio(packet.data.get(), packet.length, packet.is_final);
}

// =============================================================================
// Audio Callbacks
// =============================================================================

void setupAudioCallbacks() {
    g_app.audio.onEvent([](AudioEvent event, const void* data) {
        switch (event) {
            case AudioEvent::VOICE_DETECTED:
                g_app.display.setAudioStatus(AudioIndicator::LISTENING);
                break;
                
            case AudioEvent::VOICE_LOST:
                // Voice ended, could trigger processing
                break;
                
            case AudioEvent::ERROR:
                Serial.println("Audio error");
                break;
                
            default:
                break;
        }
    });
}

// =============================================================================
// Keyboard Callbacks
// =============================================================================

void setupKeyboardCallbacks() {
    g_app.keyboard.onEvent([](KeyboardEvent event, const void* data) {
        switch (event) {
            case KeyboardEvent::KEY_PRESSED: {
                auto* key_event = static_cast<const KeyEvent*>(data);
                
                // Check for voice toggle key
                if (key_event->special == SpecialKey::VOICE_TOGGLE) {
                    g_app.state_machine.postEvent(AppEvent::VOICE_KEY_PRESSED);
                    return;
                }
                
                // Check for ancient mode trigger (Fn+A)
                if (key_event->fn && key_event->character == 'a') {
                    g_app.state_machine.postEvent(AppEvent::ANCIENT_MODE_TRIGGER);
                    return;
                }
                break;
            }
                
            case KeyboardEvent::INPUT_SUBMITTED: {
                auto* text = static_cast<const char*>(data);
                
                // Check for ancient mode trigger phrases
                if (checkAncientModeTrigger(text)) {
                    g_app.state_machine.postEvent(AppEvent::ANCIENT_MODE_TRIGGER);
                    return;
                }
                
                // Display user message
                g_app.display.addMessage(text, DisplayMessageType::USER_MSG);
                
                // Send to gateway
                sendTextToGateway(text);
                
                // Trigger state transition
                g_app.state_machine.postEvent(AppEvent::TEXT_SUBMITTED);
                break;
            }
                
            case KeyboardEvent::INPUT_CHANGED: {
                auto* buffer = static_cast<const InputBuffer*>(data);
                g_app.display.setInputText(buffer->getText(), buffer->getCursor());
                break;
            }
                
            default:
                break;
        }
    });
}

// =============================================================================
// WiFi Management
// =============================================================================

void connectWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", g_app.context.config.wifi_ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_app.context.config.wifi_ssid, g_app.context.config.wifi_password);
    
    g_app.wifi_connect_start = millis();
}

void updateWiFiStatus() {
    static bool was_connected = false;
    bool is_connected = (WiFi.status() == WL_CONNECTED);
    
    if (is_connected != was_connected) {
        was_connected = is_connected;
        
        if (is_connected) {
            Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
            g_app.context.state.wifi_connected = true;
            g_app.state_machine.postEvent(AppEvent::WIFI_CONNECTED);
        } else {
            Serial.println("WiFi disconnected");
            g_app.context.state.wifi_connected = false;
            g_app.state_machine.postEvent(AppEvent::WIFI_DISCONNECTED);
        }
    }
    
    // Update WiFi signal strength
    if (is_connected) {
        int8_t rssi = WiFi.RSSI();
        g_app.display.setWiFiSignal(rssi);
    }
    
    // Auto-reconnect if disconnected
    if (!is_connected && 
        millis() - g_app.wifi_connect_start > WIFI_RECONNECT_INTERVAL_MS) {
        WiFi.reconnect();
        g_app.wifi_connect_start = millis();
    }
}

// =============================================================================
// Display Management
// =============================================================================

void updateDisplay() {
    // Update avatar animation if needed
    // This would call into the avatar system
    
    // Redraw display
    g_app.display.renderMainScreen();
}

void updateStatusBar() {
    // Update connection status
    if (g_app.websocket.isAuthenticated()) {
        g_app.display.setConnectionStatus(ConnectionIndicator::CONNECTED);
    } else if (g_app.websocket.isConnected()) {
        g_app.display.setConnectionStatus(ConnectionIndicator::CONNECTING);
    } else {
        g_app.display.setConnectionStatus(ConnectionIndicator::DISCONNECTED);
    }
    
    // Update audio status based on state
    switch (g_app.state_machine.getCurrentState()) {
        case AppState::VOICE_INPUT:
            g_app.display.setAudioStatus(AudioIndicator::LISTENING);
            break;
        case AppState::AI_PROCESSING:
            g_app.display.setAudioStatus(AudioIndicator::PROCESSING);
            break;
        case AppState::AI_RESPONDING:
            g_app.display.setAudioStatus(AudioIndicator::SPEAKING);
            break;
        default:
            g_app.display.setAudioStatus(AudioIndicator::IDLE);
            break;
    }
}

// =============================================================================
// Ancient Mode
// =============================================================================

void enterAncientMode() {
    g_app.ancient_mode_active = true;
    g_app.ancient_mode_start = millis();
    g_app.display.addMessage("Ancient wisdom awakened...", DisplayMessageType::STATUS_MSG);
    // Additional ancient mode initialization would go here
}

void exitAncientMode() {
    g_app.ancient_mode_active = false;
    g_app.display.addMessage("Returning to present...", DisplayMessageType::STATUS_MSG);
}

bool checkAncientModeTrigger(const char* text) {
    String t = String(text);
    t.toLowerCase();
    
    return (t.indexOf("ancient wisdom") >= 0 ||
            t.indexOf("speak as minerva") >= 0 ||
            t.indexOf("owl mode") >= 0 ||
            t.indexOf("by the thirty-seven claws") >= 0);
}
