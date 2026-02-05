/**
 * @file config_manager.cpp
 * @brief Configuration manager implementation
 */

#include "config_manager.h"
#include <Arduino.h>

namespace OpenClaw {

ConfigManager::ConfigManager() {
    memset(last_error_, 0, sizeof(last_error_));
    setDefaults();
}

ConfigManager::~ConfigManager() {
    if (initialized_) {
        LittleFS.end();
    }
}

bool ConfigManager::begin() {
    if (initialized_) {
        return true;
    }
    
    if (!LittleFS.begin(true)) {
        strncpy(last_error_, "Failed to initialize LittleFS", sizeof(last_error_) - 1);
        return false;
    }
    
    initialized_ = true;
    
    // Try to load existing config
    if (!load()) {
        // Save defaults if no config exists
        save();
    }
    
    return true;
}

bool ConfigManager::load() {
    if (!initialized_) {
        strncpy(last_error_, "ConfigManager not initialized", sizeof(last_error_) - 1);
        return false;
    }
    
    return loadFromFile(CONFIG_FILE);
}

bool ConfigManager::save() {
    if (!initialized_) {
        strncpy(last_error_, "ConfigManager not initialized", sizeof(last_error_) - 1);
        return false;
    }
    
    return saveToFile(CONFIG_FILE);
}

void ConfigManager::resetToDefaults() {
    setDefaults();
}

bool ConfigManager::isValid() const {
    return config_.isValid();
}

void ConfigManager::printConfig() const {
    Serial.println("=== OpenClaw Configuration ===");
    
    Serial.println("\n[WiFi]");
    Serial.printf("  SSID: %s\n", config_.wifi.ssid.c_str());
    Serial.printf("  DHCP: %s\n", config_.wifi.dhcp ? "true" : "false");
    
    Serial.println("\n[Gateway]");
    Serial.printf("  WebSocket URL: %s\n", config_.gateway.websocket_url.c_str());
    Serial.printf("  Fallback URL: %s\n", config_.gateway.fallback_http_url.c_str());
    
    Serial.println("\n[Device]");
    Serial.printf("  ID: %s\n", config_.device.id.c_str());
    Serial.printf("  Name: %s\n", config_.device.name.c_str());
    Serial.printf("  Version: %s\n", config_.device.firmware_version.c_str());
    
    Serial.println("\n[Audio]");
    Serial.printf("  Sample Rate: %d Hz\n", config_.audio.sample_rate);
    Serial.printf("  Frame Duration: %d ms\n", config_.audio.frame_duration_ms);
    Serial.printf("  Codec: %s\n", config_.audio.codec.c_str());
    Serial.printf("  Mic Gain: %d\n", config_.audio.mic_gain);
    
    Serial.println("==============================");
}

void ConfigManager::setDefaults() {
    // WiFi defaults
    config_.wifi.ssid = "";
    config_.wifi.password = "";
    config_.wifi.dhcp = true;
    config_.wifi.static_ip = "";
    config_.wifi.gateway = "";
    config_.wifi.subnet = "255.255.255.0";
    
    // Gateway defaults
    config_.gateway.websocket_url = DEFAULT_GATEWAY_URL;
    config_.gateway.fallback_http_url = DEFAULT_FALLBACK_URL;
    config_.gateway.api_key = "";
    config_.gateway.reconnect_interval_ms = 5000;
    config_.gateway.ping_interval_ms = 30000;
    config_.gateway.connection_timeout_ms = 10000;
    
    // Device defaults
    config_.device.id = DEFAULT_DEVICE_ID;
    config_.device.name = DEFAULT_DEVICE_NAME;
    config_.device.firmware_version = "1.0.0";
    config_.device.auto_connect = true;
    config_.device.save_history = false;
    config_.device.display_brightness = 128;
    
    // Audio defaults
    config_.audio.sample_rate = DEFAULT_SAMPLE_RATE;
    config_.audio.frame_duration_ms = DEFAULT_FRAME_DURATION_MS;
    config_.audio.codec = "opus";
    config_.audio.mic_gain = 64;
    config_.audio.noise_suppression = true;
    config_.audio.auto_gain_control = true;
}

bool ConfigManager::loadFromFile(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        strncpy(last_error_, "Config file not found, using defaults", sizeof(last_error_) - 1);
        return false;
    }
    
    size_t size = file.size();
    if (size == 0) {
        strncpy(last_error_, "Config file is empty", sizeof(last_error_) - 1);
        file.close();
        return false;
    }
    
    if (size > JSON_BUFFER_SIZE) {
        strncpy(last_error_, "Config file too large", sizeof(last_error_) - 1);
        file.close();
        return false;
    }
    
    std::unique_ptr<char[]> buffer(new char[size + 1]);
    file.readBytes(buffer.get(), size);
    buffer[size] = '\0';
    file.close();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buffer.get());
    
    if (error) {
        snprintf(last_error_, sizeof(last_error_), "JSON parse error: %s", error.c_str());
        return false;
    }
    
    return parseJson(doc);
}

bool ConfigManager::saveToFile(const char* path) {
    JsonDocument doc;
    
    if (!serializeToJson(doc)) {
        strncpy(last_error_, "Failed to serialize config", sizeof(last_error_) - 1);
        return false;
    }
    
    File file = LittleFS.open(path, "w");
    if (!file) {
        strncpy(last_error_, "Failed to open config file for writing", sizeof(last_error_) - 1);
        return false;
    }
    
    if (serializeJsonPretty(doc, file) == 0) {
        strncpy(last_error_, "Failed to write config file", sizeof(last_error_) - 1);
        file.close();
        return false;
    }
    
    file.close();
    return true;
}

bool ConfigManager::parseJson(const JsonDocument& doc) {
    // Parse WiFi config
    JsonObjectConst wifi = doc["wifi"];
    if (wifi) {
        config_.wifi.ssid = wifi["ssid"] | "";
        config_.wifi.password = wifi["password"] | "";
        config_.wifi.dhcp = wifi["dhcp"] | true;
        config_.wifi.static_ip = wifi["static_ip"] | "";
        config_.wifi.gateway = wifi["gateway"] | "";
        config_.wifi.subnet = wifi["subnet"] | "255.255.255.0";
    }
    
    // Parse Gateway config
    JsonObjectConst gateway = doc["gateway"];
    if (gateway) {
        config_.gateway.websocket_url = gateway["websocket_url"] | DEFAULT_GATEWAY_URL;
        config_.gateway.fallback_http_url = gateway["fallback_url"] | DEFAULT_FALLBACK_URL;
        config_.gateway.api_key = gateway["api_key"] | "";
        config_.gateway.reconnect_interval_ms = gateway["reconnect_interval_ms"] | 5000;
        config_.gateway.ping_interval_ms = gateway["ping_interval_ms"] | 30000;
        config_.gateway.connection_timeout_ms = gateway["connection_timeout_ms"] | 10000;
    }
    
    // Parse Device config
    JsonObjectConst device = doc["device"];
    if (device) {
        config_.device.id = device["id"] | DEFAULT_DEVICE_ID;
        config_.device.name = device["name"] | DEFAULT_DEVICE_NAME;
        config_.device.firmware_version = device["firmware_version"] | "1.0.0";
        config_.device.auto_connect = device["auto_connect"] | true;
        config_.device.save_history = device["save_history"] | false;
        config_.device.display_brightness = device["display_brightness"] | 128;
    }
    
    // Parse Audio config
    JsonObjectConst audio = doc["audio"];
    if (audio) {
        config_.audio.sample_rate = audio["sample_rate"] | DEFAULT_SAMPLE_RATE;
        config_.audio.frame_duration_ms = audio["frame_duration_ms"] | DEFAULT_FRAME_DURATION_MS;
        config_.audio.codec = audio["codec"] | "opus";
        config_.audio.mic_gain = audio["mic_gain"] | 64;
        config_.audio.noise_suppression = audio["noise_suppression"] | true;
        config_.audio.auto_gain_control = audio["auto_gain_control"] | true;
    }
    
    return true;
}

bool ConfigManager::serializeToJson(JsonDocument& doc) const {
    // WiFi config
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = config_.wifi.ssid;
    wifi["password"] = config_.wifi.password;
    wifi["dhcp"] = config_.wifi.dhcp;
    wifi["static_ip"] = config_.wifi.static_ip;
    wifi["gateway"] = config_.wifi.gateway;
    wifi["subnet"] = config_.wifi.subnet;
    
    // Gateway config
    JsonObject gateway = doc["gateway"].to<JsonObject>();
    gateway["websocket_url"] = config_.gateway.websocket_url;
    gateway["fallback_url"] = config_.gateway.fallback_http_url;
    gateway["api_key"] = config_.gateway.api_key;
    gateway["reconnect_interval_ms"] = config_.gateway.reconnect_interval_ms;
    gateway["ping_interval_ms"] = config_.gateway.ping_interval_ms;
    gateway["connection_timeout_ms"] = config_.gateway.connection_timeout_ms;
    
    // Device config
    JsonObject device = doc["device"].to<JsonObject>();
    device["id"] = config_.device.id;
    device["name"] = config_.device.name;
    device["firmware_version"] = config_.device.firmware_version;
    device["auto_connect"] = config_.device.auto_connect;
    device["save_history"] = config_.device.save_history;
    device["display_brightness"] = config_.device.display_brightness;
    
    // Audio config
    JsonObject audio = doc["audio"].to<JsonObject>();
    audio["sample_rate"] = config_.audio.sample_rate;
    audio["frame_duration_ms"] = config_.audio.frame_duration_ms;
    audio["codec"] = config_.audio.codec;
    audio["mic_gain"] = config_.audio.mic_gain;
    audio["noise_suppression"] = config_.audio.noise_suppression;
    audio["auto_gain_control"] = config_.audio.auto_gain_control;
    
    return true;
}

} // namespace OpenClaw
