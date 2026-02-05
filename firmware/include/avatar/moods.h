/**
 * @file moods.h
 * @brief Mood state definitions and transitions for procedural avatar
 * 
 * Defines all emotional states, their visual parameters, and
 * transition behaviors between moods.
 */

#ifndef AVATAR_MOODS_H
#define AVATAR_MOODS_H

#include <Arduino.h>
#include "geometry.h"

namespace Avatar {

// =============================================================================
// Mood Enumeration
// =============================================================================

enum class Mood {
    IDLE,           // Default resting state
    LISTENING,      // Paying attention to input
    THINKING,       // Processing/processing
    TOOL_USE,       // Using a tool/function
    SPEAKING,       // Actively speaking
    EXCITED,        // High energy positive
    JUDGING,        // Skeptical/evaluating
    ERROR,          // Something went wrong
    ANCIENT_MODE,   // Mystical ancient wisdom mode
    
    MOOD_COUNT
};

// =============================================================================
// Mood Parameters Structure
// =============================================================================

struct MoodParams {
    // Eye parameters
    float eyeOpenness;          // Base eye openness
    float eyeScaleX;            // Horizontal eye stretch
    float eyeScaleY;            // Vertical eye stretch
    float pupilDilation;        // Pupil size multiplier
    float pupilShimmer;         // Processing shimmer intensity
    
    // Eyebrow parameters
    float eyebrowAngle;         // -1 (sad) to 1 (angry/surprised)
    float eyebrowHeight;        // Vertical position offset
    float eyebrowTension;       // How furrowed (0-1)
    
    // Beak parameters
    float beakOpenness;         // Base beak openness
    float beakTension;          // Tight vs relaxed
    
    // Body parameters
    float breathRate;           // Breaths per second
    float chestExpansion;       // How much chest moves
    float headTilt;             // Head angle offset
    
    // Feather parameters
    float featherRuffle;        // Base ruffle amount
    float earTuftPerk;          // Ear tuft elevation (0-1)
    
    // Effects
    float glowIntensity;        // Eye glow
    float sepiaAmount;          // Ancient mode tint
    float glitchAmount;         // Error shake
    
    // Timing
    uint16_t blinkMinInterval;  // Min ms between blinks
    uint16_t blinkMaxInterval;  // Max ms between blinks
    
    // Constructor with defaults
    MoodParams() :
        eyeOpenness(1.0f),
        eyeScaleX(1.0f),
        eyeScaleY(1.0f),
        pupilDilation(1.0f),
        pupilShimmer(0.0f),
        eyebrowAngle(0.0f),
        eyebrowHeight(0.0f),
        eyebrowTension(0.0f),
        beakOpenness(0.0f),
        beakTension(0.0f),
        breathRate(0.25f),
        chestExpansion(1.0f),
        headTilt(0.0f),
        featherRuffle(0.1f),
        earTuftPerk(0.0f),
        glowIntensity(0.5f),
        sepiaAmount(0.0f),
        glitchAmount(0.0f),
        blinkMinInterval(2000),
        blinkMaxInterval(5000)
    {}
};

// =============================================================================
// Mood Presets
// =============================================================================

namespace MoodPresets {
    
    inline MoodParams getIdle() {
        MoodParams p;
        p.eyeOpenness = 1.0f;
        p.eyeScaleX = 1.0f;
        p.eyeScaleY = 1.0f;
        p.pupilDilation = 1.0f;
        p.eyebrowAngle = 0.0f;
        p.eyebrowHeight = 0.0f;
        p.beakOpenness = 0.0f;
        p.breathRate = 0.25f;
        p.featherRuffle = 0.05f;
        p.earTuftPerk = 0.0f;
        p.glowIntensity = 0.3f;
        p.blinkMinInterval = 2000;
        p.blinkMaxInterval = 5000;
        return p;
    }
    
    inline MoodParams getListening() {
        MoodParams p;
        p.eyeOpenness = 1.0f;
        p.eyeScaleX = 1.05f;
        p.eyeScaleY = 1.05f;
        p.pupilDilation = 0.9f;
        p.eyebrowAngle = 0.1f;
        p.eyebrowHeight = 0.05f;
        p.beakOpenness = 0.05f;
        p.breathRate = 0.3f;
        p.featherRuffle = 0.1f;
        p.earTuftPerk = 0.6f;  // Ears perk up
        p.glowIntensity = 0.5f;
        p.blinkMinInterval = 3000;  // Blinks less when focused
        p.blinkMaxInterval = 6000;
        return p;
    }
    
    inline MoodParams getThinking() {
        MoodParams p;
        p.eyeOpenness = 0.7f;  // Narrowed eyes
        p.eyeScaleX = 1.1f;
        p.eyeScaleY = 0.8f;
        p.pupilDilation = 0.7f;
        p.pupilShimmer = 0.8f;  // Shimmer effect
        p.eyebrowAngle = 0.2f;
        p.eyebrowHeight = 0.1f;
        p.eyebrowTension = 0.3f;
        p.beakOpenness = 0.02f;
        p.beakTension = 0.2f;
        p.breathRate = 0.35f;  // Faster breathing
        p.featherRuffle = 0.15f;
        p.earTuftPerk = 0.3f;
        p.glowIntensity = 0.7f;
        p.blinkMinInterval = 4000;
        p.blinkMaxInterval = 8000;
        return p;
    }
    
    inline MoodParams getToolUse() {
        MoodParams p;
        p.eyeOpenness = 0.9f;
        p.eyeScaleX = 1.0f;
        p.eyeScaleY = 1.0f;
        p.pupilDilation = 0.85f;
        p.eyebrowAngle = -0.1f;
        p.eyebrowHeight = -0.05f;
        p.eyebrowTension = 0.2f;
        p.beakOpenness = 0.15f;  // Slightly open
        p.beakTension = 0.3f;
        p.breathRate = 0.2f;  // Controlled breathing
        p.featherRuffle = 0.08f;
        p.earTuftPerk = 0.2f;
        p.glowIntensity = 0.6f;
        p.blinkMinInterval = 2500;
        p.blinkMaxInterval = 4500;
        return p;
    }
    
    inline MoodParams getSpeaking() {
        MoodParams p;
        p.eyeOpenness = 1.0f;
        p.eyeScaleX = 1.0f;
        p.eyeScaleY = 1.0f;
        p.pupilDilation = 1.0f;
        p.eyebrowAngle = 0.15f;  // Expressive
        p.eyebrowHeight = 0.1f;
        p.breathRate = 0.4f;  // Speaking breathing
        p.featherRuffle = 0.12f;
        p.earTuftPerk = 0.3f;
        p.glowIntensity = 0.6f;
        p.blinkMinInterval = 1500;  // More blinking when talking
        p.blinkMaxInterval = 3000;
        return p;
    }
    
    inline MoodParams getExcited() {
        MoodParams p;
        p.eyeOpenness = 1.2f;  // Wide eyes
        p.eyeScaleX = 1.15f;
        p.eyeScaleY = 1.1f;
        p.pupilDilation = 1.2f;
        p.eyebrowAngle = 0.5f;  // High eyebrows
        p.eyebrowHeight = 0.2f;
        p.beakOpenness = 0.1f;
        p.breathRate = 0.5f;  // Fast breathing
        p.chestExpansion = 1.2f;
        p.featherRuffle = 0.4f;  // Very ruffled
        p.earTuftPerk = 0.8f;
        p.glowIntensity = 0.9f;
        p.blinkMinInterval = 500;  // Rapid blinking
        p.blinkMaxInterval = 1500;
        return p;
    }
    
    inline MoodParams getJudging() {
        MoodParams p;
        p.eyeOpenness = 0.85f;
        p.eyeScaleX = 1.0f;
        p.eyeScaleY = 1.0f;
        p.pupilDilation = 0.8f;
        p.eyebrowAngle = 0.6f;  // One raised
        p.eyebrowHeight = 0.15f;
        p.eyebrowTension = 0.1f;
        p.beakOpenness = 0.0f;
        p.beakTension = 0.1f;
        p.breathRate = 0.15f;  // Slow, deliberate
        p.featherRuffle = 0.05f;
        p.earTuftPerk = 0.1f;
        p.glowIntensity = 0.4f;
        p.blinkMinInterval = 4000;  // Slow judgment blink
        p.blinkMaxInterval = 8000;
        return p;
    }
    
    inline MoodParams getError() {
        MoodParams p;
        p.eyeOpenness = 1.0f;
        p.eyeScaleX = 1.0f;
        p.eyeScaleY = 1.0f;
        p.pupilDilation = 0.5f;  // Small pupils
        p.eyebrowAngle = -0.3f;  // Worried
        p.eyebrowHeight = -0.1f;
        p.beakOpenness = 0.2f;  // Surprised
        p.breathRate = 0.6f;  // Panic breathing
        p.featherRuffle = 0.5f;  // Distressed
        p.earTuftPerk = 0.0f;  // Flat ears
        p.glitchAmount = 1.0f;
        p.glowIntensity = 0.8f;
        p.blinkMinInterval = 500;
        p.blinkMaxInterval = 1000;
        return p;
    }
    
    inline MoodParams getAncientMode() {
        MoodParams p;
        p.eyeOpenness = 0.9f;
        p.eyeScaleX = 1.0f;
        p.eyeScaleY = 1.0f;
        p.pupilDilation = 0.6f;
        p.pupilShimmer = 0.3f;
        p.eyebrowAngle = 0.0f;
        p.eyebrowHeight = 0.0f;
        p.eyebrowTension = 0.0f;
        p.beakOpenness = 0.0f;
        p.beakTension = 0.0f;
        p.breathRate = 0.12f;  // Very slow
        p.chestExpansion = 0.7f;
        p.featherRuffle = 0.02f;  // Still
        p.earTuftPerk = 0.0f;
        p.sepiaAmount = 0.7f;
        p.glowIntensity = 1.0f;  // Strong glow
        p.blinkMinInterval = 5000;  // Very slow blinks
        p.blinkMaxInterval = 10000;
        return p;
    }
    
    inline MoodParams getForMood(Mood mood) {
        switch (mood) {
            case Mood::IDLE: return getIdle();
            case Mood::LISTENING: return getListening();
            case Mood::THINKING: return getThinking();
            case Mood::TOOL_USE: return getToolUse();
            case Mood::SPEAKING: return getSpeaking();
            case Mood::EXCITED: return getExcited();
            case Mood::JUDGING: return getJudging();
            case Mood::ERROR: return getError();
            case Mood::ANCIENT_MODE: return getAncientMode();
            default: return getIdle();
        }
    }
}

// =============================================================================
// Mood Transition
// =============================================================================

class MoodTransition {
public:
    Mood fromMood;
    Mood toMood;
    float progress;  // 0-1
    float duration;  // ms
    
    MoodTransition() : fromMood(Mood::IDLE), toMood(Mood::IDLE), 
                       progress(0), duration(300) {}
    
    void start(Mood from, Mood to, float durationMs = 300) {
        fromMood = from;
        toMood = to;
        progress = 0;
        duration = durationMs;
    }
    
    void update(float deltaMs) {
        if (progress < 1.0f) {
            progress += deltaMs / duration;
            if (progress > 1.0f) progress = 1.0f;
        }
    }
    
    bool isComplete() const { return progress >= 1.0f; }
    
    MoodParams getBlendedParams() const {
        MoodParams from = MoodPresets::getForMood(fromMood);
        MoodParams to = MoodPresets::getForMood(toMood);
        
        float t = Ease::inOutCubic(progress);
        
        MoodParams result;
        result.eyeOpenness = lerp(from.eyeOpenness, to.eyeOpenness, t);
        result.eyeScaleX = lerp(from.eyeScaleX, to.eyeScaleX, t);
        result.eyeScaleY = lerp(from.eyeScaleY, to.eyeScaleY, t);
        result.pupilDilation = lerp(from.pupilDilation, to.pupilDilation, t);
        result.pupilShimmer = lerp(from.pupilShimmer, to.pupilShimmer, t);
        result.eyebrowAngle = lerp(from.eyebrowAngle, to.eyebrowAngle, t);
        result.eyebrowHeight = lerp(from.eyebrowHeight, to.eyebrowHeight, t);
        result.eyebrowTension = lerp(from.eyebrowTension, to.eyebrowTension, t);
        result.beakOpenness = lerp(from.beakOpenness, to.beakOpenness, t);
        result.beakTension = lerp(from.beakTension, to.beakTension, t);
        result.breathRate = lerp(from.breathRate, to.breathRate, t);
        result.chestExpansion = lerp(from.chestExpansion, to.chestExpansion, t);
        result.headTilt = lerp(from.headTilt, to.headTilt, t);
        result.featherRuffle = lerp(from.featherRuffle, to.featherRuffle, t);
        result.earTuftPerk = lerp(from.earTuftPerk, to.earTuftPerk, t);
        result.glowIntensity = lerp(from.glowIntensity, to.glowIntensity, t);
        result.sepiaAmount = lerp(from.sepiaAmount, to.sepiaAmount, t);
        result.glitchAmount = lerp(from.glitchAmount, to.glitchAmount, t);
        
        // Intervals use from mood until near end
        if (t < 0.5f) {
            result.blinkMinInterval = from.blinkMinInterval;
            result.blinkMaxInterval = from.blinkMaxInterval;
        } else {
            result.blinkMinInterval = to.blinkMinInterval;
            result.blinkMaxInterval = to.blinkMaxInterval;
        }
        
        return result;
    }
};

// =============================================================================
// Input Source for Eye Tracking
// =============================================================================

enum class InputSource {
    CENTER,     // Looking straight ahead
    KEYBOARD,   // Looking toward keyboard (slightly down-left)
    MIC,        // Looking toward mic (slightly down-right)
    USER,       // Looking at user (slightly up)
    SIDE_LEFT,  // Side glance left
    SIDE_RIGHT  // Side glance right
};

struct LookTarget {
    Vec2 position;
    float weight;
    
    LookTarget() : position(0, 0), weight(0) {}
    LookTarget(float x, float y, float w) : position(x, y), weight(w) {}
};

namespace LookPositions {
    // Normalized positions (-1 to 1 range)
    inline Vec2 CENTER(0, 0);
    inline Vec2 KEYBOARD(-0.3f, 0.2f);
    inline Vec2 MIC(0.3f, 0.2f);
    inline Vec2 USER(0, -0.2f);
    inline Vec2 SIDE_LEFT(-0.6f, 0);
    inline Vec2 SIDE_RIGHT(0.6f, 0);
    
    inline Vec2 getForSource(InputSource source) {
        switch (source) {
            case InputSource::CENTER: return CENTER;
            case InputSource::KEYBOARD: return KEYBOARD;
            case InputSource::MIC: return MIC;
            case InputSource::USER: return USER;
            case InputSource::SIDE_LEFT: return SIDE_LEFT;
            case InputSource::SIDE_RIGHT: return SIDE_RIGHT;
            default: return CENTER;
        }
    }
}

} // namespace Avatar

#endif // AVATAR_MOODS_H
