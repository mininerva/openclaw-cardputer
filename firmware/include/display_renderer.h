/**
 * @file display_renderer.h
 * @brief Display rendering for OpenClaw Cardputer
 * 
 * Features:
 * - Optimized rendering for 240x135 display
 * - Message history with scrolling
 * - Status bar with connection/audio indicators
 * - Avatar animation area
 * - Text wrapping and formatting
 */

#ifndef OPENCLAW_DISPLAY_RENDERER_H
#define OPENCLAW_DISPLAY_RENDERER_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <vector>
#include <string>
#include <memory>
#include "protocol.h"

namespace OpenClaw {

// Display constants
constexpr int16_t DISPLAY_WIDTH = 240;
constexpr int16_t DISPLAY_HEIGHT = 135;
constexpr int16_t STATUS_BAR_HEIGHT = 16;
constexpr int16_t AVATAR_AREA_HEIGHT = 64;
constexpr int16_t INPUT_AREA_HEIGHT = 24;
constexpr int16_t MESSAGE_AREA_Y = STATUS_BAR_HEIGHT + AVATAR_AREA_HEIGHT;
constexpr int16_t MESSAGE_AREA_HEIGHT = DISPLAY_HEIGHT - MESSAGE_AREA_Y - INPUT_AREA_HEIGHT;
constexpr int16_t INPUT_AREA_Y = DISPLAY_HEIGHT - INPUT_AREA_HEIGHT;

constexpr uint8_t MAX_MESSAGE_HISTORY = 50;
constexpr uint8_t VISIBLE_MESSAGES = 4;

// Colors
namespace Colors {
    constexpr uint16_t BACKGROUND = 0x0000;      // Black
    constexpr uint16_t TEXT_USER = 0xFFFF;       // White
    constexpr uint16_t TEXT_AI = 0x07E0;         // Green
    constexpr uint16_t TEXT_SYSTEM = 0x8410;     // Gray
    constexpr uint16_t TEXT_ERROR = 0xF800;      // Red
    constexpr uint16_t TEXT_INPUT = 0xFFE0;      // Yellow
    constexpr uint16_t STATUS_BAR_BG = 0x1082;   // Dark blue-gray
    constexpr uint16_t STATUS_GOOD = 0x07E0;     // Green
    constexpr uint16_t STATUS_WARN = 0xFFE0;     // Yellow
    constexpr uint16_t STATUS_BAD = 0xF800;      // Red
    constexpr uint16_t CURSOR = 0xFFFF;          // White
    constexpr uint16_t SCROLLBAR = 0x8410;       // Gray
}

// Display message types (for UI rendering)
enum class DisplayMessageType : uint8_t {
    USER_MSG,
    AI_MSG,
    SYSTEM_MSG,
    ERROR_MSG,
    STATUS_MSG
};

// Connection status indicator
enum class ConnectionIndicator {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// Audio status indicator
enum class AudioIndicator {
    IDLE,
    LISTENING,
    PROCESSING,
    SPEAKING,
    ERROR
};

// Message structure
struct DisplayMessage {
    String text;
    DisplayMessageType type;
    uint32_t timestamp;
    bool is_final;
    
    DisplayMessage() : type(DisplayMessageType::SYSTEM_MSG), timestamp(0), is_final(true) {}
    DisplayMessage(const char* t, DisplayMessageType ty) 
        : text(t), type(ty), timestamp(millis()), is_final(true) {}
};

// Display configuration
struct DisplayConfig {
    uint8_t brightness;
    bool invert_colors;
    uint8_t font_size;
    bool show_timestamps;
    bool auto_scroll;
    uint16_t text_color_user;
    uint16_t text_color_ai;
    uint16_t text_color_system;
    uint16_t text_color_error;
    
    DisplayConfig()
        : brightness(128),
          invert_colors(false),
          font_size(1),
          show_timestamps(false),
          auto_scroll(true),
          text_color_user(Colors::TEXT_USER),
          text_color_ai(Colors::TEXT_AI),
          text_color_system(Colors::TEXT_SYSTEM),
          text_color_error(Colors::TEXT_ERROR) {}
};

// Status bar data
struct StatusBarData {
    ConnectionIndicator connection;
    AudioIndicator audio;
    int8_t wifi_rssi;
    uint8_t battery_percent;
    bool charging;
    char status_text[32];
    
    StatusBarData()
        : connection(ConnectionIndicator::DISCONNECTED),
          audio(AudioIndicator::IDLE),
          wifi_rssi(-100),
          battery_percent(0),
          charging(false) {
        status_text[0] = '\0';
    }
};

// Text rendering helper
class TextRenderer {
public:
    TextRenderer(M5Canvas* canvas);
    
    // Text measurement
    int16_t getTextWidth(const char* text);
    int16_t getTextHeight(const char* text, int16_t max_width);
    
    // Text wrapping
    std::vector<String> wrapText(const char* text, int16_t max_width);
    
    // Render text with wrapping
    void renderWrappedText(const char* text, int16_t x, int16_t y, 
                           int16_t max_width, int16_t max_height,
                           uint16_t color);
    
    // Render single line
    void renderLine(const char* text, int16_t x, int16_t y, uint16_t color);

private:
    M5Canvas* canvas_;
};

// Display renderer class
class DisplayRenderer {
public:
    DisplayRenderer();
    ~DisplayRenderer();
    
    // Disable copy
    DisplayRenderer(const DisplayRenderer&) = delete;
    DisplayRenderer& operator=(const DisplayRenderer&) = delete;
    
    // Initialize
    bool begin(const DisplayConfig& config = DisplayConfig());
    
    // Update (call in main loop for animations)
    void update();
    
    // Deinitialize
    void end();
    
    // Clear display
    void clear();
    
    // Message management
    void addMessage(const char* text, DisplayMessageType type);
    void addMessage(const String& text, DisplayMessageType type);
    void updateLastMessage(const char* text, bool is_final);
    void clearMessages();
    
    // Scrolling
    void scrollUp();
    void scrollDown();
    void scrollToBottom();
    void setScrollPosition(uint8_t position);
    
    // Input display
    void setInputText(const char* text, size_t cursor_pos);
    void clearInput();
    void showInputCursor(bool show);
    
    // Status bar
    void setConnectionStatus(ConnectionIndicator status);
    void setAudioStatus(AudioIndicator status);
    void setWiFiSignal(int8_t rssi);
    void setBatteryStatus(uint8_t percent, bool charging);
    void setStatusText(const char* text);
    
    // Avatar area (for external avatar renderer)
    void clearAvatarArea();
    M5Canvas* getAvatarCanvas() { return avatar_canvas_; }
    
    // Screen rendering
    void renderBootScreen(const char* firmware_version);
    void renderConnectionScreen(const char* ssid);
    void renderErrorScreen(const char* error);
    void renderMainScreen();
    void renderStatusBar();
    void renderMessages();
    void renderInputArea();
    
    // Configuration
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return config_.brightness; }
    void setConfig(const DisplayConfig& config);
    const DisplayConfig& getConfig() const { return config_; }
    
    // Force redraw
    void redraw();
    
    // Get dimensions
    int16_t getAvatarAreaX() const { return 0; }
    int16_t getAvatarAreaY() const { return STATUS_BAR_HEIGHT; }
    int16_t getAvatarAreaWidth() const { return DISPLAY_WIDTH; }
    int16_t getAvatarAreaHeight() const { return AVATAR_AREA_HEIGHT; }

private:
    // Configuration
    DisplayConfig config_;
    
    // Canvas for off-screen rendering
    M5Canvas* main_canvas_;
    M5Canvas* avatar_canvas_;
    M5Canvas* message_canvas_;
    
    // Text renderer
    std::unique_ptr<TextRenderer> text_renderer_;
    
    // Message history
    std::vector<DisplayMessage> messages_;
    uint8_t scroll_position_;
    
    // Input state
    String input_text_;
    size_t input_cursor_pos_;
    bool show_cursor_;
    uint32_t cursor_blink_time_;
    bool cursor_visible_;
    
    // Status bar state
    StatusBarData status_data_;
    uint32_t last_status_update_;
    
    // Display state
    bool needs_redraw_;
    bool initialized_;
    
    // Private methods
    bool createCanvases();
    void destroyCanvases();
    
    void drawMessage(const DisplayMessage& msg, int16_t y, int16_t max_height);
    uint16_t getMessageColor(MessageType type) const;
    const char* getMessagePrefix(MessageType type) const;
    
    void drawStatusIcon(int16_t x, int16_t y, ConnectionIndicator status);
    void drawAudioIcon(int16_t x, int16_t y, AudioIndicator status);
    void drawWiFiIcon(int16_t x, int16_t y, int8_t rssi);
    void drawBatteryIcon(int16_t x, int16_t y, uint8_t percent, bool charging);
    
    void updateCursorBlink();
    void markDirty() { needs_redraw_ = true; }
};

// Utility functions
const char* displayMessageTypeToString(DisplayMessageType type);
const char* connectionIndicatorToString(ConnectionIndicator status);
const char* audioIndicatorToString(AudioIndicator status);
uint16_t colorForDisplayMessageType(DisplayMessageType type);

} // namespace OpenClaw

#endif // OPENCLAW_DISPLAY_RENDERER_H
