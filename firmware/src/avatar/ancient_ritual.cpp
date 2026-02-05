/**
 * @file ancient_ritual.cpp
 * @brief Ancient Mode activation ritual implementation
 */

#include "avatar/ancient_ritual.h"
#include <cstring>

namespace Avatar {

// Global instance
AncientRitual g_ancientRitual;

AncientRitual::AncientRitual() 
    : ritualState_(RitualState::INACTIVE),
      lastTrigger_(AncientTrigger::NONE) {
}

AncientRitual::~AncientRitual() {
}

void AncientRitual::begin() {
    reset();
    lastCheckedHour_ = 255;
    lastBatteryLevel_ = 100;
}

void AncientRitual::update() {
    if (ritualState_ == RitualState::RITUAL_COMPLETE || 
        ritualState_ == RitualState::QUEST_REQUIRED) {
        return;
    }
    
    // Update gesture detection
    updateGestureDetection();
    
    // Check time trigger (3:33 AM)
    if (ritualState_ == RitualState::INACTIVE) {
        if (checkTimeTrigger()) {
            ritualState_ = RitualState::TIME_WAITING;
        }
    }
    
    // Check battery trigger (below 5%)
    if (ritualState_ == RitualState::INACTIVE) {
        if (checkBatteryTrigger()) {
            ritualState_ = RitualState::BATTERY_CRITICAL;
        }
    }
}

AncientTrigger AncientRitual::checkActivation() {
    switch (ritualState_) {
        case RitualState::GESTURE_CONFIRMED:
            completeRitual(AncientTrigger::GESTURE);
            return AncientTrigger::GESTURE;
            
        case RitualState::PHRASE_CONFIRMED:
            completeRitual(AncientTrigger::PHRASE);
            return AncientTrigger::PHRASE;
            
        case RitualState::SEQUENCE_CONFIRMED:
            completeRitual(AncientTrigger::SEQUENCE);
            return AncientTrigger::SEQUENCE;
            
        case RitualState::TIME_WAITING:
            completeRitual(AncientTrigger::TIME);
            return AncientTrigger::TIME;
            
        case RitualState::BATTERY_CRITICAL:
            completeRitual(AncientTrigger::BATTERY);
            return AncientTrigger::BATTERY;
            
        default:
            return AncientTrigger::NONE;
    }
}

float AncientRitual::getRitualProgress() const {
    switch (ritualState_) {
        case RitualState::INACTIVE:
            return 0.0f;
        case RitualState::GESTURE_DETECTING:
            return 0.2f;
        case RitualState::GESTURE_CONFIRMED:
            return 0.4f;
        case RitualState::PHRASE_LISTENING:
            return 0.3f;
        case RitualState::PHRASE_CONFIRMED:
            return 0.5f;
        case RitualState::SEQUENCE_INPUT:
            return 0.25f + (konamiIndex_ / (float)KONAMI_LENGTH) * 0.5f;
        case RitualState::SEQUENCE_CONFIRMED:
            return 0.6f;
        case RitualState::TIME_WAITING:
        case RitualState::BATTERY_CRITICAL:
            return 0.5f;
        case RitualState::RITUAL_COMPLETE:
        case RitualState::QUEST_REQUIRED:
            return 1.0f;
        default:
            return 0.0f;
    }
}

bool AncientRitual::checkVoicePhrase(const char* text) {
    if (!text) return false;
    
    String t = String(text);
    t.toLowerCase();
    
    for (uint8_t i = 0; i < NUM_AWAKEN_PHRASES; i++) {
        if (t.indexOf(AWAKEN_PHRASES[i]) >= 0) {
            ritualState_ = RitualState::PHRASE_CONFIRMED;
            return true;
        }
    }
    
    return false;
}

bool AncientRitual::processKonamiKey(char key) {
    uint32_t now = millis();
    
    // Reset if timeout
    if (now - lastKonamiTime_ > KONAMI_TIMEOUT_MS) {
        konamiIndex_ = 0;
    }
    
    lastKonamiTime_ = now;
    
    // Check if key matches sequence
    if (key == KONAMI_SEQUENCE[konamiIndex_]) {
        konamiIndex_++;
        ritualState_ = RitualState::SEQUENCE_INPUT;
        
        if (konamiIndex_ >= KONAMI_LENGTH) {
            ritualState_ = RitualState::SEQUENCE_CONFIRMED;
            konamiIndex_ = 0;
            return true;
        }
    } else {
        // Wrong key - reset
        konamiIndex_ = 0;
        if (key == KONAMI_SEQUENCE[0]) {
            konamiIndex_ = 1;
        }
    }
    
    return false;
}

void AncientRitual::reset() {
    ritualState_ = RitualState::INACTIVE;
    lastTrigger_ = AncientTrigger::NONE;
    konamiIndex_ = 0;
    questCompleted_ = false;
    questRequired_ = false;
}

void AncientRitual::completeRitual(AncientTrigger trigger) {
    lastTrigger_ = trigger;
    ritualState_ = RitualState::RITUAL_COMPLETE;
    
    // Require quest for certain triggers
    if (trigger == AncientTrigger::GESTURE || 
        trigger == AncientTrigger::SEQUENCE) {
        requireQuest();
    }
}

const char* AncientRitual::getTriggerName(AncientTrigger trigger) {
    switch (trigger) {
        case AncientTrigger::NONE: return "None";
        case AncientTrigger::GESTURE: return "Gesture";
        case AncientTrigger::PHRASE: return "Voice Phrase";
        case AncientTrigger::SEQUENCE: return "Konami Code";
        case AncientTrigger::TIME: return "3:33 AM";
        case AncientTrigger::BATTERY: return "Low Battery";
        case AncientTrigger::MANUAL: return "Manual";
        default: return "Unknown";
    }
}

bool AncientRitual::detectGesture() {
    // Check if screen is tilted down (using IMU if available)
    // For now, check if both side buttons are held
    // This would need actual IMU integration for the 23° tilt
    
    // Simulated with button check
    if (gestureButtonLeft_ && gestureButtonRight_) {
        uint32_t heldTime = millis() - gestureStartTime_;
        if (heldTime > 3000) {  // Hold for 3 seconds
            return true;
        }
    }
    
    return false;
}

void AncientRitual::updateGestureDetection() {
    // This would integrate with actual button/IMU state
    // For now, it's a placeholder for the gesture detection logic
    
    if (ritualState_ == RitualState::INACTIVE) {
        if (gestureButtonLeft_ || gestureButtonRight_) {
            if (ritualState_ != RitualState::GESTURE_DETECTING) {
                ritualState_ = RitualState::GESTURE_DETECTING;
                gestureStartTime_ = millis();
            }
        }
    }
    
    if (ritualState_ == RitualState::GESTURE_DETECTING) {
        if (detectGesture()) {
            ritualState_ = RitualState::GESTURE_CONFIRMED;
        } else if (!gestureButtonLeft_ && !gestureButtonRight_) {
            // Buttons released too early
            ritualState_ = RitualState::INACTIVE;
        }
    }
}

bool AncientRitual::checkTimeTrigger() {
    // Get current time
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (timeinfo) {
        // Check for 3:33 AM
        if (timeinfo->tm_hour == 3 && timeinfo->tm_min == 33) {
            // Only trigger once per day
            if (lastCheckedHour_ != 3 || lastCheckedMinute_ != 33) {
                lastCheckedHour_ = 3;
                lastCheckedMinute_ = 33;
                return true;
            }
        }
        lastCheckedHour_ = timeinfo->tm_hour;
        lastCheckedMinute_ = timeinfo->tm_min;
    }
    
    return false;
}

bool AncientRitual::checkBatteryTrigger() {
    // Get battery level (would integrate with power management)
    // For now, placeholder
    uint8_t batteryLevel = 100;  // Would get actual level
    
    if (batteryLevel < 5 && lastBatteryLevel_ >= 5) {
        lastBatteryLevel_ = batteryLevel;
        return true;
    }
    
    lastBatteryLevel_ = batteryLevel;
    return false;
}

// =============================================================================
// Ancient Dialect Implementation
// =============================================================================

String AncientDialect::toAncientSpeak(const String& text) {
    String result = text;
    
    // Replace modern words with archaic equivalents
    result.replace("you", "thou");
    result.replace("You", "Thou");
    result.replace("your", "thy");
    result.replace("Your", "Thy");
    result.replace("yours", "thine");
    result.replace("are", "art");
    result.replace("is", "be");
    result.replace("was", "wert");
    result.replace("have", "hast");
    result.replace("has", "hath");
    result.replace("do", "dost");
    result.replace("does", "doth");
    result.replace("the", "ye");
    result.replace("hello", "hail");
    result.replace("goodbye", "fare thee well");
    result.replace("yes", "aye");
    result.replace("no", "nay");
    result.replace("my", "mine");
    result.replace("i am", "i be");
    
    return result;
}

String AncientDialect::getGreeting() {
    uint8_t index = random(0, 8);
    return String(GREETINGS[index]);
}

String AncientDialect::getFarewell() {
    uint8_t index = random(0, 8);
    return String(FAREWELLS[index]);
}

String AncientDialect::addFlourish(const String& text) {
    String result = text;
    
    // Add random flourish at end
    uint8_t index = random(0, 6);
    result += " ";
    result += FLOURISHES[index];
    
    return result;
}

} // namespace Avatar
