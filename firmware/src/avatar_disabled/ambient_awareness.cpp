/**
 * @file ambient_awareness.cpp
 * @brief Environmental awareness implementation
 */

#include "avatar/ambient_awareness.h"

namespace Avatar {

// Global instance
AmbientAwareness g_ambient;

AmbientAwareness::AmbientAwareness() {
    state_.lightLevel = 0.5f;
    state_.temperature = 22.0f;
    state_.dayPhase = DayPhase::AFTERNOON;
    state_.isDark = false;
    state_.isCold = false;
    state_.isHot = false;
    state_.hour = 12;
    state_.minute = 0;
}

AmbientAwareness::~AmbientAwareness() {
}

void AmbientAwareness::begin() {
    lastUpdateTime_ = millis();
    
    // Initialize with default/estimated values
    state_.lightLevel = 0.5f;
    state_.temperature = 22.0f;
}

void AmbientAwareness::update() {
    uint32_t now = millis();
    if (now - lastUpdateTime_ < UPDATE_INTERVAL_MS) return;
    
    lastUpdateTime_ = now;
    
    // Update light level
    updateLightLevel();
    
    // Update day phase based on time
    state_.dayPhase = calculateDayPhase(state_.hour);
    
    // Update temperature flags
    state_.isCold = state_.temperature < COLD_THRESHOLD;
    state_.isHot = state_.temperature > HOT_THRESHOLD;
}

const char* AmbientAwareness::getDayPhaseName(DayPhase phase) {
    switch (phase) {
        case DayPhase::NIGHT: return "Night";
        case DayPhase::DAWN: return "Dawn";
        case DayPhase::MORNING: return "Morning";
        case DayPhase::AFTERNOON: return "Afternoon";
        case DayPhase::EVENING: return "Evening";
        case DayPhase::TWILIGHT: return "Twilight";
        default: return "Unknown";
    }
}

uint8_t AmbientAwareness::getAdaptiveBrightness() const {
    // Brighter in daylight, dimmer at night
    float baseBrightness = 0.5f;
    
    switch (state_.dayPhase) {
        case DayPhase::NIGHT:
            baseBrightness = 0.3f;
            break;
        case DayPhase::DAWN:
        case DayPhase::TWILIGHT:
            baseBrightness = 0.4f;
            break;
        case DayPhase::MORNING:
        case DayPhase::EVENING:
            baseBrightness = 0.6f;
            break;
        case DayPhase::AFTERNOON:
            baseBrightness = 0.8f;
            break;
    }
    
    // Adjust for ambient light
    if (state_.lightLevel < DARK_THRESHOLD) {
        baseBrightness *= 0.7f;  // Dim in dark environments
    } else if (state_.lightLevel > BRIGHT_THRESHOLD) {
        baseBrightness = std::min(1.0f, baseBrightness * 1.2f);  // Brighten in sunlight
    }
    
    return (uint8_t)(baseBrightness * 255);
}

float AmbientAwareness::getAdaptiveGlow() const {
    // Glow more in dark, less in bright light
    float baseGlow = 0.5f;
    
    if (state_.isDark) {
        baseGlow = 0.8f;
    } else if (state_.lightLevel > BRIGHT_THRESHOLD) {
        baseGlow = 0.3f;
    }
    
    // Night owls glow more at night
    if (isNightOwlTime()) {
        baseGlow = std::min(1.0f, baseGlow * 1.3f);
    }
    
    return baseGlow;
}

bool AmbientAwareness::isSleepyTime() const {
    // Sleepy between 2 AM and 6 AM
    return (state_.hour >= 2 && state_.hour < 6);
}

bool AmbientAwareness::isNightOwlTime() const {
    // Night owls are active 10 PM to 4 AM
    return (state_.hour >= 22 || state_.hour < 4);
}

float AmbientAwareness::getTemperatureAnimation() const {
    if (state_.isCold) {
        // Shivering animation
        float shiver = std::sin(millis() * 0.02f) * 0.5f + 0.5f;
        return shiver * 0.3f;  // 0-0.3 shiver amount
    } else if (state_.isHot) {
        // Panting animation
        float pant = std::sin(millis() * 0.01f) * 0.5f + 0.5f;
        return pant * 0.5f;  // 0-0.5 pant amount
    }
    return 0.0f;
}

void AmbientAwareness::setLightSensorReading(uint16_t rawValue) {
    // ESP32 ADC is 12-bit (0-4095)
    state_.lightLevel = rawValue / 4095.0f;
    state_.isDark = state_.lightLevel < DARK_THRESHOLD;
}

void AmbientAwareness::setTemperature(float celsius) {
    state_.temperature = celsius;
    state_.isCold = celsius < COLD_THRESHOLD;
    state_.isHot = celsius > HOT_THRESHOLD;
}

void AmbientAwareness::updateTime(uint8_t hour, uint8_t minute) {
    state_.hour = hour;
    state_.minute = minute;
    state_.dayPhase = calculateDayPhase(hour);
}

// =============================================================================
// Private Methods
// =============================================================================

DayPhase AmbientAwareness::calculateDayPhase(uint8_t hour) const {
    if (hour >= 0 && hour < 5) {
        return DayPhase::NIGHT;
    } else if (hour >= 5 && hour < 8) {
        return DayPhase::DAWN;
    } else if (hour >= 8 && hour < 12) {
        return DayPhase::MORNING;
    } else if (hour >= 12 && hour < 17) {
        return DayPhase::AFTERNOON;
    } else if (hour >= 17 && hour < 20) {
        return DayPhase::EVENING;
    } else {
        return DayPhase::TWILIGHT;
    }
}

void AmbientAwareness::updateLightLevel() {
    // If no sensor available, estimate from time of day
    if (state_.lightLevel <= 0) {
        switch (state_.dayPhase) {
            case DayPhase::NIGHT:
                state_.lightLevel = 0.1f;
                break;
            case DayPhase::DAWN:
            case DayPhase::TWILIGHT:
                state_.lightLevel = 0.3f;
                break;
            case DayPhase::MORNING:
            case DayPhase::EVENING:
                state_.lightLevel = 0.6f;
                break;
            case DayPhase::AFTERNOON:
                state_.lightLevel = 0.9f;
                break;
        }
        state_.isDark = state_.lightLevel < DARK_THRESHOLD;
    }
}

} // namespace Avatar
