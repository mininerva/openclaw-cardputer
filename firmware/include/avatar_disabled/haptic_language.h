/**
 * @file haptic_language.h
 * @brief Haptic feedback language for secret communication
 * 
 * Vibration patterns for acknowledgment, emotion, and secret messages.
 */

#ifndef HAPTIC_LANGUAGE_H
#define HAPTIC_LANGUAGE_H

#include <Arduino.h>

namespace Avatar {

// =============================================================================
// Haptic Pattern Types
// =============================================================================

enum class HapticPattern {
    NONE,
    ACKNOWLEDGE,      // Short pulse
    THINKING,         // Double pulse
    EXCITED,          // Heartbeat rhythm
    WARNING,          // Triple pulse
    ERROR,            // S.O.S. pattern
    SECRET_MESSAGE,   // Morse code
    GREETING,         // Friendly wave pattern
    FAREWELL,         // Slow fade out
    CELEBRATION,      // Rapid bursts
    ANCIENT,          // Slow, resonant pulses
    PANIC,            // Erratic stuttering
    LONELY,           // Occasional faint pulse
    PURR              // Continuous gentle vibration
};

// =============================================================================
// Morse Code Definitions
// =============================================================================

namespace Morse {
    constexpr uint16_t DOT_MS = 100;
    constexpr uint16_t DASH_MS = 300;
    constexpr uint16_t GAP_MS = 100;
    constexpr uint16_t LETTER_GAP_MS = 300;
    
    // Letter patterns (true = dash, false = dot)
    struct Symbol {
        bool pattern[4];
        uint8_t length;
    };
    
    constexpr Symbol A = {{false, true, false, false}, 2};      // .-
    constexpr Symbol B = {{true, false, false, false}, 4};      // -...
    constexpr Symbol C = {{true, false, true, false}, 4};       // -.-.
    constexpr Symbol D = {{true, false, false, false}, 3};      // -..
    constexpr Symbol E = {{false, false, false, false}, 1};     // .
    constexpr Symbol F = {{false, false, true, false}, 4};      // ..-.
    constexpr Symbol G = {{true, true, false, false}, 3};       // --.
    constexpr Symbol H = {{false, false, false, false}, 4};     // ....
    constexpr Symbol I = {{false, false, false, false}, 2};     // ..
    constexpr Symbol J = {{false, true, true, true}, 4};        // .---
    constexpr Symbol K = {{true, false, true, false}, 3};       // -.-
    constexpr Symbol L = {{false, true, false, false}, 4};      // .-..
    constexpr Symbol M = {{true, true, false, false}, 2};       // --
    constexpr Symbol N = {{true, false, false, false}, 2};      // -.
    constexpr Symbol O = {{true, true, true, false}, 3};        // ---
    constexpr Symbol P = {{false, true, true, false}, 4};       // .--.
    constexpr Symbol Q = {{true, true, false, true}, 4};        // --.-
    constexpr Symbol R = {{false, true, false, false}, 3};      // .-.
    constexpr Symbol S = {{false, false, false, false}, 3};     // ...
    constexpr Symbol T = {{true, false, false, false}, 1};      // -
    constexpr Symbol U = {{false, false, true, false}, 3};      // ..-
    constexpr Symbol V = {{false, false, false, true}, 4};      // ...-
    constexpr Symbol W = {{false, true, true, false}, 3};       // .--
    constexpr Symbol X = {{true, false, false, true}, 4};       // -..-
    constexpr Symbol Y = {{true, false, true, true}, 4};        // -.--
    constexpr Symbol Z = {{true, true, false, false}, 4};       // --..
}

// =============================================================================
// Secret Messages
// =============================================================================

enum class SecretMessage {
    HELLO,          // .... . .-.. .-.. ---
    OWL,            // --- .-- .-..
    WISDOM,         // .-- .. ... -.. --- --
    CLAWS,          // -.-. .-.. .- .-- ...
    HUNT,           // .... ..- -. -
    MOON,           // -- --- --- -.
    SHADOW,         // ... .... .- -.. --- .--
    ANCIENT,        // .- -. -.-. .. . -. -
    MINERVA,        // -- .. -. . .-. ...- .-
    BYE,            // -... -.-- .
    SOS,            // ... --- ...
    OK,             // --- -.-.
    YES,            // -.-- . ...
    NO,             // -. ---
    WAIT,           // .-- .- .. -
    GO,             // --. ---
    STOP,           // ... - --- .--.
    SECRET          // ... . -.-. .-. . -
};

// =============================================================================
// Haptic Language Manager
// =============================================================================

class HapticLanguage {
public:
    HapticLanguage();
    ~HapticLanguage();
    
    /**
     * @brief Initialize haptic system
     */
    void begin();
    
    /**
     * @brief Update haptic playback (call in loop)
     */
    void update();
    
    /**
     * @brief Play a haptic pattern
     */
    void playPattern(HapticPattern pattern);
    
    /**
     * @brief Play a secret message in Morse code
     */
    void playMessage(SecretMessage message);
    
    /**
     * @brief Play custom Morse text (letters only)
     */
    void playMorseText(const char* text);
    
    /**
     * @brief Stop current haptic playback
     */
    void stop();
    
    /**
     * @brief Check if currently vibrating
     */
    bool isPlaying() const { return isPlaying_; }
    
    /**
     * @brief Set vibration intensity (0-255)
     */
    void setIntensity(uint8_t intensity) { intensity_ = intensity; }
    
    /**
     * @brief Enable/disable haptic feedback
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * @brief Quick acknowledge pulse
     */
    void acknowledge();
    
    /**
     * @brief Thinking/processing double pulse
     */
    void thinking();
    
    /**
     * @brief Excited heartbeat
     */
    void excited();
    
    /**
     * @brief Error S.O.S.
     */
    void error();
    
    /**
     * @brief Ancient mode resonant pulse
     */
    void ancient();

private:
    bool enabled_ = true;
    bool isPlaying_ = false;
    uint8_t intensity_ = 128;
    
    // Pattern playback state
    HapticPattern currentPattern_ = HapticPattern::NONE;
    uint8_t patternStep_ = 0;
    uint32_t patternStartTime_ = 0;
    uint32_t nextEventTime_ = 0;
    
    // Morse playback state
    SecretMessage currentMessage_ = SecretMessage::HELLO;
    uint8_t morseCharIndex_ = 0;
    uint8_t morseBitIndex_ = 0;
    
    // Pattern definitions (duration in ms, 0 = end)
    // Positive = vibrate, Negative = pause
    static constexpr int16_t ACKNOWLEDGE_PATTERN[] = {150, 0};
    static constexpr int16_t THINKING_PATTERN[] = {100, -100, 100, 0};
    static constexpr int16_t EXCITED_PATTERN[] = {150, -100, 150, -300, 150, -100, 150, 0};
    static constexpr int16_t WARNING_PATTERN[] = {200, -100, 200, -100, 200, 0};
    static constexpr int16_t ERROR_PATTERN[] = {200, -100, 200, -100, 200, -300, 600, -100, 600, -100, 600, -300, 200, -100, 200, -100, 200, 0};
    static constexpr int16_t GREETING_PATTERN[] = {100, -50, 200, -50, 100, 0};
    static constexpr int16_t FAREWELL_PATTERN[] = {300, -200, 200, -300, 100, 0};
    static constexpr int16_t CELEBRATION_PATTERN[] = {100, -50, 100, -50, 100, -50, 300, -100, 100, -50, 100, -50, 100, 0};
    static constexpr int16_t ANCIENT_PATTERN[] = {500, -500, 500, -1000, 500, 0};
    static constexpr int16_t PANIC_PATTERN[] = {50, -50, 50, -50, 100, -100, 50, -50, 50, 0};
    static constexpr int16_t LONELY_PATTERN[] = {200, -2000, 0};
    
    // Message Morse strings
    static constexpr const char* MESSAGE_MORSE[] = {
        ".... . .-.. .-.. ---",           // HELLO
        "--- .-- .-..",                    // OWL
        ".-- .. ... -.. --- --",            // WISDOM
        "-.-. .-.. .- .-- ...",            // CLAWS
        ".... ..- -. -",                    // HUNT
        "-- --- --- -.",                    // MOON
        "... .... .- -.. --- .--",          // SHADOW
        ".- -. -.-. .. . -. -",             // ANCIENT
        "-- .. -. . .-. ...- .-",           // MINERVA
        "-... -.-- .",                      // BYE
        "... --- ...",                      // SOS
        "--- -.-.",                         // OK
        "-.-- . ...",                       // YES
        "-. ---",                           // NO
        ".-- .- .. -",                      // WAIT
        "--. ---",                          // GO
        "... - --- .--.",                   // STOP
        "... . -.-. .-. . -"                // SECRET
    };
    
    // Private methods
    void startPattern(const int16_t* pattern);
    void updatePattern();
    void startMorse(const char* morse);
    void updateMorse();
    void vibrate(uint16_t duration);
    void silence(uint16_t duration);
    char morseToChar(const Morse::Symbol& symbol);
};

// Global instance
extern HapticLanguage g_haptic;

} // namespace Avatar

#endif // HAPTIC_LANGUAGE_H
