/**
 * @file keyboard_input.cpp
 * @brief Keyboard input implementation
 */

#include "keyboard_input.h"

namespace OpenClaw {

KeyboardInput::KeyboardInput() {
    memset(last_error_, 0, sizeof(last_error_));
}

KeyboardInput::~KeyboardInput() {
    end();
}

bool KeyboardInput::begin() {
    if (initialized_) {
        return true;
    }
    
    if (!createQueue()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

void KeyboardInput::update() {
    if (!initialized_) return;
    
    uint32_t now = millis();
    
    // Poll at fixed interval
    if (now - last_poll_time_ < KEYBOARD_POLL_INTERVAL_MS) {
        return;
    }
    last_poll_time_ = now;
    
    pollKeyboard();
    
    if (repeat_enabled_ && key_pressed_) {
        processRepeat();
    }
}

void KeyboardInput::end() {
    if (!initialized_) return;
    
    destroyQueue();
    initialized_ = false;
}

void KeyboardInput::setCallback(KeyboardCallback* callback) {
    callback_ = callback;
}

bool KeyboardInput::readEvent(KeyEvent& event) {
    if (!event_queue_) return false;
    return xQueueReceive(event_queue_, &event, 0) == pdTRUE;
}

void KeyboardInput::clearInput() {
    input_buffer_.clear();
    input_changed_ = true;
    if (callback_) {
        callback_>onInputChanged(input_buffer_);
    }
}

void KeyboardInput::setInputText(const char* text) {
    input_buffer_.clear();
    size_t len = strlen(text);
    for (size_t i = 0; i < len && i < KEYBOARD_BUFFER_SIZE - 1; i++) {
        input_buffer_.insert(text[i]);
    }
    input_changed_ = true;
    if (callback_) {
        callback_>onInputChanged(input_buffer_);
    }
}

bool KeyboardInput::createQueue() {
    event_queue_ = xQueueCreate(KEYBOARD_QUEUE_LENGTH, sizeof(KeyEvent));
    if (!event_queue_) {
        strncpy(last_error_, "Failed to create event queue", sizeof(last_error_) - 1);
        return false;
    }
    return true;
}

void KeyboardInput::destroyQueue() {
    if (event_queue_) {
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
    }
}

void KeyboardInput::pollKeyboard() {
    // Update modifier states
    updateModifiers();
    
    // Check if any key is pressed
    if (M5Cardputer.Keyboard.isPressed()) {
        // Get the key status
        auto status = M5Cardputer.Keyboard.getKeyStatus();
        
        if (status.size() > 0) {
            char key = status[0].first;
            bool pressed = status[0].second;
            
            if (pressed && !key_pressed_) {
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
                
            } else if (!pressed && key_pressed_) {
                // Key release
                handleKeyRelease();
            }
            
            key_pressed_ = pressed;
        }
    } else if (key_pressed_) {
        handleKeyRelease();
        key_pressed_ = false;
    }
}

void KeyboardInput::handleKeyPress(const KeyEvent& event) {
    if (event.isSpecial()) {
        // Handle special keys
        switch (event.special) {
            case SpecialKey::ENTER:
            case SpecialKey::SEND_KEY:
                if (input_buffer_.length > 0) {
                    if (callback_) {
                        callback_>onInputSubmit(input_buffer_.c_str());
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
                
            case SpecialKey::LEFT:
                input_buffer_.moveCursorLeft();
                input_changed_ = true;
                break;
                
            case SpecialKey::RIGHT:
                input_buffer_.moveCursorRight();
                input_changed_ = true;
                break;
                
            case SpecialKey::HOME:
                input_buffer_.moveCursorHome();
                input_changed_ = true;
                break;
                
            case SpecialKey::END:
                input_buffer_.moveCursorEnd();
                input_changed_ = true;
                break;
                
            case SpecialKey::ESCAPE:
                input_buffer_.clear();
                input_changed_ = true;
                break;
                
            case SpecialKey::VOICE_KEY:
                // Voice key handled separately
                break;
                
            default:
                break;
        }
    } else if (event.isPrintable()) {
        // Handle printable character
        if (input_buffer_.insert(event.character)) {
            input_changed_ = true;
        }
    }
    
    if (input_changed_ && callback_) {
        callback_>onInputChanged(input_buffer_);
    }
}

void KeyboardInput::handleKeyRelease() {
    // Key release handling if needed
}

void KeyboardInput::processRepeat() {
    uint32_t now = millis();
    uint32_t elapsed = now - key_press_time_;
    
    if (elapsed > repeat_delay_ms_) {
        uint32_t repeat_elapsed = now - last_repeat_time_;
        
        if (repeat_elapsed >= repeat_rate_ms_) {
            last_repeat_time_ = now;
            
            // Repeat the last key event
            KeyEvent repeat_event = last_key_event_;
            repeat_event.timestamp = now;
            
            handleKeyPress(repeat_event);
            postEvent(repeat_event);
        }
    }
}

KeyEvent KeyboardInput::translateKey(char key, bool shift, bool fn, bool ctrl, bool opt) {
    KeyEvent event;
    event.shift = shift;
    event.fn = fn;
    event.ctrl = ctrl;
    event.alt = opt;
    
    // Handle special keys
    if (key == 0x0D || key == '\r') {
        event.special = SpecialKey::ENTER;
        event.character = '\r';
    } else if (key == 0x08 || key == '\b') {
        event.special = SpecialKey::BACKSPACE;
        event.character = '\b';
    } else if (key == 0x1B) {
        event.special = SpecialKey::ESCAPE;
        event.character = 0x1B;
    } else if (key == 0x09 || key == '\t') {
        event.special = SpecialKey::TAB;
        event.character = '\t';
    } else if (fn) {
        // Fn combinations
        switch (key) {
            case '1': event.special = SpecialKey::FUNCTION_1; break;
            case '2': event.special = SpecialKey::FUNCTION_2; break;
            case '3': event.special = SpecialKey::FUNCTION_3; break;
            case '4': event.special = SpecialKey::FUNCTION_4; break;
            case '5': event.special = SpecialKey::FUNCTION_5; break;
            case 'v':
            case 'V': event.special = SpecialKey::VOICE_KEY; break;
            case 's':
            case 'S': event.special = SpecialKey::SEND_KEY; break;
            default: event.character = key; break;
        }
    } else if (ctrl) {
        // Ctrl combinations
        switch (key) {
            case 'a': 
                event.special = SpecialKey::HOME; 
                break;
            case 'e': 
                event.special = SpecialKey::END; 
                break;
            case 'c':
                // Ctrl+C - copy (not implemented)
                break;
            case 'v':
                // Ctrl+V - paste (not implemented)
                break;
            default: event.character = key; break;
        }
    } else {
        // Regular key
        event.character = key;
    }
    
    return event;
}

void KeyboardInput::postEvent(const KeyEvent& event) {
    if (event_queue_) {
        xQueueSend(event_queue_, &event, 0);
    }
    
    if (callback_) {
        callback_>onKeyEvent(event);
    }
}

void KeyboardInput::updateModifiers() {
    // Check modifier keys using M5Cardputer API
    // Note: This is a simplified implementation
    // The actual implementation would check the keyboard matrix directly
    
    // For now, we'll track modifiers based on key combinations
    // This would need to be enhanced based on actual Cardputer keyboard matrix
}

} // namespace OpenClaw
