/**
 * @file main.cpp
 * @brief OpenClaw Cardputer ADV - Main Application with Procedural Avatar
 * 
 * Thin client firmware for M5Stack Cardputer ADV that connects
 * to an OpenClaw gateway for voice and text AI interaction.
 * Features a real-time procedural owl avatar.
 */

#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string>

#include "config_manager.h"
#include "audio_capture.h"
#include "keyboard_input.h"
#include "display_manager.h"
#include "gateway_client.h"
#include "avatar/procedural_avatar.h"

using namespace OpenClaw;
using namespace Avatar;

// =============================================================================
// Constants
// =============================================================================

constexpr const char* FIRMWARE_VERSION = "1.1.0";
constexpr const char* FIRMWARE_NAME = "OpenClaw Cardputer";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t MAIN_LOOP_DELAY_MS = 16;  // ~60 FPS for avatar
constexpr uint32_t AVATAR_UPDATE_MS = 33;    // ~30 FPS avatar update

// Avatar display region
constexpr int16_t AVATAR_HEIGHT = 128;
constexpr int16_t TEXT_AREA_Y = AVATAR_HEIGHT;
constexpr int16_t TEXT_AREA_HEIGHT = 135 - AVATAR_HEIGHT;

// =============================================================================
// Global State
// =============================================================================

enum class AppState {
    BOOT,
    CONFIG_LOAD,
    WIFI_CONNECT,
    GATEWAY_CONNECT,
    READY,
    VOICE_INPUT,
    THINKING,
    SPEAKING,
    ERROR,
    ANCIENT_MODE
};

struct AppContext {
    AppState state = AppState::BOOT;
    AppState previous_state = AppState::BOOT;
    uint32_t state_enter_time = 0;
    
    // Managers
    ConfigManager config;
    AudioCapture audio;
    KeyboardInput keyboard;
    DisplayManager display;
    GatewayClient gateway;
    ProceduralAvatar avatar;
    
    // Runtime state
    bool wifi_connected = false;
    bool gateway_ready = false;
    bool voice_mode = false;
    uint32_t last_wifi_reconnect = 0;
    uint32_t last_activity = 0;
    uint32_t last_avatar_update = 0;
    
    // Audio buffer for voice streaming
    String audio_buffer;
    
    // Ancient mode trigger
    bool ancient_mode_active = false;
};

static AppContext g_app;

// =============================================================================
// Forward Declarations
// =============================================================================

void changeState(AppState new_state);
void stateBoot();
void stateConfigLoad();
void stateWiFiConnect();
void stateGatewayConnect();
void stateReady();
void stateVoiceInput();
void stateThinking();
void stateSpeaking();
void stateError();
void stateAncientMode();

void connectWiFi();
void disconnectWiFi();
void updateWiFiStatus();
void updateAvatar();

void sendTextMessage(const char* text);
void sendAudioChunk(const uint8_t* data, size_t len, bool final);
void handleGatewayMessage(const GatewayMessage& msg);
void checkAncientModeTrigger(const char* text);

// =============================================================================
// Callback Classes
// =============================================================================

class AppKeyboardCallback : public KeyboardCallback {
public:
    void onKeyEvent(const KeyEvent& event) override {
        // Handle special keys
        if (event.special == SpecialKey::VOICE_KEY) {
            if (event.pressed) {
                if (g_app.state == AppState::VOICE_INPUT) {
                    changeState(AppState::READY);
                } else {
                    changeState(AppState::VOICE_INPUT);
                }
            }
            return;
        }
        
        // Ancient mode trigger: Fn+A
        if (event.fn && event.character == 'a') {
            if (g_app.state == AppState::ANCIENT_MODE) {
                changeState(AppState::READY);
            } else {
                changeState(AppState::ANCIENT_MODE);
            }
            return;
        }
        
        // Look at keyboard when typing
        if (event.isPrintable() && event.pressed) {
            g_app.avatar.lookAt(InputSource::KEYBOARD);
            
            // Reset look after delay
            static uint32_t last_type_time = 0;
            last_type_time = millis();
        }
        
        // Update activity timestamp
        g_app.last_activity = millis();
    }
    
    void onInputSubmit(const char* text) override {
        if (strlen(text) == 0) return;
        
        // Check for ancient mode trigger phrase
        checkAncientModeTrigger(text);
        
        // Display user message
        g_app.display.addMessage(text, MessageType::USER);
        
        // Avatar looks at user when sending
        g_app.avatar.lookAt(InputSource::USER);
        
        // Send to gateway
        sendTextMessage(text);
        
        g_app.last_activity = millis();
    }
    
    void onInputChanged(const InputBuffer& buffer) override {
        g_app.display.setInputText(buffer.c_str(), buffer.cursor);
    }
};

class AppAudioCallback : public AudioCaptureCallback {
public:
    void onAudioFrame(const AudioFrame& frame) override {
        if (g_app.state != AppState::VOICE_INPUT) return;
        
        // Stream audio to gateway
        if (frame.voice_detected || frame.num_samples > 0) {
            // Encode and send
            g_app.gateway.sendAudioRaw(
                reinterpret_cast<const uint8_t*>(frame.samples),
                frame.num_samples * sizeof(int16_t),
                false
            );
        }
    }
    
    void onVoiceActivity(bool detected) override {
        if (detected) {
            g_app.display.setAudioStatus(AudioStatus::LISTENING);
            // Avatar looks at mic when voice detected
            g_app.avatar.lookAt(InputSource::MIC);
        } else {
            g_app.display.setAudioStatus(AudioStatus::IDLE);
            // Return to center
            g_app.avatar.lookAt(InputSource::CENTER);
        }
    }
    
    void onAudioError(int error) override {
        g_app.display.addMessagef(MessageType::ERROR, "Audio error: %d", error);
        g_app.avatar.triggerError();
    }
};

class AppGatewayCallback : public GatewayClientCallback {
public:
    void onConnected() override {
        g_app.display.addMessage("Gateway connected", MessageType::STATUS);
    }
    
    void onDisconnected(uint16_t code, const char* reason) override {
        g_app.display.addMessagef(MessageType::STATUS, 
            "Gateway disconnected: %d", code);
        g_app.gateway_ready = false;
        g_app.avatar.setMood(Mood::IDLE);
    }
    
    void onAuthenticated() override {
        g_app.display.addMessage("Authenticated", MessageType::STATUS);
        g_app.gateway_ready = true;
        g_app.avatar.blink(BlinkType::DOUBLE);  // Happy blink
        changeState(AppState::READY);
    }
    
    void onAuthFailed(const char* error) override {
        g_app.display.addMessagef(MessageType::ERROR, "Auth failed: %s", error);
        g_app.avatar.triggerError();
        changeState(AppState::ERROR);
    }
    
    void onTextResponse(const char* text, bool is_final) override {
        // Add to display
        g_app.display.addMessage(text, MessageType::AI);
        
        if (is_final) {
            g_app.display.setAudioStatus(AudioStatus::IDLE);
            // Avatar speaks the response
            g_app.avatar.speak(text);
            changeState(AppState::SPEAKING);
        } else {
            g_app.display.setAudioStatus(AudioStatus::PROCESSING);
            g_app.avatar.setMood(Mood::THINKING);
            changeState(AppState::THINKING);
        }
    }
    
    void onAudioResponse(const char* audio_data) override {
        // Audio response received
        g_app.display.setAudioStatus(AudioStatus::SPEAKING);
    }
    
    void onError(const char* error) override {
        g_app.display.addMessagef(MessageType::ERROR, "Gateway error: %s", error);
        g_app.avatar.triggerError();
    }
    
    void onStateChanged(ConnectionState state) override {
        switch (state) {
            case ConnectionState::DISCONNECTED:
                g_app.display.setConnectionStatus(ConnectionStatus::DISCONNECTED);
                break;
            case ConnectionState::CONNECTING:
            case ConnectionState::RECONNECTING:
                g_app.display.setConnectionStatus(ConnectionStatus::CONNECTING);
                g_app.avatar.setMood(Mood::LISTENING);  // Attentive while connecting
                break;
            case ConnectionState::CONNECTED:
            case ConnectionState::AUTHENTICATING:
                g_app.display.setConnectionStatus(ConnectionStatus::CONNECTING);
                break;
            case ConnectionState::AUTHENTICATED:
                g_app.display.setConnectionStatus(ConnectionStatus::CONNECTED);
                break;
            case ConnectionState::ERROR:
                g_app.display.setConnectionStatus(ConnectionStatus::ERROR);
                g_app.avatar.triggerError();
                break;
        }
    }
};

// =============================================================================
// Static Callback Instances
// =============================================================================

static AppKeyboardCallback s_keyboard_callback;
static AppAudioCallback s_audio_callback;
static AppGatewayCallback s_gateway_callback;

// =============================================================================
// Setup and Main Loop
// =============================================================================

void setup() {
    // Initialize M5Cardputer
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println(FIRMWARE_NAME);
    Serial.printf("Version: %s\n", FIRMWARE_VERSION);
    Serial.println("Procedural Avatar System Enabled");
    Serial.println("========================================\n");
    
    // Initialize display early for boot screen
    g_app.display.begin(DeviceConfig());
    g_app.display.showBootScreen(FIRMWARE_VERSION);
    
    // Set initial state
    changeState(AppState::BOOT);
}

void loop() {
    uint32_t now = millis();
    
    // Update M5 systems
    M5Cardputer.update();
    
    // Update subsystems
    g_app.keyboard.update();
    g_app.gateway.update();
    g_app.display.update();
    
    // Update avatar at ~30 FPS
    if (now - g_app.last_avatar_update >= AVATAR_UPDATE_MS) {
        updateAvatar();
        g_app.last_avatar_update = now;
    }
    
    // Process gateway messages
    GatewayMessage msg;
    while (g_app.gateway.readMessage(msg)) {
        handleGatewayMessage(msg);
    }
    
    // State machine
    switch (g_app.state) {
        case AppState::BOOT:
            stateBoot();
            break;
        case AppState::CONFIG_LOAD:
            stateConfigLoad();
            break;
        case AppState::WIFI_CONNECT:
            stateWiFiConnect();
            break;
        case AppState::GATEWAY_CONNECT:
            stateGatewayConnect();
            break;
        case AppState::READY:
            stateReady();
            break;
        case AppState::VOICE_INPUT:
            stateVoiceInput();
            break;
        case AppState::THINKING:
            stateThinking();
            break;
        case AppState::SPEAKING:
            stateSpeaking();
            break;
        case AppState::ERROR:
            stateError();
            break;
        case AppState::ANCIENT_MODE:
            stateAncientMode();
            break;
    }
    
    // Update WiFi status
    updateWiFiStatus();
    
    delay(MAIN_LOOP_DELAY_MS);
}

// =============================================================================
// State Machine Implementation
// =============================================================================

void changeState(AppState new_state) {
    if (g_app.state == new_state) return;
    
    g_app.previous_state = g_app.state;
    g_app.state = new_state;
    g_app.state_enter_time = millis();
    
    Serial.printf("State: %d -> %d\n", g_app.previous_state, new_state);
    
    // State exit actions
    switch (g_app.previous_state) {
        case AppState::VOICE_INPUT:
            g_app.audio.stop();
            g_app.display.setAudioStatus(AudioStatus::IDLE);
            g_app.gateway.sendAudio("", true);
            g_app.avatar.setMood(Mood::IDLE);
            break;
        case AppState::SPEAKING:
            g_app.avatar.stopSpeaking();
            break;
        case AppState::THINKING:
            g_app.avatar.setMood(Mood::IDLE);
            break;
        case AppState::ANCIENT_MODE:
            g_app.avatar.setAncientMode(false);
            g_app.ancient_mode_active = false;
            break;
        default:
            break;
    }
    
    // State entry actions
    switch (new_state) {
        case AppState::VOICE_INPUT:
            g_app.display.addMessage("Listening...", MessageType::STATUS);
            g_app.audio.start();
            g_app.avatar.setMood(Mood::LISTENING);
            g_app.avatar.lookAt(InputSource::MIC);
            break;
        case AppState::READY:
            g_app.display.clearInput();
            g_app.avatar.setMood(Mood::IDLE);
            g_app.avatar.lookAt(InputSource::CENTER);
            break;
        case AppState::THINKING:
            g_app.avatar.setMood(Mood::THINKING);
            g_app.avatar.lookAt(InputSource::USER);
            break;
        case AppState::SPEAKING:
            g_app.avatar.setMood(Mood::SPEAKING);
            g_app.avatar.lookAt(InputSource::USER);
            break;
        case AppState::ERROR:
            g_app.avatar.triggerError();
            break;
        case AppState::ANCIENT_MODE:
            g_app.ancient_mode_active = true;
            g_app.avatar.setAncientMode(true);
            g_app.display.addMessage("Ancient wisdom awakened...", MessageType::STATUS);
            break;
        default:
            break;
    }
}

void stateBoot() {
    // Short boot delay then move to config load
    if (millis() - g_app.state_enter_time > 2000) {
        changeState(AppState::CONFIG_LOAD);
    }
}

void stateConfigLoad() {
    // Initialize configuration manager
    if (!g_app.config.begin()) {
        g_app.display.showErrorScreen(g_app.config.getLastError());
        changeState(AppState::ERROR);
        return;
    }
    
    // Print config for debugging
    g_app.config.printConfig();
    
    // Check if WiFi is configured
    if (!g_app.config.wifi().isValid()) {
        g_app.display.showErrorScreen("WiFi not configured");
        changeState(AppState::ERROR);
        return;
    }
    
    // Initialize keyboard
    g_app.keyboard.begin();
    g_app.keyboard.setCallback(&s_keyboard_callback);
    
    // Initialize audio
    g_app.audio.begin(g_app.config.audio());
    g_app.audio.setCallback(&s_audio_callback);
    
    // Initialize gateway client
    g_app.gateway.begin(g_app.config.gateway(), g_app.config.device());
    g_app.gateway.setCallback(&s_gateway_callback);
    
    // Initialize avatar
    g_app.avatar.begin(&M5Cardputer.Display);
    g_app.avatar.setMood(Mood::IDLE);
    
    changeState(AppState::WIFI_CONNECT);
}

void stateWiFiConnect() {
    // Show connection screen
    g_app.display.showConnectionScreen(g_app.config.wifi().ssid.c_str());
    
    // Attempt connection
    connectWiFi();
    
    // Wait for connection
    uint32_t elapsed = millis() - g_app.state_enter_time;
    
    if (WiFi.status() == WL_CONNECTED) {
        g_app.wifi_connected = true;
        g_app.display.addMessagef(MessageType::STATUS, 
            "WiFi connected: %s", WiFi.localIP().toString().c_str());
        g_app.avatar.setMood(Mood::LISTENING);  // Attentive
        changeState(AppState::GATEWAY_CONNECT);
    } else if (elapsed > WIFI_CONNECT_TIMEOUT_MS) {
        g_app.display.addMessage("WiFi connection timeout", MessageType::ERROR);
        g_app.avatar.triggerError();
        g_app.last_wifi_reconnect = millis();
        changeState(AppState::ERROR);
    }
}

void stateGatewayConnect() {
    if (!g_app.wifi_connected) {
        changeState(AppState::WIFI_CONNECT);
        return;
    }
    
    // Initiate gateway connection
    if (g_app.gateway.getState() == ConnectionState::DISCONNECTED) {
        g_app.gateway.connect();
    }
    
    // Wait for authentication (handled in callback)
    uint32_t elapsed = millis() - g_app.state_enter_time;
    if (elapsed > g_app.config.gateway().connection_timeout_ms &&
        !g_app.gateway_ready) {
        g_app.display.addMessage("Gateway connection timeout", MessageType::ERROR);
        g_app.avatar.triggerError();
        changeState(AppState::ERROR);
    }
}

void stateReady() {
    // Normal operation - handled by callbacks
    
    // Check for connection loss
    if (!g_app.wifi_connected) {
        changeState(AppState::WIFI_CONNECT);
        return;
    }
    
    if (!g_app.gateway.isConnected() && !g_app.gateway_ready) {
        changeState(AppState::GATEWAY_CONNECT);
        return;
    }
    
    // Return to idle mood if not already
    if (g_app.avatar.getCurrentMood() != Mood::IDLE) {
        g_app.avatar.setMood(Mood::IDLE);
    }
}

void stateVoiceInput() {
    // Audio capture is handled in callback
    
    // Check for connection loss
    if (!g_app.wifi_connected) {
        changeState(AppState::WIFI_CONNECT);
        return;
    }
    
    // Check for voice timeout (30 seconds max)
    uint32_t elapsed = millis() - g_app.state_enter_time;
    if (elapsed > 30000) {
        g_app.display.addMessage("Voice timeout", MessageType::STATUS);
        changeState(AppState::READY);
    }
}

void stateThinking() {
    // Wait for response to complete
    // State transitions handled in gateway callback
    
    // Timeout after 60 seconds
    uint32_t elapsed = millis() - g_app.state_enter_time;
    if (elapsed > 60000) {
        g_app.display.addMessage("Response timeout", MessageType::ERROR);
        changeState(AppState::READY);
    }
}

void stateSpeaking() {
    // Wait for speaking animation to complete
    if (!g_app.avatar.isSpeaking()) {
        changeState(AppState::READY);
    }
    
    // Timeout after response complete + 5 seconds
    uint32_t elapsed = millis() - g_app.state_enter_time;
    if (elapsed > 30000) {
        changeState(AppState::READY);
    }
}

void stateError() {
    // Try to recover after delay
    uint32_t elapsed = millis() - g_app.state_enter_time;
    
    if (elapsed > 5000) {
        // Try to reconnect WiFi if needed
        if (!g_app.wifi_connected) {
            changeState(AppState::WIFI_CONNECT);
        } else if (!g_app.gateway_ready) {
            changeState(AppState::GATEWAY_CONNECT);
        } else {
            changeState(AppState::READY);
        }
    }
}

void stateAncientMode() {
    // Ancient mode stays active until manually exited (Fn+A)
    // Or after 5 minutes of inactivity
    uint32_t elapsed = millis() - g_app.last_activity;
    if (elapsed > 300000) {  // 5 minutes
        changeState(AppState::READY);
    }
}

// =============================================================================
// Avatar Update
// =============================================================================

void updateAvatar() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    float deltaMs = now - lastUpdate;
    lastUpdate = now;
    
    // Update avatar animation
    g_app.avatar.update(deltaMs);
    
    // Render avatar (top portion of screen)
    g_app.avatar.render();
}

// =============================================================================
// WiFi Functions
// =============================================================================

void connectWiFi() {
    const WiFiConfig& wifi = g_app.config.wifi();
    
    Serial.printf("Connecting to WiFi: %s\n", wifi.ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    
    if (!wifi.dhcp && wifi.static_ip.length() > 0) {
        IPAddress ip, gateway, subnet;
        ip.fromString(wifi.static_ip);
        gateway.fromString(wifi.gateway);
        subnet.fromString(wifi.subnet);
        WiFi.config(ip, gateway, subnet);
    }
    
    WiFi.begin(wifi.ssid.c_str(), wifi.password.c_str());
}

void disconnectWiFi() {
    WiFi.disconnect();
    g_app.wifi_connected = false;
}

void updateWiFiStatus() {
    static int8_t last_rssi = 0;
    
    bool currently_connected = (WiFi.status() == WL_CONNECTED);
    
    if (currently_connected != g_app.wifi_connected) {
        g_app.wifi_connected = currently_connected;
        if (currently_connected) {
            Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("WiFi disconnected");
        }
    }
    
    // Update signal strength
    if (currently_connected) {
        int8_t rssi = WiFi.RSSI();
        if (abs(rssi - last_rssi) > 5) {
            g_app.display.setWiFiSignal(rssi);
            last_rssi = rssi;
        }
    } else {
        g_app.display.setWiFiSignal(-100);
    }
    
    // Auto-reconnect
    if (!currently_connected && 
        millis() - g_app.last_wifi_reconnect > WIFI_RECONNECT_INTERVAL_MS) {
        g_app.last_wifi_reconnect = millis();
        Serial.println("WiFi reconnecting...");
        WiFi.reconnect();
    }
}

// =============================================================================
// Communication Functions
// =============================================================================

void sendTextMessage(const char* text) {
    if (!g_app.gateway.isReady()) {
        g_app.display.addMessage("Not connected to gateway", MessageType::ERROR);
        return;
    }
    
    // Set thinking mood while waiting
    g_app.avatar.setMood(Mood::THINKING);
    changeState(AppState::THINKING);
    
    if (!g_app.gateway.sendText(text)) {
        g_app.display.addMessage("Failed to send message", MessageType::ERROR);
        g_app.avatar.triggerError();
    }
}

void sendAudioChunk(const uint8_t* data, size_t len, bool final) {
    if (!g_app.gateway.isReady()) return;
    
    g_app.gateway.sendAudioRaw(data, len, final);
}

void handleGatewayMessage(const GatewayMessage& msg) {
    // Handle custom messages not processed by callback
    switch (msg.type) {
        case GatewayMessageType::COMMAND: {
            // Process command from gateway
            const char* cmd = msg.payload.c_str();
            if (strcmp(cmd, "blink") == 0) {
                g_app.avatar.blink(BlinkType::SINGLE);
            } else if (strcmp(cmd, "judge") == 0) {
                g_app.avatar.setMood(Mood::JUDGING);
                g_app.avatar.blink(BlinkType::SLOW);
            } else if (strcmp(cmd, "excited") == 0) {
                g_app.avatar.setMood(Mood::EXCITED);
            }
            break;
        }
        default:
            break;
    }
}

// =============================================================================
// Special Features
// =============================================================================

void checkAncientModeTrigger(const char* text) {
    // Check for ancient mode trigger phrases
    String t = String(text);
    t.toLowerCase();
    
    if (t.indexOf("ancient wisdom") >= 0 ||
        t.indexOf("speak as minerva") >= 0 ||
        t.indexOf("owl mode") >= 0 ||
        t.indexOf("by the thirty-seven claws") >= 0) {
        changeState(AppState::ANCIENT_MODE);
    }
}
