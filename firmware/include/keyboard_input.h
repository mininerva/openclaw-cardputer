/**
 * @file keyboard_input.h
 * @brief Cardputer keyboard input handling for OpenClaw
 * 
 * Manages the Cardputer's built-in keyboard with support for
 * text input, special keys, and input buffering.
 */

#ifndef KEYBOARD_INPUT_H
#define KEYBOARD_INPUT_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace OpenClaw {

// Keyboard constants
constexpr size_t KEYBOARD_BUFFER_SIZE = 256;
constexpr size_t KEYBOARD_QUEUE_LENGTH = 16;
constexpr uint32_t KEY_REPEAT_DELAY_MS = 500;
constexpr uint32_t KEY_REPEAT_RATE_MS = 50;
constexpr uint32_t KEYBOARD_POLL_INTERVAL_MS = 10;

/**
 * @brief Special key codes
 */
enum class SpecialKey : uint8_t {
    NONE = 0,
    ENTER = '\r',
    BACKSPACE = '\b',
    ESCAPE = 0x1B,
    TAB = '\t',
    UP = 0x80,
    DOWN,
    LEFT,
    RIGHT,
    HOME,
    END,
    PAGE_UP,
    PAGE_DOWN,
    DELETE,
    INSERT,
    FUNCTION_1,  // Fn+1
    FUNCTION_2,
    FUNCTION_3,
    FUNCTION_4,
    FUNCTION_5,
    VOICE_KEY,   // Special voice input key
    SEND_KEY     // Send message key
};

/**
 * @brief Keyboard event structure
 */
struct KeyEvent {
    char character;           // ASCII character (0 for special keys)
    SpecialKey special;       // Special key code
    bool pressed;             // true = pressed, false = released
    bool shift;               // Shift modifier
    bool ctrl;                // Ctrl modifier
    bool alt;                 // Alt/Opt modifier
    bool fn;                  // Fn modifier
    uint32_t timestamp;
    
    KeyEvent() : character(0), special(SpecialKey::NONE), 
                 pressed(false), shift(false), ctrl(false), 
                 alt(false), fn(false), timestamp(0) {}
    
    bool isPrintable() const { return character >= 32 && character < 127; }
    bool isSpecial() const { return special != SpecialKey::NONE; }
    bool isEnter() const { return character == '\r' || special == SpecialKey::ENTER; }
    bool isBackspace() const { return character == '\b' || special == SpecialKey::BACKSPACE; }
};

/**
 * @brief Input line buffer for text entry
 */
struct InputBuffer {
    char buffer[KEYBOARD_BUFFER_SIZE];
    size_t length;
    size_t cursor;
    
    InputBuffer() : length(0), cursor(0) {
        buffer[0] = '\0';
    }
    
    void clear() {
        length = 0;
        cursor = 0;
        buffer[0] = '\0';
    }
    
    bool insert(char c) {
        if (length >= KEYBOARD_BUFFER_SIZE - 1) return false;
        
        // Shift characters after cursor
        for (size_t i = length; i > cursor; --i) {
            buffer[i] = buffer[i - 1];
        }
        
        buffer[cursor] = c;
        cursor++;
        length++;
        buffer[length] = '\0';
        return true;
    }
    
    bool backspace() {
        if (cursor == 0 || length == 0) return false;
        
        // Shift characters before cursor
        for (size_t i = cursor - 1; i < length; ++i) {
            buffer[i] = buffer[i + 1];
        }
        
        cursor--;
        length--;
        buffer[length] = '\0';
        return true;
    }
    
    bool deleteChar() {
        if (cursor >= length) return false;
        
        for (size_t i = cursor; i < length; ++i) {
            buffer[i] = buffer[i + 1];
        }
        
        length--;
        buffer[length] = '\0';
        return true;
    }
    
    void moveCursorLeft() {
        if (cursor > 0) cursor--;
    }
    
    void moveCursorRight() {
        if (cursor < length) cursor++;
    }
    
    void moveCursorHome() {
        cursor = 0;
    }
    
    void moveCursorEnd() {
        cursor = length;
    }
    
    const char* c_str() const { return buffer; }
};

/**
 * @brief Keyboard callback interface
 */
class KeyboardCallback {
public:
    virtual ~KeyboardCallback() = default;
    
    /**
     * @brief Called on key event
     * @param event Key event data
     */
    virtual void onKeyEvent(const KeyEvent& event) = 0;
    
    /**
     * @brief Called when input line is submitted
     * @param text Complete input text
     */
    virtual void onInputSubmit(const char* text) {}
    
    /**
     * @brief Called when input buffer changes
     * @param buffer Current buffer state
     */
    virtual void onInputChanged(const InputBuffer& buffer) {}
};

/**
 * @brief Keyboard input manager class
 * 
 * Handles Cardputer keyboard scanning, key debouncing,
 * repeat handling, and input buffering.
 */
class KeyboardInput {
public:
    KeyboardInput();
    ~KeyboardInput();
    
    // Disable copy
    KeyboardInput(const KeyboardInput&) = delete;
    KeyboardInput& operator=(const KeyboardInput&) = delete;
    
    /**
     * @brief Initialize keyboard input
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Update keyboard state (call in loop)
     */
    void update();
    
    /**
     * @brief Deinitialize keyboard input
     */
    void end();
    
    /**
     * @brief Set callback for keyboard events
     */
    void setCallback(KeyboardCallback* callback);
    
    /**
     * @brief Read key event from queue (non-blocking)
     * @param event Output event
     * @return true if event available
     */
    bool readEvent(KeyEvent& event);
    
    /**
     * @brief Get current input buffer
     */
    const InputBuffer& getInputBuffer() const { return input_buffer_; }
    
    /**
     * @brief Get mutable input buffer
     */
    InputBuffer& getMutableInputBuffer() { return input_buffer_; }
    
    /**
     * @brief Clear input buffer
     */
    void clearInput();
    
    /**
     * @brief Set input buffer text
     */
    void setInputText(const char* text);
    
    /**
     * @brief Check if a key is currently pressed
     */
    bool isKeyPressed() const { return key_pressed_; }
    
    /**
     * @brief Check if shift is held
     */
    bool isShiftPressed() const { return shift_pressed_; }
    
    /**
     * @brief Check if Fn is held
     */
    bool isFnPressed() const { return fn_pressed_; }
    
    /**
     * @brief Check if Ctrl is held
     */
    bool isCtrlPressed() const { return ctrl_pressed_; }
    
    /**
     * @brief Check if Opt/Alt is held
     */
    bool isOptPressed() const { return opt_pressed_; }
    
    /**
     * @brief Set key repeat delay
     */
    void setRepeatDelay(uint32_t delay_ms) { repeat_delay_ms_ = delay_ms; }
    
    /**
     * @brief Set key repeat rate
     */
    void setRepeatRate(uint32_t rate_ms) { repeat_rate_ms_ = rate_ms; }
    
    /**
     * @brief Enable/disable key repeat
     */
    void setRepeatEnabled(bool enabled) { repeat_enabled_ = enabled; }
    
    /**
     * @brief Get the last error message
     */
    const char* getLastError() const { return last_error_; }

private:
    // State
    bool initialized_ = false;
    KeyboardCallback* callback_ = nullptr;
    InputBuffer input_buffer_;
    
    // Key state
    bool key_pressed_ = false;
    bool shift_pressed_ = false;
    bool fn_pressed_ = false;
    bool ctrl_pressed_ = false;
    bool opt_pressed_ = false;
    
    // Repeat handling
    bool repeat_enabled_ = true;
    uint32_t repeat_delay_ms_ = KEY_REPEAT_DELAY_MS;
    uint32_t repeat_rate_ms_ = KEY_REPEAT_RATE_MS;
    uint32_t key_press_time_ = 0;
    uint32_t last_repeat_time_ = 0;
    KeyEvent last_key_event_;
    
    // Queue for async event handling
    QueueHandle_t event_queue_ = nullptr;
    
    // Timing
    uint32_t last_poll_time_ = 0;
    
    // Error
    char last_error_[64];
    
    // Private methods
    bool createQueue();
    void destroyQueue();
    void pollKeyboard();
    void handleKeyPress(const KeyEvent& event);
    void handleKeyRelease();
    void processRepeat();
    KeyEvent translateKey(char key, bool shift, bool fn, bool ctrl, bool opt);
    void postEvent(const KeyEvent& event);
    void updateModifiers();
};

} // namespace OpenClaw

#endif // KEYBOARD_INPUT_H
