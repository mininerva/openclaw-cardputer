/**
 * @file haptic_language.cpp
 * @brief Haptic feedback language implementation
 */

#include "avatar/haptic_language.h"
#include <M5Cardputer.h>

namespace Avatar {

// Global instance
HapticLanguage g_haptic;

HapticLanguage::HapticLanguage() 
    : enabled_(true),
      isPlaying_(false),
      intensity_(128),
      currentPattern_(HapticPattern::NONE) {
}

HapticLanguage::~HapticLanguage() {
}

void HapticLanguage::begin() {
    // M5Cardputer doesn't have built-in vibration motor
    // This would integrate with external haptic hardware
    // For now, placeholder implementation
    enabled_ = true;
}

void HapticLanguage::update() {
    if (!enabled_ || !isPlaying_) return;
    
    if (currentPattern_ == HapticPattern::SECRET_MESSAGE) {
        updateMorse();
    } else {
        updatePattern();
    }
}

void HapticLanguage::playPattern(HapticPattern pattern) {
    if (!enabled_) return;
    
    currentPattern_ = pattern;
    patternStep_ = 0;
    patternStartTime_ = millis();
    isPlaying_ = true;
    
    switch (pattern) {
        case HapticPattern::ACKNOWLEDGE:
            startPattern(ACKNOWLEDGE_PATTERN);
            break;
        case HapticPattern::THINKING:
            startPattern(THINKING_PATTERN);
            break;
        case HapticPattern::EXCITED:
            startPattern(EXCITED_PATTERN);
            break;
        case HapticPattern::WARNING:
            startPattern(WARNING_PATTERN);
            break;
        case HapticPattern::ERROR:
            startPattern(ERROR_PATTERN);
            break;
        case HapticPattern::GREETING:
            startPattern(GREETING_PATTERN);
            break;
        case HapticPattern::FAREWELL:
            startPattern(FAREWELL_PATTERN);
            break;
        case HapticPattern::CELEBRATION:
            startPattern(CELEBRATION_PATTERN);
            break;
        case HapticPattern::ANCIENT:
            startPattern(ANCIENT_PATTERN);
            break;
        case HapticPattern::PANIC:
            startPattern(PANIC_PATTERN);
            break;
        case HapticPattern::LONELY:
            startPattern(LONELY_PATTERN);
            break;
        default:
            isPlaying_ = false;
            break;
    }
}

void HapticLanguage::playMessage(SecretMessage message) {
    if (!enabled_) return;
    
    currentMessage_ = message;
    currentPattern_ = HapticPattern::SECRET_MESSAGE;
    morseCharIndex_ = 0;
    morseBitIndex_ = 0;
    isPlaying_ = true;
    
    const char* morse = MESSAGE_MORSE[(size_t)message];
    startMorse(morse);
}

void HapticLanguage::playMorseText(const char* text) {
    if (!enabled_ || !text) return;
    
    // Convert text to morse and play
    // Simplified: just play the first few characters
    currentPattern_ = HapticPattern::SECRET_MESSAGE;
    isPlaying_ = true;
    
    // Would convert text to morse string here
    startMorse(".... . .-.. .-.. ---");  // Default to "HELLO"
}

void HapticLanguage::stop() {
    isPlaying_ = false;
    currentPattern_ = HapticPattern::NONE;
    
    // Stop vibration
    // M5Cardputer.Power.setVibration(0);  // If available
}

void HapticLanguage::acknowledge() {
    playPattern(HapticPattern::ACKNOWLEDGE);
}

void HapticLanguage::thinking() {
    playPattern(HapticPattern::THINKING);
}

void HapticLanguage::excited() {
    playPattern(HapticPattern::EXCITED);
}

void HapticLanguage::error() {
    playPattern(HapticPattern::ERROR);
}

void HapticLanguage::ancient() {
    playPattern(HapticPattern::ANCIENT);
}

// =============================================================================
// Private Methods
// =============================================================================

void HapticLanguage::startPattern(const int16_t* pattern) {
    patternStep_ = 0;
    nextEventTime_ = millis();
    
    // First event
    int16_t duration = pattern[0];
    if (duration > 0) {
        vibrate(duration);
    } else if (duration < 0) {
        silence(-duration);
    }
}

void HapticLanguage::updatePattern() {
    if (millis() < nextEventTime_) return;
    
    patternStep_++;
    
    // Get next pattern value based on current pattern
    const int16_t* pattern = nullptr;
    
    switch (currentPattern_) {
        case HapticPattern::ACKNOWLEDGE: pattern = ACKNOWLEDGE_PATTERN; break;
        case HapticPattern::THINKING: pattern = THINKING_PATTERN; break;
        case HapticPattern::EXCITED: pattern = EXCITED_PATTERN; break;
        case HapticPattern::WARNING: pattern = WARNING_PATTERN; break;
        case HapticPattern::ERROR: pattern = ERROR_PATTERN; break;
        case HapticPattern::GREETING: pattern = GREETING_PATTERN; break;
        case HapticPattern::FAREWELL: pattern = FAREWELL_PATTERN; break;
        case HapticPattern::CELEBRATION: pattern = CELEBRATION_PATTERN; break;
        case HapticPattern::ANCIENT: pattern = ANCIENT_PATTERN; break;
        case HapticPattern::PANIC: pattern = PANIC_PATTERN; break;
        case HapticPattern::LONELY: pattern = LONELY_PATTERN; break;
        default: break;
    }
    
    if (!pattern) {
        isPlaying_ = false;
        return;
    }
    
    int16_t duration = pattern[patternStep_];
    
    if (duration == 0) {
        // End of pattern
        isPlaying_ = false;
        return;
    }
    
    if (duration > 0) {
        vibrate(duration);
    } else {
        silence(-duration);
    }
}

void HapticLanguage::startMorse(const char* morse) {
    morseCharIndex_ = 0;
    morseBitIndex_ = 0;
    nextEventTime_ = millis();
    
    // Would parse morse string and start playback
    // For now, placeholder
}

void HapticLanguage::updateMorse() {
    // Would update morse playback state
    // For now, placeholder
    
    // End after simulated duration
    if (millis() - patternStartTime_ > 5000) {
        isPlaying_ = false;
    }
}

void HapticLanguage::vibrate(uint16_t duration) {
    // Set vibration motor
    // M5Cardputer.Power.setVibration(intensity_);  // If available
    
    nextEventTime_ = millis() + duration;
}

void HapticLanguage::silence(uint16_t duration) {
    // Stop vibration
    // M5Cardputer.Power.setVibration(0);  // If available
    
    nextEventTime_ = millis() + duration;
}

} // namespace Avatar
