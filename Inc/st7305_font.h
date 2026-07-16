/*
 * st7305_font.h
 *
 * Bridges the popular afiskon/stm32-ssd1306 font format (ssd1306_fonts.h /
 * ssd1306_fonts.c - SSD1306_Font_t) onto the ST7305 driver, without any
 * dependency on the actual SSD1306 display/I2C code. Only the font DATA
 * format and the SSD1306_Font_t type are reused; drawing goes straight
 * into the ST7305 logical framebuffer via ST7305_SetPixel(), exactly as
 * described in st7305.h ("bring your own font").
 *
 * Font glyph layout (as produced by ssd1306_fonts.c):
 *   - one uint16_t per pixel row, MSB-aligned (bit 15 = leftmost column)
 *   - `height` rows per glyph, glyphs stored back-to-back starting at
 *     the space character (0x20)
 *   - monospace fonts (6x8, 7x10, 11x18, 16x26, 16x24): every glyph is
 *     `width` pixels wide, char_width == NULL
 *   - proportional fonts (16x15 / "Font_16x15"): actual per-character
 *     width comes from the separate char_width[] table, `width` in the
 *     struct is just the storage width (16)
 *
 * Usage:
 *   #include "ssd1306_fonts.h"   // for Font_7x10 etc. (needs the right
 *                                 // SSD1306_INCLUDE_FONT_x defined in
 *                                 // your ssd1306_conf.h, as usual)
 *   #include "st7305_font.h"
 *   ...
 *   ST7305_DrawString(&lcd, 5, 5, "Hello!", &Font_7x10, ST7305_COLOR_BLACK);
 *   ST7305_Flush(&lcd);
 */

#ifndef ST7305_FONT_H
#define ST7305_FONT_H

#include "st7305.h"
#include "ssd1306_fonts.h"   /* brings in ssd1306.h for SSD1306_Font_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Draws a single ASCII character (0x20-0x7E) at (x, y) = top-left corner
 * of the glyph cell. Returns the glyph's pixel width (0 if the
 * character is outside the supported range), so callers can advance a
 * cursor without needing to know font internals.
 *
 * Drawing is OPAQUE: every pixel in the glyph's (width x height) cell
 * is written - foreground where the glyph has ink, background (the
 * inverse of `color`) everywhere else. This means redrawing different
 * text at the same (x,y) correctly erases the previous glyph shape,
 * even if the new character is a different shape/width than the old
 * one at that position - no manual clearing needed for the common
 * "update this line of text" case. If your new string is SHORTER than
 * the old one at the same position, the leftover tail beyond where the
 * new string stops drawing will NOT be erased (nothing draws there) -
 * clear that region yourself (e.g. ST7305_ClearAllWhite(), or a
 * SetPixel loop over just that rectangle) before drawing if string
 * length can vary. */
uint8_t ST7305_DrawChar(st7305_t *dev, int16_t x, int16_t y, char ch,
                         const SSD1306_Font_t *font, st7305_color_t color);

/* Draws a NUL-terminated string starting at (x, y), advancing left to
 * right with 1px spacing between glyphs. Does not wrap; characters that
 * would land past ST7305_WIDTH are still drawn (and clipped for free by
 * ST7305_SetPixel's bounds check) but the cursor keeps advancing off-
 * screen, so break your own strings into lines for a multi-line UI. */
void ST7305_DrawString(st7305_t *dev, int16_t x, int16_t y, const char *str,
                        const SSD1306_Font_t *font, st7305_color_t color);

/* Pixel width the string would occupy if drawn with ST7305_DrawString,
 * without actually drawing anything. Handy for centering/right-aligning
 * text before you commit to a position. */
uint16_t ST7305_StringWidth(const char *str, const SSD1306_Font_t *font);

#ifdef __cplusplus
}
#endif

#endif /* ST7305_FONT_H */
