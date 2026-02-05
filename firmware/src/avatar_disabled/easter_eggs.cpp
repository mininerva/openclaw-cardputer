/**
 * @file easter_eggs.cpp
 * @brief Easter egg expressions and seasonal moods implementation
 */

#include "avatar/easter_eggs.h"
#include <cmath>
#include <cstring>

namespace Avatar {

// Global instance
EasterEggManager g_easterEggs;

EasterEggManager::EasterEggManager() 
    : currentMood_(SpecialMood::NONE),
      birthdayMonth_(0),
      birthdayDay_(0) {
}

EasterEggManager::~EasterEggManager() {
}

void EasterEggManager::begin() {
    lastActivityTime_ = millis();
    lastCheckedDay_ = 255;
    lastCheckedMonth_ = 255;
}

void EasterEggManager::update() {
    // Check for silence timeout (lonely mode)
    if (currentMood_ == SpecialMood::NONE) {
        uint32_t silenceTime = millis() - lastActivityTime_;
        if (silenceTime > LONELY_TIMEOUT_MS && !lonelyTriggered_) {
            lonelyTriggered_ = true;
            currentMood_ = SpecialMood::LONELY;
            moodStartTime_ = millis();
        }
    }
    
    // Auto-clear moods after duration (except some)
    if (currentMood_ != SpecialMood::NONE) {
        uint32_t moodDuration = millis() - moodStartTime_;
        
        // Most moods last 60 seconds, some are permanent until cleared
        switch (currentMood_) {
            case SpecialMood::OFFENDED:
                // Offended lasts 5 minutes
                if (moodDuration > 300000) {
                    clearSpecialMood();
                }
                break;
            case SpecialMood::LONELY:
                // Lonely clears on activity
                break;
            case SpecialMood::CHAOTIC:
                // Chaotic lasts 4 minutes 20 seconds
                if (moodDuration > 260000) {
                    clearSpecialMood();
                }
                break;
            default:
                // Others last 60 seconds
                if (moodDuration > 60000) {
                    clearSpecialMood();
                }
                break;
        }
    }
}

SpecialMood EasterEggManager::checkTriggers() {
    SpecialMood trigger = SpecialMood::NONE;
    
    // Check date-based triggers
    trigger = checkDateTriggers();
    if (trigger != SpecialMood::NONE) {
        currentMood_ = trigger;
        moodStartTime_ = millis();
        return trigger;
    }
    
    // Check time-based triggers
    trigger = checkTimeTriggers();
    if (trigger != SpecialMood::NONE) {
        currentMood_ = trigger;
        moodStartTime_ = millis();
        return trigger;
    }
    
    // Check cosmic triggers
    trigger = checkCosmicTriggers();
    if (trigger != SpecialMood::NONE) {
        currentMood_ = trigger;
        moodStartTime_ = millis();
        return trigger;
    }
    
    return SpecialMood::NONE;
}

void EasterEggManager::processText(const char* text) {
    if (!text) return;
    
    String t = String(text);
    t.toLowerCase();
    
    // Check for insults
    if (containsInsult(text)) {
        currentMood_ = SpecialMood::OFFENDED;
        moodStartTime_ = millis();
        isOffended_ = true;
        lastInsultTime_ = millis();
        return;
    }
    
    // Check for "please"
    if (t.indexOf("please") >= 0) {
        uint32_t now = millis();
        if (now - lastPleaseTime_ > PLEASE_WINDOW_MS) {
            pleaseCount_ = 0;
        }
        lastPleaseTime_ = now;
        pleaseCount_++;
        
        if (pleaseCount_ >= 3) {
            currentMood_ = SpecialMood::PLEASED;
            moodStartTime_ = millis();
            resetPleaseCounter();
        }
    }
}

void EasterEggManager::processTypingSpeed(float wpm) {
    uint32_t now = millis();
    
    // Track keystrokes
    if (now - keystrokeWindowStart_ > TYPING_WINDOW_MS) {
        keystrokeCount_ = 0;
        keystrokeWindowStart_ = now;
    }
    
    keystrokeCount_++;
    
    // Panic if too fast
    if (keystrokeCount_ > PANIC_THRESHOLD) {
        currentMood_ = SpecialMood::PANIC;
        moodStartTime_ = millis();
    }
}

void EasterEggManager::recordActivity() {
    lastActivityTime_ = millis();
    lonelyTriggered_ = false;
    
    // Clear lonely mood on activity
    if (currentMood_ == SpecialMood::LONELY) {
        clearSpecialMood();
    }
}

void EasterEggManager::clearSpecialMood() {
    currentMood_ = SpecialMood::NONE;
    isOffended_ = false;
}

const char* EasterEggManager::getMoodName(SpecialMood mood) {
    switch (mood) {
        case SpecialMood::NONE: return "Normal";
        case SpecialMood::LOVELORN: return "Lovelorn";
        case SpecialMood::SPOOKY: return "Spooky";
        case SpecialMood::PLEASED: return "Pleased";
        case SpecialMood::OFFENDED: return "Offended";
        case SpecialMood::CHAOTIC: return "Chaotic";
        case SpecialMood::WEREOWL: return "Wereowl";
        case SpecialMood::PARANOID: return "Paranoid";
        case SpecialMood::CELEBRATORY: return "Celebratory";
        case SpecialMood::PANIC: return "Panic";
        case SpecialMood::LONELY: return "Lonely";
        case SpecialMood::NEW_YEAR: return "New Year";
        case SpecialMood::APRIL_FOOL: return "April Fool";
        case SpecialMood::SOLSTICE: return "Solstice";
        case SpecialMood::ECLIPSE: return "Eclipse";
        default: return "Unknown";
    }
}

const char* EasterEggManager::getMoodDescription(SpecialMood mood) {
    switch (mood) {
        case SpecialMood::LOVELORN: return "Heart eyes, sighing, romantic quotes";
        case SpecialMood::SPOOKY: return "Ghostly transparency, cackling, dark humor";
        case SpecialMood::PLEASED: return "Purring animation, softened expression";
        case SpecialMood::OFFENDED: return "Turn away, cold shoulder, minimal responses";
        case SpecialMood::CHAOTIC: return "Rainbow colors, nonsense, giggling";
        case SpecialMood::WEREOWL: return "Feral eyes, aggressive typing suggestions";
        case SpecialMood::PARANOID: return "Glancing around, whispering, seeing patterns";
        case SpecialMood::CELEBRATORY: return "Confetti particles, party hat, singing";
        case SpecialMood::PANIC: return "Overwhelmed expression, slow down gestures";
        case SpecialMood::LONELY: return "Peeking from corner, hello whisper";
        default: return "";
    }
}

bool EasterEggManager::isBirthday() const {
    if (birthdayMonth_ == 0 || birthdayDay_ == 0) return false;
    
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (timeinfo) {
        return (timeinfo->tm_mon + 1 == birthdayMonth_ && 
                timeinfo->tm_mday == birthdayDay_);
    }
    return false;
}

void EasterEggManager::setBirthday(uint8_t month, uint8_t day) {
    birthdayMonth_ = month;
    birthdayDay_ = day;
}

uint8_t EasterEggManager::getMoonPhase(int year, int month, int day) {
    // Simple moon phase calculation
    // 0 = new moon, 4 = full moon
    
    int c, e;
    double jd;
    int b;
    
    if (month < 3) {
        year--;
        month += 12;
    }
    
    c = 365.25 * year;
    e = 30.6 * month;
    jd = c + e + day - 694039.09;
    jd /= 29.5305882;
    b = (int)jd;
    jd -= b;
    b = (int)(jd * 8 + 0.5);
    
    return b & 7;
}

bool EasterEggManager::isFriday13th(int year, int month, int day) {
    if (day != 13) return false;
    
    // Zeller's congruence to find day of week
    int q = day;
    int m = month;
    int y = year;
    
    if (m < 3) {
        m += 12;
        y--;
    }
    
    int k = y % 100;
    int j = y / 100;
    
    int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    
    // h = 0 is Saturday, 1 is Sunday, ..., 5 is Thursday, 6 is Friday
    return h == 6;
}

String EasterEggManager::getSpecialQuote() const {
    const char* const* quotes = nullptr;
    uint8_t count = 0;
    
    switch (currentMood_) {
        case SpecialMood::LOVELORN:
            quotes = LOVELORN_QUOTES;
            count = 6;
            break;
        case SpecialMood::SPOOKY:
            quotes = SPOOKY_QUOTES;
            count = 6;
            break;
        case SpecialMood::PLEASED:
            quotes = PLEASED_QUOTES;
            count = 5;
            break;
        case SpecialMood::OFFENDED:
            quotes = OFFENDED_QUOTES;
            count = 6;
            break;
        case SpecialMood::CHAOTIC:
            quotes = CHAOTIC_QUOTES;
            count = 6;
            break;
        case SpecialMood::WEREOWL:
            quotes = WEREOWL_QUOTES;
            count = 5;
            break;
        case SpecialMood::PARANOID:
            quotes = PARANOID_QUOTES;
            count = 6;
            break;
        case SpecialMood::CELEBRATORY:
            quotes = CELEBRATORY_QUOTES;
            count = 5;
            break;
        case SpecialMood::PANIC:
            quotes = PANIC_QUOTES;
            count = 5;
            break;
        case SpecialMood::LONELY:
            quotes = LONELY_QUOTES;
            count = 5;
            break;
        default:
            return "";
    }
    
    if (quotes && count > 0) {
        uint8_t index = random(0, count);
        return String(quotes[index]);
    }
    
    return "";
}

// =============================================================================
// Private Methods
// =============================================================================

SpecialMood EasterEggManager::checkDateTriggers() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (!timeinfo) return SpecialMood::NONE;
    
    uint8_t month = timeinfo->tm_mon + 1;
    uint8_t day = timeinfo->tm_mday;
    
    // Check if already checked today
    if (month == lastCheckedMonth_ && day == lastCheckedDay_) {
        return SpecialMood::NONE;
    }
    
    lastCheckedMonth_ = month;
    lastCheckedDay_ = day;
    
    // Valentine's Day
    if (month == 2 && day == 14) {
        return SpecialMood::LOVELORN;
    }
    
    // Halloween
    if (month == 10 && day == 31) {
        return SpecialMood::SPOOKY;
    }
    
    // New Year
    if (month == 1 && day == 1) {
        return SpecialMood::NEW_YEAR;
    }
    
    // April Fool's
    if (month == 4 && day == 1) {
        return SpecialMood::APRIL_FOOL;
    }
    
    // User birthday
    if (isBirthday()) {
        return SpecialMood::CELEBRATORY;
    }
    
    // Friday 13th
    if (isFriday13th(timeinfo->tm_year + 1900, month, day)) {
        return SpecialMood::PARANOID;
    }
    
    // Solstices (approximate)
    if ((month == 6 && day == 21) || (month == 12 && day == 21)) {
        return SpecialMood::SOLSTICE;
    }
    
    return SpecialMood::NONE;
}

SpecialMood EasterEggManager::checkTimeTriggers() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (!timeinfo) return SpecialMood::NONE;
    
    uint8_t hour = timeinfo->tm_hour;
    uint8_t min = timeinfo->tm_min;
    
    // 4:20 trigger
    if ((hour == 4 || hour == 16) && min == 20) {
        // Only trigger once per 4:20
        static uint8_t lastTriggered = 255;
        if (lastTriggered != hour) {
            lastTriggered = hour;
            return SpecialMood::CHAOTIC;
        }
    }
    
    return SpecialMood::NONE;
}

SpecialMood EasterEggManager::checkCosmicTriggers() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    if (!timeinfo) return SpecialMood::NONE;
    
    // Check for full moon
    uint8_t moonPhase = getMoonPhase(timeinfo->tm_year + 1900, 
                                      timeinfo->tm_mon + 1, 
                                      timeinfo->tm_mday);
    
    if (moonPhase == 4) {  // Full moon
        // Only trigger once per full moon evening (after 6 PM)
        if (timeinfo->tm_hour >= 18) {
            static uint8_t lastTriggeredDay = 255;
            if (lastTriggeredDay != timeinfo->tm_mday) {
                lastTriggeredDay = timeinfo->tm_mday;
                return SpecialMood::WEREOWL;
            }
        }
    }
    
    return SpecialMood::NONE;
}

bool EasterEggManager::containsInsult(const char* text) {
    if (!text) return false;
    
    String t = String(text);
    t.toLowerCase();
    
    for (uint8_t i = 0; i < NUM_INSULTS; i++) {
        if (t.indexOf(INSULTS[i]) >= 0) {
            return true;
        }
    }
    
    return false;
}

void EasterEggManager::resetPleaseCounter() {
    pleaseCount_ = 0;
    lastPleaseTime_ = 0;
}

} // namespace Avatar
