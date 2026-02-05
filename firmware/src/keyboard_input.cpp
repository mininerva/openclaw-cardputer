/**
 * @file keyboard_input.cpp
 * @brief Keyboard input implementation
 */

#include "keyboard_input.h"

namespace OpenClaw {

KeyboardInput::KeyboardInput()
    : initialized_(false),
      callback_(nullptr),
      key_pressed_(false),
      shift_pressed_(false),
      fn_pressed_(false),
      ctrl_pressed_(false),
      opt_pressed_(false),
      input_changed_(false),
      repeat_enabled_(true),
      repeat_delay_ms_(KEY_REPEAT_DELAY_MS),
      repeat_rate_ms_(KEY_REPEAT_RATE_MS),
      key_press_time_(0),
      last_repeat_time_(0),
      last_poll_time_(0),
      event_queue_(nullptr) {
    memset(&last_key_event_, 0, sizeof(last_key_event_));
    memset(last_error_, 0, sizeof(last_error_));
}

KeyboardInput::~KeyboardInput() {
    end();
}

bool KeyboardInput::begin() {
    if (initialized_) {
        return true;
    }
    
    // Create event queue
    event_queue_ = xQueueCreate(KEYBOARD_QUEUE_SIZE, sizeof(KeyEvent));
    if (!event_queue_) {
        strncpy(last_error_, "Failed to create event queue", sizeof(last_error_) - 1);
        return false;
    }
    
    initialized_ = true;
    return true;
}

void KeyboardInput::end() {
    if (event_queue_) {
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
    }
    initialized_ = false;
}

void KeyboardInput::update() {
    if (!initialized_) {
        return;
    }
    
    uint32_t now = millis();
    if (now - last_poll_time_ < KEYBOARD_POLL_INTERVAL_MS) {
        return;
    }
    last_poll_time_ = now;
    
    pollKeyboard();
    processKeyRepeat();
    
    // Notify callback if input changed
    if (input_changed_ && callback_) {
        callback_->onInputChanged(input_buffer_);
        input_changed_ = false;
    }
}

void KeyboardInput::pollKeyboard() {
    // Update modifier states
    updateModifiers();
    
    // Update keyboard state
    M5Cardputer.Keyboard.updateKeyList();
    M5Cardputer.Keyboard.updateKeysState();
    
    // Check if any key is pressed
    if (M5Cardputer.Keyboard.isPressed()) {
        auto& keyList = M5Cardputer.Keyboard.keyList();
        
        if (keyList.size() > 0 && !key_pressed_) {
            // Get first key
            auto keyCoor = keyList[0];
            auto keyValue = M5Cardputer.Keyboard.getKeyValue(keyCoor);
            char key = keyValue.value_first;
            
            // Key press
            KeyEvent event = translateKey(key, shift_pressed_, fn_pressed_, 
                                           ctrl_pressed_, opt_pressed_);
            event.pressed = true;
            event.timestamp = millis();
            
            handleKeyPress(event);
            postEvent(event);
            
            // Store for repeat
            last_key_event_ = event;
            key_press_time_ = millis();
            key_pressed_ = true;
        }
    } else if (key_pressed_) {
        // Key release
        key_pressed_ = false;
        KeyEvent event = last_key_event_;
        event.pressed = false;
        event.timestamp = millis();
        postEvent(event);
        handleKeyRelease();
    }
}

void KeyboardInput::updateModifiers() {
    // Check modifier keys using keysState
    auto& keysState = M5Cardputer.Keyboard.keysState();
    
    // Note: This is a simplified version - actual modifier detection
    // would need to check the keysState for shift/fn/ctrl/opt keys
    // For now, we'll use the M5Cardputer.Keyboard state
}

KeyEvent KeyboardInput::translateKey(char key, bool shift, bool fn, bool ctrl, bool opt) {
    KeyEvent event;
    event.character = key;
    event.special = SpecialKey::NONE;
    event.shift = shift;
    event.fn = fn;
    event.ctrl = ctrl;
    event.alt = opt;
    event.pressed = true;
    event.timestamp = millis();
    
    // Handle special keys
    switch (key) {
        case KEY_BACKSPACE:
            event.special = SpecialKey::BACKSPACE;
            break;
        case KEY_TAB:
            event.special = SpecialKey::TAB;
            break;
        case KEY_ENTER:
        case '\r':
        case '\n':
            event.special = SpecialKey::ENTER;
            break;
        case KEY_LEFT_SHIFT:
            event.special = SpecialKey::SHIFT_KEY;
            break;
        case KEY_LEFT_CTRL:
            event.special = SpecialKey::CTRL_KEY;
            break;
        case KEY_LEFT_ALT:
            event.special = SpecialKey::OPT_KEY;
            break;
        case KEY_FN:
            event.special = SpecialKey::FN_KEY;
            break;
        default:
            break;
    }
    
    return event;
}

void KeyboardInput::handleKeyPress(const KeyEvent& event) {
    if (event.isSpecial()) {
        // Handle special keys
        switch (event.special) {
            case SpecialKey::ENTER:
                if (input_buffer_.length > 0) {
                    if (callback_) {
                        callback_->onInputSubmit(input_buffer_.c_str());
                    }
                    input_buffer_.clear();
                    input_changed_ = true;
                }
                break;
                
            case SpecialKey::BACKSPACE:
                if (input_buffer_.backspace()) {
                    input_changed_ = true;
                }
                break;
                
            case SpecialKey::DELETE:
                if (input_buffer_.deleteChar()) {
                    input_changed_ = true;
                }
                break;
                
            default:
                break;
        }
    } else if (event.isPrintable()) {
        // Handle printable characters
        char c = event.character;
        
        // Apply shift transformation
        if (event.shift) {
            c = applyShift(c);
        }
        
        if (input_buffer_.insert(c)) {
            input_changed_ = true;
        }
    }
}

void KeyboardInput::handleKeyRelease() {
    // Key release handling
}

void KeyboardInput::processKeyRepeat() {
    if (!repeat_enabled_ || !key_pressed_) {
        return;
    }
    
    uint32_t now = millis();
    uint32_t repeat_interval = (now - key_press_time_ < repeat_delay_ms_) 
                               ? repeat_delay_ms_ 
                               : repeat_rate_ms_;
    
    if (now - last_repeat_time_ >= repeat_interval) {
        last_repeat_time_ = now;
        
        // Repeat the last key
        KeyEvent event = last_key_event_;
        event.timestamp = now;
        handleKeyPress(event);
    }
}

void KeyboardInput::postEvent(const KeyEvent& event) {
    if (event_queue_) {
        xQueueSend(event_queue_, &event, 0);
    }
}

bool KeyboardInput::readEvent(KeyEvent& event) {
    if (!event_queue_) {
        return false;
    }
    return xQueueReceive(event_queue_, &event, 0) == pdTRUE;
}

void KeyboardInput::clearInput() {
    input_buffer_.clear();
    input_changed_ = true;
}

void KeyboardInput::setInputText(const char* text) {
    input_buffer_.setText(text);
    input_changed_ = true;
}

char KeyboardInput::applyShift(char c) const {
    // Simple shift transformation
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    // Add more shift transformations as needed
    return c;
}

} // namespace OpenClaw
