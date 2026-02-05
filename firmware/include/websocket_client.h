/**
 * @file websocket_client.h
 * @brief Improved WebSocket client for OpenClaw gateway
 * 
 * Features:
 * - Automatic reconnection with exponential backoff
 * - Binary protocol support
 * - Connection state machine
 * - Message queuing
 * - Ping/pong keepalive
 * - Thread-safe operation
 */

#ifndef OPENCLAW_WEBSOCKET_CLIENT_H
#define OPENCLAW_WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <memory>
#include <functional>
#include "protocol.h"

namespace OpenClaw {

// Forward declarations
class WebSocketClient;

// Connection state
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    WAITING_AUTH,
    AUTHENTICATED,
    RECONNECTING,
    ERROR
};

// Connection configuration
struct WebSocketConfig {
    String host;
    uint16_t port;
    String path;
    bool use_ssl;
    String api_key;
    String device_id;
    String device_name;
    String firmware_version;
    
    // Timing parameters
    uint32_t connect_timeout_ms;
    uint32_t reconnect_interval_ms;
    uint32_t reconnect_max_interval_ms;
    uint32_t ping_interval_ms;
    uint32_t pong_timeout_ms;
    uint8_t max_reconnect_attempts;
    
    // Queue sizes
    size_t send_queue_size;
    size_t receive_queue_size;
    
    WebSocketConfig()
        : host(""), port(8765), path("/ws"), use_ssl(false),
          api_key(""), device_id(""), device_name("Cardputer"), firmware_version("2.0.0"),
          connect_timeout_ms(10000), reconnect_interval_ms(1000),
          reconnect_max_interval_ms(60000), ping_interval_ms(30000),
          pong_timeout_ms(5000), max_reconnect_attempts(0), // 0 = unlimited
          send_queue_size(16), receive_queue_size(16) {}
};

// Connection statistics
struct ConnectionStats {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t messages_dropped;
    uint32_t reconnect_count;
    uint32_t ping_count;
    uint32_t pong_count;
    uint32_t errors;
    uint32_t connection_duration_ms;
    int8_t last_rssi;
    
    ConnectionStats()
        : messages_sent(0), messages_received(0), messages_dropped(0),
          reconnect_count(0), ping_count(0), pong_count(0), errors(0),
          connection_duration_ms(0), last_rssi(0) {}
};

// Event types
enum class WebSocketEvent {
    CONNECTED,
    DISCONNECTED,
    AUTHENTICATED,
    AUTH_FAILED,
    MESSAGE_RECEIVED,
    ERROR,
    STATE_CHANGED
};

// Event callback type
using WebSocketEventCallback = std::function<void(WebSocketEvent event, const void* data)>;

// WebSocket client class
class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();
    
    // Disable copy
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;
    
    // Initialize with configuration
    bool begin(const WebSocketConfig& config);
    
    // Connect to server
    bool connect();
    
    // Disconnect from server
    void disconnect();
    
    // Update client (call in main loop)
    void update();
    
    // Deinitialize
    void end();
    
    // Send message
    bool send(const ProtocolMessage& message);
    bool sendText(const char* text);
    bool sendAudio(const uint8_t* data, size_t length, bool is_final);
    bool sendPing();
    
    // Receive message (non-blocking)
    bool receive(ProtocolMessage& message);
    
    // Check connection state
    bool isConnected() const;
    bool isAuthenticated() const;
    ConnectionState getState() const;
    const char* getStateString() const;
    
    // Set event callback
    void onEvent(WebSocketEventCallback callback);
    
    // Get statistics
    ConnectionStats getStats() const;
    void resetStats();
    
    // Get last error
    const char* getLastError() const { return last_error_; }
    
    // Get connection info
    uint32_t getConnectionTime() const;
    uint32_t getReconnectDelay() const { return current_reconnect_delay_; }
    
    // Force reconnection
    void reconnect();
    
private:
    // Configuration
    WebSocketConfig config_;
    
    // WebSocket client
    WebSocketsClient ws_client_;
    
    // State
    ConnectionState state_;
    WebSocketEventCallback event_callback_;
    
    // Timing
    uint32_t last_connect_attempt_;
    uint32_t last_ping_time_;
    uint32_t last_pong_time_;
    uint32_t connection_start_time_;
    uint32_t current_reconnect_delay_;
    uint8_t reconnect_attempts_;
    
    // Statistics
    ConnectionStats stats_;
    SemaphoreHandle_t stats_mutex_;
    
    // Message queues
    QueueHandle_t send_queue_;
    QueueHandle_t receive_queue_;
    
    // Protocol parser for incoming data
    ProtocolParser parser_;
    
    // Error buffer
    char last_error_[128];
    
    // Authentication state
    bool auth_sent_;
    uint32_t auth_sent_time_;
    
    // Static instance for callback
    static WebSocketClient* instance_;
    
    // Private methods
    bool createQueues();
    void destroyQueues();
    bool createMutexes();
    void destroyMutexes();
    
    void setState(ConnectionState new_state);
    void handleConnect();
    void handleDisconnect(uint16_t code);
    void handleMessage(const uint8_t* data, size_t length);
    void handleError(const char* error);
    void handleAuthResponse(const ProtocolMessage& msg);
    
    void processSendQueue();
    void sendAuthMessage();
    void updateReconnectLogic();
    void updatePingPong();
    
    void updateStats(const std::function<void(ConnectionStats&)>& updater);
    
    // WebSocket event handler (static)
    static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
};

// Utility functions
const char* connectionStateToString(ConnectionState state);
const char* webSocketEventToString(WebSocketEvent event);

} // namespace OpenClaw

#endif // OPENCLAW_WEBSOCKET_CLIENT_H
