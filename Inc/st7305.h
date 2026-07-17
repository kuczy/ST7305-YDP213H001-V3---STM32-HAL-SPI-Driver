/*
 * st7305.h
 *
 * Driver for Sitronix ST7305 ultra-low-power reflective LCD controller
 * over SPI, using STM32 HAL.
 *
 * Verified panel geometry: 2.13" 122 x 250 (matches YDP213H001-class panels).
 * Init sequence below is a known-working sequence for this exact
 * controller/panel combination (register values confirmed against a
 * working factory-derived init on real hardware), NOT the generic
 * datasheet example, which does not work on all 2.13" 122x250 units.
 *
 * DESIGN
 * ------
 * The ST7305 stores 1 bpp pixels in an unusual "2 pixels per nibble,
 * odd/even row interleaved" RAM layout. Rather than exposing that mess to
 * the caller, this driver keeps a normal, byte-per-8-pixels "logical"
 * framebuffer (MSB-first, row major - the same layout used by u8g2,
 * Adafruit GFX glyphs, and most bitmap/font tools). ST7305_Flush()
 * re-packs the logical buffer into the controller's native layout and
 * transmits it. This means any font renderer or bitmap converter that
 * outputs standard 1bpp row-major bytes can be wired in directly:
 * just call ST7305_DrawBitmap() / ST7305_SetPixel() from your font code.
 *
 * FONTS
 * -----
 * This driver intentionally contains NO font data or glyph rendering.
 * Bring your own font table + a small "draw_glyph" function that calls
 * ST7305_SetPixel() (or blits into a bitmap and calls ST7305_DrawBitmap()).
 * Any font format that resolves to (width, height, per-row bytes MSB-first)
 * works without modification, e.g. u8g2 fonts, Adafruit GFX fonts (after
 * their bit-packing is unpacked to bytes), or a plain generated .h array.
 */

#ifndef ST7305_H
#define ST7305_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l0xx_hal.h"   /* change to match your STM32 family, e.g. stm32l4xx_hal.h */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Panel geometry                                                          */
/* ---------------------------------------------------------------------- */
#define ST7305_WIDTH            122u
#define ST7305_HEIGHT           250u

/* Physical RAM layout constants (from verified working init/memory map) */
#define ST7305_COL_START        0x19
#define ST7305_COL_END          0x23
#define ST7305_ROW_START        0x00
#define ST7305_ROW_END          0x7C
#define ST7305_BYTES_PER_ROW    33u                 /* physical bytes per RAM row   */
#define ST7305_RAM_ROWS         (ST7305_HEIGHT / 2) /* two logical rows share 1 RAM row */
#define ST7305_RAM_BUF_SIZE     (ST7305_BYTES_PER_ROW * ST7305_RAM_ROWS) /* 4125 */

/* Logical (caller-facing) framebuffer: standard 1bpp, MSB-first, row-major */
#define ST7305_FB_STRIDE        ((ST7305_WIDTH + 7u) / 8u)  /* bytes per row = 16 */
#define ST7305_FB_SIZE          (ST7305_FB_STRIDE * ST7305_HEIGHT)

typedef enum {
    ST7305_COLOR_WHITE = 0,   /* pixel "off" - reflective, no ink */
    ST7305_COLOR_BLACK = 1    /* pixel "on"  */
} st7305_color_t;

/* Screen orientation. Values are literal degrees (clockwise), so they
 * can be compared/printed directly. 360 is accepted everywhere a
 * rotation is passed in and is treated identically to 0. */
typedef enum {
    ST7305_ROTATION_0   = 0,
    ST7305_ROTATION_90  = 90,
    ST7305_ROTATION_180 = 180,
    ST7305_ROTATION_270 = 270
} st7305_rotation_t;

typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cs_port;
    uint16_t      cs_pin;

    GPIO_TypeDef *dc_port;
    uint16_t      dc_pin;

    GPIO_TypeDef *rst_port;
    uint16_t      rst_pin;

    st7305_rotation_t rotation;

    bool auto_power_cycle; /* see ST7305_SetAutoPowerCycle() */

    /* Dirty-rectangle tracking (physical coordinates, inclusive bounds),
     * maintained automatically by ST7305_SetPixel(). ST7305_Flush() only
     * transmits the rows this bounding box touches. See ST7305_Flush(). */
    bool     dirty;
    uint16_t dirty_x0, dirty_y0, dirty_x1, dirty_y1;

    uint8_t fb[ST7305_FB_SIZE];   /* logical framebuffer, caller-visible,
                                    * always stored in native (0 deg)
                                    * physical panel orientation - the
                                    * rotation is applied as a coordinate
                                    * transform in SetPixel/GetPixel, not
                                    * by physically rotating this buffer */
} st7305_t;

/* ---------------------------------------------------------------------- */
/* Lifecycle                                                               */
/* ---------------------------------------------------------------------- */

/* Fills in dev with the given handles/pins and performs full panel init.
 * Call once after hspi and the CS/DC/RST GPIOs are already configured
 * as outputs by CubeMX/HAL. Blocking; takes a few hundred ms due to the
 * panel's power-up delay.
 *
 * rotation_degrees: one of 0, 90, 180, 270, 360 (360 == 0). Any other
 * value falls back to 0. This sets the initial orientation; use
 * ST7305_GetWidth()/ST7305_GetHeight() afterwards instead of the raw
 * ST7305_WIDTH/ST7305_HEIGHT macros if you want your layout code to
 * automatically follow the current rotation (those macros always mean
 * the physical panel, not the logical/rotated view). */
void ST7305_Init(st7305_t *dev,
                  SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                  GPIO_TypeDef *dc_port,  uint16_t dc_pin,
                  GPIO_TypeDef *rst_port, uint16_t rst_pin,
                  uint16_t rotation_degrees);

/* Changes orientation after Init(). Same accepted values as above
 * (0/90/180/270/360). Only changes how subsequent SetPixel/GetPixel/
 * DrawBitmap/DrawString calls map logical->physical coordinates - it
 * does NOT touch what's already in the framebuffer or on the panel, so
 * old content will look "wrong" (rotated) until you ST7305_ClearBuffer()
 * + redraw + re-flush. */
void ST7305_SetRotation(st7305_t *dev, uint16_t rotation_degrees);

/* Logical width/height for the CURRENT rotation: at 0/180 this equals
 * ST7305_WIDTH/ST7305_HEIGHT; at 90/270 they're swapped. Use these
 * instead of the raw macros so layout code automatically adapts when
 * you change rotation. */
uint16_t ST7305_GetWidth(const st7305_t *dev);
uint16_t ST7305_GetHeight(const st7305_t *dev);

/* Hardware reset pulse only (used internally by Init, exposed in case
 * you need to recover from a fault without a full re-init). */
void ST7305_HWReset(st7305_t *dev);

/* ---------------------------------------------------------------------- */
/* Low power / deep sleep                                                  */
/* ---------------------------------------------------------------------- */

/* Puts the panel into MIPI DCS Idle Mode: a lower-power drive scheme
 * that KEEPS the image visible (unlike SLPIN/DISPOFF - see
 * ST7305_PowerDown() below - which stops pixel drive entirely and lets
 * the liquid crystal relax, blanking the screen). This is the one to
 * use before putting the STM32 into Stop/Standby if you want the
 * picture to still be there when you wake up. */
void ST7305_Sleep(st7305_t *dev);

/* Reverses ST7305_Sleep(): back to full/normal drive mode. RAM content
 * was never lost, so no re-Flush() is required, only a re-Flush() if
 * you drew new content while asleep. */
void ST7305_WakeUp(st7305_t *dev);

/* Hard power-down: SLPIN + DISPOFF. Stops the panel's internal
 * common/segment drive entirely - the liquid crystal relaxes and the
 * image WILL disappear. Only use this if you don't need the picture
 * retained, or right before physically cutting power to the panel
 * (VCI/IOVCC). After a long power-down you may need a full
 * ST7305_Init() again rather than ST7305_PowerUp() to resynchronize. */
void ST7305_PowerDown(st7305_t *dev);
void ST7305_PowerUp(st7305_t *dev);

/* Controls whether ST7305_Flush() automatically wraps itself in
 * ST7305_WakeUp() ... ST7305_Sleep() (Idle Mode - see above). Enabled
 * by default (set in ST7305_Init()) for lower average current draw
 * while keeping the image visible between updates. Costs ~15-20ms of
 * extra latency per Flush() (Idle Mode settling delays) on top of the
 * SPI transfer time. Call ST7305_SetAutoPowerCycle(dev, false) if you
 * want the panel to stay continuously in full-power/non-idle mode
 * instead (lower per-Flush latency, slightly higher average current
 * draw, no functional difference to what's on screen). */
void ST7305_SetAutoPowerCycle(st7305_t *dev, bool enable);

/* ---------------------------------------------------------------------- */
/* Drawing (operates on the logical framebuffer only, no SPI traffic)      */
/* ---------------------------------------------------------------------- */

/* Clears the logical framebuffer to white and marks the whole physical
 * panel dirty (so the next ST7305_Flush() pushes a full blank frame).
 * Equivalent to ST7305_ClearAllWhite() below - kept for compatibility
 * with earlier code. */
void ST7305_ClearBuffer(st7305_t *dev);

/* Fills the entire framebuffer white/black and marks the whole panel
 * dirty. Use these instead of looping SetPixel over every coordinate
 * when you want to blank or invert the whole screen - much faster
 * (a single memset) and guarantees a full-frame refresh regardless of
 * what the dirty-tracking would otherwise have picked up. */
void ST7305_ClearAllWhite(st7305_t *dev);
void ST7305_ClearAllBlack(st7305_t *dev);

void ST7305_SetPixel(st7305_t *dev, int16_t x, int16_t y, st7305_color_t color);
st7305_color_t ST7305_GetPixel(st7305_t *dev, int16_t x, int16_t y);

/* Blits a standard 1bpp MSB-first, row-major bitmap (stride = (w+7)/8
 * bytes/row) into the framebuffer at (x, y). This is the same format
 * u8g2 XBM/glyphs and most "image2cpp"-style converters produce, so it
 * doubles as your bitmap AND your font glyph blitter: render each glyph
 * to a small bitmap (or call SetPixel per glyph pixel) and pass it here.
 *
 * Drawing is OPAQUE: every pixel in the w x h footprint is written -
 * black where the bitmap has a set bit, white everywhere else - so
 * redrawing a bitmap over old content (text, another bitmap, shapes)
 * always fully replaces it, no remnants left behind. If you draw a
 * SMALLER bitmap over a spot that previously held a LARGER one, the
 * area outside the new bitmap's footprint isn't touched - clear that
 * region yourself first if needed (e.g. ST7305_ClearAllWhite() or a
 * SetPixel/Paint_Rect loop over just that rectangle). */
void ST7305_DrawBitmap(st7305_t *dev, int16_t x, int16_t y,
                        uint8_t w, uint8_t h, const uint8_t *bitmap);

/* ---------------------------------------------------------------------- */
/* Transfer to panel                                                       */
/* ---------------------------------------------------------------------- */

/* PARTIAL UPDATE: transmits only the rows that changed since the last
 * Flush() (tracked automatically every time SetPixel/DrawBitmap/
 * DrawString actually changes a pixel - a redraw of unchanged content,
 * e.g. a digit that stayed the same, does NOT get re-sent). This is
 * what fixes the "fast flicker while writing" you saw under the
 * microscope: less data over SPI per update means a much shorter
 * window where the panel's internal scan can catch content mid-write.
 *
 * Column addressing is always sent full-width (a conservative choice -
 * only the confirmed, verified row-addressing math is used to narrow
 * the window; narrowing columns too would need further hardware
 * verification of the column address encoding before it'd be safe to
 * rely on). Row granularity is 2 physical scanlines (the panel's native
 * RAM row pairing), so a 1px-tall change may cause a 2-row update.
 *
 * If nothing changed since the last Flush(), this returns immediately
 * (not even a WakeUp()/Sleep() cycle) - safe and cheap to call
 * "defensively" after any drawing code.
 *
 * If ST7305_SetAutoPowerCycle() is enabled (default), this also wakes
 * the panel up beforehand and puts it back into Idle Mode afterwards. */
void ST7305_Flush(st7305_t *dev);

/* Forces a full-frame refresh regardless of dirty-tracking state.
 * Needed after ST7305_ClearAllWhite()/Black() (those already mark the
 * whole panel dirty, so a plain ST7305_Flush() works too - FlushAll is
 * there for the cases you want to be explicit, e.g. right after
 * ST7305_Init(), or after changing rotation with old content on
 * screen). Also resets dirty-tracking afterwards. */
void ST7305_FlushAll(st7305_t *dev);

/* Diagnostic: wall-clock duration (ms) of the most recently completed
 * ST7305_Flush() call, including any auto-power-cycle delays if
 * enabled. Inspect via a debugger Live Expression, or print it over
 * UART/SWO - useful for correlating visible flicker with how long a
 * given update actually takes. */
extern volatile uint32_t st7305_last_flush_ms;

#ifdef __cplusplus
}
#endif

#endif /* ST7305_H */
