/**
 * @file gateway_client.h
 * @brief WebSocket client for OpenClaw gateway communication
 * 
 * Manages real-time bidirectional communication with the
 * gateway bridge server using WebSocket protocol.
 */

#ifndef GATEWAY_CLIENT_H
#define GATEWAY_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "config_manager.h"

namespace OpenClaw {

// WebSocket constants
constexpr size_t WS_BUFFER_SIZE = 4096;
constexpr size_t WS_QUEUE_LENGTH = 16;
constexpr uint32_t WS_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WS_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t WS_PING_INTERVAL_MS = 30000;

/**
 * @brief Message types for gateway protocol
 */
enum class GatewayMessageType : uint8_t {
    UNKNOWN = 0,
    AUTH,           // Authentication
    AUTH_RESPONSE,  // Auth result
    AUDIO,          // Audio data (base64)
    TEXT,           // Text message
    RESPONSE,       // AI response
    STATUS,         // Status update
    PING,           // Keepalive ping
    PONG,           // Keepalive pong
    ERROR,          // Error message
    COMMAND         // Control command
};

/**
 * @brief Connection state
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    AUTHENTICATED,
    RECONNECTING,
    ERROR
};

/**
 * @brief Gateway message structure
 */
struct GatewayMessage {
    GatewayMessageType type;
    String payload;
    JsonDocument* json_data;
    uint32_t timestamp;
    
    GatewayMessage() : type(GatewayMessageType::UNKNOWN), 
                       json_data(nullptr), timestamp(0) {}
    
    ~GatewayMessage() {
        if (json_data) {
            delete json_data;
            json_data = nullptr;
        }
    }
    
    // Move constructor
    GatewayMessage(GatewayMessage&& other) noexcept
        : type(other.type), payload(other.payload),
          json_data(other.json_data), timestamp(other.timestamp) {
        other.json_data = nullptr;
    }
    
    // Move assignment
    GatewayMessage& operator=(GatewayMessage&& other) noexcept {
        if (this != &other) {
            type = other.type;
            payload = other.payload;
            if (json_data) delete json_data;
            json_data = other.json_data;
            timestamp = other.timestamp;
            other.json_data = nullptr;
        }
        return *this;
    }
    
    // Disable copy
    GatewayMessage(const GatewayMessage&) = delete;
    GatewayMessage& operator=(const GatewayMessage&) = delete;
};

/**
 * @brief Gateway client callback interface
 */
class GatewayClientCallback {
public:
    virtual ~GatewayClientCallback() = default;
    
    /**
     * @brief Called when connection established
     */
    virtual void onConnected() {}
    
    /**
     * @brief Called when connection closed
     * @param code Close code
     * @param reason Close reason
     */
    virtual void onDisconnected(uint16_t code, const char* reason) {}
    
    /**
     * @brief Called on authentication success
     */
    virtual void onAuthenticated() {}
    
    /**
     * @brief Called on authentication failure
     * @param error Error message
     */
    virtual void onAuthFailed(const char* error) {}
    
    /**
     * @brief Called when text response received
     * @param text Response text
     * @param is_final true if final response
     */
    virtual void onTextResponse(const char* text, bool is_final) {}
    
    /**
     * @brief Called when audio response received
     * @param audio_data Base64 audio data
     */
    virtual void onAudioResponse(const char* audio_data) {}
    
    /**
     * @brief Called on error
     * @param error Error message
     */
    virtual void onError(const char* error) {}
    
    /**
     * @brief Called when connection state changes
     * @param state New connection state
     */
    virtual void onStateChanged(ConnectionState state) {}
};

/**
 * @brief Gateway client class
 * 
 * Manages WebSocket connection to the OpenClaw gateway bridge
 * with automatic reconnection and message queuing.
 */
class GatewayClient {
public:
    GatewayClient();
    ~GatewayClient();
    
    // Disable copy
    GatewayClient(const GatewayClient&) = delete;
    GatewayClient& operator=(const GatewayClient&) = delete;
    
    /**
     * @brief Initialize client
     * @param config Gateway configuration
     * @return true if initialization successful
     */
    bool begin(const GatewayConfig& config, const DeviceConfig& device);
    
    /**
     * @brief Connect to gateway
     * @return true if connection initiated
     */
    bool connect();
    
    /**
     * @brief Disconnect from gateway
     */
    void disconnect();
    
    /**
     * @brief Update client (call in loop)
     */
    void update();
    
    /**
     * @brief Deinitialize client
     */
    void end();
    
    /**
     * @brief Send text message
     * @param text Message text
     * @return true if queued for sending
     */
    bool sendText(const char* text);
    
    /**
     * @brief Send audio data
     * @param audio_data Base64 encoded audio
     * @param is_final true if final audio chunk
     * @return true if queued for sending
     */
    bool sendAudio(const char* audio_data, bool is_final = false);
    
    /**
     * @brief Send raw audio bytes
     * @param data Audio data buffer
     * @param length Data length
     * @param is_final true if final chunk
     * @return true if queued for sending
     */
    bool sendAudioRaw(const uint8_t* data, size_t length, bool is_final = false);
    
    /**
     * @brief Send ping message
     * @return true if sent
     */
    bool sendPing();
    
    /**
     * @brief Send custom JSON message
     * @param json JSON document
     * @return true if queued for sending
     */
    bool sendJson(const JsonDocument& json);
    
    /**
     * @brief Set callback for events
     */
    void setCallback(GatewayClientCallback* callback);
    
    /**
     * @brief Check if connected and authenticated
     */
    bool isReady() const { return state_ == ConnectionState::AUTHENTICATED; }
    
    /**
     * @brief Check if connected
     */
    bool isConnected() const { return state_ == ConnectionState::CONNECTED || 
                                      state_ == ConnectionState::AUTHENTICATING ||
                                      state_ == ConnectionState::AUTHENTICATED; }
    
    /**
     * @brief Get current connection state
     */
    ConnectionState getState() const { return state_; }
    
    /**
     * @brief Get state as string
     */
    const char* getStateString() const;
    
    /**
     * @brief Read received message (non-blocking)
     * @param msg Output message
     * @return true if message available
     */
    bool readMessage(GatewayMessage& msg);
    
    /**
     * @brief Get last error message
     */
    const char* getLastError() const { return last_error_; }
    
    /**
     * @brief Get connection duration in milliseconds
     */
    uint32_t getConnectionDuration() const;
    
    /**
     * @brief Get messages sent count
     */
    uint32_t getMessagesSent() const { return messages_sent_; }
    
    /**
     * @brief Get messages received count
     */
    uint32_t getMessagesReceived() const { return messages_received_; }

private:
    // WebSocket client
    WebSocketsClient ws_client_;
    
    // Configuration
    GatewayConfig gateway_config_;
    DeviceConfig device_config_;
    
    // State
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    GatewayClientCallback* callback_ = nullptr;
    
    // Connection info
    String host_;
    uint16_t port_ = 8765;
    String path_;
    bool use_ssl_ = false;
    
    // Timing
    uint32_t last_connect_attempt_ = 0;
    uint32_t last_ping_time_ = 0;
    uint32_t connection_start_time_ = 0;
    uint32_t reconnect_delay_ms_ = WS_RECONNECT_INTERVAL_MS;
    
    // Statistics
    uint32_t messages_sent_ = 0;
    uint32_t messages_received_ = 0;
    uint32_t reconnect_count_ = 0;
    
    // Message queue
    QueueHandle_t receive_queue_ = nullptr;
    
    // Error
    char last_error_[128];
    
    // Task stack size for async mode
    static constexpr uint32_t TASK_STACK_SIZE = 8192;
    static constexpr UBaseType_t TASK_PRIORITY = 4;
    
    // Private methods
    bool parseUrl(const char* url);
    bool createQueue();
    void destroyQueue();
    void setState(ConnectionState state);
    void handleConnect();
    void handleDisconnect(uint16_t code);
    void handleMessage(const char* data);
    void sendAuth();
    void processIncomingMessage(const JsonDocument& doc);
    GatewayMessageType parseMessageType(const char* type_str);
    const char* messageTypeToString(GatewayMessageType type);
    bool queueMessage(GatewayMessage&& msg);
    
    // WebSocket event handler (static wrapper)
    static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    
    // Instance pointer for static callback
    static GatewayClient* instance_;
};

} // namespace OpenClaw

#endif // GATEWAY_CLIENT_H
