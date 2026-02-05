/**
 * @file avatar_sensors.h
 * @brief IMU sensor integration for avatar reactions
 * 
 * Uses Cardputer's accelerometer/gyroscope to:
 * - Detect tilt for "curious head tilt" animation
 * - Detect shake for "annoyed" mood
 * - Detect free-fall for "panic" expression
 * - Detect orientation for "sleep when face-down"
 */

#ifndef AVATAR_SENSORS_H
#define AVATAR_SENSORS_H

#include <M5Unified.h>
#include "avatar/procedural_avatar.h"

namespace Avatar {

class AvatarSensors {
public:
    AvatarSensors();
    
    /**
     * @brief Initialize IMU
     */
    bool begin();
    
    /**
     * @brief Update sensor readings (call in loop)
     */
    void update();
    
    /**
     * @brief Get current tilt angle (-1 to 1 for each axis)
     */
    float getTiltX() const { return tilt_x_; }
    float getTiltY() const { return tilt_y_; }
    
    /**
     * @brief Check if device is being shaken
     */
    bool isShaking() const { return shaking_; }
    
    /**
     * @brief Check if device is face-down (sleep mode)
     */
    bool isFaceDown() const { return face_down_; }
    
    /**
     * @brief Check if device is in free-fall
     */
    bool isFreeFall() const { return free_fall_; }
    
    /**
     * @brief Get orientation as string
     */
    const char* getOrientation() const;
    
    /**
     * @brief Get battery level (0-100)
     */
    uint8_t getBatteryLevel() const { return battery_level_; }
    
    /**
     * @brief Check if battery is low (<20%)
     */
    bool isLowBattery() const { return battery_level_ < 20; }

private:
    bool imu_available_;
    
    // Raw sensor data
    float accel_x_, accel_y_, accel_z_;
    float gyro_x_, gyro_y_, gyro_z_;
    
    // Processed data
    float tilt_x_, tilt_y_;
    bool shaking_;
    bool face_down_;
    bool free_fall_;
    
    // Shake detection
    float shake_intensity_;
    uint32_t last_shake_time_;
    
    // Update timing
    uint32_t last_update_;
    
    // Battery
    uint8_t battery_level_;
    uint32_t last_battery_check_;
    
    void readSensors();
    void processOrientation();
    void detectShake();
    void checkBattery();
};

// Global instance
extern AvatarSensors g_sensors;

} // namespace Avatar

#endif // AVATAR_SENSORS_H
