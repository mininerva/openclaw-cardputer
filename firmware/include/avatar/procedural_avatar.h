/**
 * @file procedural_avatar.h
 * @brief Main procedural avatar system for OpenClaw Cardputer
 * 
 * Real-time rendered owl avatar with procedural animation,
 * mood states, and interactive behaviors.
 */

#ifndef PROCEDURAL_AVATAR_H
#define PROCEDURAL_AVATAR_H

#include <M5GFX.h>
#include "avatar/geometry.h"
#include "avatar/animation.h"
#include "avatar/moods.h"

namespace Avatar {

// Avatar dimensions
constexpr int16_t AVATAR_SIZE = 128;
constexpr int16_t AVATAR_X = 56;  // Centered: (240-128)/2
constexpr int16_t AVATAR_Y = 0;   // Top of screen

// Component positions (relative to avatar center)
constexpr Vec2 LEFT_EYE_POS(-28, -10);
constexpr Vec2 RIGHT_EYE_POS(28, -10);
constexpr Vec2 BEAK_POS(0, 15);
constexpr Vec2 LEFT_EAR_POS(-45, -35);
constexpr Vec2 RIGHT_EAR_POS(45, -35);

/**
 * @brief Main procedural avatar class
 * 
 * Manages all avatar rendering, animation, and state.
 * Call update() every frame, then render() to draw.
 */
class ProceduralAvatar {
public:
    ProceduralAvatar();
    ~ProceduralAvatar();
    
    // Disable copy
    ProceduralAvatar(const ProceduralAvatar&) = delete;
    ProceduralAvatar& operator=(const ProceduralAvatar&) = delete;
    
    /**
     * @brief Initialize the avatar
     * @param gfx Pointer to M5GFX display
     * @return true if initialized successfully
     */
    bool begin(M5GFX* gfx);
    
    /**
     * @brief Update animation state
     * @param deltaMs Milliseconds since last update
     */
    void update(float deltaMs);
    
    /**
     * @brief Render the avatar to display
     */
    void render();
    
    /**
     * @brief Set current mood
     * @param mood New mood state
     * @param transitionMs Transition duration in milliseconds
     */
    void setMood(Mood mood, float transitionMs = 300);
    
    /**
     * @brief React to device tilt (from IMU)
     * @param tiltX -1.0 to 1.0 (left to right)
     * @param tiltY -1.0 to 1.0 (forward to back)
     */
    void setTilt(float tiltX, float tiltY);
    
    /**
     * @brief Trigger shake reaction
     */
    void onShake();
    
    /**
     * @brief Set sleep mode (face down)
     * @param sleeping true to sleep, false to wake
     */
    void setSleeping(bool sleeping);
    
    /**
     * @brief Set low battery state (hungry owl)
     * @param low true when battery <20%
     */
    void setLowBattery(bool low);
    
    /**
     * @brief Set where the avatar should look
     * @param source Input source to look toward
     */
    void lookAt(InputSource source);
    
    /**
     * @brief Trigger speaking animation
     * @param text Text being spoken (for syllable estimation)
     */
    void speak(const String& text);
    
    /**
     * @brief Stop speaking animation
     */
    void stopSpeaking();
    
    /**
     * @brief Check if currently speaking
     */
    bool isSpeaking() const { return beak_.isSpeaking; }
    
    /**
     * @brief Force a blink
     * @param type Type of blink
     */
    void blink(BlinkType type = BlinkType::SINGLE);
    
    /**
     * @brief Set activity level (affects feather ruffling)
     * @param level 0.0 to 1.0
     */
    void setActivityLevel(float level);
    
    /**
     * @brief Set ancient mode overlay
     * @param enabled True to enable ancient mode effects
     */
    void setAncientMode(bool enabled);
    
    /**
     * @brief Trigger error/glitch effect
     */
    void triggerError();
    
    /**
     * @brief Check if avatar is ready to render
     */
    bool isReady() const { return initialized_; }
    
    /**
     * @brief Get current mood parameters (for external use)
     */
    const MoodParams& getCurrentParams() const { return currentParams_; }
    
    /**
     * @brief Set custom eye glow color
     * @param color RGB565 color
     */
    void setEyeGlowColor(uint16_t color);

private:
    // Display reference
    M5GFX* gfx_ = nullptr;
    bool initialized_ = false;
    
    // State
    Mood currentMood_ = Mood::IDLE;
    Mood previousMood_ = Mood::IDLE;
    MoodTransition moodTransition_;
    MoodParams currentParams_;
    
    // Animation controllers
    BlinkController blink_;
    BreathController breath_{0.25f};
    RuffleController ruffle_;
    BeakController beak_;
    
    // Eye tracking
    InputSource lookTarget_ = InputSource::CENTER;
    Vec2 pupilLeft_;
    Vec2 pupilRight_;
    AnimatedValue pupilX_{0, 0.1f};
    AnimatedValue pupilY_{0, 0.1f};
    
    // Head tilt tracking (from IMU)
    float tiltX_ = 0;
    float tiltY_ = 0;
    
    // Low battery state
    bool lowBattery_ = false;
    uint32_t lowBatteryStartTime_ = 0;
    
    // Timing
    uint32_t lastUpdateTime_ = 0;
    float deltaMs_ = 0;
    
    // Effects
    bool ancientMode_ = false;
    float ancientBlend_ = 0;
    AnimatedValue ancientBlendAnim_{0, 0.5f};
    
    bool errorMode_ = false;
    uint32_t errorStartTime_ = 0;
    float glitchOffset_ = 0;
    
    uint16_t customGlowColor_ = 0;
    bool useCustomGlow_ = false;
    
    // Ancient mode animation phase
    float runePhase_ = 0;
    
    // Battery status
    uint8_t batteryPercent_ = 100;
    uint32_t lastBatteryCheck_ = 0;
    
    // Rendering methods
    void drawBackground();
    void drawBody();
    void drawChest();
    void drawEarTufts();
    void drawLeftEye();
    void drawRightEye();
    void drawBeak();
    void drawEyebrows();
    void drawFeatherDetails();
    void drawAncientOverlay();
    void drawErrorOverlay();
    
    // Component drawing
    void drawEye(const Vec2& pos, float scaleX, float scaleY, 
                  float openness, const Vec2& pupilOffset);
    void drawPupil(int16_t cx, int16_t cy, float size, float shimmer);
    void drawEyebrow(const Vec2& eyePos, float angle, float height, bool left);
    
    // Utility
    void updatePupilPositions();
    Vec2 getGlitchOffset();
    uint16_t getEyeGlowColor() const;
};

/**
 * @brief Global avatar instance
 * 
 * Use this for the main avatar in the application.
 */
extern ProceduralAvatar g_avatar;

} // namespace Avatar

#endif // PROCEDURAL_AVATAR_H
