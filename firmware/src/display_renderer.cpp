/**
 * @file display_renderer.cpp
 * @brief Display renderer implementation (stub)
 */

#include "display_renderer.h"

namespace OpenClaw {

TextRenderer::TextRenderer(M5Canvas* canvas) : canvas_(canvas) {
}

int16_t TextRenderer::getTextWidth(const char* text) {
    return canvas_ ? canvas_->textWidth(text) : 0;
}

int16_t TextRenderer::getTextHeight(const char* text, int16_t max_width) {
    return canvas_ ? canvas_->fontHeight() : 8;
}

std::vector<String> TextRenderer::wrapText(const char* text, int16_t max_width) {
    std::vector<String> lines;
    lines.push_back(String(text));
    return lines;
}

void TextRenderer::renderWrappedText(const char* text, int16_t x, int16_t y,
                                      int16_t max_width, int16_t max_height,
                                      uint16_t color) {
    if (canvas_) {
        canvas_->setTextColor(color);
        canvas_->drawString(text, x, y);
    }
}

void TextRenderer::renderLine(const char* text, int16_t x, int16_t y, uint16_t color) {
    if (canvas_) {
        canvas_->setTextColor(color);
        canvas_->drawString(text, x, y);
    }
}

DisplayRenderer::DisplayRenderer()
    : main_canvas_(nullptr),
      avatar_canvas_(nullptr),
      message_canvas_(nullptr),
      text_renderer_(nullptr),
      scroll_position_(0),
      input_cursor_pos_(0),
      show_cursor_(true),
      cursor_blink_time_(0),
      cursor_visible_(true),
      last_status_update_(0),
      needs_redraw_(true),
      initialized_(false) {
}

DisplayRenderer::~DisplayRenderer() {
    end();
}

bool DisplayRenderer::begin(const DisplayConfig& config) {
    config_ = config;
    initialized_ = true;
    return createCanvases();
}

void DisplayRenderer::end() {
    destroyCanvases();
    initialized_ = false;
}

void DisplayRenderer::update() {
    updateCursorBlink();
}

void DisplayRenderer::clear() {
    if (main_canvas_) {
        main_canvas_->fillSprite(Colors::BACKGROUND);
    }
    markDirty();
}

void DisplayRenderer::addMessage(const char* text, DisplayMessageType type) {
    DisplayMessage msg(text, type);
    messages_.push_back(msg);
    if (messages_.size() > MAX_MESSAGE_HISTORY) {
        messages_.erase(messages_.begin());
    }
    markDirty();
}

void DisplayRenderer::addMessage(const String& text, DisplayMessageType type) {
    addMessage(text.c_str(), type);
}

void DisplayRenderer::updateLastMessage(const char* text, bool is_final) {
    if (!messages_.empty()) {
        messages_.back().text = text;
        messages_.back().is_final = is_final;
        markDirty();
    }
}

void DisplayRenderer::clearMessages() {
    messages_.clear();
    markDirty();
}

void DisplayRenderer::scrollUp() {
    if (scroll_position_ > 0) {
        scroll_position_--;
        markDirty();
    }
}

void DisplayRenderer::scrollDown() {
    if (scroll_position_ < messages_.size() - VISIBLE_MESSAGES) {
        scroll_position_++;
        markDirty();
    }
}

void DisplayRenderer::scrollToBottom() {
    if (messages_.size() > VISIBLE_MESSAGES) {
        scroll_position_ = messages_.size() - VISIBLE_MESSAGES;
    } else {
        scroll_position_ = 0;
    }
    markDirty();
}

void DisplayRenderer::setScrollPosition(uint8_t position) {
    scroll_position_ = position;
    markDirty();
}

void DisplayRenderer::setInputText(const char* text, size_t cursor_pos) {
    input_text_ = text;
    input_cursor_pos_ = cursor_pos;
    markDirty();
}

void DisplayRenderer::clearInput() {
    input_text_ = "";
    input_cursor_pos_ = 0;
    markDirty();
}

void DisplayRenderer::showInputCursor(bool show) {
    show_cursor_ = show;
    markDirty();
}

void DisplayRenderer::setConnectionStatus(ConnectionIndicator status) {
    status_data_.connection = status;
    markDirty();
}

void DisplayRenderer::setAudioStatus(AudioIndicator status) {
    status_data_.audio = status;
    markDirty();
}

void DisplayRenderer::setWiFiSignal(int8_t rssi) {
    status_data_.wifi_rssi = rssi;
    markDirty();
}

void DisplayRenderer::setBatteryStatus(uint8_t percent, bool charging) {
    status_data_.battery_percent = percent;
    status_data_.charging = charging;
    markDirty();
}

void DisplayRenderer::setStatusText(const char* text) {
    strncpy(status_data_.status_text, text, sizeof(status_data_.status_text) - 1);
    status_data_.status_text[sizeof(status_data_.status_text) - 1] = '\0';
    markDirty();
}

void DisplayRenderer::clearAvatarArea() {
    if (avatar_canvas_) {
        avatar_canvas_->fillSprite(Colors::BACKGROUND);
    }
    markDirty();
}

void DisplayRenderer::renderBootScreen(const char* firmware_version) {
    M5Cardputer.Display.fillScreen(Colors::BACKGROUND);
    M5Cardputer.Display.setTextColor(Colors::TEXT_USER);
    M5Cardputer.Display.drawString("OpenClaw Cardputer", 10, 10);
    M5Cardputer.Display.drawString(String("v") + firmware_version, 10, 30);
    M5Cardputer.Display.drawString("Booting...", 10, 60);
}

void DisplayRenderer::renderConnectionScreen(const char* ssid) {
    M5Cardputer.Display.fillScreen(Colors::BACKGROUND);
    M5Cardputer.Display.setTextColor(Colors::TEXT_USER);
    M5Cardputer.Display.drawString("Connecting to WiFi", 10, 10);
    M5Cardputer.Display.drawString(String("SSID: ") + ssid, 10, 30);
}

void DisplayRenderer::renderErrorScreen(const char* error) {
    M5Cardputer.Display.fillScreen(Colors::BACKGROUND);
    M5Cardputer.Display.setTextColor(Colors::TEXT_ERROR);
    M5Cardputer.Display.drawString("Error", 10, 10);
    M5Cardputer.Display.drawString(error, 10, 30);
}

void DisplayRenderer::renderMainScreen() {
    if (!needs_redraw_) return;
    renderStatusBar();
    renderMessages();
    renderInputArea();
    needs_redraw_ = false;
}

void DisplayRenderer::renderStatusBar() {
    // Stub implementation
}

void DisplayRenderer::renderMessages() {
    // Stub implementation
}

void DisplayRenderer::renderInputArea() {
    // Stub implementation
}

void DisplayRenderer::setBrightness(uint8_t brightness) {
    config_.brightness = brightness;
    M5Cardputer.Display.setBrightness(brightness);
}

void DisplayRenderer::setConfig(const DisplayConfig& config) {
    config_ = config;
    markDirty();
}

void DisplayRenderer::redraw() {
    markDirty();
    renderMainScreen();
}

bool DisplayRenderer::createCanvases() {
    // Stub - would create off-screen canvases
    return true;
}

void DisplayRenderer::destroyCanvases() {
    // Stub
}

void DisplayRenderer::updateCursorBlink() {
    uint32_t now = millis();
    if (now - cursor_blink_time_ >= 500) {
        cursor_blink_time_ = now;
        cursor_visible_ = !cursor_visible_;
        markDirty();
    }
}

uint16_t DisplayRenderer::getMessageColor(MessageType type) const {
    switch (type) {
        case MessageType::TEXT: return Colors::TEXT_USER;
        case MessageType::RESPONSE:
        case MessageType::RESPONSE_FINAL: return Colors::TEXT_AI;
        case MessageType::ERROR: return Colors::TEXT_ERROR;
        case MessageType::STATUS: return Colors::TEXT_SYSTEM;
        default: return Colors::TEXT_USER;
    }
}

const char* DisplayRenderer::getMessagePrefix(MessageType type) const {
    switch (type) {
        case MessageType::TEXT: return "> ";
        case MessageType::RESPONSE:
        case MessageType::RESPONSE_FINAL: return "< ";
        case MessageType::ERROR: return "! ";
        case MessageType::STATUS: return "* ";
        default: return "";
    }
}

const char* displayMessageTypeToString(DisplayMessageType type) {
    switch (type) {
        case DisplayMessageType::USER_MSG: return "USER";
        case DisplayMessageType::AI_MSG: return "AI";
        case DisplayMessageType::SYSTEM_MSG: return "SYSTEM";
        case DisplayMessageType::ERROR_MSG: return "ERROR";
        case DisplayMessageType::STATUS_MSG: return "STATUS";
        default: return "UNKNOWN";
    }
}

const char* connectionIndicatorToString(ConnectionIndicator status) {
    switch (status) {
        case ConnectionIndicator::DISCONNECTED: return "DISCONNECTED";
        case ConnectionIndicator::CONNECTING: return "CONNECTING";
        case ConnectionIndicator::CONNECTED: return "CONNECTED";
        case ConnectionIndicator::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* audioIndicatorToString(AudioIndicator status) {
    switch (status) {
        case AudioIndicator::IDLE: return "IDLE";
        case AudioIndicator::LISTENING: return "LISTENING";
        case AudioIndicator::PROCESSING: return "PROCESSING";
        case AudioIndicator::SPEAKING: return "SPEAKING";
        case AudioIndicator::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

uint16_t colorForDisplayMessageType(DisplayMessageType type) {
    switch (type) {
        case DisplayMessageType::USER_MSG: return Colors::TEXT_USER;
        case DisplayMessageType::AI_MSG: return Colors::TEXT_AI;
        case DisplayMessageType::SYSTEM_MSG: return Colors::TEXT_SYSTEM;
        case DisplayMessageType::ERROR_MSG: return Colors::TEXT_ERROR;
        case DisplayMessageType::STATUS_MSG: return Colors::TEXT_SYSTEM;
        default: return Colors::TEXT_USER;
    }
}

} // namespace OpenClaw
