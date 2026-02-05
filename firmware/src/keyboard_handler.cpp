/**
 * @file keyboard_handler.cpp
 * @brief Keyboard handler implementation (stub)
 */

#include "keyboard_handler.h"

namespace OpenClaw {

KeyboardHandler::KeyboardHandler()
    : initialized_(false),
      event_callback_(nullptr),
      key_pressed_(false),
      last_character_(0),
      key_press_time_(0),
      last_repeat_time_(0),
      repeat_count_(0),
      shift_pressed_(false),
      fn_pressed_(false),
      ctrl_pressed_(false),
      opt_pressed_(false),
      last_shift_state_(false),
      last_fn_state_(false),
      last_ctrl_state_(false),
      last_opt_state_(false),
      key_repeat_enabled_(true),
      key_repeat_delay_ms_(DEFAULT_KEY_REPEAT_DELAY_MS),
      key_repeat_rate_ms_(DEFAULT_KEY_REPEAT_RATE_MS),
      event_queue_(nullptr),
      state_mutex_(nullptr),
      last_poll_time_(0),
      key_press_count_(0),
      input_submit_count_(0) {
}

KeyboardHandler::~KeyboardHandler() {
    end();
}

bool KeyboardHandler::begin() {
    if (initialized_) return true;
    if (!createQueue() || !createMutexes()) return false;
    initialized_ = true;
    return true;
}

void KeyboardHandler::end() {
    destroyQueue();
    destroyMutexes();
    initialized_ = false;
}

void KeyboardHandler::update() {
    if (!initialized_) return;
    // Stub implementation
}

bool KeyboardHandler::readEvent(KeyEvent& event) {
    if (!event_queue_) return false;
    return xQueueReceive(event_queue_, &event, 0) == pdTRUE;
}

void KeyboardHandler::clearInput() {
    input_buffer_.clear();
}

void KeyboardHandler::submitInput() {
    input_submit_count_++;
    if (event_callback_) {
        event_callback_(KeyboardEvent::INPUT_SUBMITTED, input_buffer_.getText());
    }
}

bool KeyboardHandler::setInputText(const char* text) {
    return input_buffer_.setText(text);
}

void KeyboardHandler::onEvent(KeyboardEventCallback callback) {
    event_callback_ = callback;
}

bool KeyboardHandler::createQueue() {
    event_queue_ = xQueueCreate(KEYBOARD_QUEUE_SIZE, sizeof(KeyEvent));
    return event_queue_ != nullptr;
}

void KeyboardHandler::destroyQueue() {
    if (event_queue_) {
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
    }
}

bool KeyboardHandler::createMutexes() {
    state_mutex_ = xSemaphoreCreateMutex();
    return state_mutex_ != nullptr;
}

void KeyboardHandler::destroyMutexes() {
    if (state_mutex_) {
        vSemaphoreDelete(state_mutex_);
        state_mutex_ = nullptr;
    }
}

// InputBuffer implementation
InputBuffer::InputBuffer()
    : length_(0), cursor_(0) {
    buffer_[0] = '\0';
}

bool InputBuffer::insert(char c) {
    if (length_ >= KEYBOARD_BUFFER_SIZE - 1) return false;
    for (size_t i = length_; i > cursor_; --i) {
        buffer_[i] = buffer_[i - 1];
    }
    buffer_[cursor_] = c;
    cursor_++;
    length_++;
    buffer_[length_] = '\0';
    return true;
}

bool InputBuffer::backspace() {
    if (cursor_ == 0 || length_ == 0) return false;
    for (size_t i = cursor_ - 1; i < length_; ++i) {
        buffer_[i] = buffer_[i + 1];
    }
    cursor_--;
    length_--;
    buffer_[length_] = '\0';
    return true;
}

bool InputBuffer::deleteChar() {
    if (cursor_ >= length_) return false;
    for (size_t i = cursor_; i < length_; ++i) {
        buffer_[i] = buffer_[i + 1];
    }
    length_--;
    buffer_[length_] = '\0';
    return true;
}

bool InputBuffer::insertString(const char* str) {
    while (*str) {
        if (!insert(*str++)) return false;
    }
    return true;
}

void InputBuffer::clear() {
    length_ = 0;
    cursor_ = 0;
    buffer_[0] = '\0';
}

void InputBuffer::moveCursorLeft() {
    if (cursor_ > 0) cursor_--;
}

void InputBuffer::moveCursorRight() {
    if (cursor_ < length_) cursor_++;
}

void InputBuffer::moveCursorHome() {
    cursor_ = 0;
}

void InputBuffer::moveCursorEnd() {
    cursor_ = length_;
}

void InputBuffer::moveCursorTo(size_t pos) {
    if (pos <= length_) cursor_ = pos;
}

String InputBuffer::getTextBeforeCursor() const {
    return String(buffer_).substring(0, cursor_);
}

String InputBuffer::getTextAfterCursor() const {
    return String(buffer_).substring(cursor_);
}

bool InputBuffer::setText(const char* text) {
    clear();
    size_t len = strlen(text);
    if (len >= KEYBOARD_BUFFER_SIZE) return false;
    memcpy(buffer_, text, len);
    length_ = len;
    cursor_ = len;
    buffer_[length_] = '\0';
    return true;
}

const char* specialKeyToString(SpecialKey key) {
    // Use if-else chain instead of switch to avoid duplicate case values
    // (some enum values like ENTER=0x0D may collide with auto-assigned values)
    if (key == SpecialKey::NONE) return "NONE";
    if (key == SpecialKey::UP) return "UP";
    if (key == SpecialKey::DOWN) return "DOWN";
    if (key == SpecialKey::LEFT) return "LEFT";
    if (key == SpecialKey::RIGHT) return "RIGHT";
    if (key == SpecialKey::HOME) return "HOME";
    if (key == SpecialKey::END) return "END";
    if (key == SpecialKey::PAGE_UP) return "PAGE_UP";
    if (key == SpecialKey::PAGE_DOWN) return "PAGE_DOWN";
    if (key == SpecialKey::ENTER) return "ENTER";
    if (key == SpecialKey::BACKSPACE) return "BACKSPACE";
    if (key == SpecialKey::DELETE) return "DELETE";
    if (key == SpecialKey::TAB) return "TAB";
    if (key == SpecialKey::INSERT) return "INSERT";
    if (key == SpecialKey::SHIFT_PRESS) return "SHIFT_PRESS";
    if (key == SpecialKey::SHIFT_RELEASE) return "SHIFT_RELEASE";
    if (key == SpecialKey::FN_PRESS) return "FN_PRESS";
    if (key == SpecialKey::FN_RELEASE) return "FN_RELEASE";
    if (key == SpecialKey::CTRL_PRESS) return "CTRL_PRESS";
    if (key == SpecialKey::CTRL_RELEASE) return "CTRL_RELEASE";
    if (key == SpecialKey::OPT_PRESS) return "OPT_PRESS";
    if (key == SpecialKey::OPT_RELEASE) return "OPT_RELEASE";
    if (key == SpecialKey::F1) return "F1";
    if (key == SpecialKey::F2) return "F2";
    if (key == SpecialKey::F3) return "F3";
    if (key == SpecialKey::F4) return "F4";
    if (key == SpecialKey::F5) return "F5";
    if (key == SpecialKey::VOICE_TOGGLE) return "VOICE_TOGGLE";
    if (key == SpecialKey::SEND) return "SEND";
    if (key == SpecialKey::ESCAPE) return "ESCAPE";
    if (key == SpecialKey::ANCIENT_MODE) return "ANCIENT_MODE";
    return "UNKNOWN";
}

const char* keyboardEventToString(KeyboardEvent event) {
    switch (event) {
        case KeyboardEvent::KEY_PRESSED: return "KEY_PRESSED";
        case KeyboardEvent::KEY_RELEASED: return "KEY_RELEASED";
        case KeyboardEvent::KEY_REPEATED: return "KEY_REPEATED";
        case KeyboardEvent::TEXT_INPUT: return "TEXT_INPUT";
        case KeyboardEvent::INPUT_SUBMITTED: return "INPUT_SUBMITTED";
        case KeyboardEvent::INPUT_CHANGED: return "INPUT_CHANGED";
        case KeyboardEvent::MODIFIER_CHANGED: return "MODIFIER_CHANGED";
        case KeyboardEvent::SPECIAL_KEY: return "SPECIAL_KEY";
        default: return "UNKNOWN";
    }
}

bool isAncientModeTrigger(const char* text) {
    String t = String(text);
    t.toLowerCase();
    return (t.indexOf("ancient wisdom") >= 0 ||
            t.indexOf("speak as minerva") >= 0 ||
            t.indexOf("owl mode") >= 0 ||
            t.indexOf("by the thirty-seven claws") >= 0);
}

} // namespace OpenClaw
