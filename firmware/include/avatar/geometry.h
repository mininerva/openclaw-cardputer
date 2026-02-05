/**
 * @file geometry.h
 * @brief Procedural geometry primitives for avatar rendering
 * 
 * Low-level drawing primitives for eyes, beak, feathers, and other
 * avatar components. All shapes drawn procedurally with anti-aliasing
 * where possible.
 */

#ifndef AVATAR_GEOMETRY_H
#define AVATAR_GEOMETRY_H

#include <M5GFX.h>
#include <cmath>

namespace Avatar {

// Color palette for avatar
namespace Colors {
    constexpr uint16_t FEATHER_BASE = 0x5A6B;      // Dark slate blue-gray
    constexpr uint16_t FEATHER_LIGHT = 0x8C73;     // Lighter feather highlight
    constexpr uint16_t FEATHER_DARK = 0x3128;      // Shadow areas
    constexpr uint16_t BEAK_BASE = 0xEBA0;         // Golden amber
    constexpr uint16_t BEAK_TIP = 0xC480;          // Darker beak tip
    constexpr uint16_t EYE_WHITE = 0xFFFF;         // Sclera
    constexpr uint16_t EYE_GLOW = 0x07FF;          // Cyan glow (normal)
    constexpr uint16_t EYE_GLOW_ANCIENT = 0xFD20;  // Amber glow (ancient mode)
    constexpr uint16_t PUPIL = 0x1082;             // Dark pupil
    constexpr uint16_t HIGHLIGHT = 0xFFFF;         // Specular highlight
    constexpr uint16_t BLUSH = 0xC9E8;             // Pink blush
    constexpr uint16_t RUNE_GLOW = 0x87F0;         // Ancient rune color
}

// Utility functions
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline uint16_t lerpColor(uint16_t c1, uint16_t c2, float t) {
    // Extract RGB565 components
    uint8_t r1 = (c1 >> 11) & 0x1F;
    uint8_t g1 = (c1 >> 5) & 0x3F;
    uint8_t b1 = c1 & 0x1F;
    
    uint8_t r2 = (c2 >> 11) & 0x1F;
    uint8_t g2 = (c2 >> 5) & 0x3F;
    uint8_t b2 = c2 & 0x1F;
    
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
    
    return (r << 11) | (g << 5) | b;
}

inline float smoothstep(float edge0, float edge1, float x) {
    float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

// 2D Vector for positions
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
    
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    
    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const {
        float len = length();
        if (len < 0.0001f) return Vec2(0, 0);
        return Vec2(x / len, y / len);
    }
    
    Vec2 lerpTo(const Vec2& target, float t) const {
        return Vec2(lerp(x, target.x, t), lerp(y, target.y, t));
    }
};

/**
 * @brief Draw a filled circle with optional gradient
 */
void drawFilledCircle(M5GFX* gfx, int16_t cx, int16_t cy, int16_t r, 
                       uint16_t color, uint16_t edgeColor = 0, float edgeWidth = 0);

/**
 * @brief Draw an anti-aliased circle outline
 */
void drawAAS Circle(M5GFX* gfx, int16_t cx, int16_t cy, int16_t r, 
                     uint16_t color, float thickness = 1.0f);

/**
 * @brief Draw an ellipse (procedural)
 */
void drawEllipse(M5GFX* gfx, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                  uint16_t color, float rotation = 0);

/**
 * @brief Draw a filled ellipse
 */
void drawFilledEllipse(M5GFX* gfx, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                        uint16_t color, float rotation = 0);

/**
 * @brief Draw a bezier curve
 */
void drawBezier(M5GFX* gfx, const Vec2& p0, const Vec2& p1, const Vec2& p2,
                 uint16_t color, float thickness = 1.0f);

/**
 * @brief Draw a filled bezier shape
 */
void drawFilledBezier(M5GFX* gfx, const Vec2& p0, const Vec2& p1, const Vec2& p2,
                       const Vec2& p3, uint16_t color);

/**
 * @brief Draw a procedural feather
 * @param gfx Display
 * @param x Base X position
 * @param y Base Y position
 * @param length Feather length
 * @param angle Rotation angle in radians
 * @param width Feather width at base
 * @param color Base color
 * @param ruffle Amount of ruffle (0-1)
 */
void drawFeather(M5GFX* gfx, float x, float y, float length, float angle,
                  float width, uint16_t color, float ruffle = 0);

/**
 * @brief Draw multiple feathers as a tuft
 */
void drawFeatherTuft(M5GFX* gfx, float x, float y, int count, float spread,
                      float length, uint16_t color, float ruffle = 0);

/**
 * @brief Draw a glowing rune symbol (ancient mode)
 */
void drawRune(M5GFX* gfx, float x, float y, float size, uint8_t symbol,
               uint16_t color, float glowIntensity);

/**
 * @brief Apply sepia tint to a region (ancient mode)
 */
void applySepiaTint(M5GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                     float intensity);

/**
 * @brief Draw scanline effect (ancient/glitch mode)
 */
void drawScanlines(M5GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                    float intensity);

} // namespace Avatar

#endif // AVATAR_GEOMETRY_H
