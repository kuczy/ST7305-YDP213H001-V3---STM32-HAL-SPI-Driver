# ST7305-YDP213H001-V3---STM32-HAL-SPI-Driver
YDP213H001-V3 monochrome display controller (reflective, no backlight) on an SPI controller: ST7305 chip

The controller is provided “as is.”
It enables STM32 microprocessors to interface with the YDP213H001-V3 display.

Functions:
- Controller initialization,
- Clearing the image buffer in the ST7305 chip,
- Filling the screen with white,
- Filling the screen with black,
- Rotating the screen by 0, 90, 180, or 270 degrees,
- Displaying text (I used the font file from the SSD1306),
- Displaying bitmaps from an external file containing a bitmap array,
- Displaying shapes: straight line, transparent rectangle, white-filled rectangle, black-filled rectangle, transparent circle, white-filled circle, black-filled circle.

I wrote the driver so that after each transmission, the display enters sleep mode and wakes up only during data transfer—which should help conserve power.
I implemented screen refresh using partial refresh, which also helps with data transfer speed.


Użytkowanie:
1. WAŻNE: w pliku st7305.h zmień #include na swój typ miktoprocesora:
   
```
#include "stm32l0xx_hal.h" /* change to match your STM32 family, e.g. stm32l4xx_hal.h */
```
