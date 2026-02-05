/**
 * @file main.cpp
 * @brief OpenClaw Cardputer ADV - Main Application
 * 
 * Thin client firmware for M5Stack Cardputer ADV that connects
 * to an OpenClaw gateway for voice and text AI interaction.
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

using namespace OpenClaw;

// =============================================================================
// Constants
// =============================================================================

constexpr const char* FIRMWARE_VERSION = "1.0.0";
constexpr const char* FIRMWARE_NAME = "OpenClaw Cardputer";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t MAIN_LOOP_DELAY_MS = 10;

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
    ERROR
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
    
    // Runtime state
    bool wifi_connected = false;
    bool gateway_ready = false;
    bool voice_mode = false;
    uint32_t last_wifi_reconnect = 0;
    uint32_t last_activity = 0;
    
    // Audio buffer for voice streaming
    String audio_buffer;
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
void stateError();

void connectWiFi();
void disconnectWiFi();
void updateWiFiStatus();

void sendTextMessage(const char* text);
void sendAudioChunk(const uint8_t* data, size_t len, bool final);
void handleGatewayMessage(const GatewayMessage& msg);

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
        
        // Update activity timestamp
        g_app.last_activity = millis();
    }
    
    void onInputSubmit(const char* text) override {
        if (strlen(text) == 0) return;
        
        // Display user message
        g_app.display.addMessage(text, MessageType::USER);
        
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
        } else {
            g_app.display.setAudioStatus(AudioStatus::IDLE);
        }
    }
    
    void onAudioError(int error) override {
        g_app.display.addMessagef(MessageType::ERROR, "Audio error: %d", error);
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
    }
    
    void onAuthenticated() override {
        g_app.display.addMessage("Authenticated", MessageType::STATUS);
        g_app.gateway_ready = true;
        changeState(AppState::READY);
    }
    
    void onAuthFailed(const char* error) override {
        g_app.display.addMessagef(MessageType::ERROR, "Auth failed: %s", error);
        changeState(AppState::ERROR);
    }
    
    void onTextResponse(const char* text, bool is_final) override {
        g_app.display.addMessage(text, MessageType::AI);
        
        if (is_final) {
            g_app.display.setAudioStatus(AudioStatus::IDLE);
        } else {
            g_app.display.setAudioStatus(AudioStatus::PROCESSING);
        }
    }
    
    void onAudioResponse(const char* audio_data) override {
        // Audio response received - could play back if speaker available
        g_app.display.setAudioStatus(AudioStatus::SPEAKING);
    }
    
    void onError(const char* error) override {
        g_app.display.addMessagef(MessageType::ERROR, "Gateway error: %s", error);
    }
    
    void onStateChanged(ConnectionState state) override {
        switch (state) {
            case ConnectionState::DISCONNECTED:
                g_app.display.setConnectionStatus(ConnectionStatus::DISCONNECTED);
                break;
            case ConnectionState::CONNECTING:
            case ConnectionState::RECONNECTING:
                g_app.display.setConnectionStatus(ConnectionStatus::CONNECTING);
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
    Serial.println("========================================\n");
    
    // Initialize display early for boot screen
    g_app.display.begin(DeviceConfig());
    g_app.display.showBootScreen(FIRMWARE_VERSION);
    
    // Set initial state
    changeState(AppState::BOOT);
}

void loop() {
    // Update M5 systems
    M5Cardputer.update();
    
    // Update subsystems
    g_app.keyboard.update();
    g_app.gateway.update();
    g_app.display.update();
    
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
        case AppState::ERROR:
            stateError();
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
            // Send final audio marker
            g_app.gateway.sendAudio("", true);
            break;
        default:
            break;
    }
    
    // State entry actions
    switch (new_state) {
        case AppState::VOICE_INPUT:
            g_app.display.addMessage("Listening...", MessageType::STATUS);
            g_app.audio.start();
            break;
        case AppState::READY:
            g_app.display.clearInput();
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
        changeState(AppState::GATEWAY_CONNECT);
    } else if (elapsed > WIFI_CONNECT_TIMEOUT_MS) {
        g_app.display.addMessage("WiFi connection timeout", MessageType::ERROR);
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
    
    if (!g_app.gateway.sendText(text)) {
        g_app.display.addMessage("Failed to send message", MessageType::ERROR);
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
            break;
        }
        default:
            break;
    }
}
