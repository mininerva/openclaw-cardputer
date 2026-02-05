/**
 * @file protocol.cpp
 * @brief Protocol implementation
 */

#include "protocol.h"
#include <ArduinoJson.h>

namespace OpenClaw {

// CRC16 lookup table (CCITT-FALSE)
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t ProtocolMessage::calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

ProtocolMessage::ProtocolMessage() 
    : type_(MessageType::UNKNOWN), flags_(MessageFlags::NONE),
      payload_(nullptr), payload_length_(0), timestamp_(0) {}

ProtocolMessage::ProtocolMessage(MessageType type, const uint8_t* payload, size_t length)
    : type_(type), flags_(MessageFlags::NONE), payload_(nullptr), 
      payload_length_(0), timestamp_(millis()) {
    if (payload && length > 0) {
        setPayload(payload, length);
    }
}

ProtocolMessage::~ProtocolMessage() = default;

ProtocolMessage::ProtocolMessage(ProtocolMessage&& other) noexcept
    : type_(other.type_), flags_(other.flags_), 
      payload_(std::move(other.payload_)),
      payload_length_(other.payload_length_), timestamp_(other.timestamp_) {
    other.payload_length_ = 0;
}

ProtocolMessage& ProtocolMessage::operator=(ProtocolMessage&& other) noexcept {
    if (this != &other) {
        type_ = other.type_;
        flags_ = other.flags_;
        payload_ = std::move(other.payload_);
        payload_length_ = other.payload_length_;
        timestamp_ = other.timestamp_;
        other.payload_length_ = 0;
    }
    return *this;
}

void ProtocolMessage::setPayload(const uint8_t* data, size_t length) {
    if (length > PROTOCOL_MAX_PAYLOAD_SIZE) {
        length = PROTOCOL_MAX_PAYLOAD_SIZE;
    }
    payload_.reset(new uint8_t[length]);
    if (data && length > 0) {
        memcpy(payload_.get(), data, length);
    }
    payload_length_ = length;
}

ProtocolMessage ProtocolMessage::createAuth(const char* device_id, const char* device_name,
                                             const char* version, const char* api_key) {
    ProtocolMessage msg(MessageType::AUTH);
    
    JsonDocument doc;
    doc["device_id"] = device_id;
    doc["device_name"] = device_name;
    doc["version"] = version;
    if (api_key) {
        doc["api_key"] = api_key;
    }
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createAuthResponse(bool success, const char* error) {
    ProtocolMessage msg(MessageType::AUTH_RESPONSE);
    
    JsonDocument doc;
    doc["success"] = success;
    if (error) {
        doc["error"] = error;
    }
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createText(const char* text, const char* device_id) {
    ProtocolMessage msg(MessageType::TEXT);
    
    JsonDocument doc;
    doc["text"] = text;
    doc["device_id"] = device_id;
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createAudio(const uint8_t* data, size_t length, bool is_final,
                                              const char* codec) {
    ProtocolMessage msg(MessageType::AUDIO);
    
    if (is_final) {
        msg.flags_ = msg.flags_ | MessageFlags::FINAL;
    }
    
    // For binary audio data, encode as base64 in JSON wrapper
    JsonDocument doc;
    doc["codec"] = codec;
    doc["is_final"] = is_final;
    
    // Base64 encode the audio data
    size_t encoded_len = ((length + 2) / 3) * 4 + 1;
    char* encoded = new char[encoded_len];
    // Note: Base64 encoding would go here
    // For now, we'll use a placeholder
    doc["data"] = "base64_encoded_audio_data";
    delete[] encoded;
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createResponse(const char* text, bool is_final) {
    ProtocolMessage msg(is_final ? MessageType::RESPONSE_FINAL : MessageType::RESPONSE);
    
    JsonDocument doc;
    doc["text"] = text;
    doc["is_final"] = is_final;
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createStatus(const char* status) {
    ProtocolMessage msg(MessageType::STATUS);
    
    JsonDocument doc;
    doc["status"] = status;
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createError(const char* error, int error_code) {
    ProtocolMessage msg(MessageType::ERROR);
    
    JsonDocument doc;
    doc["error"] = error;
    if (error_code != 0) {
        doc["code"] = error_code;
    }
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createPing() {
    ProtocolMessage msg(MessageType::PING);
    
    JsonDocument doc;
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createPong(uint32_t ping_timestamp) {
    ProtocolMessage msg(MessageType::PONG);
    
    JsonDocument doc;
    doc["ping_timestamp"] = ping_timestamp;
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

ProtocolMessage ProtocolMessage::createAudioConfig(uint16_t sample_rate, uint8_t channels,
                                                    uint8_t bits_per_sample, const char* codec) {
    ProtocolMessage msg(MessageType::AUDIO_CONFIG);
    
    JsonDocument doc;
    doc["sample_rate"] = sample_rate;
    doc["channels"] = channels;
    doc["bits_per_sample"] = bits_per_sample;
    doc["codec"] = codec;
    
    String json;
    serializeJson(doc, json);
    msg.setJsonPayload(json);
    
    return msg;
}

bool ProtocolMessage::serialize(uint8_t* buffer, size_t buffer_size, size_t& out_length) const {
    size_t total_size = getTotalSize();
    if (buffer_size < total_size) {
        return false;
    }
    
    // Build header
    ProtocolHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<uint8_t>(type_);
    header.flags = static_cast<uint8_t>(flags_);
    header.payload_length = payload_length_;
    header.timestamp = timestamp_;
    
    // Encode header
    header.encode(buffer);
    
    // Copy payload
    if (payload_ && payload_length_ > 0) {
        memcpy(buffer + PROTOCOL_HEADER_SIZE, payload_.get(), payload_length_);
    }
    
    // Calculate and append CRC
    uint16_t crc = calculateCRC16(buffer, PROTOCOL_HEADER_SIZE + payload_length_);
    buffer[PROTOCOL_HEADER_SIZE + payload_length_] = crc & 0xFF;
    buffer[PROTOCOL_HEADER_SIZE + payload_length_ + 1] = (crc >> 8) & 0xFF;
    
    out_length = total_size;
    return true;
}

bool ProtocolMessage::deserialize(const uint8_t* buffer, size_t buffer_length) {
    if (buffer_length < PROTOCOL_HEADER_SIZE + PROTOCOL_FOOTER_SIZE) {
        return false;
    }
    
    // Decode header
    ProtocolHeader header;
    if (!header.decode(buffer)) {
        return false;
    }
    
    // Check payload length
    if (buffer_length < PROTOCOL_HEADER_SIZE + header.payload_length + PROTOCOL_FOOTER_SIZE) {
        return false;
    }
    
    // Verify CRC
    size_t data_length = PROTOCOL_HEADER_SIZE + header.payload_length;
    uint16_t received_crc = buffer[data_length] | (buffer[data_length + 1] << 8);
    uint16_t calculated_crc = calculateCRC16(buffer, data_length);
    if (received_crc != calculated_crc) {
        return false;
    }
    
    // Set fields
    type_ = static_cast<MessageType>(header.type);
    flags_ = static_cast<MessageFlags>(header.flags);
    timestamp_ = header.timestamp;
    
    // Copy payload
    if (header.payload_length > 0) {
        setPayload(buffer + PROTOCOL_HEADER_SIZE, header.payload_length);
    } else {
        payload_.reset();
        payload_length_ = 0;
    }
    
    return true;
}

bool ProtocolMessage::isValid() const {
    return type_ != MessageType::UNKNOWN && 
           payload_length_ <= PROTOCOL_MAX_PAYLOAD_SIZE;
}

const char* ProtocolMessage::getTypeString() const {
    switch (type_) {
        case MessageType::AUTH: return "AUTH";
        case MessageType::AUTH_RESPONSE: return "AUTH_RESPONSE";
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        case MessageType::TEXT: return "TEXT";
        case MessageType::AUDIO: return "AUDIO";
        case MessageType::RESPONSE: return "RESPONSE";
        case MessageType::RESPONSE_FINAL: return "RESPONSE_FINAL";
        case MessageType::STATUS: return "STATUS";
        case MessageType::COMMAND: return "COMMAND";
        case MessageType::ERROR: return "ERROR";
        case MessageType::AUDIO_CONFIG: return "AUDIO_CONFIG";
        default: return "UNKNOWN";
    }
}

bool ProtocolMessage::getJsonPayload(String& json_string) const {
    if (!payload_ || payload_length_ == 0) {
        return false;
    }
    json_string = String((char*)payload_.get(), payload_length_);
    return true;
}

bool ProtocolMessage::setJsonPayload(const String& json_string) {
    setPayload((const uint8_t*)json_string.c_str(), json_string.length());
    return true;
}

// ProtocolParser implementation
ProtocolParser::ProtocolParser() 
    : state_(ParseState::WAITING_MAGIC), buffer_pos_(0), expected_length_(0) {
    memset(buffer_, 0, sizeof(buffer_));
}

ProtocolParser::~ProtocolParser() = default;

void ProtocolParser::reset() {
    state_ = ParseState::WAITING_MAGIC;
    buffer_pos_ = 0;
    expected_length_ = 0;
}

bool ProtocolParser::feed(const uint8_t* data, size_t length, ProtocolMessage& out_message) {
    for (size_t i = 0; i < length; i++) {
        buffer_[buffer_pos_++] = data[i];
        
        switch (state_) {
            case ParseState::WAITING_MAGIC:
                if (buffer_[0] == PROTOCOL_MAGIC) {
                    state_ = ParseState::WAITING_HEADER;
                    expected_length_ = 10; // Full header size
                } else {
                    buffer_pos_ = 0; // Reset, wait for magic
                }
                break;
                
            case ParseState::WAITING_HEADER:
                if (buffer_pos_ >= expected_length_) {
                    if (!current_header_.decode(buffer_)) {
                        reset();
                        continue;
                    }
                    if (current_header_.payload_length > 0) {
                        state_ = ParseState::WAITING_PAYLOAD;
                        expected_length_ = PROTOCOL_HEADER_SIZE + current_header_.payload_length;
                    } else {
                        state_ = ParseState::WAITING_CRC;
                        expected_length_ = PROTOCOL_HEADER_SIZE + PROTOCOL_FOOTER_SIZE;
                    }
                }
                break;
                
            case ParseState::WAITING_PAYLOAD:
                if (buffer_pos_ >= expected_length_) {
                    state_ = ParseState::WAITING_CRC;
                    expected_length_ = PROTOCOL_HEADER_SIZE + current_header_.payload_length + PROTOCOL_FOOTER_SIZE;
                }
                break;
                
            case ParseState::WAITING_CRC:
                if (buffer_pos_ >= expected_length_) {
                    if (out_message.deserialize(buffer_, buffer_pos_)) {
                        reset();
                        return true;
                    } else {
                        reset();
                    }
                }
                break;
                
            default:
                reset();
                break;
        }
        
        // Prevent buffer overflow
        if (buffer_pos_ >= PROTOCOL_MAX_MESSAGE_SIZE) {
            reset();
        }
    }
    
    return false;
}

size_t ProtocolParser::bytesNeeded() const {
    if (expected_length_ > buffer_pos_) {
        return expected_length_ - buffer_pos_;
    }
    return 0;
}

const char* protocolErrorToString(ProtocolError error) {
    switch (error) {
        case ProtocolError::NONE: return "No error";
        case ProtocolError::INVALID_MAGIC: return "Invalid magic byte";
        case ProtocolError::INVALID_VERSION: return "Invalid protocol version";
        case ProtocolError::INVALID_TYPE: return "Invalid message type";
        case ProtocolError::PAYLOAD_TOO_LARGE: return "Payload too large";
        case ProtocolError::CRC_MISMATCH: return "CRC mismatch";
        case ProtocolError::BUFFER_TOO_SMALL: return "Buffer too small";
        case ProtocolError::PARSE_ERROR: return "Parse error";
        case ProtocolError::TIMEOUT: return "Timeout";
        case ProtocolError::NOT_AUTHENTICATED: return "Not authenticated";
        case ProtocolError::RATE_LIMITED: return "Rate limited";
        default: return "Unknown error";
    }
}

} // namespace OpenClaw
