/**
 * @file geometry.cpp
 * @brief Geometry primitive implementations
 */

#include "avatar/geometry.h"

namespace Avatar {

void drawFilledCircle(M5GFX* gfx, int16_t cx, int16_t cy, int16_t r, 
                       uint16_t color, uint16_t edgeColor, float edgeWidth) {
    if (!gfx) return;
    
    if (edgeWidth > 0 && edgeColor != 0) {
        // Draw with gradient edge
        for (int16_t y = -r; y <= r; y++) {
            for (int16_t x = -r; x <= r; x++) {
                float dist = std::sqrt(x * x + y * y);
                if (dist <= r) {
                    float edgeStart = r - edgeWidth;
                    float t = (dist > edgeStart) ? (dist - edgeStart) / edgeWidth : 0;
                    t = std::max(0.0f, std::min(1.0f, t));
                    uint16_t c = (t > 0) ? lerpColor(color, edgeColor, t) : color;
                    gfx->drawPixel(cx + x, cy + y, c);
                }
            }
        }
    } else {
        gfx->fillCircle(cx, cy, r, color);
    }
}

void drawAAS Circle(M5GFX* gfx, int16_t cx, int16_t cy, int16_t r, 
                     uint16_t color, float thickness) {
    if (!gfx) return;
    
    // Draw multiple circles for anti-aliasing effect
    gfx->drawCircle(cx, cy, r, color);
    
    if (thickness > 1.0f) {
        for (int i = 1; i < (int)thickness; i++) {
            gfx->drawCircle(cx, cy, r - i, color);
            gfx->drawCircle(cx, cy, r + i, color);
        }
    }
}

void drawEllipse(M5GFX* gfx, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                  uint16_t color, float rotation) {
    if (!gfx) return;
    
    float cos_r = std::cos(rotation);
    float sin_r = std::sin(rotation);
    
    int16_t steps = std::max(rx, ry) * 2;
    float prevX = 0, prevY = 0;
    
    for (int i = 0; i <= steps; i++) {
        float angle = (2 * PI * i) / steps;
        float x = rx * std::cos(angle);
        float y = ry * std::sin(angle);
        
        // Rotate
        float rotX = x * cos_r - y * sin_r;
        float rotY = x * sin_r + y * cos_r;
        
        int16_t px = cx + (int16_t)rotX;
        int16_t py = cy + (int16_t)rotY;
        
        if (i > 0) {
            gfx->drawLine(cx + (int16_t)prevX, cy + (int16_t)prevY, px, py, color);
        }
        
        prevX = rotX;
        prevY = rotY;
    }
}

void drawFilledEllipse(M5GFX* gfx, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                        uint16_t color, float rotation) {
    if (!gfx) return;
    
    // Simple scanline fill
    for (int16_t y = -ry; y <= ry; y++) {
        float yNorm = (float)y / ry;
        float xWidth = rx * std::sqrt(1 - yNorm * yNorm);
        
        int16_t x1 = cx - (int16_t)xWidth;
        int16_t x2 = cx + (int16_t)xWidth;
        
        gfx->drawFastHLine(cx + y, x1, x2 - x1, color);
    }
}

void drawBezier(M5GFX* gfx, const Vec2& p0, const Vec2& p1, const Vec2& p2,
                 uint16_t color, float thickness) {
    if (!gfx) return;
    
    const int steps = 20;
    Vec2 prev = p0;
    
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        float mt = 1 - t;
        
        // Quadratic bezier
        Vec2 curr(
            mt * mt * p0.x + 2 * mt * t * p1.x + t * t * p2.x,
            mt * mt * p0.y + 2 * mt * t * p1.y + t * t * p2.y
        );
        
        gfx->drawLine((int16_t)prev.x, (int16_t)prev.y, 
                       (int16_t)curr.x, (int16_t)curr.y, color);
        prev = curr;
    }
}

void drawFilledBezier(M5GFX* gfx, const Vec2& p0, const Vec2& p1, 
                       const Vec2& p2, const Vec2& p3, uint16_t color) {
    // Simplified: draw as polygon
    const int steps = 10;
    int16_t x[steps * 2];
    int16_t y[steps * 2];
    
    // Top curve (p0 -> p1 -> p2)
    for (int i = 0; i < steps; i++) {
        float t = (float)i / (steps - 1);
        float mt = 1 - t;
        x[i] = (int16_t)(mt * mt * p0.x + 2 * mt * t * p1.x + t * t * p2.x);
        y[i] = (int16_t)(mt * mt * p0.y + 2 * mt * t * p1.y + t * t * p2.y);
    }
    
    // Bottom curve (p0 -> p3 -> p2) - reversed
    for (int i = 0; i < steps; i++) {
        float t = (float)i / (steps - 1);
        float mt = 1 - t;
        x[steps * 2 - 1 - i] = (int16_t)(mt * mt * p0.x + 2 * mt * t * p3.x + t * t * p2.x);
        y[steps * 2 - 1 - i] = (int16_t)(mt * mt * p0.y + 2 * mt * t * p3.y + t * t * p2.y);
    }
    
    gfx->fillPolygon(x, y, steps * 2, color);
}

void drawFeather(M5GFX* gfx, float x, float y, float length, float angle,
                  float width, uint16_t color, float ruffle) {
    if (!gfx) return;
    
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    
    // Ruffle offset
    float ruffleX = std::sin(angle * 3 + ruffle * 10) * ruffle * 2;
    float ruffleY = std::cos(angle * 2 + ruffle * 8) * ruffle * 2;
    
    // Calculate points
    Vec2 base(x + ruffleX, y + ruffleY);
    Vec2 tip(
        base.x + length * cos_a,
        base.y + length * sin_a
    );
    
    // Control points for curved feather
    Vec2 ctrl1(
        base.x + length * 0.3f * cos_a - width * 0.5f * sin_a,
        base.y + length * 0.3f * sin_a + width * 0.5f * cos_a
    );
    Vec2 ctrl2(
        base.x + length * 0.3f * cos_a + width * 0.5f * sin_a,
        base.y + length * 0.3f * sin_a - width * 0.5f * cos_a
    );
    
    // Draw feather shape
    drawFilledBezier(gfx, base, ctrl1, tip, ctrl2, color);
    
    // Draw shaft
    gfx->drawLine((int16_t)base.x, (int16_t)base.y, 
                   (int16_t)tip.x, (int16_t)tip.y, 
                   lerpColor(color, 0x0000, 0.3f));
}

void drawFeatherTuft(M5GFX* gfx, float x, float y, int count, float spread,
                      float length, uint16_t color, float ruffle) {
    if (!gfx) return;
    
    float startAngle = -spread / 2;
    float angleStep = spread / (count - 1);
    
    for (int i = 0; i < count; i++) {
        float angle = startAngle + angleStep * i;
        float featherLength = length * (0.8f + 0.4f * std::sin(i * 0.5f));
        float featherWidth = length * 0.15f;
        
        drawFeather(gfx, x, y, featherLength, angle, featherWidth, color, ruffle);
    }
}

void drawRune(M5GFX* gfx, float x, float y, float size, uint8_t symbol,
               uint16_t color, float glowIntensity) {
    if (!gfx) return;
    
    // Glow effect
    if (glowIntensity > 0) {
        uint16_t glowColor = lerpColor(0x0000, color, glowIntensity * 0.5f);
        for (int r = 1; r <= 3; r++) {
            gfx->drawCircle((int16_t)x, (int16_t)y, (int16_t)(size + r * 2), glowColor);
        }
    }
    
    // Draw rune based on symbol type
    int16_t ix = (int16_t)x;
    int16_t iy = (int16_t)y;
    int16_t s = (int16_t)size;
    
    switch (symbol % 8) {
        case 0: // Circle with vertical line
            gfx->drawCircle(ix, iy, s, color);
            gfx->drawLine(ix, iy - s, ix, iy + s, color);
            break;
        case 1: // Triangle
            gfx->drawLine(ix, iy - s, ix - s, iy + s, color);
            gfx->drawLine(ix - s, iy + s, ix + s, iy + s, color);
            gfx->drawLine(ix + s, iy + s, ix, iy - s, color);
            break;
        case 2: // Cross
            gfx->drawLine(ix - s, iy, ix + s, iy, color);
            gfx->drawLine(ix, iy - s, ix, iy + s, color);
            break;
        case 3: // Diamond
            gfx->drawLine(ix, iy - s, ix + s, iy, color);
            gfx->drawLine(ix + s, iy, ix, iy + s, color);
            gfx->drawLine(ix, iy + s, ix - s, iy, color);
            gfx->drawLine(ix - s, iy, ix, iy - s, color);
            break;
        case 4: // Spiral-ish
            for (int i = 0; i < 20; i++) {
                float a = i * 0.5f;
                float r = s * (i / 20.0f);
                int16_t px = ix + (int16_t)(r * std::cos(a));
                int16_t py = iy + (int16_t)(r * std::sin(a));
                if (i > 0) {
                    gfx->drawPixel(px, py, color);
                }
            }
            break;
        case 5: // Hourglass
            gfx->drawLine(ix - s, iy - s, ix + s, iy + s, color);
            gfx->drawLine(ix + s, iy - s, ix - s, iy + s, color);
            break;
        case 6: // Crescent
            gfx->drawCircle(ix, iy, s, color);
            gfx->fillCircle(ix + s/2, iy, s - 2, 0x0000);
            break;
        case 7: // Star
            for (int i = 0; i < 5; i++) {
                float a1 = (i * 2 * PI / 5) - PI/2;
                float a2 = ((i + 2) * 2 * PI / 5) - PI/2;
                int16_t x1 = ix + (int16_t)(s * std::cos(a1));
                int16_t y1 = iy + (int16_t)(s * std::sin(a1));
                int16_t x2 = ix + (int16_t)(s * std::cos(a2));
                int16_t y2 = iy + (int16_t)(s * std::sin(a2));
                gfx->drawLine(x1, y1, x2, y2, color);
            }
            break;
    }
}

void applySepiaTint(M5GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                     float intensity) {
    if (!gfx || intensity <= 0) return;
    
    // Read-modify-write pixels
    for (int16_t py = y; py < y + h; py++) {
        for (int16_t px = x; px < x + w; px++) {
            uint16_t color = gfx->readPixel(px, py);
            
            // Extract RGB
            uint8_t r = (color >> 11) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t b = (color & 0x1F) << 3;
            
            // Sepia matrix
            uint8_t sr = std::min(255, (int)(r * 0.393f + g * 0.769f + b * 0.189f));
            uint8_t sg = std::min(255, (int)(r * 0.349f + g * 0.686f + b * 0.168f));
            uint8_t sb = std::min(255, (int)(r * 0.272f + g * 0.534f + b * 0.131f));
            
            // Blend
            r = (uint8_t)(r * (1 - intensity) + sr * intensity);
            g = (uint8_t)(g * (1 - intensity) + sg * intensity);
            b = (uint8_t)(b * (1 - intensity) + sb * intensity);
            
            // Convert back to RGB565
            uint16_t newColor = ((r >> 3) << 11) | ((g >> 2) <> 5) | (b >> 3);
            gfx->drawPixel(px, py, newColor);
        }
    }
}

void drawScanlines(M5GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                    float intensity) {
    if (!gfx || intensity <= 0) return;
    
    uint16_t lineColor = gfx->color565(0, 0, 0);
    
    for (int16_t py = y; py < y + h; py += 2) {
        for (int16_t px = x; px < x + w; px++) {
            uint16_t original = gfx->readPixel(px, py);
            uint16_t darkened = lerpColor(original, lineColor, intensity * 0.3f);
            gfx->drawPixel(px, py, darkened);
        }
    }
}

} // namespace Avatar
