/**
 * @file ambient_awareness.h
 * @brief Environmental awareness and adaptive behaviors
 * 
 * Responds to light, temperature, time of day, and other sensors.
 */

#ifndef AMBIENT_AWARENESS_H
#define AMBIENT_AWARENESS_H

#include <Arduino.h>
#include <time.h>

namespace Avatar {

// =============================================================================
// Time of Day Phases
// =============================================================================

enum class DayPhase {
    NIGHT,          // 00:00 - 05:00
    DAWN,           // 05:00 - 08:00
    MORNING,        // 08:00 - 12:00
    AFTERNOON,      // 12:00 - 17:00
    EVENING,        // 17:00 - 20:00
    TWILIGHT        // 20:00 - 00:00
};

// =============================================================================
// Ambient State
// =============================================================================

struct AmbientState {
    float lightLevel;           // 0-1 (from light sensor or screen brightness)
    float temperature;          // Celsius (if available)
    DayPhase dayPhase;
    bool isDark;
    bool isCold;
    bool isHot;
    uint8_t hour;
    uint8_t minute;
};

// =============================================================================
// Ambient Awareness Manager
// =============================================================================

class AmbientAwareness {
public:
    AmbientAwareness();
    ~AmbientAwareness();
    
    /**
     * @brief Initialize ambient sensing
     */
    void begin();
    
    /**
     * @brief Update sensor readings (call in loop)
     */
    void update();
    
    /**
     * @brief Get current ambient state
     */
    const AmbientState& getState() const { return state_; }
    
    /**
     * @brief Check if it's currently dark
     */
    bool isDark() const { return state_.isDark; }
    
    /**
     * @brief Check if it's cold (below 15°C)
     */
    bool isCold() const { return state_.isCold; }
    
    /**
     * @brief Check if it's hot (above 30°C)
     */
    bool isHot() const { return state_.isHot; }
    
    /**
     * @brief Get current day phase
     */
    DayPhase getDayPhase() const { return state_.dayPhase; }
    
    /**
     * @brief Get day phase name
     */
    static const char* getDayPhaseName(DayPhase phase);
    
    /**
     * @brief Get adaptive brightness based on ambient light
     */
    uint8_t getAdaptiveBrightness() const;
    
    /**
     * @brief Get adaptive glow intensity
     */
    float getAdaptiveGlow() const;
    
    /**
     * @brief Check if owl should be sleepy (late night)
     */
    bool isSleepyTime() const;
    
    /**
     * @brief Check if owl should be active (night owl hours)
     */
    bool isNightOwlTime() const;
    
    /**
     * @brief Get temperature-based animation offset
     * Cold = shivering, Hot = panting
     */
    float getTemperatureAnimation() const;
    
    /**
     * @brief Set light sensor reading (0-4095 for ESP32 ADC)
     */
    void setLightSensorReading(uint16_t rawValue);
    
    /**
     * @brief Set temperature reading
     */
    void setTemperature(float celsius);
    
    /**
     * @brief Update from system time
     */
    void updateTime(uint8_t hour, uint8_t minute);

private:
    AmbientState state_;
    uint32_t lastUpdateTime_ = 0;
    static constexpr uint32_t UPDATE_INTERVAL_MS = 5000;  // 5 seconds
    
    // Thresholds
    static constexpr float DARK_THRESHOLD = 0.2f;
    static constexpr float BRIGHT_THRESHOLD = 0.8f;
    static constexpr float COLD_THRESHOLD = 15.0f;
    static constexpr float HOT_THRESHOLD = 30.0f;
    
    // Private methods
    DayPhase calculateDayPhase(uint8_t hour) const;
    void updateLightLevel();
};

// Global instance
extern AmbientAwareness g_ambient;

} // namespace Avatar

#endif // AMBIENT_AWARENESS_H
