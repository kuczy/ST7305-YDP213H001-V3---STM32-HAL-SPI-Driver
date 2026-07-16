/*
 * st7305_paint.c
 * See st7305_paint.h for API docs.
 */

#include "st7305_paint.h"
#include <math.h>

/* ---------------------------------------------------------------------- */
/* Line                                                                     */
/* ---------------------------------------------------------------------- */

static void st7305_stamp_square(st7305_t *dev, int16_t cx, int16_t cy,
                                 uint8_t thickness, st7305_color_t color)
{
    int16_t half_lo = (int16_t)(thickness / 2);
    int16_t half_hi = (int16_t)(thickness - 1 - half_lo); /* handles even thickness */

    for (int16_t dy = (int16_t)-half_lo; dy <= half_hi; dy++) {
        for (int16_t dx = (int16_t)-half_lo; dx <= half_hi; dx++) {
            ST7305_SetPixel(dev, (int16_t)(cx + dx), (int16_t)(cy + dy), color);
        }
    }
}

void ST7305_Paint_Line(st7305_t *dev, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        uint8_t thickness, st7305_color_t color)
{
    if (thickness == 0) {
        thickness = 1;
    }

    int32_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int32_t dy = (y2 > y1) ? (y1 - y2) : (y2 - y1); /* note: negative-or-zero, per Bresenham convention */
    int32_t sx = (x1 < x2) ? 1 : -1;
    int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx + dy;

    int16_t cx = x1, cy = y1;

    for (;;) {
        if (thickness <= 1) {
            ST7305_SetPixel(dev, cx, cy, color);
        } else {
            st7305_stamp_square(dev, cx, cy, thickness, color);
        }

        if (cx == x2 && cy == y2) {
            break;
        }

        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx = (int16_t)(cx + sx); }
        if (e2 <= dx) { err += dx; cy = (int16_t)(cy + sy); }
    }
}

/* ---------------------------------------------------------------------- */
/* Rectangle                                                                */
/* ---------------------------------------------------------------------- */

void ST7305_Paint_Rect(st7305_t *dev, int16_t x, int16_t y, uint16_t w, uint16_t h,
                        uint8_t thickness, st7305_fill_t fill, st7305_color_t color)
{
    if (w == 0 || h == 0) {
        return;
    }
    if (thickness == 0) {
        thickness = 1;
    }

    uint16_t shorter_side = (w < h) ? w : h;
    uint16_t max_thickness = shorter_side / 2u;
    if (max_thickness == 0) {
        max_thickness = 1;
    }
    if (thickness > max_thickness) {
        thickness = (uint8_t)max_thickness;
    }

    for (int16_t yy = y; yy < (int16_t)(y + h); yy++) {
        for (int16_t xx = x; xx < (int16_t)(x + w); xx++) {
            bool on_border = (xx < (int16_t)(x + thickness)) ||
                              (xx >= (int16_t)(x + w - thickness)) ||
                              (yy < (int16_t)(y + thickness)) ||
                              (yy >= (int16_t)(y + h - thickness));

            if (on_border) {
                ST7305_SetPixel(dev, xx, yy, color);
            } else if (fill == ST7305_FILL_WHITE) {
                ST7305_SetPixel(dev, xx, yy, ST7305_COLOR_WHITE);
            } else if (fill == ST7305_FILL_BLACK) {
                ST7305_SetPixel(dev, xx, yy, ST7305_COLOR_BLACK);
            }
            /* ST7305_FILL_TRANSPARENT: interior left untouched */
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Circle / ellipse                                                         */
/* ---------------------------------------------------------------------- */

void ST7305_Paint_Circle(st7305_t *dev, int16_t x, int16_t y, uint16_t w, uint16_t h,
                          uint8_t thickness, st7305_fill_t fill, st7305_color_t color)
{
    if (w == 0 || h == 0) {
        return;
    }
    if (thickness == 0) {
        thickness = 1;
    }

    float rx = (float)w / 2.0f;
    float ry = (float)h / 2.0f;
    float cx = (float)x + rx;
    float cy = (float)y + ry;

    float inner_rx = rx - (float)thickness;
    float inner_ry = ry - (float)thickness;
    bool has_inner = (inner_rx > 0.0f) && (inner_ry > 0.0f);

    for (int16_t yy = y; yy < (int16_t)(y + h); yy++) {
        for (int16_t xx = x; xx < (int16_t)(x + w); xx++) {
            float nx = ((float)xx + 0.5f - cx) / rx;
            float ny = ((float)yy + 0.5f - cy) / ry;
            bool inside_outer = (nx * nx + ny * ny) <= 1.0f;
            if (!inside_outer) {
                continue;
            }

            bool inside_inner = false;
            if (has_inner) {
                float nx2 = ((float)xx + 0.5f - cx) / inner_rx;
                float ny2 = ((float)yy + 0.5f - cy) / inner_ry;
                inside_inner = (nx2 * nx2 + ny2 * ny2) <= 1.0f;
            }

            if (!inside_inner) {
                /* border ring */
                ST7305_SetPixel(dev, xx, yy, color);
            } else if (fill == ST7305_FILL_WHITE) {
                ST7305_SetPixel(dev, xx, yy, ST7305_COLOR_WHITE);
            } else if (fill == ST7305_FILL_BLACK) {
                ST7305_SetPixel(dev, xx, yy, ST7305_COLOR_BLACK);
            }
            /* ST7305_FILL_TRANSPARENT: interior left untouched */
        }
    }
}
