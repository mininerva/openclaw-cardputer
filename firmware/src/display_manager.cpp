/**
 * @file display_manager.cpp
 * @brief Display manager implementation
 */

#include "display_manager.h"
#include <algorithm>

namespace OpenClaw {

DisplayManager::DisplayManager() {
    memset(last_error_, 0, sizeof(last_error_));
}

DisplayManager::~DisplayManager() {
    end();
}

bool DisplayManager::begin(const DeviceConfig& config) {
    if (initialized_) {
        return true;
    }
    
    // Get display from M5Cardputer
    gfx_ = &M5Cardputer.Display;
    
    if (!gfx_) {
        strncpy(last_error_, "Failed to get display handle", sizeof(last_error_) - 1);
        return false;
    }
    
    // Initialize display
    gfx_->init();
    gfx_->setRotation(1);  // Landscape
    gfx_->setBrightness(config.display_brightness);
    brightness_ = config.display_brightness;
    
    // Clear screen
    gfx_->fillScreen(COLOR_BG);
    
    initialized_ = true;
    return true;
}

void DisplayManager::clear() {
    if (!initialized_) return;
    
    messages_.clear();
    scroll_offset_ = 0;
    gfx_->fillScreen(COLOR_BG);
    needs_redraw_ = true;
}

void DisplayManager::update() {
    if (!initialized_) return;
    
    // Clear status message if timed out
    if (show_status_ && millis() > status_clear_time_) {
        show_status_ = false;
        needs_redraw_ = true;
    }
    
    if (needs_redraw_) {
        drawStatusBar();
        drawConversation();
        drawInputArea();
        needs_redraw_ = false;
    } else if (input_changed_) {
        drawInputArea();
        input_changed_ = false;
    }
}

void DisplayManager::end() {
    if (!initialized_) return;
    
    gfx_ = nullptr;
    initialized_ = false;
}

void DisplayManager::addMessage(const char* text, MessageType type) {
    DisplayMessage msg(text, type);
    messages_.push_back(msg);
    
    // Limit message history
    if (messages_.size() > MAX_MESSAGE_LINES) {
        messages_.erase(messages_.begin());
    }
    
    // Auto-scroll to bottom
    scrollToBottom();
    needs_redraw_ = true;
}

void DisplayManager::addMessagef(MessageType type, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    addMessage(buffer, type);
}

void DisplayManager::setInputText(const char* text, size_t cursor_pos) {
    input_text_ = text;
    input_cursor_ = cursor_pos;
    input_changed_ = true;
}

void DisplayManager::clearInput() {
    input_text_.clear();
    input_cursor_ = 0;
    input_changed_ = true;
}

void DisplayManager::setConnectionStatus(ConnectionStatus status) {
    if (conn_status_ != status) {
        conn_status_ = status;
        needs_redraw_ = true;
    }
}

void DisplayManager::setAudioStatus(AudioStatus status) {
    if (audio_status_ != status) {
        audio_status_ = status;
        needs_redraw_ = true;
    }
}

void DisplayManager::setWiFiSignal(int8_t rssi) {
    if (wifi_rssi_ != rssi) {
        wifi_rssi_ = rssi;
        needs_redraw_ = true;
    }
}

void DisplayManager::showStatus(const char* message, uint32_t duration_ms) {
    status_message_ = message;
    status_clear_time_ = millis() + duration_ms;
    show_status_ = true;
    needs_redraw_ = true;
}

void DisplayManager::scrollUp() {
    if (scroll_offset_ > 0) {
        scroll_offset_--;
        needs_redraw_ = true;
    }
}

void DisplayManager::scrollDown() {
    int max_offset = std::max(0, (int)messages_.size() - VISIBLE_LINES);
    if (scroll_offset_ < max_offset) {
        scroll_offset_++;
        needs_redraw_ = true;
    }
}

void DisplayManager::scrollToBottom() {
    int max_offset = std::max(0, (int)messages_.size() - VISIBLE_LINES);
    if (scroll_offset_ != max_offset) {
        scroll_offset_ = max_offset;
        needs_redraw_ = true;
    }
}

void DisplayManager::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    if (gfx_) {
        gfx_->setBrightness(brightness);
    }
}

void DisplayManager::showBootScreen(const char* version) {
    if (!gfx_) return;
    
    gfx_->fillScreen(COLOR_BG);
    gfx_->setTextColor(COLOR_ACCENT);
    gfx_->setTextSize(2);
    
    // Draw title
    const char* title = "OpenClaw";
    int16_t title_width = strlen(title) * 12;
    gfx_->setCursor((DISPLAY_WIDTH - title_width) / 2, 30);
    gfx_->print(title);
    
    // Draw subtitle
    gfx_->setTextColor(COLOR_TEXT);
    gfx_->setTextSize(1);
    const char* subtitle = "Cardputer ADV";
    int16_t sub_width = strlen(subtitle) * 6;
    gfx_->setCursor((DISPLAY_WIDTH - sub_width) / 2, 55);
    gfx_->print(subtitle);
    
    // Draw version
    gfx_->setTextColor(COLOR_TEXT_DIM);
    char version_str[32];
    snprintf(version_str, sizeof(version_str), "v%s", version);
    int16_t ver_width = strlen(version_str) * 6;
    gfx_->setCursor((DISPLAY_WIDTH - ver_width) / 2, 75);
    gfx_->print(version_str);
    
    // Draw loading bar frame
    gfx_->drawRect(40, 100, 160, 10, COLOR_TEXT_DIM);
}

void DisplayManager::showConnectionScreen(const char* ssid) {
    if (!gfx_) return;
    
    gfx_->fillScreen(COLOR_BG);
    gfx_->setTextColor(COLOR_TEXT);
    gfx_->setTextSize(1);
    
    gfx_->setCursor(10, 50);
    gfx_->print("Connecting to:");
    
    gfx_->setTextColor(COLOR_ACCENT);
    gfx_->setCursor(10, 70);
    gfx_->print(ssid);
    
    gfx_->setTextColor(COLOR_TEXT_DIM);
    gfx_->setCursor(10, 100);
    gfx_->print("Please wait...");
}

void DisplayManager::showErrorScreen(const char* error) {
    if (!gfx_) return;
    
    gfx_->fillScreen(COLOR_BG);
    gfx_->setTextColor(COLOR_ERROR);
    gfx_->setTextSize(1);
    
    gfx_->setCursor(10, 50);
    gfx_->print("Error:");
    
    gfx_->setTextColor(COLOR_TEXT);
    gfx_->setCursor(10, 70);
    
    // Wrap error text
    std::vector<String> lines;
    wrapText(error, lines);
    int16_t y = 70;
    for (const auto& line : lines) {
        if (y > DISPLAY_HEIGHT - 20) break;
        gfx_->setCursor(10, y);
        gfx_->print(line.c_str());
        y += LINE_HEIGHT;
    }
}

void DisplayManager::redraw() {
    needs_redraw_ = true;
}

void DisplayManager::drawStatusBar() {
    if (!gfx_) return;
    
    // Draw status bar background
    gfx_->fillRect(0, 0, DISPLAY_WIDTH, STATUS_BAR_HEIGHT, COLOR_STATUS_BG);
    
    gfx_->setTextSize(1);
    
    // WiFi signal
    int8_t bars = getSignalBars();
    gfx_->setTextColor(COLOR_TEXT);
    gfx_->setCursor(2, 4);
    if (bars >= 0) {
        gfx_->printf("WiFi:%d", bars);
    } else {
        gfx_->print("WiFi:X");
    }
    
    // Connection status
    gfx_->setCursor(60, 4);
    switch (conn_status_) {
        case ConnectionStatus::DISCONNECTED:
            gfx_->setTextColor(COLOR_ERROR);
            gfx_->print("[--]");
            break;
        case ConnectionStatus::CONNECTING:
            gfx_->setTextColor(COLOR_WARNING);
            gfx_->print("[..]");
            break;
        case ConnectionStatus::CONNECTED:
            gfx_->setTextColor(COLOR_ACCENT);
            gfx_->print("[OK]");
            break;
        case ConnectionStatus::ERROR:
            gfx_->setTextColor(COLOR_ERROR);
            gfx_->print("[ER]");
            break;
    }
    
    // Audio status
    gfx_->setCursor(100, 4);
    gfx_->setTextColor(COLOR_TEXT);
    switch (audio_status_) {
        case AudioStatus::IDLE:
            gfx_->print("[  ]");
            break;
        case AudioStatus::LISTENING:
            gfx_->setTextColor(COLOR_ACCENT);
            gfx_->print("[oo]");
            break;
        case AudioStatus::PROCESSING:
            gfx_->setTextColor(COLOR_WARNING);
            gfx_->print("[~~]");
            break;
        case AudioStatus::SPEAKING:
            gfx_->setTextColor(COLOR_ACCENT);
            gfx_->print("[<>]");
            break;
    }
    
    // Status message (if active)
    if (show_status_) {
        gfx_->setTextColor(COLOR_WARNING);
        int16_t msg_x = DISPLAY_WIDTH - (status_message_.length() * 6) - 2;
        gfx_->setCursor(msg_x, 4);
        gfx_->print(status_message_.c_str());
    }
}

void DisplayManager::drawConversation() {
    if (!gfx_) return;
    
    // Clear conversation area
    int16_t conv_y = STATUS_BAR_HEIGHT;
    int16_t conv_height = DISPLAY_HEIGHT - STATUS_BAR_HEIGHT - INPUT_AREA_HEIGHT;
    gfx_->fillRect(0, conv_y, DISPLAY_WIDTH, conv_height, COLOR_BG);
    
    // Draw messages
    gfx_->setTextSize(1);
    int16_t y = conv_y + 2;
    
    int start_idx = scroll_offset_;
    int end_idx = std::min(start_idx + VISIBLE_LINES, (int)messages_.size());
    
    for (int i = start_idx; i < end_idx; i++) {
        drawMessage(messages_[i], y);
        y += LINE_HEIGHT;
    }
    
    // Draw scrollbar if needed
    if (messages_.size() > VISIBLE_LINES) {
        drawScrollbar();
    }
}

void DisplayManager::drawMessage(const DisplayMessage& msg, int16_t y) {
    gfx_->setTextColor(getMessageColor(msg.type));
    gfx_->setCursor(4, y);
    
    // Prefix based on type
    switch (msg.type) {
        case MessageType::USER:
            gfx_->print("> ");
            break;
        case MessageType::AI:
            gfx_->print("< ");
            break;
        case MessageType::SYSTEM:
            gfx_->print("# ");
            break;
        case MessageType::ERROR:
            gfx_->print("! ");
            break;
        case MessageType::STATUS:
            gfx_->print("* ");
            break;
    }
    
    // Print message (truncated if needed)
    String display_text = msg.text;
    if (display_text.length() > 35) {
        display_text = display_text.substring(0, 32) + "...";
    }
    gfx_->print(display_text.c_str());
}

void DisplayManager::drawInputArea() {
    if (!gfx_) return;
    
    int16_t input_y = DISPLAY_HEIGHT - INPUT_AREA_HEIGHT;
    
    // Draw input background
    gfx_->fillRect(0, input_y, DISPLAY_WIDTH, INPUT_AREA_HEIGHT, COLOR_INPUT_BG);
    gfx_->drawLine(0, input_y, DISPLAY_WIDTH, input_y, COLOR_TEXT_DIM);
    
    // Draw input text
    gfx_->setTextColor(COLOR_TEXT);
    gfx_->setTextSize(1);
    gfx_->setCursor(4, input_y + 6);
    
    // Show cursor indicator
    gfx_->print("> ");
    
    // Truncate if too long
    String display_input = input_text_;
    if (display_input.length() > 30) {
        display_input = "..." + display_input.substring(display_input.length() - 27);
    }
    gfx_->print(display_input.c_str());
    
    // Draw cursor
    if ((millis() / 500) % 2 == 0) {
        int16_t cursor_x = 16 + (display_input.length() * 6);
        gfx_->fillRect(cursor_x, input_y + 4, 6, 12, COLOR_ACCENT);
    }
}

void DisplayManager::drawScrollbar() {
    int16_t conv_y = STATUS_BAR_HEIGHT;
    int16_t conv_height = DISPLAY_HEIGHT - STATUS_BAR_HEIGHT - INPUT_AREA_HEIGHT;
    int16_t scrollbar_x = DISPLAY_WIDTH - SCROLLBAR_WIDTH;
    
    // Draw track
    gfx_->fillRect(scrollbar_x, conv_y, SCROLLBAR_WIDTH, conv_height, COLOR_STATUS_BG);
    
    // Draw thumb
    float thumb_ratio = (float)VISIBLE_LINES / messages_.size();
    int16_t thumb_height = (int16_t)(conv_height * thumb_ratio);
    if (thumb_height < 10) thumb_height = 10;
    
    float scroll_ratio = (float)scroll_offset_ / (messages_.size() - VISIBLE_LINES);
    int16_t thumb_y = conv_y + (int16_t)((conv_height - thumb_height) * scroll_ratio);
    
    gfx_->fillRect(scrollbar_x, thumb_y, SCROLLBAR_WIDTH, thumb_height, COLOR_ACCENT);
}

uint16_t DisplayManager::getMessageColor(MessageType type) {
    switch (type) {
        case MessageType::USER:
            return COLOR_USER_MSG;
        case MessageType::AI:
            return COLOR_AI_MSG;
        case MessageType::SYSTEM:
            return COLOR_TEXT_DIM;
        case MessageType::ERROR:
            return COLOR_ERROR;
        case MessageType::STATUS:
            return COLOR_WARNING;
        default:
            return COLOR_TEXT;
    }
}

const char* DisplayManager::getStatusIcon() {
    switch (conn_status_) {
        case ConnectionStatus::CONNECTED:
            return "●";
        case ConnectionStatus::CONNECTING:
            return "○";
        case ConnectionStatus::ERROR:
            return "✗";
        default:
            return "○";
    }
}

const char* DisplayManager::getAudioIcon() {
    switch (audio_status_) {
        case AudioStatus::LISTENING:
            return "🎤";
        case AudioStatus::PROCESSING:
            return "⚙";
        case AudioStatus::SPEAKING:
            return "🔊";
        default:
            return " ";
    }
}

int8_t DisplayManager::getSignalBars() {
    if (wifi_rssi_ >= -50) return 4;
    if (wifi_rssi_ >= -60) return 3;
    if (wifi_rssi_ >= -70) return 2;
    if (wifi_rssi_ >= -80) return 1;
    if (wifi_rssi_ >= -90) return 0;
    return -1;
}

void DisplayManager::wrapText(const String& text, std::vector<String>& lines) {
    lines.clear();
    
    int max_chars = (DISPLAY_WIDTH - 20) / CHAR_WIDTH;
    String current_line;
    
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '\n' || current_line.length() >= max_chars) {
            lines.push_back(current_line);
            current_line.clear();
            if (text[i] != '\n') {
                current_line += text[i];
            }
        } else {
            current_line += text[i];
        }
    }
    
    if (current_line.length() > 0) {
        lines.push_back(current_line);
    }
    
    if (lines.empty()) {
        lines.push_back("");
    }
}

uint16_t DisplayManager::darkenColor(uint16_t color, uint8_t percent) {
    // Extract RGB components
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    // Darken
    r = (r * (100 - percent)) / 100;
    g = (g * (100 - percent)) / 100;
    b = (b * (100 - percent)) / 100;
    
    // Recombine
    return (r << 11) | (g << 5) | b;
}

} // namespace OpenClaw
