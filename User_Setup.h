#define USER_SETUP_INFO "CYD 3.5 Resistive ST7796"

#define ST7796_DRIVER

#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#define TFT_INVERSION_OFF

#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH

#define ESP32_DMA
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TOUCH_CS   33
#define TOUCH_MOSI 13
#define TOUCH_MISO 12
#define TOUCH_CLK  14
#define TOUCH_OFFSET_ROTATION 1

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY       65000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000

#define USE_HSPI_PORT