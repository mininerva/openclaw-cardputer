/**
 * @file animation.h
 * @brief Animation utilities for procedural avatar
 * 
 * Easing functions, state blending, and animation controllers
 * for smooth procedural animations.
 */

#ifndef AVATAR_ANIMATION_H
#define AVATAR_ANIMATION_H

#include <Arduino.h>
#include <cmath>
#include "geometry.h"

namespace Avatar {

// =============================================================================
// Easing Functions
// =============================================================================

namespace Ease {
    inline float linear(float t) { return t; }
    
    inline float inQuad(float t) { return t * t; }
    inline float outQuad(float t) { return 1 - (1 - t) * (1 - t); }
    inline float inOutQuad(float t) {
        return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    }
    
    inline float inCubic(float t) { return t * t * t; }
    inline float outCubic(float t) { return 1 - std::pow(1 - t, 3); }
    inline float inOutCubic(float t) {
        return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
    }
    
    inline float inElastic(float t) {
        const float c4 = (2 * PI) / 3;
        if (t == 0) return 0;
        if (t == 1) return 1;
        return -std::pow(2, 10 * t - 10) * std::sin((t * 10 - 10.75f) * c4);
    }
    
    inline float outBounce(float t) {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        
        if (t < 1 / d1) {
            return n1 * t * t;
        } else if (t < 2 / d1) {
            return n1 * (t -= 1.5f / d1) * t + 0.75f;
        } else if (t < 2.5f / d1) {
            return n1 * (t -= 2.25f / d1) * t + 0.9375f;
        } else {
            return n1 * (t -= 2.625f / d1) * t + 0.984375f;
        }
    }
    
    inline float inOutBack(float t) {
        const float c1 = 1.70158f;
        const float c2 = c1 * 1.525f;
        
        if (t < 0.5f) {
            return (std::pow(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2;
        } else {
            return (std::pow(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
        }
    }
}

// =============================================================================
// Animation Value
// =============================================================================

class AnimatedValue {
public:
    float current;
    float target;
    float velocity;
    float smoothTime;
    float maxSpeed;
    
    AnimatedValue(float initial = 0, float smoothTime = 0.1f, float maxSpeed = 1000)
        : current(initial), target(initial), velocity(0), 
          smoothTime(smoothTime), maxSpeed(maxSpeed) {}
    
    void setTarget(float t) { target = t; }
    void setImmediate(float v) { current = target = v; velocity = 0; }
    
    void update(float deltaTime) {
        // Smooth damp algorithm
        float omega = 2.0f / smoothTime;
        float x = omega * deltaTime;
        float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        
        float change = current - target;
        float maxChange = maxSpeed * smoothTime;
        change = std::max(-maxChange, std::min(change, maxChange));
        
        float temp = (velocity + omega * change) * deltaTime;
        velocity = (velocity - omega * temp) * exp;
        current = target + (change + temp) * exp;
    }
};

// =============================================================================
// Blink Controller
// =============================================================================

enum class BlinkType {
    SINGLE,      // Normal single blink
    DOUBLE,      // Two quick blinks
    SLOW,        // Slow judgment blink
    FLUTTER,     // Rapid flutter (excited)
    GLITCH       // Error state X-eyes
};

class BlinkController {
public:
    float openness;  // 0 = closed, 1 = open
    bool isBlinking;
    
    BlinkController() : openness(1), isBlinking(false), 
                        nextBlinkTime_(random(2000, 5000)),
                        blinkType_(BlinkType::SINGLE) {}
    
    void update(float deltaMs) {
        uint32_t now = millis();
        
        if (!isBlinking_) {
            // Check if it's time to blink
            if (now > nextBlinkTime_) {
                startBlink(BlinkType::SINGLE);
            }
        } else {
            // Update blink animation
            updateBlink(deltaMs);
        }
        
        // Apply smoothing
        openness_.update(deltaMs / 1000.0f);
        openness = openness_.current;
    }
    
    void forceBlink(BlinkType type) {
        startBlink(type);
    }
    
    void setBaseInterval(uint16_t minMs, uint16_t maxMs) {
        minInterval_ = minMs;
        maxInterval_ = maxMs;
    }

private:
    AnimatedValue openness_{1, 0.05f};
    uint32_t nextBlinkTime_;
    uint32_t blinkStartTime_;
    BlinkType blinkType_;
    uint8_t blinkPhase_;
    uint16_t minInterval_ = 2000;
    uint16_t maxInterval_ = 5000;
    bool isBlinking_ = false;
    
    void startBlink(BlinkType type) {
        isBlinking_ = true;
        blinkType_ = type;
        blinkStartTime_ = millis();
        blinkPhase_ = 0;
    }
    
    void updateBlink(float deltaMs) {
        uint32_t elapsed = millis() - blinkStartTime_;
        
        switch (blinkType_) {
            case BlinkType::SINGLE:
                updateSingleBlink(elapsed);
                break;
            case BlinkType::DOUBLE:
                updateDoubleBlink(elapsed);
                break;
            case BlinkType::SLOW:
                updateSlowBlink(elapsed);
                break;
            case BlinkType::FLUTTER:
                updateFlutterBlink(elapsed);
                break;
            case BlinkType::GLITCH:
                updateGlitchBlink(elapsed);
                break;
        }
    }
    
    void updateSingleBlink(uint32_t elapsed) {
        const uint16_t duration = 150;
        if (elapsed < duration / 2) {
            openness_.setTarget(0);
        } else if (elapsed < duration) {
            openness_.setTarget(1);
        } else {
            endBlink();
        }
    }
    
    void updateDoubleBlink(uint32_t elapsed) {
        const uint16_t halfDuration = 80;
        uint8_t phase = elapsed / halfDuration;
        
        switch (phase) {
            case 0: openness_.setTarget(0); break;
            case 1: openness_.setTarget(1); break;
            case 2: openness_.setTarget(0); break;
            case 3: openness_.setTarget(1); break;
            default: endBlink(); break;
        }
    }
    
    void updateSlowBlink(uint32_t elapsed) {
        const uint16_t duration = 400;
        float t = (float)elapsed / duration;
        if (t < 0.5f) {
            openness_.setTarget(Ease::inOutCubic(t * 2) * 0.1f);
        } else if (t < 1.0f) {
            openness_.setTarget(Ease::inOutCubic((t - 0.5f) * 2) * 0.9f + 0.1f);
        } else {
            endBlink();
        }
    }
    
    void updateFlutterBlink(uint32_t elapsed) {
        const uint16_t duration = 300;
        float t = (float)elapsed / duration;
        // Rapid sine wave
        float value = (std::sin(t * PI * 8) + 1) * 0.5f;
        openness_.setTarget(0.2f + value * 0.8f);
        
        if (elapsed >= duration) {
            endBlink();
        }
    }
    
    void updateGlitchBlink(uint32_t elapsed) {
        const uint16_t duration = 500;
        if (elapsed < duration) {
            // Random jitter
            openness_.setImmediate(random(0, 100) / 100.0f);
        } else {
            openness_.setTarget(1);
            endBlink();
        }
    }
    
    void endBlink() {
        isBlinking_ = false;
        openness_.setTarget(1);
        nextBlinkTime_ = millis() + random(minInterval_, maxInterval_);
    }
};

// =============================================================================
// Breathing Controller
// =============================================================================

class BreathController {
public:
    float phase;      // 0-2π
    float intensity;  // 0-1 chest expansion
    float rate;       // Breaths per second
    
    BreathController(float rate = 0.25f) 
        : phase(0), intensity(0), rate(rate) {}
    
    void update(float deltaMs) {
        phase += rate * (deltaMs / 1000.0f) * 2 * PI;
        if (phase > 2 * PI) phase -= 2 * PI;
        
        // Sine wave with slight pause at top
        float t = phase / (2 * PI);
        intensity = (std::sin(phase) * 0.5f + 0.5f);
        // Add slight hold at peak
        if (intensity > 0.8f) {
            intensity = 0.8f + (intensity - 0.8f) * 0.5f;
        }
    }
    
    void setRate(float r) { rate = r; }
    void setPhase(float p) { phase = p; }
};

// =============================================================================
// Ruffle Controller
// =============================================================================

class RuffleController {
public:
    float amount;     // Current ruffle amount 0-1
    float activity;   // Activity level 0-1
    
    RuffleController() : amount(0), activity(0), time_(0) {}
    
    void update(float deltaMs) {
        time_ += deltaMs;
        
        // Perlin-like noise using multiple sine waves
        float noise = std::sin(time_ * 0.003f) * 0.5f +
                      std::sin(time_ * 0.007f) * 0.25f +
                      std::sin(time_ * 0.011f) * 0.125f;
        
        // Scale by activity
        amount = (noise + 1) * 0.5f * activity;
    }
    
    void setActivity(float a) { activity = std::max(0.0f, std::min(1.0f, a)); }
    
    float getOffset(float phase) const {
        return std::sin(phase + time_ * 0.01f) * amount * 3.0f;
    }

private:
    float time_;
};

// =============================================================================
// Beak Animation
// =============================================================================

class BeakController {
public:
    float openness;   // 0 = closed, 1 = wide open
    float tilt;       // -1 to 1 (side tilt)
    bool isSpeaking;
    
    BeakController() : openness(0), tilt(0), isSpeaking(false),
                       syllableIndex_(0), speakStartTime_(0) {}
    
    void update(float deltaMs) {
        if (isSpeaking_) {
            updateSpeaking(deltaMs);
        } else {
            // Return to closed
            openness_.setTarget(0);
            tilt_.setTarget(0);
        }
        
        openness_.update(deltaMs / 1000.0f);
        tilt_.update(deltaMs / 1000.0f);
        
        openness = openness_.current;
        tilt = tilt_.current;
    }
    
    void speak(const String& text) {
        isSpeaking_ = true;
        speakStartTime_ = millis();
        syllableIndex_ = 0;
        text_ = text;
        syllableCount_ = countSyllables(text);
    }
    
    void stopSpeaking() {
        isSpeaking_ = false;
    }

private:
    AnimatedValue openness_{0, 0.03f};
    AnimatedValue tilt_{0, 0.05f};
    String text_;
    uint32_t speakStartTime_;
    uint8_t syllableIndex_;
    uint8_t syllableCount_;
    bool isSpeaking_ = false;
    
    void updateSpeaking(float deltaMs) {
        uint32_t elapsed = millis() - speakStartTime_;
        
        // Estimate syllable timing (average 150ms per syllable)
        const uint16_t msPerSyllable = 150;
        uint8_t currentSyllable = elapsed / msPerSyllable;
        
        if (currentSyllable >= syllableCount_) {
            // End of speech
            isSpeaking_ = false;
            return;
        }
        
        // Animate beak based on syllable position
        float syllableProgress = (float)(elapsed % msPerSyllable) / msPerSyllable;
        
        // Open on syllable start, close in middle
        float targetOpenness;
        if (syllableProgress < 0.3f) {
            targetOpenness = Ease::outQuad(syllableProgress / 0.3f) * 0.7f;
        } else if (syllableProgress < 0.7f) {
            targetOpenness = 0.7f - Ease::inQuad((syllableProgress - 0.3f) / 0.4f) * 0.7f;
        } else {
            targetOpenness = 0;
        }
        
        openness_.setTarget(targetOpenness);
        
        // Slight tilt variation for expressiveness
        float targetTilt = std::sin(currentSyllable * 1.5f) * 0.3f;
        tilt_.setTarget(targetTilt);
    }
    
    uint8_t countSyllables(const String& text) {
        // Simple syllable counting heuristic
        uint8_t count = 0;
        bool lastWasVowel = false;
        
        for (size_t i = 0; i < text.length(); i++) {
            char c = tolower(text[i]);
            bool isVowel = (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y');
            
            if (isVowel && !lastWasVowel) {
                count++;
            }
            lastWasVowel = isVowel;
        }
        
        return std::max((uint8_t)1, count);
    }
};

} // namespace Avatar

#endif // AVATAR_ANIMATION_H
