/**
 * @file protocol.h
 * @brief Binary message protocol for OpenClaw Cardputer
 * 
 * Defines a clean binary protocol for WebSocket communication between
 * the Cardputer device and the OpenClaw gateway bridge.
 * 
 * Protocol Format:
 * - 1 byte: Message type
 * - 1 byte: Flags
 * - 2 bytes: Payload length (little-endian)
 * - 4 bytes: Timestamp (little-endian, milliseconds)
 * - N bytes: Payload (JSON or binary data)
 * - 2 bytes: CRC16 checksum
 */

#ifndef OPENCLAW_PROTOCOL_H
#define OPENCLAW_PROTOCOL_H

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <memory>

namespace OpenClaw {

// Protocol constants
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t PROTOCOL_MAGIC = 0x4F; // 'O' for OpenClaw
constexpr size_t PROTOCOL_HEADER_SIZE = 8;
constexpr size_t PROTOCOL_FOOTER_SIZE = 2;
constexpr size_t PROTOCOL_MAX_PAYLOAD_SIZE = 8192;
constexpr size_t PROTOCOL_MAX_MESSAGE_SIZE = PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE + PROTOCOL_FOOTER_SIZE;

// Message types
enum class MessageType : uint8_t {
    // Connection management
    AUTH = 0x01,           // Authentication request
    AUTH_RESPONSE = 0x02,  // Authentication response
    PING = 0x03,           // Keepalive ping
    PONG = 0x04,           // Keepalive pong
    
    // Data messages
    TEXT = 0x10,           // Text message
    AUDIO = 0x11,          // Audio data
    RESPONSE = 0x12,       // AI response (streaming)
    RESPONSE_FINAL = 0x13, // AI response (final)
    
    // Control messages
    STATUS = 0x20,         // Status update
    COMMAND = 0x21,        // Control command
    ERROR = 0x22,          // Error message
    
    // Audio codec info
    AUDIO_CONFIG = 0x30,   // Audio configuration
    
    UNKNOWN = 0xFF
};

// Message flags
enum class MessageFlags : uint8_t {
    NONE = 0x00,
    ENCRYPTED = 0x01,      // Payload is encrypted
    COMPRESSED = 0x02,     // Payload is compressed
    BINARY = 0x04,         // Payload is binary (not JSON)
    FINAL = 0x08,          // Final message in sequence
    ACK_REQUIRED = 0x10,   // Acknowledgment required
};

inline MessageFlags operator|(MessageFlags a, MessageFlags b) {
    return static_cast<MessageFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline MessageFlags operator&(MessageFlags a, MessageFlags b) {
    return static_cast<MessageFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool hasFlag(MessageFlags flags, MessageFlags flag) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

// Protocol header structure
struct __attribute__((packed)) ProtocolHeader {
    uint8_t magic;           // Protocol magic byte
    uint8_t version;         // Protocol version
    uint8_t type;            // Message type
    uint8_t flags;           // Message flags
    uint16_t payload_length; // Payload length (little-endian)
    uint32_t timestamp;      // Timestamp in milliseconds
    
    void encode(uint8_t* buffer) const {
        buffer[0] = magic;
        buffer[1] = version;
        buffer[2] = type;
        buffer[3] = flags;
        buffer[4] = payload_length & 0xFF;
        buffer[5] = (payload_length >> 8) & 0xFF;
        buffer[6] = timestamp & 0xFF;
        buffer[7] = (timestamp >> 8) & 0xFF;
        buffer[8] = (timestamp >> 16) & 0xFF;
        buffer[9] = (timestamp >> 24) & 0xFF;
    }
    
    bool decode(const uint8_t* buffer) {
        magic = buffer[0];
        version = buffer[1];
        type = buffer[2];
        flags = buffer[3];
        payload_length = buffer[4] | (buffer[5] << 8);
        timestamp = buffer[6] | (buffer[7] << 8) | (buffer[8] << 16) | (buffer[9] << 24);
        
        return magic == PROTOCOL_MAGIC && version == PROTOCOL_VERSION;
    }
};

// Message structure
class ProtocolMessage {
public:
    ProtocolMessage();
    ProtocolMessage(MessageType type, const uint8_t* payload = nullptr, size_t length = 0);
    ~ProtocolMessage();
    
    // Disable copy, enable move
    ProtocolMessage(const ProtocolMessage&) = delete;
    ProtocolMessage& operator=(const ProtocolMessage&) = delete;
    ProtocolMessage(ProtocolMessage&& other) noexcept;
    ProtocolMessage& operator=(ProtocolMessage&& other) noexcept;
    
    // Factory methods
    static ProtocolMessage createAuth(const char* device_id, const char* device_name, 
                                       const char* version, const char* api_key = nullptr);
    static ProtocolMessage createAuthResponse(bool success, const char* error = nullptr);
    static ProtocolMessage createText(const char* text, const char* device_id);
    static ProtocolMessage createAudio(const uint8_t* data, size_t length, bool is_final,
                                        const char* codec = "opus");
    static ProtocolMessage createResponse(const char* text, bool is_final);
    static ProtocolMessage createStatus(const char* status);
    static ProtocolMessage createError(const char* error, int error_code = 0);
    static ProtocolMessage createPing();
    static ProtocolMessage createPong(uint32_t ping_timestamp);
    static ProtocolMessage createAudioConfig(uint16_t sample_rate, uint8_t channels,
                                              uint8_t bits_per_sample, const char* codec);
    
    // Serialization
    bool serialize(uint8_t* buffer, size_t buffer_size, size_t& out_length) const;
    bool deserialize(const uint8_t* buffer, size_t buffer_length);
    
    // Getters
    MessageType getType() const { return type_; }
    MessageFlags getFlags() const { return flags_; }
    const uint8_t* getPayload() const { return payload_.get(); }
    size_t getPayloadLength() const { return payload_length_; }
    uint32_t getTimestamp() const { return timestamp_; }
    
    // Setters
    void setType(MessageType type) { type_ = type; }
    void setFlags(MessageFlags flags) { flags_ = flags; }
    void setPayload(const uint8_t* data, size_t length);
    void setTimestamp(uint32_t timestamp) { timestamp_ = timestamp; }
    
    // Utility
    bool isValid() const;
    size_t getTotalSize() const { return PROTOCOL_HEADER_SIZE + payload_length_ + PROTOCOL_FOOTER_SIZE; }
    const char* getTypeString() const;
    
    // JSON payload helpers
    bool getJsonPayload(String& json_string) const;
    bool setJsonPayload(const String& json_string);
    
private:
    MessageType type_;
    MessageFlags flags_;
    std::unique_ptr<uint8_t[]> payload_;
    size_t payload_length_;
    uint32_t timestamp_;
    
    // CRC16 calculation (CCITT-FALSE)
    static uint16_t calculateCRC16(const uint8_t* data, size_t length);
};

// Protocol parser for streaming data
class ProtocolParser {
public:
    ProtocolParser();
    ~ProtocolParser();
    
    // Reset parser state
    void reset();
    
    // Feed data into parser
    // Returns true if a complete message was parsed
    bool feed(const uint8_t* data, size_t length, ProtocolMessage& out_message);
    
    // Check if parser has partial data
    bool hasPartialData() const { return buffer_pos_ > 0; }
    
    // Get bytes needed for next state
    size_t bytesNeeded() const;
    
private:
    enum class ParseState {
        WAITING_MAGIC,
        WAITING_HEADER,
        WAITING_PAYLOAD,
        WAITING_CRC,
        COMPLETE
    };
    
    ParseState state_;
    uint8_t buffer_[PROTOCOL_MAX_MESSAGE_SIZE];
    size_t buffer_pos_;
    size_t expected_length_;
    ProtocolHeader current_header_;
};

// Audio codec types
enum class AudioCodec : uint8_t {
    PCM_S16LE = 0x01,      // PCM 16-bit little-endian
    PCM_S8 = 0x02,         // PCM 8-bit
    OPUS = 0x10,           // Opus compressed
    UNKNOWN = 0xFF
};

// Audio configuration structure
struct AudioConfig {
    uint16_t sample_rate;      // Sample rate in Hz (e.g., 16000)
    uint8_t channels;          // Number of channels (1 = mono, 2 = stereo)
    uint8_t bits_per_sample;   // Bits per sample (8, 16, 24, 32)
    AudioCodec codec;          // Audio codec
    uint16_t frame_duration_ms; // Frame duration in milliseconds
    
    AudioConfig() 
        : sample_rate(16000), channels(1), bits_per_sample(16),
          codec(AudioCodec::OPUS), frame_duration_ms(60) {}
    
    size_t getFrameSize() const {
        return (sample_rate * frame_duration_ms / 1000) * channels * (bits_per_sample / 8);
    }
    
    size_t getSamplesPerFrame() const {
        return sample_rate * frame_duration_ms / 1000;
    }
};

// Error codes
enum class ProtocolError : int16_t {
    NONE = 0,
    INVALID_MAGIC = -1,
    INVALID_VERSION = -2,
    INVALID_TYPE = -3,
    PAYLOAD_TOO_LARGE = -4,
    CRC_MISMATCH = -5,
    BUFFER_TOO_SMALL = -6,
    PARSE_ERROR = -7,
    TIMEOUT = -8,
    NOT_AUTHENTICATED = -9,
    RATE_LIMITED = -10
};

const char* protocolErrorToString(ProtocolError error);

} // namespace OpenClaw

#endif // OPENCLAW_PROTOCOL_H
