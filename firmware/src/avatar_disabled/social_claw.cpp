/**
 * @file social_claw.cpp
 * @brief Social features and Claw Network implementation
 */

#include "avatar/social_claw.h"

namespace Avatar {

// Global instance
SocialClaw g_socialClaw;

SocialClaw::SocialClaw() 
    : bleEnabled_(false),
      bleInitialized_(false),
      featherPoints_(0) {
    
    // Clear nearby devices
    for (int i = 0; i < MAX_NEARBY_DEVICES; i++) {
        nearbyDevices_[i].lastSeen = 0;
    }
    
    initializeAchievements();
}

SocialClaw::~SocialClaw() {
}

void SocialClaw::begin() {
    // BLE initialization would go here
    // For now, disabled by default to save power
    bleEnabled_ = false;
    bleInitialized_ = false;
}

void SocialClaw::update() {
    if (!bleEnabled_) return;
    
    // Clean up timed out devices
    cleanupTimedOutDevices();
    
    // Periodic broadcast
    uint32_t now = millis();
    if (now - lastBroadcastTime_ > BROADCAST_INTERVAL_MS) {
        // Would broadcast mood here
        lastBroadcastTime_ = now;
    }
}

void SocialClaw::setBLEEnabled(bool enabled) {
    bleEnabled_ = enabled;
    
    if (enabled && !bleInitialized_) {
        // Initialize BLE
        // BLEDevice::init("Minerva-Cardputer");
        bleInitialized_ = true;
    } else if (!enabled && bleInitialized_) {
        // Deinitialize BLE
        // BLEDevice::deinit();
        bleInitialized_ = false;
    }
}

uint8_t SocialClaw::getNearbyCount() const {
    uint8_t count = 0;
    for (int i = 0; i < MAX_NEARBY_DEVICES; i++) {
        if (nearbyDevices_[i].isValid() && !nearbyDevices_[i].isTimedOut()) {
            count++;
        }
    }
    return count;
}

const NearbyDevice* SocialClaw::getNearbyDevice(uint8_t index) const {
    if (index >= MAX_NEARBY_DEVICES) return nullptr;
    
    if (nearbyDevices_[index].isValid() && !nearbyDevices_[index].isTimedOut()) {
        return &nearbyDevices_[index];
    }
    return nullptr;
}

bool SocialClaw::isDeviceNearby(const uint8_t* mac) const {
    for (int i = 0; i < MAX_NEARBY_DEVICES; i++) {
        if (nearbyDevices_[i].isValid() && 
            !nearbyDevices_[i].isTimedOut() &&
            memcmp(nearbyDevices_[i].mac, mac, 6) == 0) {
            return true;
        }
    }
    return false;
}

void SocialClaw::broadcastMood(uint8_t moodState, bool isAncient) {
    // Would broadcast via BLE
    lastBroadcastTime_ = millis();
}

void SocialClaw::onMinnieDetected(const NearbyDevice& device) {
    // Check if already known
    int8_t slot = findDeviceSlot(device.mac);
    
    if (slot < 0) {
        // New device
        slot = findFreeSlot();
        if (slot >= 0) {
            nearbyDevices_[slot] = device;
            nearbyDevices_[slot].lastSeen = millis();
            
            // Play greeting
            playGreetingAnimation();
            
            // Award achievement
            unlockAchievement(Achievement::SOCIAL_BUTTERFLY);
            
            // Award points
            addFeatherPoints(50, "Met another Minnie!");
        }
    } else {
        // Update existing
        nearbyDevices_[slot].lastSeen = millis();
        nearbyDevices_[slot].rssi = device.rssi;
        nearbyDevices_[slot].moodState = device.moodState;
    }
}

// =============================================================================
// Feather Points System
// =============================================================================

void SocialClaw::addFeatherPoints(uint16_t points, const char* reason) {
    uint32_t oldPoints = featherPoints_;
    featherPoints_ += points;
    
    // Check for level up
    uint8_t oldLevel = getCurrentLevel();
    
    // Could trigger level up animation here
    if (getCurrentLevel() > oldLevel) {
        // Level up!
    }
}

bool SocialClaw::isAchievementUnlocked(Achievement achievement) const {
    size_t index = (size_t)achievement;
    if (index < (size_t)Achievement::ACHIEVEMENT_COUNT) {
        return achievements_[index].unlocked;
    }
    return false;
}

void SocialClaw::unlockAchievement(Achievement achievement) {
    size_t index = (size_t)achievement;
    if (index >= (size_t)Achievement::ACHIEVEMENT_COUNT) return;
    if (achievements_[index].unlocked) return;
    
    achievements_[index].unlocked = true;
    achievements_[index].unlockedTime = millis();
    
    // Award points
    addFeatherPoints(achievements_[index].featherPoints, 
                     achievements_[index].name);
}

const AchievementInfo* SocialClaw::getAchievementInfo(Achievement achievement) const {
    size_t index = (size_t)achievement;
    if (index < (size_t)Achievement::ACHIEVEMENT_COUNT) {
        return &achievements_[index];
    }
    return nullptr;
}

uint8_t SocialClaw::getUnlockedAchievementCount() const {
    uint8_t count = 0;
    for (size_t i = 0; i < (size_t)Achievement::ACHIEVEMENT_COUNT; i++) {
        if (achievements_[i].unlocked) count++;
    }
    return count;
}

uint8_t SocialClaw::getCurrentLevel() const {
    for (uint8_t i = MAX_LEVEL; i > 0; i--) {
        if (featherPoints_ >= LEVEL_THRESHOLDS[i - 1]) {
            return i;
        }
    }
    return 1;
}

float SocialClaw::getLevelProgress() const {
    uint8_t currentLevel = getCurrentLevel();
    if (currentLevel >= MAX_LEVEL) return 1.0f;
    
    uint32_t currentThreshold = LEVEL_THRESHOLDS[currentLevel - 1];
    uint32_t nextThreshold = LEVEL_THRESHOLDS[currentLevel];
    
    if (nextThreshold == currentThreshold) return 1.0f;
    
    return (float)(featherPoints_ - currentThreshold) / 
           (nextThreshold - currentThreshold);
}

const char* SocialClaw::getLevelTitle(uint8_t level) {
    if (level < 1) level = 1;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return LEVEL_TITLES[level - 1];
}

// =============================================================================
// Private Methods
// =============================================================================

void SocialClaw::initializeAchievements() {
    achievements_[(size_t)Achievement::FIRST_ROAST] = {
        Achievement::FIRST_ROAST,
        "First Roast",
        "Receive a witty response from Minerva",
        100,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::ANCIENT_SUMMONER] = {
        Achievement::ANCIENT_SUMMONER,
        "Ancient Summoner",
        "Activate Ancient Mode",
        200,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::CHAOS_AGENT] = {
        Achievement::CHAOS_AGENT,
        "Chaos Agent",
        "Trigger the Chaotic mood",
        150,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::POLITE_USER] = {
        Achievement::POLITE_USER,
        "Polite User",
        "Say 'please' 10 times",
        50,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::NIGHT_OWL] = {
        Achievement::NIGHT_OWL,
        "Night Owl",
        "Use Cardputer at 3 AM",
        75,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::SOCIAL_BUTTERFLY] = {
        Achievement::SOCIAL_BUTTERFLY,
        "Social Butterfly",
        "Meet another Cardputer in the wild",
        200,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::DEEP_THINKER] = {
        Achievement::DEEP_THINKER,
        "Deep Thinker",
        "Ask a philosophical question",
        100,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::EASTER_EGG_HUNTER] = {
        Achievement::EASTER_EGG_HUNTER,
        "Easter Egg Hunter",
        "Discover 5 different easter eggs",
        300,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::SPEED_TYPER] = {
        Achievement::SPEED_TYPER,
        "Speed Typer",
        "Type at 100+ WPM",
        150,
        false,
        0
    };
    
    achievements_[(size_t)Achievement::LOYAL_COMPANION] = {
        Achievement::LOYAL_COMPANION,
        "Loyal Companion",
        "Use Cardputer for 30 days",
        500,
        false,
        0
    };
}

int8_t SocialClaw::findDeviceSlot(const uint8_t* mac) const {
    for (int i = 0; i < MAX_NEARBY_DEVICES; i++) {
        if (nearbyDevices_[i].isValid() && 
            memcmp(nearbyDevices_[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

int8_t SocialClaw::findFreeSlot() const {
    for (int i = 0; i < MAX_NEARBY_DEVICES; i++) {
        if (!nearbyDevices_[i].isValid() || nearbyDevices_[i].isTimedOut()) {
            return i;
        }
    }
    return -1;
}

void SocialClaw::cleanupTimedOutDevices() {
    for (int i = 0; i < MAX_NEARBY_DEVICES; i++) {
        if (nearbyDevices_[i].isValid() && nearbyDevices_[i].isTimedOut()) {
            nearbyDevices_[i].lastSeen = 0;
        }
    }
}

void SocialClaw::playGreetingAnimation() {
    // Would trigger greeting animation on avatar
    // Avatar::g_avatar.playGreeting();
}

} // namespace Avatar
