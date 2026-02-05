/**
 * @file social_claw.h
 * @brief Social features and Claw Network
 * 
 * BLE beacon detection, greeting other Cardputers, mood sharing.
 */

#ifndef SOCIAL_CLAW_H
#define SOCIAL_CLAW_H

#include <Arduino.h>
#include <vector>

// Forward declaration - BLE includes would go here
// #include <BLEDevice.h>

namespace Avatar {

// =============================================================================
// Claw Network Constants
// =============================================================================

constexpr const char* CLAW_SERVICE_UUID = "4f70656e-436c-6177-4e65-74776f726b30";  // "OpenClawNetwork0"
constexpr const char* CLAW_CHARACTERISTIC_UUID = "4d696e65-7276-6143-6c61-774d6f6f64"; // "MinervaClawMood"

constexpr uint8_t MAX_NEARBY_DEVICES = 8;
constexpr uint32_t DEVICE_TIMEOUT_MS = 30000;  // 30 seconds
constexpr uint32_t BROADCAST_INTERVAL_MS = 5000;  // 5 seconds

// =============================================================================
// Nearby Device Info
// =============================================================================

struct NearbyDevice {
    char name[32];
    uint8_t mac[6];
    int8_t rssi;
    uint32_t lastSeen;
    uint8_t moodState;
    bool isAncient;
    
    bool isValid() const { return lastSeen > 0; }
    bool isTimedOut() const { return millis() - lastSeen > DEVICE_TIMEOUT_MS; }
};

// =============================================================================
// Feather Points (Gamification)
// =============================================================================

enum class Achievement {
    FIRST_ROAST,        // First witty response
    ANCIENT_SUMMONER,   // Activated ancient mode
    CHAOS_AGENT,        // Triggered chaotic mood
    POLITE_USER,        // Said please 10 times
    NIGHT_OWL,          // Used at 3 AM
    SOCIAL_BUTTERFLY,   // Met another Cardputer
    DEEP_THINKER,       // Asked philosophical question
    EASTER_EGG_HUNTER,  // Found 5 easter eggs
    SPEED_TYPER,        // Typed 100+ WPM
    LOYAL_COMPANION,    // Used for 30 days
    
    ACHIEVEMENT_COUNT
};

struct AchievementInfo {
    Achievement id;
    const char* name;
    const char* description;
    uint16_t featherPoints;
    bool unlocked;
    uint32_t unlockedTime;
};

// =============================================================================
// Social Claw Manager
// =============================================================================

class SocialClaw {
public:
    SocialClaw();
    ~SocialClaw();
    
    /**
     * @brief Initialize social features
     */
    void begin();
    
    /**
     * @brief Update social features (call in loop)
     */
    void update();
    
    /**
     * @brief Enable/disable BLE (saves power when disabled)
     */
    void setBLEEnabled(bool enabled);
    
    /**
     * @brief Check if BLE is active
     */
    bool isBLEActive() const { return bleEnabled_; }
    
    /**
     * @brief Get count of nearby devices
     */
    uint8_t getNearbyCount() const;
    
    /**
     * @brief Get nearby device info
     */
    const NearbyDevice* getNearbyDevice(uint8_t index) const;
    
    /**
     * @brief Check if a specific device is nearby
     */
    bool isDeviceNearby(const uint8_t* mac) const;
    
    /**
     * @brief Broadcast current mood to nearby devices
     */
    void broadcastMood(uint8_t moodState, bool isAncient);
    
    /**
     * @brief Called when another Minnie is detected
     */
    void onMinnieDetected(const NearbyDevice& device);
    
    // =============================================================================
    // Feather Points System
    // =============================================================================
    
    /**
     * @brief Get current feather points
     */
    uint32_t getFeatherPoints() const { return featherPoints_; }
    
    /**
     * @brief Add feather points
     */
    void addFeatherPoints(uint16_t points, const char* reason);
    
    /**
     * @brief Check if achievement is unlocked
     */
    bool isAchievementUnlocked(Achievement achievement) const;
    
    /**
     * @brief Unlock an achievement
     */
    void unlockAchievement(Achievement achievement);
    
    /**
     * @brief Get achievement info
     */
    const AchievementInfo* getAchievementInfo(Achievement achievement) const;
    
    /**
     * @brief Get achievement progress
     */
    uint8_t getUnlockedAchievementCount() const;
    
    /**
     * @brief Check for level up
     */
    uint8_t getCurrentLevel() const;
    
    /**
     * @brief Get progress to next level
     */
    float getLevelProgress() const;
    
    /**
     * @brief Get level title
     */
    static const char* getLevelTitle(uint8_t level);

private:
    bool bleEnabled_ = false;
    bool bleInitialized_ = false;
    
    // Nearby devices
    NearbyDevice nearbyDevices_[MAX_NEARBY_DEVICES];
    uint32_t lastBroadcastTime_ = 0;
    
    // Feather points
    uint32_t featherPoints_ = 0;
    AchievementInfo achievements_[(size_t)Achievement::ACHIEVEMENT_COUNT];
    
    // Level thresholds
    static constexpr uint32_t LEVEL_THRESHOLDS[] = {
        0,      // Level 1
        100,    // Level 2
        250,    // Level 3
        500,    // Level 4
        1000,   // Level 5
        2000,   // Level 6
        3500,   // Level 7
        5000,   // Level 8
        7500,   // Level 9
        10000   // Level 10 (Max)
    };
    static constexpr uint8_t MAX_LEVEL = 10;
    
    // Level titles
    static constexpr const char* LEVEL_TITLES[] = {
        "Hatchling",        // Level 1
        "Nestling",         // Level 2
        "Fledgling",        // Level 3
        "Apprentice",       // Level 4
        "Scholar",          // Level 5
        "Wisdom Keeper",    // Level 6
        "Shadow Watcher",   // Level 7
        "Moon Speaker",     // Level 8
        "Ancient One",      // Level 9
        "The Thirty-Seventh" // Level 10
    };
    
    // Achievement definitions
    void initializeAchievements();
    
    // Private methods
    int8_t findDeviceSlot(const uint8_t* mac) const;
    int8_t findFreeSlot() const;
    void cleanupTimedOutDevices();
    void playGreetingAnimation();
};

// Global instance
extern SocialClaw g_socialClaw;

} // namespace Avatar

#endif // SOCIAL_CLAW_H
