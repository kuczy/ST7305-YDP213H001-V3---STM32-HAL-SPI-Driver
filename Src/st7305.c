/*
 * st7305.c
 *
 * See st7305.h for design notes. Init sequence and RAM packing scheme
 * verified working on real 2.13" 122x250 ST7305 hardware (YDP213H001-class
 * panel); it deliberately does NOT follow the generic datasheet example,
 * which several people have reported as non-functional on this panel.
 */

#include "st7305.h"
#include <string.h>

/* ---------------------------------------------------------------------- */
/* Low level bus helpers                                                   */
/* ---------------------------------------------------------------------- */

static inline void cs_low(st7305_t *dev)  { HAL_GPIO_WritePin(dev->cs_port,  dev->cs_pin,  GPIO_PIN_RESET); }
static inline void cs_high(st7305_t *dev) { HAL_GPIO_WritePin(dev->cs_port,  dev->cs_pin,  GPIO_PIN_SET);   }
static inline void dc_cmd(st7305_t *dev)  { HAL_GPIO_WritePin(dev->dc_port,  dev->dc_pin,  GPIO_PIN_RESET); }
static inline void dc_data(st7305_t *dev) { HAL_GPIO_WritePin(dev->dc_port,  dev->dc_pin,  GPIO_PIN_SET);   }

/* Send a single command byte, no arguments. */
static void st7305_cmd(st7305_t *dev, uint8_t cmd)
{
    dc_cmd(dev);
    cs_low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    cs_high(dev);
}

/* Send a command followed by 'len' data bytes (its arguments). This
 * mirrors the writeReg(reg, count, ...) helper from the reference
 * implementation, but takes an explicit array instead of varargs so it
 * compiles cleanly as portable C without pulling in stdarg semantics
 * for a fixed, known set of init calls. */
static void st7305_cmd_args(st7305_t *dev, uint8_t cmd, const uint8_t *args, uint8_t len)
{
    dc_cmd(dev);
    cs_low(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    cs_high(dev);

    if (len > 0) {
        dc_data(dev);
        cs_low(dev);
        HAL_SPI_Transmit(dev->hspi, (uint8_t *)args, len, HAL_MAX_DELAY);
        cs_high(dev);
    }
}

#define CMD0(c)            st7305_cmd(dev, (c))
#define CMD(c, ...)        do { const uint8_t _a[] = { __VA_ARGS__ }; \
                                 st7305_cmd_args(dev, (c), _a, sizeof(_a)); } while (0)

/* ---------------------------------------------------------------------- */
/* Rotation                                                                 */
/* ---------------------------------------------------------------------- */

static st7305_rotation_t st7305_normalize_rotation(uint16_t degrees)
{
    switch (degrees) {
        case 0:   case 360: return ST7305_ROTATION_0;
        case 90:             return ST7305_ROTATION_90;
        case 180:            return ST7305_ROTATION_180;
        case 270:            return ST7305_ROTATION_270;
        default:             return ST7305_ROTATION_0; /* unsupported value: safe fallback */
    }
}

void ST7305_SetRotation(st7305_t *dev, uint16_t rotation_degrees)
{
    dev->rotation = st7305_normalize_rotation(rotation_degrees);
}

uint16_t ST7305_GetWidth(const st7305_t *dev)
{
    return (dev->rotation == ST7305_ROTATION_90 || dev->rotation == ST7305_ROTATION_270)
           ? (uint16_t)ST7305_HEIGHT : (uint16_t)ST7305_WIDTH;
}

uint16_t ST7305_GetHeight(const st7305_t *dev)
{
    return (dev->rotation == ST7305_ROTATION_90 || dev->rotation == ST7305_ROTATION_270)
           ? (uint16_t)ST7305_WIDTH : (uint16_t)ST7305_HEIGHT;
}

/* Maps a logical (rotation-aware) coordinate to the physical panel
 * coordinate the framebuffer is actually stored in. Returns false if
 * the logical coordinate is out of range for the current rotation (so
 * callers can bounds-check with a single call). */
static bool st7305_to_physical(const st7305_t *dev, int16_t x, int16_t y,
                                uint16_t *phys_x, uint16_t *phys_y)
{
    const int16_t W = (int16_t)ST7305_WIDTH;
    const int16_t H = (int16_t)ST7305_HEIGHT;

    switch (dev->rotation) {
        case ST7305_ROTATION_0:
            if (x < 0 || y < 0 || x >= W || y >= H) return false;
            *phys_x = (uint16_t)x;
            *phys_y = (uint16_t)y;
            return true;

        case ST7305_ROTATION_90: /* logical W=H, H=W, clockwise */
            if (x < 0 || y < 0 || x >= H || y >= W) return false;
            *phys_x = (uint16_t)y;
            *phys_y = (uint16_t)(H - 1 - x);
            return true;

        case ST7305_ROTATION_180:
            if (x < 0 || y < 0 || x >= W || y >= H) return false;
            *phys_x = (uint16_t)(W - 1 - x);
            *phys_y = (uint16_t)(H - 1 - y);
            return true;

        case ST7305_ROTATION_270: /* logical W=H, H=W, clockwise */
            if (x < 0 || y < 0 || x >= H || y >= W) return false;
            *phys_x = (uint16_t)(W - 1 - y);
            *phys_y = (uint16_t)x;
            return true;

        default:
            return false;
    }
}


void ST7305_HWReset(st7305_t *dev)
{
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_SET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

/* ---------------------------------------------------------------------- */
/* Init sequence (verified working register set for 122x250 ST7305)        */
/* ---------------------------------------------------------------------- */

static void st7305_run_init_sequence(st7305_t *dev)
{
    CMD(0xD6, 0x17, 0x02);
    CMD(0xD1, 0x01);
    CMD(0xC0, 0x0E, 0x05);
    CMD(0xC1, 0x3C, 0x3E, 0x3C, 0x3C);
    CMD(0xC2, 0x23, 0x21, 0x23, 0x23);
    CMD(0xC4, 0x5A, 0x5C, 0x5A, 0x5A);
    CMD(0xC5, 0x37, 0x35, 0x37, 0x37);
    CMD(0xD8, 0xA6, 0xE9);
    CMD(0xB2, 0x15);
    CMD(0xB3, 0xE5, 0xF6, 0x17, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x71);
    CMD(0xB4, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45);
    CMD(0x62, 0x32, 0x03, 0x1F);
    CMD(0xB7, 0x13);
    CMD(0xB0, 0x3F);

    CMD0(0x11);           /* SLPOUT */
    HAL_Delay(10);

    CMD(0xC9, 0x00);
    CMD(0x36, 0x48);      /* memory access control */
    CMD(0x3A, 0x11);      /* pixel format          */
    CMD(0xB9, 0x20);      /* gamma mode            */
    CMD(0xB8, 0x29);      /* panel setting         */
    CMD(0x2A, ST7305_COL_START, ST7305_COL_END);
    CMD(0x2B, ST7305_ROW_START, ST7305_ROW_END);
    CMD(0x35, 0x00);
    CMD(0xD0, 0xFF);
    CMD0(0x38);
    CMD0(0x29);           /* DISPON */
    CMD0(0x20);
    CMD(0xBB, 0x4F);
    CMD0(0x38);
    HAL_Delay(300);

    /* switch to high-power (fast/normal) drive mode; the panel boots in
     * a low-power drive mode and looks washed out / slow until this is
     * applied. If you want to deliberately run in the panel's low-power
     * drive mode (e.g. for a mostly-static UI), skip this block - refer
     * to the ST7305 datasheet section on the 0xC1/0xC2/0xC4/0xC5 voltage
     * tables for the low-power equivalents, which are not reproduced
     * here since they were not part of the verified reference sequence. */
    CMD(0xC1, 0x3C, 0x3E, 0x3C, 0x3C);
    CMD(0xC2, 0x23, 0x21, 0x23, 0x23);
    CMD(0xC4, 0x5A, 0x5C, 0x5A, 0x5A);
    CMD(0xC5, 0x37, 0x35, 0x37, 0x37);
    CMD(0xC9, 0x00);
    CMD0(0x29);           /* DISPON again */
    HAL_Delay(20);
}

void ST7305_Init(st7305_t *dev,
                  SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port,  uint16_t cs_pin,
                  GPIO_TypeDef *dc_port,  uint16_t dc_pin,
                  GPIO_TypeDef *rst_port, uint16_t rst_pin,
                  uint16_t rotation_degrees)
{
    dev->hspi     = hspi;
    dev->cs_port  = cs_port;  dev->cs_pin  = cs_pin;
    dev->dc_port  = dc_port;  dev->dc_pin  = dc_pin;
    dev->rst_port = rst_port; dev->rst_pin = rst_pin;
    dev->rotation = st7305_normalize_rotation(rotation_degrees);
    dev->auto_power_cycle = true; /* lowest average current draw by default, per requirements */
    dev->dirty = false;

    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(dev->dc_port, dev->dc_pin, GPIO_PIN_SET);

    ST7305_HWReset(dev);
    st7305_run_init_sequence(dev);

    ST7305_ClearBuffer(dev);   /* marks the whole panel dirty */
    ST7305_FlushAll(dev);      /* push one known-blank frame so the panel's
                                 * post-power-on GRAM content (undefined)
                                 * never becomes visible before your first
                                 * real draw + partial update */
}

/* ---------------------------------------------------------------------- */
/* Sleep / wake                                                            */
/* ---------------------------------------------------------------------- */

/* IMPORTANT CORRECTION: ST7305 is an active-matrix TFT-LCD, not a
 * bistable e-paper/memory technology. Liquid crystal pixels need a
 * (very low frequency, hence "ultra low power") continuous AC drive to
 * stay in their oriented state. A full SLPIN+DISPOFF cuts that drive
 * entirely, so the image visibly relaxes away - that is expected
 * controller behaviour, not a bug in this driver.
 *
 * ST7305_Sleep()/WakeUp() below use MIPI DCS Idle Mode (0x38/0x39)
 * instead: this drops into the panel's own low-power drive scheme
 * while keeping the common/segment drivers running, so the image on
 * screen is retained. This is what actually delivers the "ultra low
 * power, image stays visible" behaviour you want for deep sleep.
 *
 * If you truly want to blank the panel and cut all internal drive
 * (e.g. right before physically removing power from VCI/IOVCC), use
 * ST7305_PowerDown()/ST7305_PowerUp() instead - be aware the image
 * WILL be lost and a full ST7305_Init() re-sync may be needed depending
 * on how long the panel sat without any drive.
 */
void ST7305_Sleep(st7305_t *dev)
{
    CMD0(0x39);   /* IDMON - idle mode on, image retained, lower power */
    HAL_Delay(5);
}

void ST7305_WakeUp(st7305_t *dev)
{
    CMD0(0x38);   /* IDMOFF - back to normal (full) drive mode */
    HAL_Delay(5);
}

/* Hard power-down: obeys the old semantics (blanks the image). Only use
 * this if you don't need the picture retained, or right before cutting
 * the panel's supply entirely. */
void ST7305_PowerDown(st7305_t *dev)
{
    CMD0(0x28);   /* DISPOFF */
    CMD0(0x10);   /* SLPIN   */
    HAL_Delay(5);
}

void ST7305_PowerUp(st7305_t *dev)
{
    CMD0(0x11);   /* SLPOUT */
    HAL_Delay(10);
    CMD0(0x29);   /* DISPON */
    HAL_Delay(10);
}

void ST7305_SetAutoPowerCycle(st7305_t *dev, bool enable)
{
    dev->auto_power_cycle = enable;
}

/* ---------------------------------------------------------------------- */
/* Logical framebuffer drawing                                             */
/* ---------------------------------------------------------------------- */

static void st7305_mark_dirty(st7305_t *dev, uint16_t phys_x, uint16_t phys_y)
{
    if (!dev->dirty) {
        dev->dirty    = true;
        dev->dirty_x0 = phys_x;
        dev->dirty_x1 = phys_x;
        dev->dirty_y0 = phys_y;
        dev->dirty_y1 = phys_y;
    } else {
        if (phys_x < dev->dirty_x0) dev->dirty_x0 = phys_x;
        if (phys_x > dev->dirty_x1) dev->dirty_x1 = phys_x;
        if (phys_y < dev->dirty_y0) dev->dirty_y0 = phys_y;
        if (phys_y > dev->dirty_y1) dev->dirty_y1 = phys_y;
    }
}

static void st7305_mark_all_dirty(st7305_t *dev)
{
    dev->dirty    = true;
    dev->dirty_x0 = 0;
    dev->dirty_y0 = 0;
    dev->dirty_x1 = (uint16_t)(ST7305_WIDTH - 1u);
    dev->dirty_y1 = (uint16_t)(ST7305_HEIGHT - 1u);
}

void ST7305_ClearBuffer(st7305_t *dev)
{
    memset(dev->fb, 0x00, ST7305_FB_SIZE);
    st7305_mark_all_dirty(dev);
}

void ST7305_ClearAllWhite(st7305_t *dev)
{
    memset(dev->fb, 0x00, ST7305_FB_SIZE);
    st7305_mark_all_dirty(dev);
}

void ST7305_ClearAllBlack(st7305_t *dev)
{
    memset(dev->fb, 0xFF, ST7305_FB_SIZE);
    st7305_mark_all_dirty(dev);
}

void ST7305_SetPixel(st7305_t *dev, int16_t x, int16_t y, st7305_color_t color)
{
    uint16_t phys_x, phys_y;
    if (!st7305_to_physical(dev, x, y, &phys_x, &phys_y)) {
        return;
    }

    uint32_t byte_idx = (uint32_t)phys_y * ST7305_FB_STRIDE + (uint32_t)(phys_x >> 3);
    uint8_t  bit_mask  = (uint8_t)(0x80 >> (phys_x & 7));
    uint8_t  new_bit   = (color == ST7305_COLOR_BLACK) ? bit_mask : 0u;
    uint8_t  old_bit   = (uint8_t)(dev->fb[byte_idx] & bit_mask);

    if (old_bit == new_bit) {
        return; /* no visual change: don't grow the dirty rect for nothing */
    }

    if (color == ST7305_COLOR_BLACK) {
        dev->fb[byte_idx] |= bit_mask;
    } else {
        dev->fb[byte_idx] &= (uint8_t)~bit_mask;
    }

    st7305_mark_dirty(dev, phys_x, phys_y);
}

st7305_color_t ST7305_GetPixel(st7305_t *dev, int16_t x, int16_t y)
{
    uint16_t phys_x, phys_y;
    if (!st7305_to_physical(dev, x, y, &phys_x, &phys_y)) {
        return ST7305_COLOR_WHITE;
    }

    uint32_t byte_idx = (uint32_t)phys_y * ST7305_FB_STRIDE + (uint32_t)(phys_x >> 3);
    uint8_t  bit_mask  = (uint8_t)(0x80 >> (phys_x & 7));
    return (dev->fb[byte_idx] & bit_mask) ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
}

void ST7305_DrawBitmap(st7305_t *dev, int16_t x, int16_t y,
                        uint8_t w, uint8_t h, const uint8_t *bitmap)
{
    uint8_t stride = (uint8_t)((w + 7u) / 8u);

    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            uint8_t byte = bitmap[row * stride + (col >> 3)];
            uint8_t bit  = (uint8_t)(0x80 >> (col & 7));
            if (byte & bit) {
                ST7305_SetPixel(dev, (int16_t)(x + col), (int16_t)(y + row), ST7305_COLOR_BLACK);
            }
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Flush: repack logical framebuffer -> native RAM layout, then transmit   */
/* ---------------------------------------------------------------------- */

/* Native RAM layout, reverse-engineered from the factory driver notes:
 *   - 4 physical columns share one packed byte (2 bits each)
 *   - 2 physical rows share one packed byte set (odd/even interleave)
 *   - column addressing starts at an internal offset of 10px, which is
 *     baked into ST7305_COL_START above; add it here before packing so
 *     pixel (0,0) in the logical buffer lands on the visible top-left
 *     corner of the panel.
 */
#define ST7305_X_OFFSET 10u

volatile uint32_t st7305_last_flush_ms = 0;

/* Transmits physical scanlines [y0, y1] (inclusive) to the panel. Column
 * addressing is always sent full-width (see the "PARTIAL UPDATE" note in
 * st7305.h for why); only the row window is narrowed. y0/y1 get rounded
 * to the panel's native 2-scanline RAM row granularity. Does NOT touch
 * auto_power_cycle or timing - callers handle that. */
static void st7305_transmit_rows(st7305_t *dev, uint16_t y0, uint16_t y1)
{
    uint16_t real_y_start = y0 / 2u;
    uint16_t real_y_end   = y1 / 2u;

    uint8_t row_start_reg = (uint8_t)(ST7305_ROW_START + real_y_start);
    uint8_t row_end_reg   = (uint8_t)(ST7305_ROW_START + real_y_end);

    CMD(0x2A, ST7305_COL_START, ST7305_COL_END);
    CMD(0x2B, row_start_reg, row_end_reg);
    CMD0(0x2C); /* memory write */

    dc_data(dev);
    cs_low(dev);

    uint16_t y_begin    = (uint16_t)(real_y_start * 2u);
    uint16_t y_end_excl = (uint16_t)((real_y_end + 1u) * 2u);

    for (uint16_t y = y_begin; y < y_end_excl && y < ST7305_HEIGHT; y += 2) {
        uint8_t row_buf[ST7305_BYTES_PER_ROW];
        memset(row_buf, 0x00, sizeof(row_buf));

        /* pack physical scanlines y and y+1 into this single physical RAM row */
        for (uint8_t sub = 0; sub < 2; sub++) {
            uint16_t yy = (uint16_t)(y + sub);
            if (yy >= ST7305_HEIGHT) {
                break;
            }
            uint8_t one_two = sub; /* 0 for even row, 1 for odd row */

            for (uint16_t x = 0; x < ST7305_WIDTH; x++) {
                uint32_t byte_idx = (uint32_t)yy * ST7305_FB_STRIDE + (x >> 3);
                uint8_t  bit_mask = (uint8_t)(0x80 >> (x & 7));
                if ((dev->fb[byte_idx] & bit_mask) == 0) {
                    continue; /* white pixel: leave RAM bit cleared */
                }

                uint16_t phys_x     = x + ST7305_X_OFFSET;
                uint16_t real_x     = phys_x / 4u;
                uint8_t  line_bit_4 = (uint8_t)(phys_x % 4u);
                uint8_t  write_bit  = (uint8_t)(7u - (line_bit_4 * 2u + one_two));

                row_buf[real_x] |= (uint8_t)(1u << write_bit);
            }
        }

        HAL_SPI_Transmit(dev->hspi, row_buf, sizeof(row_buf), HAL_MAX_DELAY);
    }

    cs_high(dev);
}

/* Shared wrapper: timing + auto power-cycle around a row-range transfer. */
static void st7305_do_flush(st7305_t *dev, uint16_t y0, uint16_t y1)
{
    uint32_t t0 = HAL_GetTick();

    if (dev->auto_power_cycle) {
        ST7305_WakeUp(dev);
    }

    st7305_transmit_rows(dev, y0, y1);

    if (dev->auto_power_cycle) {
        ST7305_Sleep(dev);
    }

    st7305_last_flush_ms = HAL_GetTick() - t0;
}

void ST7305_Flush(st7305_t *dev)
{
    if (!dev->dirty) {
        return; /* nothing changed since last Flush(): skip entirely,
                  * not even a WakeUp()/Sleep() cycle */
    }

    st7305_do_flush(dev, dev->dirty_y0, dev->dirty_y1);
    dev->dirty = false;
}

void ST7305_FlushAll(st7305_t *dev)
{
    st7305_do_flush(dev, 0, (uint16_t)(ST7305_HEIGHT - 1u));
    dev->dirty = false;
}
