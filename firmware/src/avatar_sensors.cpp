/**
 * @file avatar_sensors.cpp
 * @brief IMU sensor implementation
 */

#include "avatar_sensors.h"
#include <cmath>

namespace Avatar {

AvatarSensors g_sensors;

AvatarSensors::AvatarSensors()
    : imu_available_(false),
      accel_x_(0), accel_y_(0), accel_z_(0),
      gyro_x_(0), gyro_y_(0), gyro_z_(0),
      tilt_x_(0), tilt_y_(0),
      shaking_(false), face_down_(false), free_fall_(false),
      shake_intensity_(0), last_shake_time_(0), last_update_(0),
      battery_level_(100), last_battery_check_(0) {
}

bool AvatarSensors::begin() {
    // Check if IMU is available on this device
    auto imu_type = M5.Imu.getType();
    imu_available_ = (imu_type != m5::imu_none);
    
    if (imu_available_) {
        Serial.printf("[Sensors] IMU detected: %d\n", imu_type);
    } else {
        Serial.println("[Sensors] No IMU available");
    }
    
    return imu_available_;
}

void AvatarSensors::update() {
    if (!imu_available_) return;
    
    uint32_t now = millis();
    if (now - last_update_ < 50) return;  // Update at 20Hz max
    last_update_ = now;
    
    readSensors();
    processOrientation();
    detectShake();
    
    // Check battery every 30 seconds
    if (now - last_battery_check_ > 30000) {
        checkBattery();
        last_battery_check_ = now;
    }
}

void AvatarSensors::readSensors() {
    float ax, ay, az;
    float gx, gy, gz;
    
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
        accel_x_ = ax;
        accel_y_ = ay;
        accel_z_ = az;
    }
    
    if (M5.Imu.getGyro(&gx, &gy, &gz)) {
        gyro_x_ = gx;
        gyro_y_ = gy;
        gyro_z_ = gz;
    }
}

void AvatarSensors::processOrientation() {
    // Calculate tilt from accelerometer
    // When flat: Z = 1.0, X = 0, Y = 0
    // When tilted left: X increases
    // When tilted forward: Y increases
    
    tilt_x_ = accel_x_;  // -1 to 1 (approx)
    tilt_y_ = accel_y_;  // -1 to 1 (approx)
    
    // Face down detection (Z negative when flipped)
    face_down_ = (accel_z_ < -0.7f);
    
    // Free-fall detection (all accelerometers near zero)
    float total_accel = std::sqrt(accel_x_*accel_x_ + accel_y_*accel_y_ + accel_z_*accel_z_);
    free_fall_ = (total_accel < 0.3f);
}

void AvatarSensors::detectShake() {
    // Calculate total gyroscopic activity
    float gyro_mag = std::sqrt(gyro_x_*gyro_x_ + gyro_y_*gyro_y_ + gyro_z_*gyro_z_);
    
    // Threshold for shake detection (degrees per second)
    const float SHAKE_THRESHOLD = 500.0f;
    
    if (gyro_mag > SHAKE_THRESHOLD) {
        shake_intensity_ = std::min(shake_intensity_ + 0.2f, 1.0f);
        last_shake_time_ = millis();
    } else {
        shake_intensity_ *= 0.9f;  // Decay
    }
    
    shaking_ = (shake_intensity_ > 0.3f);
}

const char* AvatarSensors::getOrientation() const {
    if (face_down_) return "face_down";
    if (std::abs(tilt_x_) < 0.3f && std::abs(tilt_y_) < 0.3f) return "flat";
    if (tilt_x_ > 0.5f) return "tilted_left";
    if (tilt_x_ < -0.5f) return "tilted_right";
    if (tilt_y_ > 0.5f) return "tilted_forward";
    if (tilt_y_ < -0.5f) return "tilted_back";
    return "level";
}

void AvatarSensors::checkBattery() {
    // Get battery level from M5.Power
    battery_level_ = M5.Power.getBatteryLevel();
    
    // Log low battery
    static bool last_low = false;
    bool now_low = isLowBattery();
    if (now_low && !last_low) {
        Serial.printf("[Sensors] ⚠️ Low battery: %d%%\n", battery_level_);
    }
    last_low = now_low;
}

} // namespace Avatar
