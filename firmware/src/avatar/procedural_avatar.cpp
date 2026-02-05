/**
 * @file procedural_avatar.cpp
 * @brief Main procedural avatar implementation
 */

#include "avatar/procedural_avatar.h"
#include <cmath>

namespace Avatar {

// Global avatar instance
ProceduralAvatar g_avatar;

ProceduralAvatar::ProceduralAvatar() 
    : currentMood_(Mood::IDLE),
      previousMood_(Mood::IDLE) {
}

ProceduralAvatar::~ProceduralAvatar() {
}

bool ProceduralAvatar::begin(M5GFX* gfx) {
    gfx_ = gfx;
    if (!gfx_) return false;
    
    initialized_ = true;
    currentParams_ = MoodPresets::getIdle();
    
    // Initialize pupil positions
    pupilLeft_ = Vec2(0, 0);
    pupilRight_ = Vec2(0, 0);
    
    return true;
}

void ProceduralAvatar::update(float deltaMs) {
    if (!initialized_) return;
    
    deltaMs_ = deltaMs;
    
    // Update mood transition
    moodTransition_.update(deltaMs);
    if (!moodTransition_.isComplete()) {
        currentParams_ = moodTransition_.getBlendedParams();
    } else {
        currentParams_ = MoodPresets::getForMood(currentMood_);
    }
    
    // Update animation controllers
    blink_.update(deltaMs);
    breath_.update(deltaMs);
    ruffle_.update(deltaMs);
    beak_.update(deltaMs);
    
    // Update breathing rate based on mood
    breath_.setRate(currentParams_.breathRate);
    
    // Update ruffle activity
    ruffle_.setActivity(currentParams_.featherRuffle);
    
    // Update blink intervals
    blink_.setBaseInterval(currentParams_.blinkMinInterval, 
                           currentParams_.blinkMaxInterval);
    
    // Update pupil tracking
    updatePupilPositions();
    
    // Update ancient mode blend
    ancientBlendAnim_.update(deltaMs / 1000.0f);
    ancientBlend_ = ancientBlendAnim_.current;
    
    // Update error mode
    if (errorMode_ && millis() - errorStartTime_ > 1000) {
        errorMode_ = false;
    }
    
    // Update rune animation
    runePhase_ += deltaMs * 0.001f;
    
    // Idle micro-animations
    if (currentMood_ == Mood::IDLE) {
        // Occasional ear twitch (random, rare)
        if (random(1000) == 0) {  // 0.1% chance per frame
            // Trigger ear twitch (subtle scale pulse)
            breath_.rate *= 1.5f;
        }
    }
}

void ProceduralAvatar::render() {
    if (!initialized_ || !gfx_) return;
    
    // Apply glitch offset if in error mode
    Vec2 glitch(0, 0);
    if (errorMode_) {
        glitch = getGlitchOffset();
    }
    
    // Draw components in order (back to front)
    drawBackground();
    drawBody();
    drawChest();
    drawEarTufts();
    drawLeftEye();
    drawRightEye();
    drawBeak();
    drawEyebrows();
    drawFeatherDetails();
    
    // Draw overlays
    if (ancientBlend_ > 0) {
        drawAncientOverlay();
    }
    if (errorMode_) {
        drawErrorOverlay();
    }
}

void ProceduralAvatar::setMood(Mood mood, float transitionMs) {
    if (currentMood_ == mood) return;
    
    previousMood_ = currentMood_;
    currentMood_ = mood;
    
    moodTransition_.start(previousMood_, currentMood_, transitionMs);
    
    // Special handling for ancient mode
    if (mood == Mood::ANCIENT_MODE) {
        ancientBlendAnim_.setTarget(1.0f);
    } else if (previousMood_ == Mood::ANCIENT_MODE) {
        ancientBlendAnim_.setTarget(0.0f);
    }
}

void ProceduralAvatar::lookAt(InputSource source) {
    lookTarget_ = source;
}

void ProceduralAvatar::speak(const String& text) {
    beak_.speak(text);
    if (currentMood_ != Mood::SPEAKING) {
        setMood(Mood::SPEAKING, 100);
    }
}

void ProceduralAvatar::stopSpeaking() {
    beak_.stopSpeaking();
    if (currentMood_ == Mood::SPEAKING) {
        setMood(Mood::IDLE, 200);
    }
}

void ProceduralAvatar::blink(BlinkType type) {
    blink_.forceBlink(type);
}

void ProceduralAvatar::setActivityLevel(float level) {
    ruffle_.setActivity(level);
}

void ProceduralAvatar::setAncientMode(bool enabled) {
    if (enabled) {
        setMood(Mood::ANCIENT_MODE, 500);
    } else {
        setMood(Mood::IDLE, 500);
    }
}

void ProceduralAvatar::triggerError() {
    errorMode_ = true;
    errorStartTime_ = millis();
    blink_.forceBlink(BlinkType::GLITCH);
    setMood(Mood::ERROR, 100);
}

void ProceduralAvatar::setEyeGlowColor(uint16_t color) {
    customGlowColor_ = color;
    useCustomGlow_ = true;
}

// =============================================================================
// Rendering Methods
// =============================================================================

void ProceduralAvatar::drawBackground() {
    // Clear avatar area
    gfx_->fillRect(AVATAR_X, AVATAR_Y, AVATAR_SIZE, AVATAR_SIZE, 0x0000);
}

void ProceduralAvatar::drawBody() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2 + 10;
    
    // Main body (oval)
    int16_t bodyW = 70;
    int16_t bodyH = 80;
    
    // Apply breathing
    float breathScale = 1.0f + breath_.intensity * 0.05f * currentParams_.chestExpansion;
    bodyW = (int16_t)(bodyW * breathScale);
    bodyH = (int16_t)(bodyH * breathScale);
    
    // Body color with lighting
    uint16_t bodyColor = Colors::FEATHER_BASE;
    if (ancientBlend_ > 0) {
        bodyColor = lerpColor(bodyColor, 0x8C53, ancientBlend_);
    }
    
    drawFilledEllipse(gfx_, (int16_t)centerX, (int16_t)centerY, 
                      bodyW / 2, bodyH / 2, bodyColor);
    
    // Body highlight
    uint16_t highlightColor = Colors::FEATHER_LIGHT;
    drawFilledEllipse(gfx_, (int16_t)centerX - 10, (int16_t)centerY - 15,
                      bodyW / 4, bodyH / 5, highlightColor);
}

void ProceduralAvatar::drawChest() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2 + 20;
    
    // Chest patch (lighter feathers)
    int16_t chestW = 40;
    int16_t chestH = 50;
    
    // Breathing animation
    float breathScale = 1.0f + breath_.intensity * 0.08f * currentParams_.chestExpansion;
    chestW = (int16_t)(chestW * breathScale);
    chestH = (int16_t)(chestH * breathScale);
    
    uint16_t chestColor = lerpColor(Colors::FEATHER_BASE, Colors::FEATHER_LIGHT, 0.5f);
    
    drawFilledEllipse(gfx_, (int16_t)centerX, (int16_t)centerY,
                      chestW / 2, chestH / 2, chestColor);
    
    // Chest feather texture lines
    for (int i = -2; i <= 2; i++) {
        int16_t lineY = (int16_t)centerY + i * 8;
        gfx_->drawLine((int16_t)centerX - 15, lineY, 
                        (int16_t)centerX + 15, lineY,
                        lerpColor(chestColor, Colors::FEATHER_DARK, 0.3f));
    }
}

void ProceduralAvatar::drawEarTufts() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2;
    float centerY = AVATAR_Y + 35;
    
    // Left ear tuft
    float leftPerk = currentParams_.earTuftPerk;
    float leftAngle = -PI / 3 - leftPerk * 0.3f;
    float leftRuffle = ruffle_.getOffset(0) * (1 + leftPerk);
    
    drawFeatherTuft(gfx_, 
                    centerX - 35, centerY - 10,
                    5, PI / 4, 25,
                    Colors::FEATHER_BASE, leftRuffle);
    
    // Right ear tuft
    float rightAngle = -2 * PI / 3 + leftPerk * 0.3f;
    float rightRuffle = ruffle_.getOffset(PI) * (1 + leftPerk);
    
    drawFeatherTuft(gfx_,
                    centerX + 35, centerY - 10,
                    5, PI / 4, 25,
                    Colors::FEATHER_BASE, rightRuffle);
}

void ProceduralAvatar::drawLeftEye() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2 + LEFT_EYE_POS.x;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2 + LEFT_EYE_POS.y;
    
    // Apply head tilt
    centerY += currentParams_.headTilt * 5;
    
    // Pupil offset with smoothing
    Vec2 pupilOffset(pupilX_.current * 8, pupilY_.current * 6);
    
    drawEye(Vec2(centerX, centerY),
            currentParams_.eyeScaleX,
            currentParams_.eyeScaleY,
            blink_.openness * currentParams_.eyeOpenness,
            pupilOffset);
}

void ProceduralAvatar::drawRightEye() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2 + RIGHT_EYE_POS.x;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2 + RIGHT_EYE_POS.y;
    
    // Apply head tilt
    centerY += currentParams_.headTilt * 5;
    
    // Pupil offset (mirrored slightly for natural look)
    Vec2 pupilOffset(pupilX_.current * 8, pupilY_.current * 6);
    
    drawEye(Vec2(centerX, centerY),
            currentParams_.eyeScaleX,
            currentParams_.eyeScaleY,
            blink_.openness * currentParams_.eyeOpenness,
            pupilOffset);
}

void ProceduralAvatar::drawEye(const Vec2& pos, float scaleX, float scaleY,
                                float openness, const Vec2& pupilOffset) {
    if (openness <= 0.05f) {
        // Closed eye - draw line
        gfx_->drawLine((int16_t)(pos.x - 12 * scaleX), (int16_t)pos.y,
                        (int16_t)(pos.x + 12 * scaleX), (int16_t)pos.y,
                        Colors::FEATHER_DARK);
        return;
    }
    
    // Eye dimensions
    int16_t rx = (int16_t)(14 * scaleX);
    int16_t ry = (int16_t)(16 * scaleY * openness);
    
    // Sclera (white)
    drawFilledEllipse(gfx_, (int16_t)pos.x, (int16_t)pos.y, rx, ry, Colors::EYE_WHITE);
    
    // Eye glow
    uint16_t glowColor = getEyeGlowColor();
    if (currentParams_.glowIntensity > 0) {
        for (int r = 1; r <= 3; r++) {
            uint16_t fadeColor = lerpColor(Colors::EYE_WHITE, glowColor, 
                                           currentParams_.glowIntensity * (1.0f - r * 0.2f));
            drawEllipse(gfx_, (int16_t)pos.x, (int16_t)pos.y, 
                        rx + r, ry + r, fadeColor);
        }
    }
    
    // Pupil
    int16_t pupilX = (int16_t)(pos.x + pupilOffset.x);
    int16_t pupilY = (int16_t)(pos.y + pupilOffset.y);
    int16_t pupilSize = (int16_t)(6 * currentParams_.pupilDilation);
    
    drawPupil(pupilX, pupilY, pupilSize, currentParams_.pupilShimmer);
    
    // Highlight
    gfx_->fillCircle(pupilX - 2, pupilY - 2, 2, Colors::HIGHLIGHT);
    
    // Eyelid (for blinking and expressions)
    if (openness < 0.9f) {
        int16_t lidHeight = (int16_t)(ry * 2 * (1 - openness));
        gfx_->fillRect((int16_t)pos.x - rx - 2, (int16_t)pos.y - ry - 2,
                        rx * 2 + 4, lidHeight, Colors::FEATHER_BASE);
    }
}

void ProceduralAvatar::drawPupil(int16_t cx, int16_t cy, float size, float shimmer) {
    // Main pupil
    gfx_->fillCircle(cx, cy, (int16_t)size, Colors::PUPIL);
    
    // Shimmer effect for thinking/processing
    if (shimmer > 0) {
        float shimmerOffset = std::sin(millis() * 0.01f) * shimmer * 2;
        uint16_t shimmerColor = lerpColor(Colors::PUPIL, getEyeGlowColor(), shimmer * 0.5f);
        gfx_->fillCircle(cx + (int16_t)shimmerOffset, cy, (int16_t)(size * 0.7f), shimmerColor);
    }
}

void ProceduralAvatar::drawBeak() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2 + BEAK_POS.x;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2 + BEAK_POS.y;
    
    // Beak opens based on speaking and mood
    float openAmount = std::max(currentParams_.beakOpenness, beak_.openness);
    
    // Upper beak
    Vec2 upperBase(centerX, centerY - 5);
    Vec2 upperTip(centerX + beak_.tilt * 3, centerY + 15);
    Vec2 upperLeft(centerX - 8, centerY + 2);
    Vec2 upperRight(centerX + 8, centerY + 2);
    
    drawFilledBezier(gfx_, upperLeft, upperBase, upperTip, upperRight, Colors::BEAK_BASE);
    
    // Lower beak (moves when speaking)
    float lowerY = centerY + 5 + openAmount * 8;
    Vec2 lowerBase(centerX, lowerY);
    Vec2 lowerTip(centerX + beak_.tilt * 2, centerY + 12 + openAmount * 5);
    Vec2 lowerLeft(centerX - 6, lowerY - 2);
    Vec2 lowerRight(centerX + 6, lowerY - 2);
    
    drawFilledBezier(gfx_, lowerLeft, lowerBase, lowerTip, lowerRight, Colors::BEAK_TIP);
    
    // Beak line
    gfx_->drawLine((int16_t)centerX - 8, (int16_t)centerY + 2,
                    (int16_t)centerX + 8, (int16_t)centerY + 2,
                    Colors::FEATHER_DARK);
}

void ProceduralAvatar::drawEyebrows() {
    float centerX = AVATAR_X + AVATAR_SIZE / 2;
    
    // Left eyebrow
    Vec2 leftEyePos(centerX + LEFT_EYE_POS.x, AVATAR_Y + AVATAR_SIZE / 2 + LEFT_EYE_POS.y - 20);
    drawEyebrow(leftEyePos, currentParams_.eyebrowAngle, 
                currentParams_.eyebrowHeight, true);
    
    // Right eyebrow
    Vec2 rightEyePos(centerX + RIGHT_EYE_POS.x, AVATAR_Y + AVATAR_SIZE / 2 + RIGHT_EYE_POS.y - 20);
    drawEyebrow(rightEyePos, currentParams_.eyebrowAngle + currentParams_.eyebrowTension * 0.3f,
                currentParams_.eyebrowHeight, false);
}

void ProceduralAvatar::drawEyebrow(const Vec2& eyePos, float angle, float height, bool left) {
    float baseX = eyePos.x;
    float baseY = eyePos.y + height * 10;
    
    float len = 12;
    float thickness = 3;
    
    // Calculate endpoints based on angle
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    
    float x1 = baseX - len * cos_a;
    float y1 = baseY - len * sin_a;
    float x2 = baseX + len * cos_a;
    float y2 = baseY + len * sin_a;
    
    // Draw eyebrow as thick line
    for (float t = 0; t <= 1; t += 0.1f) {
        float px = x1 + (x2 - x1) * t;
        float py = y1 + (y2 - y1) * t;
        gfx_->fillCircle((int16_t)px, (int16_t)py, (int16_t)thickness, Colors::FEATHER_DARK);
    }
}

void ProceduralAvatar::drawFeatherDetails() {
    // Add some detail feathers around the face
    float centerX = AVATAR_X + AVATAR_SIZE / 2;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2;
    
    // Side cheek feathers
    for (int i = 0; i < 3; i++) {
        float angle = PI / 4 + i * PI / 8;
        float ruffle = ruffle_.getOffset(i);
        
        // Left cheek
        drawFeather(gfx_, centerX - 40, centerY + 10 + i * 5,
                    15, angle + ruffle * 0.1f, 4,
                    Colors::FEATHER_LIGHT, ruffle);
        
        // Right cheek
        drawFeather(gfx_, centerX + 40, centerY + 10 + i * 5,
                    15, PI - angle - ruffle * 0.1f, 4,
                    Colors::FEATHER_LIGHT, ruffle);
    }
}

void ProceduralAvatar::drawAncientOverlay() {
    // Sepia tint
    applySepiaTint(gfx_, AVATAR_X, AVATAR_Y, AVATAR_SIZE, AVATAR_SIZE, ancientBlend_);
    
    // Draw floating runes
    float centerX = AVATAR_X + AVATAR_SIZE / 2;
    float centerY = AVATAR_Y + AVATAR_SIZE / 2;
    
    for (int i = 0; i < 3; i++) {
        float angle = runePhase_ + i * 2 * PI / 3;
        float dist = 45 + std::sin(runePhase_ * 2 + i) * 5;
        float rx = centerX + std::cos(angle) * dist;
        float ry = centerY + std::sin(angle) * dist * 0.7f;
        
        drawRune(gfx_, rx, ry, 8, i, Colors::RUNE_GLOW, 
                 ancientBlend_ * (0.5f + 0.5f * std::sin(runePhase_ * 3 + i)));
    }
    
    // Scanlines
    drawScanlines(gfx_, AVATAR_X, AVATAR_Y, AVATAR_SIZE, AVATAR_SIZE, ancientBlend_ * 0.3f);
}

void ProceduralAvatar::drawErrorOverlay() {
    // X-eyes flash
    uint32_t elapsed = millis() - errorStartTime_;
    if ((elapsed / 100) % 2 == 0) {
        float centerX = AVATAR_X + AVATAR_SIZE / 2;
        float centerY = AVATAR_Y + AVATAR_SIZE / 2;
        
        // Draw X over eyes
        uint16_t red = 0xF800;
        int16_t eyeOffset = 28;
        int16_t eyeY = (int16_t)(centerY + LEFT_EYE_POS.y);
        
        // Left X
        gfx_->drawLine((int16_t)(centerX - eyeOffset - 5), eyeY - 5,
                        (int16_t)(centerX - eyeOffset + 5), eyeY + 5, red);
        gfx_->drawLine((int16_t)(centerX - eyeOffset - 5), eyeY + 5,
                        (int16_t)(centerX - eyeOffset + 5), eyeY - 5, red);
        
        // Right X
        gfx_->drawLine((int16_t)(centerX + eyeOffset - 5), eyeY - 5,
                        (int16_t)(centerX + eyeOffset + 5), eyeY + 5, red);
        gfx_->drawLine((int16_t)(centerX + eyeOffset - 5), eyeY + 5,
                        (int16_t)(centerX + eyeOffset + 5), eyeY - 5, red);
    }
}

// =============================================================================
// Utility Methods
// =============================================================================

void ProceduralAvatar::updatePupilPositions() {
    // Get target position based on look source
    Vec2 target = LookPositions::getForSource(lookTarget_);
    
    // Add subtle drift when idle
    if (currentMood_ == Mood::IDLE && lookTarget_ == InputSource::CENTER) {
        float driftX = std::sin(millis() * 0.0005f) * 0.15f;
        float driftY = std::cos(millis() * 0.0007f) * 0.1f;
        target.x += driftX;
        target.y += driftY;
    }
    
    // Smooth toward target
    pupilX_.setTarget(target.x);
    pupilY_.setTarget(target.y);
    
    pupilX_.update(deltaMs_ / 1000.0f);
    pupilY_.update(deltaMs_ / 1000.0f);
}

Vec2 ProceduralAvatar::getGlitchOffset() {
    float shake = std::max(0.0f, 1.0f - (millis() - errorStartTime_) / 1000.0f);
    return Vec2(
        (random(100) / 100.0f - 0.5f) * shake * 4,
        (random(100) / 100.0f - 0.5f) * shake * 4
    );
}

uint16_t ProceduralAvatar::getEyeGlowColor() const {
    if (useCustomGlow_) {
        return customGlowColor_;
    }
    if (ancientBlend_ > 0.5f) {
        return Colors::EYE_GLOW_ANCIENT;
    }
    return Colors::EYE_GLOW;
}

} // namespace Avatar
