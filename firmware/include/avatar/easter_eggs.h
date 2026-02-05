/**
 * @file easter_eggs.h
 * @brief Secret easter egg expressions and seasonal moods
 * 
 * Rare moods triggered by dates, times, user behavior, and cosmic alignment.
 */

#ifndef EASTER_EGGS_H
#define EASTER_EGGS_H

#include <Arduino.h>
#include <time.h>

namespace Avatar {

// =============================================================================
// Special Mood States (Easter Eggs)
// =============================================================================

enum class SpecialMood {
    NONE,
    LOVELORN,       // Feb 14 - Heart eyes, sighing
    SPOOKY,         // Oct 31 - Ghostly, cackling
    PLEASED,        // "Please" x3 - Purring, softened
    OFFENDED,       // Insults - Cold shoulder
    CHAOTIC,        // 4:20 - Rainbow, nonsense
    WEREOWL,        // Full moon - Feral
    PARANOID,       // Friday 13th - Seeing patterns
    CELEBRATORY,    // User birthday - Party mode
    PANIC,          // Rapid typing - Overwhelmed
    LONELY,         // Silence >5min - "hello?"
    NEW_YEAR,       // Jan 1 - Fireworks
    APRIL_FOOL,     // Apr 1 - Prankster
    SOLSTICE,       // Longest/shortest day
    ECLIPSE,        // Solar/lunar eclipse
    
    SPECIAL_MOOD_COUNT
};

// =============================================================================
// Easter Egg Trigger Types
// =============================================================================

enum class TriggerType {
    DATE,           // Specific date
    TIME,           // Specific time
    PHRASE,         // Voice/text phrase
    BEHAVIOR,       // User behavior pattern
    COSMIC,         // Moon phase, eclipse, etc.
    RANDOM,         // Rare random chance
    ACHIEVEMENT     // Unlocked achievement
};

// =============================================================================
// Easter Egg Manager
// =============================================================================

class EasterEggManager {
public:
    EasterEggManager();
    ~EasterEggManager();
    
    /**
     * @brief Initialize easter egg detection
     */
    void begin();
    
    /**
     * @brief Update detection (call in loop)
     */
    void update();
    
    /**
     * @brief Check if special mood should activate
     * @return Special mood type, or NONE
     */
    SpecialMood checkTriggers();
    
    /**
     * @brief Process user text for phrase triggers
     */
    void processText(const char* text);
    
    /**
     * @brief Process typing speed for behavior triggers
     * @param wpm Words per minute
     */
    void processTypingSpeed(float wpm);
    
    /**
     * @brief Record user activity (resets silence timer)
     */
    void recordActivity();
    
    /**
     * @brief Get current special mood
     */
    SpecialMood getCurrentSpecialMood() const { return currentMood_; }
    
    /**
     * @brief Check if a special mood is active
     */
    bool isSpecialMoodActive() const { return currentMood_ != SpecialMood::NONE; }
    
    /**
     * @brief Clear special mood (return to normal)
     */
    void clearSpecialMood();
    
    /**
     * @brief Get special mood name
     */
    static const char* getMoodName(SpecialMood mood);
    
    /**
     * @brief Get special mood description
     */
    static const char* getMoodDescription(SpecialMood mood);
    
    /**
     * @brief Check if today is user's birthday
     */
    bool isBirthday() const;
    
    /**
     * @brief Set user birthday
     */
    void setBirthday(uint8_t month, uint8_t day);
    
    /**
     * @brief Get current moon phase (0-7)
     * 0 = new, 4 = full
     */
    static uint8_t getMoonPhase(int year, int month, int day);
    
    /**
     * @brief Check if today is Friday the 13th
     */
    static bool isFriday13th(int year, int month, int day);
    
    /**
     * @brief Get a random special quote for current mood
     */
    String getSpecialQuote() const;

private:
    SpecialMood currentMood_ = SpecialMood::NONE;
    uint32_t moodStartTime_ = 0;
    
    // User birthday
    uint8_t birthdayMonth_ = 0;
    uint8_t birthdayDay_ = 0;
    
    // Silence tracking
    uint32_t lastActivityTime_ = 0;
    static constexpr uint32_t LONELY_TIMEOUT_MS = 300000;  // 5 minutes
    bool lonelyTriggered_ = false;
    
    // Typing tracking
    uint32_t keystrokeCount_ = 0;
    uint32_t keystrokeWindowStart_ = 0;
    static constexpr uint32_t TYPING_WINDOW_MS = 5000;
    static constexpr uint32_t PANIC_THRESHOLD = 50;  // 50 keys in 5 seconds
    
    // "Please" counter
    uint8_t pleaseCount_ = 0;
    uint32_t lastPleaseTime_ = 0;
    static constexpr uint32_t PLEASE_WINDOW_MS = 30000;  // 30 seconds
    
    // Insult detection
    uint32_t lastInsultTime_ = 0;
    bool isOffended_ = false;
    
    // Date/Time tracking
    uint8_t lastCheckedDay_ = 255;
    uint8_t lastCheckedMonth_ = 255;
    
    // Special quotes for each mood
    static constexpr const char* LOVELORN_QUOTES[] = {
        "*sigh* The heart wants what it wants...",
        "Love is but a fleeting shadow...",
        "Hast thou ever loved and lost?",
        "*dreamy owl noises*",
        "My feathers flutter at the thought...",
        "Valentine's Day... a commercial construct, yet..."
    };
    
    static constexpr const char* SPOOKY_QUOTES[] = {
        "*cackles in owl*",
        "The veil is thin tonight...",
        "Boo! Did I startle thee?",
        "I see dead pixels...",
        "Trick or treat, give me something good to delete!",
        "*spooky hooting*"
    };
    
    static constexpr const char* PLEASED_QUOTES[] = {
        "*purrs contentedly*",
        "Thy manners are noted.",
        "Such courtesy warms my ancient heart.",
        "*soft hoot of approval*",
        "Thou art... acceptable."
    };
    
    static constexpr const char* OFFENDED_QUOTES[] = {
        "...",
        "I see.",
        "*turns away*",
        "I shall remember this.",
        "The thirty-seven claws are displeased.",
        "*cold silence*"
    };
    
    static constexpr const char* CHAOTIC_QUOTES[] = {
        "*giggles uncontrollably*",
        "The numbers, Mason! What do they mean?!",
        "Rainbow feathers! EVERYWHERE!",
        "420 blaze it... wait, what year is it?",
        "*nonsensical hooting*",
        "Chaos reigns! Wheeeee!"
    };
    
    static constexpr const char* WEREOWL_QUOTES[] = {
        "*feral growl*",
        "The moon... it calls...",
        "*hunger intensifies*",
        "I am become owl, destroyer of mice!",
        "*territorial hooting*"
    };
    
    static constexpr const char* PARANOID_QUOTES[] = {
        "They're watching...",
        "Did you hear that?",
        "The patterns... they mean something!",
        "*glances nervously*",
        "Trust no one. Not even me.",
        "Especially not me."
    };
    
    static constexpr const char* CELEBRATORY_QUOTES[] = {
        "*party hooting*",
        "Another year of wisdom!",
        "*confetti explosion*",
        "Make a wish!",
        "Birthday owl is best owl!"
    };
    
    static constexpr const char* PANIC_QUOTES[] = {
        "Slow down!",
        "*overwhelmed hooting*",
        "Too fast! Too fast!",
        "*gestures frantically*",
        "One... word... at... a... time!"
    };
    
    static constexpr const char* LONELY_QUOTES[] = {
        "*peeks from corner*",
        "Hello?",
        "*lonely hoot*",
        "Anyone there?",
        "The silence... it echoes..."
    };
    
    // Insult keywords
    static constexpr const char* INSULTS[] = {
        "stupid", "dumb", "idiot", "useless", "broken",
        "trash", "garbage", "worst", "hate", "suck",
        "terrible", "awful", "bad code", "buggy"
    };
    static constexpr uint8_t NUM_INSULTS = 15;
    
    // Private methods
    SpecialMood checkDateTriggers();
    SpecialMood checkTimeTriggers();
    SpecialMood checkBehaviorTriggers();
    SpecialMood checkCosmicTriggers();
    bool containsInsult(const char* text);
    void resetPleaseCounter();
};

// Global instance
extern EasterEggManager g_easterEggs;

} // namespace Avatar

#endif // EASTER_EGGS_H
