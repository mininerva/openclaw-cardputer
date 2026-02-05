/**
 * @file ancient_ritual.h
 * @brief Ancient Mode activation ritual system
 * 
 * Multiple trigger methods for entering the mystical Ancient Mode,
 * including gestures, voice phrases, konami code, and environmental triggers.
 */

#ifndef ANCIENT_RITUAL_H
#define ANCIENT_RITUAL_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include "keyboard_input.h"

namespace Avatar {

// Import SpecialKey from OpenClaw namespace
using OpenClaw::SpecialKey;

// =============================================================================
// Ancient Trigger Types
// =============================================================================

enum class AncientTrigger {
    NONE,
    GESTURE,      // Screen down, tilted 23°, hold both side buttons
    PHRASE,       // Voice: "Minerva, awaken"
    SEQUENCE,     // Key combo: ↑↑↓↓←→←→BA (konami code)
    TIME,         // 3:33 AM local time (auto-trigger)
    BATTERY,      // Below 5% (desperation mode)
    MANUAL        // Fn+A or API call
};

// =============================================================================
// Ritual State Machine
// =============================================================================

enum class RitualState {
    INACTIVE,
    GESTURE_DETECTING,
    GESTURE_CONFIRMED,
    PHRASE_LISTENING,
    PHRASE_CONFIRMED,
    SEQUENCE_INPUT,
    SEQUENCE_CONFIRMED,
    TIME_WAITING,
    BATTERY_CRITICAL,
    RITUAL_COMPLETE,
    QUEST_REQUIRED  // Cannot exit without completing quest
};

// =============================================================================
// Konami Code Sequence
// =============================================================================

constexpr uint8_t KONAMI_LENGTH = 10;
constexpr SpecialKey KONAMI_SEQUENCE[KONAMI_LENGTH] = {
    SpecialKey::UP,
    SpecialKey::UP,
    SpecialKey::DOWN,
    SpecialKey::DOWN,
    SpecialKey::LEFT,
    SpecialKey::RIGHT,
    SpecialKey::LEFT,
    SpecialKey::RIGHT,
    SpecialKey::FUNCTION_1,  // B
    SpecialKey::FUNCTION_2   // A
};

// =============================================================================
// Ancient Ritual Manager
// =============================================================================

class AncientRitual {
public:
    AncientRitual();
    ~AncientRitual();
    
    /**
     * @brief Initialize ritual detection
     */
    void begin();
    
    /**
     * @brief Update ritual detection (call in loop)
     */
    void update();
    
    /**
     * @brief Check if ancient mode should activate
     * @return Trigger type that activated, or NONE
     */
    AncientTrigger checkActivation();
    
    /**
     * @brief Check if currently in ritual state
     */
    bool isInRitual() const { return ritualState_ != RitualState::INACTIVE; }
    
    /**
     * @brief Get current ritual state
     */
    RitualState getRitualState() const { return ritualState_; }
    
    /**
     * @brief Get ritual progress (0-1)
     */
    float getRitualProgress() const;
    
    /**
     * @brief Check voice phrase for activation
     * @param text Recognized text
     * @return true if phrase detected
     */
    bool checkVoicePhrase(const char* text);
    
    /**
     * @brief Process key for konami code
     * @param key Key event
     * @return true if konami code completed
     */
    bool processKonamiKey(SpecialKey key);
    
    /**
     * @brief Reset ritual state
     */
    void reset();
    
    /**
     * @brief Complete the ritual (enter ancient mode)
     */
    void completeRitual(AncientTrigger trigger);
    
    /**
     * @brief Check if quest is completed (allows exit)
     */
    bool isQuestCompleted() const { return questCompleted_; }
    
    /**
     * @brief Mark quest as completed
     */
    void completeQuest() { questCompleted_ = true; }
    
    /**
     * @brief Force quest requirement (cannot exit without completing)
     */
    void requireQuest() { questRequired_ = true; ritualState_ = RitualState::QUEST_REQUIRED; }
    
    /**
     * @brief Get trigger as string
     */
    static const char* getTriggerName(AncientTrigger trigger);

private:
    RitualState ritualState_ = RitualState::INACTIVE;
    AncientTrigger lastTrigger_ = AncientTrigger::NONE;
    
    // Gesture detection
    uint32_t gestureStartTime_ = 0;
    bool gestureButtonLeft_ = false;
    bool gestureButtonRight_ = false;
    
    // Konami code
    uint8_t konamiIndex_ = 0;
    uint32_t lastKonamiTime_ = 0;
    static constexpr uint32_t KONAMI_TIMEOUT_MS = 3000;
    
    // Time trigger
    uint8_t lastCheckedHour_ = 255;
    uint8_t lastCheckedMinute_ = 255;
    
    // Battery
    uint8_t lastBatteryLevel_ = 100;
    
    // Quest
    bool questRequired_ = false;
    bool questCompleted_ = false;
    
    // Voice phrases
    static constexpr const char* AWAKEN_PHRASES[] = {
        "minerva awaken",
        "minerva, awaken",
        "awaken minerva",
        "speak ancient",
        "ancient wisdom",
        "owl mode activate",
        "by the thirty seven claws",
        "by the thirty-seven claws"
    };
    static constexpr uint8_t NUM_AWAKEN_PHRASES = 8;
    
    // Private methods
    bool detectGesture();
    bool checkTimeTrigger();
    bool checkBatteryTrigger();
    void updateGestureDetection();
};

// =============================================================================
// Old English Response Generator
// =============================================================================

class AncientDialect {
public:
    /**
     * @brief Convert modern text to Old English style
     */
    static String toAncientSpeak(const String& text);
    
    /**
     * @brief Get random archaic greeting
     */
    static String getGreeting();
    
    /**
     * @brief Get random archaic farewell
     */
    static String getFarewell();
    
    /**
     * @brief Add archaic flourishes to text
     */
    static String addFlourish(const String& text);

private:
    static constexpr const char* GREETINGS[] = {
        "Hail and well met,",
        "Greetings, seeker of wisdom,",
        "The owl sees thee,",
        "By moon and claw, I greet thee,",
        "Speak, and be heard,",
        "The ancient ones listen,",
        "Thou hast summoned me,",
        "Approach, and fear not,"
    };
    
    static constexpr const char* FAREWELLS[] = {
        "Go with wisdom's blessing.",
        "The owl watches ever.",
        "Until the stars align again.",
        "May thy path be illuminated.",
        "Fare thee well, traveler.",
        "The shadows remember thee.",
        "Wisdom guide thy steps.",
        "The thirty-seven claws protect."
    };
    
    static constexpr const char* FLOURISHES[] = {
        "... *ancient knowing* ...",
        "... *rune shimmer* ...",
        "... *owl hoots softly* ...",
        "... *feathers rustle* ...",
        "... *moonlight glints* ...",
        "... *shadows deepen* ..."
    };
};

// Global instance
extern AncientRitual g_ancientRitual;

} // namespace Avatar

#endif // ANCIENT_RITUAL_H
