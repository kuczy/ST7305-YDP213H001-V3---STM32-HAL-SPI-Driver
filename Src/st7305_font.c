/*
 * st7305_font.c
 * See st7305_font.h for the format this reads and usage notes.
 */

#include "st7305_font.h"

uint8_t ST7305_DrawChar(st7305_t *dev, int16_t x, int16_t y, char ch,
                         const SSD1306_Font_t *font, st7305_color_t color)
{
    if (ch < 0x20 || ch > 0x7E) {
        return 0; /* unsupported character, e.g. control chars / non-ASCII */
    }

    /* Opaque drawing: every pixel in the glyph's cell (width x height)
     * is written, foreground where the glyph has ink, background
     * (the inverse color) everywhere else. This is what makes
     * redrawing different text over the same spot correctly erase the
     * previous glyph shape - transparent drawing (old behaviour) left
     * background pixels untouched, so a new, differently-shaped
     * character left remnants of the old one visible underneath it. */
    st7305_color_t bg_color = (color == ST7305_COLOR_BLACK) ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;

    uint32_t glyph_index = (uint32_t)(ch - 0x20);
    uint8_t  glyph_width  = font->char_width ? font->char_width[glyph_index] : font->width;
    const uint16_t *glyph = &font->data[glyph_index * font->height];

    for (uint8_t row = 0; row < font->height; row++) {
        uint16_t line = glyph[row];

        for (uint8_t col = 0; col < glyph_width; col++) {
            uint16_t bit_mask = (uint16_t)(0x8000u >> col);
            st7305_color_t pixel_color = (line & bit_mask) ? color : bg_color;
            ST7305_SetPixel(dev, (int16_t)(x + col), (int16_t)(y + row), pixel_color);
        }
    }

    return glyph_width;
}

void ST7305_DrawString(st7305_t *dev, int16_t x, int16_t y, const char *str,
                        const SSD1306_Font_t *font, st7305_color_t color)
{
    int16_t cursor_x = x;

    while (*str != '\0') {
        uint8_t w = ST7305_DrawChar(dev, cursor_x, y, *str, font, color);
        cursor_x = (int16_t)(cursor_x + w + 1); /* +1px spacing between glyphs */
        str++;
    }
}

uint16_t ST7305_StringWidth(const char *str, const SSD1306_Font_t *font)
{
    uint16_t total = 0;

    while (*str != '\0') {
        char ch = *str;
        if (ch >= 0x20 && ch <= 0x7E) {
            uint32_t glyph_index = (uint32_t)(ch - 0x20);
            uint8_t  glyph_width = font->char_width ? font->char_width[glyph_index] : font->width;
            total = (uint16_t)(total + glyph_width + 1);
        }
        str++;
    }

    if (total > 0) {
        total = (uint16_t)(total - 1); /* no trailing spacing after the last glyph */
    }
    return total;
}
