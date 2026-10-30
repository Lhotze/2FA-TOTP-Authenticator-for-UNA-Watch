#ifndef TA_FONT_HPP
#define TA_FONT_HPP

#include <cstdint>

namespace ta {

// A fixed width bitmap font: characters 32 .. 255 (ISO 8859-1), `h` rows each, one byte per row (two bytes, high byte first, when w > 8), the highest bit is the leftmost pixel.
struct BitFont {
    int w, h, bpr;                  // bpr: bytes per row (1 or 2)
    const uint8_t *data;
    const uint8_t *glyph(uint8_t c) const { return c < 32 ? data : data + (c - 32) * h * bpr; }
};

constexpr int kFontCount = 4;
extern const BitFont kFonts[kFontCount];     // 5x8, 6x12, 7x14, 10x20

} // namespace ta

#endif // TA_FONT_HPP
