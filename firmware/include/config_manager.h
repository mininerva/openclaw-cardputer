/**
 * @file config_manager.h
 * @brief Configuration management for OpenClaw Cardputer
 * 
 * Handles WiFi credentials, gateway settings, and device configuration
 * stored in LittleFS filesystem.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace OpenClaw {

// Default configuration values
constexpr const char* DEFAULT_DEVICE_ID = "cardputer-001";
constexpr const char* DEFAULT_DEVICE_NAME = "Cardputer";
constexpr const char* DEFAULT_GATEWAY_URL = "ws://192.168.1.100:8765/ws";
constexpr const char* DEFAULT_FALLBACK_URL = "http://192.168.1.100:8765/api";
constexpr uint16_t DEFAULT_SAMPLE_RATE = 16000;
constexpr uint8_t DEFAULT_FRAME_DURATION_MS = 60;

/**
 * @brief WiFi configuration structure
 */
struct WiFiConfig {
    String ssid;
    String password;
    bool dhcp = true;
    String static_ip;
    String gateway;
    String subnet;
    
    bool isValid() const {
        return ssid.length() > 0 && password.length() >= 8;
    }
};

/**
 * @brief Gateway configuration structure
 */
struct GatewayConfig {
    String websocket_url;
    String fallback_http_url;
    String api_key;
    uint16_t reconnect_interval_ms = 5000;
    uint16_t ping_interval_ms = 30000;
    uint16_t connection_timeout_ms = 10000;
    
    bool isValid() const {
        return websocket_url.length() > 0;
    }
};

/**
 * @brief Device configuration structure
 */
struct DeviceConfig {
    String id;
    String name;
    String firmware_version = "1.0.0";
    bool auto_connect = true;
    bool save_history = false;
    uint8_t display_brightness = 128;
    
    bool isValid() const {
        return id.length() > 0 && name.length() > 0;
    }
};

/**
 * @brief Audio configuration structure
 */
struct AudioSettings {
    uint16_t sample_rate = DEFAULT_SAMPLE_RATE;
    uint8_t frame_duration_ms = DEFAULT_FRAME_DURATION_MS;
    String codec = "opus";  // "opus" or "pcm"
    uint8_t mic_gain = 64;  // 0-100
    bool noise_suppression = true;
    bool auto_gain_control = true;
    
    bool isValid() const {
        return sample_rate >= 8000 && sample_rate <= 48000 &&
               frame_duration_ms >= 20 && frame_duration_ms <= 120;
    }
};

/**
 * @brief Complete configuration container
 */
struct AppConfig {
    WiFiConfig wifi;
    GatewayConfig gateway;
    DeviceConfig device;
    AudioSettings audio;

    bool isValid() const {
        return wifi.isValid() && gateway.isValid() &&
               device.isValid() && audio.isValid();
    }
};

/**
 * @brief Configuration manager class
 * 
 * Manages loading, saving, and accessing configuration from LittleFS.
 * Provides defaults and validation for all settings.
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // Disable copy
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    /**
     * @brief Initialize the configuration manager
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Load configuration from filesystem
     * @return true if loaded successfully
     */
    bool load();
    
    /**
     * @brief Save current configuration to filesystem
     * @return true if saved successfully
     */
    bool save();
    
    /**
     * @brief Reset to default configuration
     */
    void resetToDefaults();
    
    /**
     * @brief Check if configuration is valid
     */
    bool isValid() const;
    
    /**
     * @brief Get current configuration (read-only)
     */
    const AppConfig& getConfig() const { return config_; }
    
    /**
     * @brief Get mutable configuration
     */
    AppConfig& getMutableConfig() { return config_; }
    
    // Convenience accessors
    const WiFiConfig& wifi() const { return config_.wifi; }
    const GatewayConfig& gateway() const { return config_.gateway; }
    const DeviceConfig& device() const { return config_.device; }
    const AudioSettings& audio() const { return config_.audio; }

    WiFiConfig& wifi() { return config_.wifi; }
    GatewayConfig& gateway() { return config_.gateway; }
    DeviceConfig& device() { return config_.device; }
    AudioSettings& audio() { return config_.audio; }
    
    /**
     * @brief Get the last error message
     */
    const char* getLastError() const { return last_error_; }
    
    /**
     * @brief Print configuration to Serial for debugging
     */
    void printConfig() const;

private:
    static constexpr const char* CONFIG_FILE = "/config.json";
    static constexpr size_t JSON_BUFFER_SIZE = 2048;
    
    AppConfig config_;
    char last_error_[128];
    bool initialized_ = false;
    
    bool loadFromFile(const char* path);
    bool saveToFile(const char* path);
    void setDefaults();
    bool parseJson(const JsonDocument& doc);
    bool serializeToJson(JsonDocument& doc) const;
};

} // namespace OpenClaw

#endif // CONFIG_MANAGER_H
