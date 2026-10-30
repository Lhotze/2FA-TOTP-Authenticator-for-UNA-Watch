/**
 ******************************************************************************
 * @file    Draw.hpp
 * @brief   Small pixel drawing helpers on the 240 x 240 picture (ABGR2222) and the bitmap fonts.
 ******************************************************************************
 */

#ifndef TA_DRAW_HPP
#define TA_DRAW_HPP

#include <ta/Font.hpp>

#include <cstdint>
#include <cstring>

namespace ta {
namespace draw {

constexpr int kW = 240;
constexpr int kH = 240;

constexpr uint8_t col(int r, int g, int b) { return static_cast<uint8_t>((3 << 6) | ((b / 85) << 4) | ((g / 85) << 2) | (r / 85)); }
constexpr uint8_t BLACK = col(0, 0, 0), WHITE = col(255, 255, 255), TEAL = col(0, 170, 170), TEAL_D = col(0, 85, 85), TEAL_L = col(85, 255, 255);
constexpr uint8_t YELLOW = col(255, 255, 0), ORANGE = col(255, 170, 0), RED = col(255, 85, 85), GREEN = col(85, 255, 85);
constexpr uint8_t GRAY = col(170, 170, 170), GRAY_D = col(85, 85, 85), NAVY = col(0, 0, 85), PAPER = col(255, 255, 170);

inline void px(uint8_t *b, int x, int y, uint8_t c)
{
    if (x >= 0 && y >= 0 && x < kW && y < kH) b[y * kW + x] = c;
}

inline void fillRect(uint8_t *b, int x, int y, int w, int h, uint8_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > kW) w = kW - x;
    if (y + h > kH) h = kH - y;
    for (int j = 0; j < h; ++j) std::memset(b + (y + j) * kW + x, c, w > 0 ? w : 0);
}

inline void frame(uint8_t *b, int x, int y, int w, int h, int t, uint8_t c)
{
    fillRect(b, x, y, w, t, c);
    fillRect(b, x, y + h - t, w, t, c);
    fillRect(b, x, y, t, h, c);
    fillRect(b, x + w - t, y, t, h, c);
}

inline int textWidth(int len, int font) { return len * kFonts[font].w; }

// n characters of an ISO 8859-1 string
inline void text(uint8_t *b, int x, int y, const char *s, int n, int font, uint8_t c)
{
    const BitFont &f = kFonts[font];
    for (int i = 0; i < n; ++i, x += f.w) {
        const uint8_t *g = f.glyph(static_cast<uint8_t>(s[i]));
        for (int r = 0; r < f.h; ++r) {
            const unsigned bits = f.bpr == 2 ? (static_cast<unsigned>(g[2 * r]) << 8) | g[2 * r + 1] : static_cast<unsigned>(g[r]) << 8;
            if (!bits) continue;
            const int yy = y + r;
            if (yy < 0 || yy >= kH) continue;
            for (int k = 0; k < f.w; ++k)
                if (bits & (0x8000u >> k)) px(b, x + k, yy, c);
        }
    }
}

// the letters bigger: every pixel of the font a square of scale x scale
inline void textScaled(uint8_t *b, int x, int y, const char *s, int n, int font, int scale, uint8_t c)
{
    const BitFont &f = kFonts[font];
    for (int i = 0; i < n; ++i, x += f.w * scale) {
        const uint8_t *g = f.glyph(static_cast<uint8_t>(s[i]));
        for (int r = 0; r < f.h; ++r) {
            const unsigned bits = f.bpr == 2 ? (static_cast<unsigned>(g[2 * r]) << 8) | g[2 * r + 1] : static_cast<unsigned>(g[r]) << 8;
            if (!bits) continue;
            for (int k = 0; k < f.w; ++k)
                if (bits & (0x8000u >> k)) fillRect(b, x + k * scale, y + r * scale, scale, scale, c);
        }
    }
}

inline void text(uint8_t *b, int x, int y, const char *s, int font, uint8_t c) { text(b, x, y, s, static_cast<int>(std::strlen(s)), font, c); }

inline void textCentered(uint8_t *b, int cx, int y, const char *s, int font, uint8_t c)
{
    const int n = static_cast<int>(std::strlen(s));
    text(b, cx - n * kFonts[font].w / 2, y, s, n, font, c);
}

} // namespace draw
} // namespace ta

#endif // TA_DRAW_HPP
