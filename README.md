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

-----------------------------------------------------------------------------------------------------------------------------
Użytkowanie:
WAŻNE: w pliku st7305.h zmień #include na swój typ miktoprocesora:
   
```
#include "stm32l0xx_hal.h" /* change to match your STM32 family, e.g. stm32l4xx_hal.h */
```

SPI Configuration:
You can use the configuration shown here—I used SPI1.
The pin names don't matter for SPI:

![SPI_Config](_images/SPI-configuration.jpg)

GPIO Configuration:
Add GPIO pins to your project—you don't have to follow the standard pin naming convention—the port and pin names are specified when the controller is initialized. Here's how it looks in my code:

![GPIO_Config](_images/GPIO-configuration.jpg)

Implementation in the main.c file:
```
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "st7305.h"         // ST7305 chip driver file
#include "st7305_font.h"    // Font support file
#include "st7305_paint.h"   // Drawing functions support file
#include "images.h"         // File containing graphics in the form of bitmaps
/* USER CODE END Includes */
```

```
/* USER CODE BEGIN PV */
static st7305_t lcd;         // The name of the display instance we will be referring to
/* USER CODE END PV */
```

```
/* USER CODE BEGIN 2 */
	ST7305_Init(&lcd, &hspi1,
	            LCD_CS_GPIO_Port,  LCD_CS_Pin,   // Enter the names of the GPIO ports/pins you have assigned in your project
	            LCD_DC_GPIO_Port,  LCD_DC_Pin,   // Enter the names of the GPIO ports/pins you have assigned in your project
	            LCD_RST_GPIO_Port, LCD_RST_Pin,  // Enter the names of the GPIO ports/pins you have assigned in your project
	            90);
	ST7305_ClearBuffer(&lcd);                    // Clearing the internal memory buffer in the ST7305 chip in the display
/* USER CODE END 2 */
