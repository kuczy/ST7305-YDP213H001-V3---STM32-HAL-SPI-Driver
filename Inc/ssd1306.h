/*
 * ssd1306.h  (minimal stand-in)
 *
 * The real afiskon/stm32-ssd1306 ssd1306.h pulls in a full I2C OLED
 * driver (handles, addressing, HAL I2C calls, etc.) that this project
 * doesn't use or need - we only want its font DATA FORMAT, via
 * ssd1306_fonts.h/.c, to feed the ST7305 driver (see st7305_font.h).
 *
 * This file provides just enough for ssd1306_fonts.h to compile:
 *   - the SSD1306_Font_t struct
 *   - the SSD1306_INCLUDE_FONT_xxx switches, so you control which font
 *     tables actually get compiled in (each one costs flash)
 *
 * Turn on whichever fonts you actually use by uncommenting below
 * (matching whatever font(s) you call ST7305_DrawString/DrawChar with).
 */

#ifndef __SSD1306_H__
#define __SSD1306_H__

#include <stdint.h>
#include <stddef.h>   /* for NULL, used by the {..., NULL} font table entries */

/* ---- pick your font(s) here ------------------------------------- */
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18
//#define SSD1306_INCLUDE_FONT_16x26
//#define SSD1306_INCLUDE_FONT_16x24
//#define SSD1306_INCLUDE_FONT_16x15
/* -------------------------------------------------------------------- */

typedef struct {
    const uint8_t width;
    const uint8_t height;
    const uint16_t *data;
    const uint8_t *char_width; /* NULL for monospace fonts */
} SSD1306_Font_t;

#endif /* __SSD1306_H__ */
