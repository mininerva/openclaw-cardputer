/**
 * @file keyboard_handler.h
 * @brief Cardputer keyboard input handling for OpenClaw
 * 
 * Features:
 * - Full keyboard matrix scanning
 * - Modifier key support (Shift, Fn, Ctrl, Opt)
 * - Key repeat with configurable delay/rate
 * - Input buffering with cursor support
 * - Event-based architecture
 */

#ifndef OPENCLAW_KEYBOARD_HANDLER_H
#define OPENCLAW_KEYBOARD_HANDLER_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <memory>
#include <functional>
#include <string>

namespace OpenClaw {

// Forward declaration
class KeyboardHandler;

// Keyboard constants
constexpr size_t KEYBOARD_BUFFER_SIZE = 256;
constexpr size_t KEYBOARD_QUEUE_SIZE = 32;
constexpr uint32_t DEFAULT_KEY_REPEAT_DELAY_MS = 400;
constexpr uint32_t DEFAULT_KEY_REPEAT_RATE_MS = 50;
constexpr uint32_t KEYBOARD_POLL_INTERVAL_MS = 10;

// Special key codes
enum class SpecialKey : uint8_t {
    NONE = 0,
    
    // Navigation
    UP = 0x80,
    DOWN,
    LEFT,
    RIGHT,
    HOME,
    END,
    PAGE_UP,
    PAGE_DOWN,
    
    // Editing
    ENTER = '\r',
    BACKSPACE = '\b',
    DELETE = 0x7F,
    TAB = '\t',
    INSERT,
    
    // Modifiers (for event reporting)
    SHIFT_PRESS,
    SHIFT_RELEASE,
    FN_PRESS,
    FN_RELEASE,
    CTRL_PRESS,
    CTRL_RELEASE,
    OPT_PRESS,
    OPT_RELEASE,
    
    // Function keys
    F1,
    F2,
    F3,
    F4,
    F5,
    
    // Special functions
    VOICE_TOGGLE,   // Toggle voice input mode
    SEND,           // Send message
    ESCAPE,
    
    // Ancient mode
    ANCIENT_MODE    // Fn+A - Ancient wisdom mode
};

// Key event structure
struct KeyEvent {
    char character;           // ASCII character (0 for special keys)
    SpecialKey special;       // Special key code
    bool pressed;             // true = pressed, false = released
    bool shift;               // Shift modifier active
    bool ctrl;                // Ctrl modifier active
    bool opt;                 // Opt/Alt modifier active
    bool fn;                  // Fn modifier active
    uint32_t timestamp;       // Event timestamp
    uint8_t repeat_count;     // Repeat counter (0 = first press)
    
    KeyEvent()
        : character(0), special(SpecialKey::NONE), pressed(false),
          shift(false), ctrl(false), opt(false), fn(false),
          timestamp(0), repeat_count(0) {}
    
    bool isPrintable() const {
        return character >= 32 && character < 127;
    }
    
    bool isSpecial() const {
        return special != SpecialKey::NONE;
    }
    
    bool isEnter() const {
        return character == '\r' || character == '\n' || special == SpecialKey::ENTER;
    }
    
    bool isBackspace() const {
        return character == '\b' || special == SpecialKey::BACKSPACE;
    }
    
    bool isDelete() const {
        return special == SpecialKey::DELETE;
    }
    
    bool isNavigation() const {
        return special >= SpecialKey::UP && special <= SpecialKey::PAGE_DOWN;
    }
    
    bool isModifier() const {
        return special >= SpecialKey::SHIFT_PRESS && special <= SpecialKey::OPT_RELEASE;
    }
};

// Input buffer for text entry
class InputBuffer {
public:
    InputBuffer();
    
    // Buffer operations
    bool insert(char c);
    bool backspace();
    bool deleteChar();
    bool insertString(const char* str);
    void clear();
    
    // Cursor movement
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorHome();
    void moveCursorEnd();
    void moveCursorTo(size_t pos);
    
    // Getters
    const char* getText() const { return buffer_; }
    size_t getLength() const { return length_; }
    size_t getCursor() const { return cursor_; }
    bool isEmpty() const { return length_ == 0; }
    bool isFull() const { return length_ >= KEYBOARD_BUFFER_SIZE - 1; }
    
    // Text manipulation
    String getTextBeforeCursor() const;
    String getTextAfterCursor() const;
    bool setText(const char* text);
    
private:
    char buffer_[KEYBOARD_BUFFER_SIZE];
    size_t length_;
    size_t cursor_;
};

// Keyboard event types
enum class KeyboardEvent {
    KEY_PRESSED,
    KEY_RELEASED,
    KEY_REPEATED,
    TEXT_INPUT,
    INPUT_SUBMITTED,
    INPUT_CHANGED,
    MODIFIER_CHANGED,
    SPECIAL_KEY
};

// Keyboard event callback
using KeyboardEventCallback = std::function<void(KeyboardEvent event, const void* data)>;

// Keyboard handler class
class KeyboardHandler {
public:
    KeyboardHandler();
    ~KeyboardHandler();
    
    // Disable copy
    KeyboardHandler(const KeyboardHandler&) = delete;
    KeyboardHandler& operator=(const KeyboardHandler&) = delete;
    
    // Initialize
    bool begin();
    
    // Update (call in main loop)
    void update();
    
    // Deinitialize
    void end();
    
    // Read key event (non-blocking)
    bool readEvent(KeyEvent& event);
    
    // Get current input buffer
    const InputBuffer& getInputBuffer() const { return input_buffer_; }
    InputBuffer& getInputBuffer() { return input_buffer_; }
    
    // Buffer operations
    void clearInput();
    void submitInput();
    bool setInputText(const char* text);
    
    // Set event callback
    void onEvent(KeyboardEventCallback callback);
    
    // Key repeat configuration
    void setKeyRepeatEnabled(bool enabled) { key_repeat_enabled_ = enabled; }
    bool isKeyRepeatEnabled() const { return key_repeat_enabled_; }
    
    void setKeyRepeatDelay(uint32_t delay_ms) { key_repeat_delay_ms_ = delay_ms; }
    uint32_t getKeyRepeatDelay() const { return key_repeat_delay_ms_; }
    
    void setKeyRepeatRate(uint32_t rate_ms) { key_repeat_rate_ms_ = rate_ms; }
    uint32_t getKeyRepeatRate() const { return key_repeat_rate_ms_; }
    
    // Modifier state
    bool isShiftPressed() const { return shift_pressed_; }
    bool isFnPressed() const { return fn_pressed_; }
    bool isCtrlPressed() const { return ctrl_pressed_; }
    bool isOptPressed() const { return opt_pressed_; }
    
    // Key state
    bool isKeyPressed() const { return key_pressed_; }
    char getLastCharacter() const { return last_character_; }
    
    // Statistics
    uint32_t getKeyPressCount() const { return key_press_count_; }
    uint32_t getInputSubmitCount() const { return input_submit_count_; }

private:
    // State
    bool initialized_;
    KeyboardEventCallback event_callback_;
    
    // Input buffer
    InputBuffer input_buffer_;
    
    // Key state
    bool key_pressed_;
    char last_character_;
    uint32_t key_press_time_;
    uint32_t last_repeat_time_;
    uint8_t repeat_count_;
    KeyEvent last_key_event_;
    
    // Modifier state
    bool shift_pressed_;
    bool fn_pressed_;
    bool ctrl_pressed_;
    bool opt_pressed_;
    bool last_shift_state_;
    bool last_fn_state_;
    bool last_ctrl_state_;
    bool last_opt_state_;
    
    // Key repeat
    bool key_repeat_enabled_;
    uint32_t key_repeat_delay_ms_;
    uint32_t key_repeat_rate_ms_;
    
    // Event queue
    QueueHandle_t event_queue_;
    SemaphoreHandle_t state_mutex_;
    
    // Timing
    uint32_t last_poll_time_;
    
    // Statistics
    uint32_t key_press_count_;
    uint32_t input_submit_count_;
    
    // Private methods
    bool createQueue();
    void destroyQueue();
    bool createMutexes();
    void destroyMutexes();
    
    void pollKeyboard();
    void processKeyState();
    void handleKeyPress(char key, bool shift, bool fn, bool ctrl, bool opt);
    void handleKeyRelease();
    void processKeyRepeat();
    void updateModifiers();
    
    KeyEvent translateKey(char key, bool shift, bool fn, bool ctrl, bool opt);
    SpecialKey getSpecialKey(char key, bool shift, bool fn, bool ctrl, bool opt);
    void postEvent(const KeyEvent& event);
    void notifyEvent(KeyboardEvent event, const void* data);
    
    // Character translation
    char applyShift(char c) const;
    char applyFn(char c) const;
    char applyCtrl(char c) const;
};

// Utility functions
const char* specialKeyToString(SpecialKey key);
const char* keyboardEventToString(KeyboardEvent event);
bool isAncientModeTrigger(const char* text);

} // namespace OpenClaw

#endif // OPENCLAW_KEYBOARD_HANDLER_H
