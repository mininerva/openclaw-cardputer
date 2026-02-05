/**
 * @file display_manager.h
 * @brief Display management for OpenClaw Cardputer
 * 
 * Handles the Cardputer's 240x135 color LCD for rendering
 * conversation history, status indicators, and UI elements.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <vector>
#include <string>
#include "config_manager.h"

namespace OpenClaw {

// Display constants
constexpr uint16_t DISPLAY_WIDTH = 240;
constexpr uint16_t DISPLAY_HEIGHT = 135;
constexpr uint8_t STATUS_BAR_HEIGHT = 16;
constexpr uint8_t INPUT_AREA_HEIGHT = 20;
constexpr uint8_t SCROLLBAR_WIDTH = 4;
constexpr uint8_t MAX_MESSAGE_LINES = 100;
constexpr uint8_t VISIBLE_LINES = 6;

// Color scheme
constexpr uint16_t COLOR_BG = 0x0000;           // Black
constexpr uint16_t COLOR_TEXT = 0xFFFF;         // White
constexpr uint16_t COLOR_TEXT_DIM = 0x8410;     // Gray
constexpr uint16_t COLOR_ACCENT = 0x07E0;       // Green
constexpr uint16_t COLOR_WARNING = 0xFFE0;      // Yellow
constexpr uint16_t COLOR_ERROR = 0xF800;        // Red
constexpr uint16_t COLOR_USER_MSG = 0x07FF;     // Cyan
constexpr uint16_t COLOR_AI_MSG = 0xFFFF;       // White
constexpr uint16_t COLOR_STATUS_BG = 0x1082;    // Dark gray
constexpr uint16_t COLOR_INPUT_BG = 0x2104;     // Darker gray

/**
 * @brief Message types for display
 */
enum class MessageType {
    USER,       // User message
    AI,         // AI response
    SYSTEM,     // System message
    ERROR,      // Error message
    STATUS      // Status update
};

/**
 * @brief Display message structure
 */
struct DisplayMessage {
    String text;
    MessageType type;
    uint32_t timestamp;
    bool rendered;
    
    DisplayMessage() : type(MessageType::SYSTEM), timestamp(0), rendered(false) {}
    DisplayMessage(const char* t, MessageType ty) 
        : text(t), type(ty), timestamp(millis()), rendered(false) {}
};

/**
 * @brief Connection status indicators
 */
enum class ConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

/**
 * @brief Audio status indicators
 */
enum class AudioStatus {
    IDLE,
    LISTENING,
    PROCESSING,
    SPEAKING
};

/**
 * @brief Display manager class
 * 
 * Manages the Cardputer LCD for conversation display,
 * status bars, and input area rendering.
 */
class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();
    
    // Disable copy
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;
    
    /**
     * @brief Initialize display
     * @param config Device configuration
     * @return true if initialization successful
     */
    bool begin(const DeviceConfig& config);
    
    /**
     * @brief Clear display and redraw
     */
    void clear();
    
    /**
     * @brief Update display (call in loop)
     */
    void update();
    
    /**
     * @brief Deinitialize display
     */
    void end();
    
    /**
     * @brief Add message to conversation
     * @param text Message text
     * @param type Message type
     */
    void addMessage(const char* text, MessageType type = MessageType::SYSTEM);
    
    /**
     * @brief Add formatted message
     */
    void addMessagef(MessageType type, const char* format, ...);
    
    /**
     * @brief Update input area text
     * @param text Current input text
     * @param cursor_pos Cursor position
     */
    void setInputText(const char* text, size_t cursor_pos = 0);
    
    /**
     * @brief Clear input area
     */
    void clearInput();
    
    /**
     * @brief Set connection status
     */
    void setConnectionStatus(ConnectionStatus status);
    
    /**
     * @brief Set audio status
     */
    void setAudioStatus(AudioStatus status);
    
    /**
     * @brief Set WiFi signal strength
     * @param rssi Signal strength in dBm
     */
    void setWiFiSignal(int8_t rssi);
    
    /**
     * @brief Show status message temporarily
     */
    void showStatus(const char* message, uint32_t duration_ms = 2000);
    
    /**
     * @brief Scroll conversation up
     */
    void scrollUp();
    
    /**
     * @brief Scroll conversation down
     */
    void scrollDown();
    
    /**
     * @brief Scroll to bottom of conversation
     */
    void scrollToBottom();
    
    /**
     * @brief Set display brightness
     * @param brightness 0-255
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief Get current brightness
     */
    uint8_t getBrightness() const { return brightness_; }
    
    /**
     * @brief Show boot screen
     */
    void showBootScreen(const char* version);
    
    /**
     * @brief Show connection screen
     */
    void showConnectionScreen(const char* ssid);
    
    /**
     * @brief Show error screen
     */
    void showErrorScreen(const char* error);
    
    /**
     * @brief Get the last error message
     */
    const char* getLastError() const { return last_error_; }
    
    /**
     * @brief Force full redraw
     */
    void redraw();

private:
    // Display reference
    M5GFX* gfx_ = nullptr;
    
    // State
    bool initialized_ = false;
    uint8_t brightness_ = 128;
    
    // Messages
    std::vector<DisplayMessage> messages_;
    int16_t scroll_offset_ = 0;
    bool needs_redraw_ = true;
    
    // Input area
    String input_text_;
    size_t input_cursor_ = 0;
    bool input_changed_ = true;
    
    // Status
    ConnectionStatus conn_status_ = ConnectionStatus::DISCONNECTED;
    AudioStatus audio_status_ = AudioStatus::IDLE;
    int8_t wifi_rssi_ = -100;
    
    // Status message
    String status_message_;
    uint32_t status_clear_time_ = 0;
    bool show_status_ = false;
    
    // Error
    char last_error_[64];
    
    // Font settings
    static constexpr float FONT_SIZE = 1.0;
    static constexpr uint8_t LINE_HEIGHT = 16;
    static constexpr uint8_t CHAR_WIDTH = 6;
    
    // Private methods
    void drawStatusBar();
    void drawConversation();
    void drawInputArea();
    void drawScrollbar();
    void drawMessage(const DisplayMessage& msg, int16_t y);
    uint16_t getMessageColor(MessageType type);
    const char* getStatusIcon();
    const char* getAudioIcon();
    int8_t getSignalBars();
    void wrapText(const String& text, std::vector<String>& lines);
    uint16_t darkenColor(uint16_t color, uint8_t percent);
};

} // namespace OpenClaw

#endif // DISPLAY_MANAGER_H
