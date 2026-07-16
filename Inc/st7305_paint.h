/*
 * st7305_paint.h
 *
 * Basic vector shape drawing (line, rectangle, circle/ellipse) on top of
 * the ST7305 driver's logical framebuffer. Everything here works purely
 * through ST7305_SetPixel() - no SPI traffic, no knowledge of the
 * panel's native RAM layout, no dependency on fonts. Call
 * ST7305_Flush() afterwards as usual to push the result to the panel.
 *
 * All coordinates are in the same LOGICAL (rotation-aware) space as
 * ST7305_SetPixel()/DrawBitmap()/DrawString() - if you called
 * ST7305_SetRotation(), shapes drawn here follow it automatically.
 */

#ifndef ST7305_PAINT_H
#define ST7305_PAINT_H

#include "st7305.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fill behaviour for closed shapes (rectangle, circle/ellipse). In ALL
 * three modes, the outline (of the given `thickness`, in `color`) is
 * always drawn - the modes only differ in what happens to the
 * interior:
 *   TRANSPARENT - interior left untouched (draw a frame/ring around
 *                 whatever's already there).
 *   FILL_WHITE  - interior painted white, i.e. erased, with the
 *                 outline still visible in `color`.
 *   FILL_BLACK  - interior painted solid black, outline still visible
 *                 in `color` (usually you'd pass ST7305_COLOR_BLACK for
 *                 both so the shape reads as one solid blob; pass a
 *                 different combination only if you specifically want
 *                 a visible seam between border and fill, which isn't
 *                 meaningful on a 1bpp display, so black/black is the
 *                 normal choice here).
 */
typedef enum {
    ST7305_FILL_TRANSPARENT = 0,
    ST7305_FILL_WHITE       = 1,
    ST7305_FILL_BLACK       = 2
} st7305_fill_t;

/* Draws a straight line from (x1,y1) to (x2,y2), `thickness` pixels
 * wide (thickness 0 is treated as 1). Uses integer Bresenham for the
 * centerline; thickness > 1 is achieved by stamping a
 * thickness x thickness square at every centerline point, so the line
 * has no gaps at any angle. */
void ST7305_Paint_Line(st7305_t *dev, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        uint8_t thickness, st7305_color_t color);

/* Draws a rectangle whose top-left corner is (x,y), width w, height h.
 * The `thickness`-pixel border in `color` is always drawn; `fill`
 * controls the interior: untouched (TRANSPARENT), painted white
 * (FILL_WHITE - erases whatever was there), or painted black
 * (FILL_BLACK). Thickness is automatically clamped so the border can't
 * exceed half the shorter side. w == 0 or h == 0 does nothing. */
void ST7305_Paint_Rect(st7305_t *dev, int16_t x, int16_t y, uint16_t w, uint16_t h,
                        uint8_t thickness, st7305_fill_t fill, st7305_color_t color);

/* Draws a circle/ellipse inscribed in the bounding box at top-left
 * (x,y), width w, height h (w == h gives a circle; w != h gives an
 * ellipse - handy since text/UI layout is usually done in terms of a
 * bounding box anyway). The `thickness`-pixel ring in `color` is always
 * drawn; `fill` controls the interior: untouched (TRANSPARENT), painted
 * white (FILL_WHITE - erases whatever was there), or painted black
 * (FILL_BLACK). w == 0 or h == 0 does nothing. */
void ST7305_Paint_Circle(st7305_t *dev, int16_t x, int16_t y, uint16_t w, uint16_t h,
                          uint8_t thickness, st7305_fill_t fill, st7305_color_t color);

#ifdef __cplusplus
}
#endif

#endif /* ST7305_PAINT_H */
