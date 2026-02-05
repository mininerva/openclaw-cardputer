/**
 * @file gateway_client.cpp
 * @brief Gateway client implementation
 */

#include "gateway_client.h"
#include <base64.h>

namespace OpenClaw {

// Static instance pointer for callback
GatewayClient* GatewayClient::instance_ = nullptr;

GatewayClient::GatewayClient() {
    memset(last_error_, 0, sizeof(last_error_));
    instance_ = this;
}

GatewayClient::~GatewayClient() {
    end();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

bool GatewayClient::begin(const GatewayConfig& config, const DeviceConfig& device) {
    gateway_config_ = config;
    device_config_ = device;
    
    // Parse WebSocket URL
    if (!parseUrl(config.websocket_url.c_str())) {
        return false;
    }
    
    // Create receive queue
    if (!createQueue()) {
        return false;
    }
    
    // Configure WebSocket client
    ws_client_.setReconnectInterval(config.reconnect_interval_ms);
    ws_client_.enableHeartbeat(config.ping_interval_ms, 2000, 2);
    
    return true;
}

bool GatewayClient::connect() {
    if (state_ != ConnectionState::DISCONNECTED) {
        strncpy(last_error_, "Already connected or connecting", sizeof(last_error_) - 1);
        return false;
    }
    
    setState(ConnectionState::CONNECTING);
    
    // Set up event handler
    ws_client_.onEvent(webSocketEvent);
    
    // Begin connection
    if (use_ssl_) {
        ws_client_.beginSSL(host_.c_str(), port_, path_.c_str(), "", "");
    } else {
        ws_client_.begin(host_.c_str(), port_, path_.c_str(), "");
    }
    
    last_connect_attempt_ = millis();
    return true;
}

void GatewayClient::disconnect() {
    ws_client_.disconnect();
    setState(ConnectionState::DISCONNECTED);
}

void GatewayClient::update() {
    ws_client_.loop();
    
    // Handle reconnection
    if (state_ == ConnectionState::DISCONNECTED || state_ == ConnectionState::ERROR) {
        uint32_t now = millis();
        if (now - last_connect_attempt_ > reconnect_delay_ms_) {
            reconnect_count_++;
            connect();
        }
    }
    
    // Send periodic ping
    if (state_ == ConnectionState::AUTHENTICATED) {
        uint32_t now = millis();
        if (now - last_ping_time_ > gateway_config_.ping_interval_ms) {
            sendPing();
            last_ping_time_ = now;
        }
    }
}

void GatewayClient::end() {
    disconnect();
    destroyQueue();
}

bool GatewayClient::sendText(const char* text) {
    if (state_ != ConnectionState::AUTHENTICATED) {
        strncpy(last_error_, "Not authenticated", sizeof(last_error_) - 1);
        return false;
    }
    
    JsonDocument doc;
    doc["type"] = "text";
    doc["payload"] = text;
    doc["device_id"] = device_config_.id;
    doc["timestamp"] = millis();
    
    return sendJson(doc);
}

bool GatewayClient::sendAudio(const char* audio_data, bool is_final) {
    if (state_ != ConnectionState::AUTHENTICATED) {
        strncpy(last_error_, "Not authenticated", sizeof(last_error_) - 1);
        return false;
    }
    
    JsonDocument doc;
    doc["type"] = "audio";
    doc["payload"] = audio_data;
    doc["is_final"] = is_final;
    doc["device_id"] = device_config_.id;
    doc["timestamp"] = millis();
    doc["codec"] = "opus";
    
    return sendJson(doc);
}

bool GatewayClient::sendAudioRaw(const uint8_t* data, size_t length, bool is_final) {
    // Encode to base64
    String base64_data = base64::encode(data, length);
    return sendAudio(base64_data.c_str(), is_final);
}

bool GatewayClient::sendPing() {
    JsonDocument doc;
    doc["type"] = "ping";
    doc["timestamp"] = millis();
    
    return sendJson(doc);
}

bool GatewayClient::sendJson(const JsonDocument& json) {
    String payload;
    serializeJson(json, payload);
    
    bool result = ws_client_.sendTXT(payload);
    if (result) {
        messages_sent_++;
    }
    
    return result;
}

void GatewayClient::setCallback(GatewayClientCallback* callback) {
    callback_ = callback;
}

const char* GatewayClient::getStateString() const {
    switch (state_) {
        case ConnectionState::DISCONNECTED:
            return "DISCONNECTED";
        case ConnectionState::CONNECTING:
            return "CONNECTING";
        case ConnectionState::CONNECTED:
            return "CONNECTED";
        case ConnectionState::AUTHENTICATING:
            return "AUTHENTICATING";
        case ConnectionState::AUTHENTICATED:
            return "AUTHENTICATED";
        case ConnectionState::RECONNECTING:
            return "RECONNECTING";
        case ConnectionState::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

bool GatewayClient::readMessage(GatewayMessage& msg) {
    if (!receive_queue_) return false;
    return xQueueReceive(receive_queue_, &msg, 0) == pdTRUE;
}

uint32_t GatewayClient::getConnectionDuration() const {
    if (connection_start_time_ == 0) return 0;
    return millis() - connection_start_time_;
}

bool GatewayClient::parseUrl(const char* url) {
    String url_str = url;
    
    // Determine protocol
    if (url_str.startsWith("wss://")) {
        use_ssl_ = true;
        port_ = 443;
        url_str.remove(0, 6);
    } else if (url_str.startsWith("ws://")) {
        use_ssl_ = false;
        port_ = 80;
        url_str.remove(0, 5);
    } else {
        snprintf(last_error_, sizeof(last_error_), "Invalid URL scheme: %s", url);
        return false;
    }
    
    // Extract host and port
    int path_start = url_str.indexOf('/');
    int port_start = url_str.indexOf(':');
    
    if (port_start > 0 && (path_start < 0 || port_start < path_start)) {
        // Port specified
        host_ = url_str.substring(0, port_start);
        if (path_start > 0) {
            port_ = url_str.substring(port_start + 1, path_start).toInt();
            path_ = url_str.substring(path_start);
        } else {
            port_ = url_str.substring(port_start + 1).toInt();
            path_ = "/";
        }
    } else {
        // No port specified
        if (path_start > 0) {
            host_ = url_str.substring(0, path_start);
            path_ = url_str.substring(path_start);
        } else {
            host_ = url_str;
            path_ = "/";
        }
    }
    
    return true;
}

bool GatewayClient::createQueue() {
    receive_queue_ = xQueueCreate(WS_QUEUE_LENGTH, sizeof(GatewayMessage));
    if (!receive_queue_) {
        strncpy(last_error_, "Failed to create receive queue", sizeof(last_error_) - 1);
        return false;
    }
    return true;
}

void GatewayClient::destroyQueue() {
    if (receive_queue_) {
        // Clear any pending messages
        GatewayMessage msg;
        while (xQueueReceive(receive_queue_, &msg, 0) == pdTRUE) {
            // Message destructor handles cleanup
        }
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
    }
}

void GatewayClient::setState(ConnectionState state) {
    if (state_ != state) {
        state_ = state;
        
        if (callback_) {
            callback_>onStateChanged(state);
        }
        
        // Track connection start
        if (state == ConnectionState::AUTHENTICATED) {
            connection_start_time_ = millis();
        }
    }
}

void GatewayClient::handleConnect() {
    setState(ConnectionState::CONNECTED);
    sendAuth();
}

void GatewayClient::handleDisconnect(uint16_t code) {
    setState(ConnectionState::DISCONNECTED);
    connection_start_time_ = 0;
    
    if (callback_) {
        callback_>onDisconnected(code, "Connection closed");
    }
}

void GatewayClient::handleMessage(const char* data) {
    messages_received_++;
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        snprintf(last_error_, sizeof(last_error_), "JSON parse error: %s", error.c_str());
        return;
    }
    
    processIncomingMessage(doc);
}

void GatewayClient::sendAuth() {
    setState(ConnectionState::AUTHENTICATING);
    
    JsonDocument doc;
    doc["type"] = "auth";
    doc["device_id"] = device_config_.id;
    doc["device_name"] = device_config_.name;
    doc["version"] = device_config_.firmware_version;
    doc["api_key"] = gateway_config_.api_key;
    
    sendJson(doc);
}

void GatewayClient::processIncomingMessage(const JsonDocument& doc) {
    const char* type_str = doc["type"] | "unknown";
    GatewayMessageType type = parseMessageType(type_str);
    
    switch (type) {
        case GatewayMessageType::AUTH_RESPONSE: {
            bool success = doc["success"] | false;
            if (success) {
                setState(ConnectionState::AUTHENTICATED);
                if (callback_) {
                    callback_>onAuthenticated();
                }
            } else {
                const char* error = doc["error"] | "Authentication failed";
                setState(ConnectionState::ERROR);
                if (callback_) {
                    callback_>onAuthFailed(error);
                }
            }
            break;
        }
        
        case GatewayMessageType::RESPONSE: {
            const char* text = doc["payload"] | "";
            bool is_final = doc["is_final"] | true;
            if (callback_) {
                callback_>onTextResponse(text, is_final);
            }
            break;
        }
        
        case GatewayMessageType::AUDIO: {
            const char* audio_data = doc["payload"] | "";
            if (callback_) {
                callback_>onAudioResponse(audio_data);
            }
            break;
        }
        
        case GatewayMessageType::PONG: {
            // Pong received, connection is alive
            break;
        }
        
        case GatewayMessageType::ERROR: {
            const char* error = doc["payload"] | "Unknown error";
            setState(ConnectionState::ERROR);
            if (callback_) {
                callback_>onError(error);
            }
            break;
        }
        
        default: {
            // Unknown message type, queue for processing
            GatewayMessage msg;
            msg.type = type;
            msg.timestamp = millis();
            
            // Copy payload
            if (doc["payload"]) {
                msg.payload = doc["payload"].as<String>();
            }
            
            queueMessage(std::move(msg));
            break;
        }
    }
}

GatewayMessageType GatewayClient::parseMessageType(const char* type_str) {
    if (strcmp(type_str, "auth") == 0) return GatewayMessageType::AUTH;
    if (strcmp(type_str, "auth_response") == 0) return GatewayMessageType::AUTH_RESPONSE;
    if (strcmp(type_str, "audio") == 0) return GatewayMessageType::AUDIO;
    if (strcmp(type_str, "text") == 0) return GatewayMessageType::TEXT;
    if (strcmp(type_str, "response") == 0) return GatewayMessageType::RESPONSE;
    if (strcmp(type_str, "status") == 0) return GatewayMessageType::STATUS;
    if (strcmp(type_str, "ping") == 0) return GatewayMessageType::PING;
    if (strcmp(type_str, "pong") == 0) return GatewayMessageType::PONG;
    if (strcmp(type_str, "error") == 0) return GatewayMessageType::ERROR;
    if (strcmp(type_str, "command") == 0) return GatewayMessageType::COMMAND;
    return GatewayMessageType::UNKNOWN;
}

const char* GatewayClient::messageTypeToString(GatewayMessageType type) {
    switch (type) {
        case GatewayMessageType::AUTH: return "auth";
        case GatewayMessageType::AUTH_RESPONSE: return "auth_response";
        case GatewayMessageType::AUDIO: return "audio";
        case GatewayMessageType::TEXT: return "text";
        case GatewayMessageType::RESPONSE: return "response";
        case GatewayMessageType::STATUS: return "status";
        case GatewayMessageType::PING: return "ping";
        case GatewayMessageType::PONG: return "pong";
        case GatewayMessageType::ERROR: return "error";
        case GatewayMessageType::COMMAND: return "command";
        default: return "unknown";
    }
}

bool GatewayClient::queueMessage(GatewayMessage&& msg) {
    if (!receive_queue_) return false;
    return xQueueSend(receive_queue_, &msg, 0) == pdTRUE;
}

void GatewayClient::webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    if (!instance_) return;
    
    switch (type) {
        case WStype_DISCONNECTED:
            instance_>handleDisconnect(0);
            break;
            
        case WStype_CONNECTED:
            instance_>handleConnect();
            break;
            
        case WStype_TEXT:
            instance_>handleMessage((const char*)payload);
            break;
            
        case WStype_BIN:
            // Binary data not used in this implementation
            break;
            
        case WStype_ERROR:
            instance_>setState(ConnectionState::ERROR);
            if (instance_>callback_) {
                instance_>callback_>onError("WebSocket error");
            }
            break;
            
        case WStype_PING:
        case WStype_PONG:
            // Handled by WebSocketsClient
            break;
            
        default:
            break;
    }
}

} // namespace OpenClaw
