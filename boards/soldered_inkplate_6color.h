/*
 * Board: Soldered Inkplate 6COLOR
 *   - Module: ESP32 (WROVER, quad PSRAM), classic ESP32 -- no BLE, like the M5Paper
 *   - Panel:  5.85" 600x448 7-colour ACeP, single controller, SPI (UC81xx family)
 *
 * Reuses spectra6_spi_single with EPD_INIT_INKPLATE6COLOR, which swaps in
 * Soldered's init block. Pins, init bytes and the battery path come from
 * Soldered's Inkplate-Arduino-library (Inkplate6COLOR/pins.h, Driver.cpp) and the
 * Soldered-Inkplate-ESPHome inkplate_spi component. The server's inky_7colour
 * order differs from the panel's at indices 2..5 (checked on glass by the
 * community inkplate-tesserae-driver firmware), so frames go through
 * EPD_NIBBLE_REMAP before they reach the panel.
 *
 * Flash is assumed 4 MB (not stated in Soldered's sources). RTC (PCF85063A at
 * 0x51, INT on GPIO39), touchpads and the SD slot are not used.
 * UNVERIFIED ON HARDWARE.
 */
#pragma once

/* Panel pin map (VSPI, no MISO). No panel power-enable pin. */
#define EPD_PIN_SCLK   18
#define EPD_PIN_MOSI   23
#define EPD_PIN_CS     27
#define EPD_PIN_DC     33
#define EPD_PIN_RST    19
#define EPD_PIN_BUSY   32   /* active low: 0 = busy */

#define EPD_SPI_HOST   SPI3_HOST
#define EPD_SPI_HZ     (2 * 1000 * 1000)

/* Panel geometry. Landscape-native 600x448, 4bpp packed = 134400 bytes. */
#define EPD_WIDTH      600
#define EPD_HEIGHT     448
#define EPD_BUF_BYTES  ((EPD_WIDTH * EPD_HEIGHT) / 2)

/* Soldered's library mirrors both axes at rotation 0. */
#define EPD_ROTATE_180 1

/* Soldered's init block, reset settle and refresh tail in spectra6_spi_single. */
#define EPD_INIT_INKPLATE6COLOR 1

/* 7-colour ACeP indices (nibble values), the panel's native order. */
#define EPD_COL_BLACK   0x0
#define EPD_COL_WHITE   0x1
#define EPD_COL_GREEN   0x2
#define EPD_COL_BLUE    0x3
#define EPD_COL_RED     0x4
#define EPD_COL_YELLOW  0x5
#define EPD_COL_ORANGE  0x6

/* Server nibble (inky_7colour: 0 black 1 white 2 yellow 3 red 4 blue 5 green
 * 6 orange) -> panel nibble. 7..15 are reserved and paint white. */
#define EPD_NIBBLE_REMAP { \
    EPD_COL_BLACK, EPD_COL_WHITE, EPD_COL_YELLOW, EPD_COL_RED, \
    EPD_COL_BLUE,  EPD_COL_GREEN, EPD_COL_ORANGE, EPD_COL_WHITE, \
    EPD_COL_WHITE, EPD_COL_WHITE, EPD_COL_WHITE,  EPD_COL_WHITE, \
    EPD_COL_WHITE, EPD_COL_WHITE, EPD_COL_WHITE,  EPD_COL_WHITE }

#define TESSERAE_DEVICE_MODEL  "Soldered_Inkplate_6COLOR"
#define TESSERAE_DEVICE_KIND   "soldered_inkplate_6color"
#define TESSERAE_BLE_HARDWARE_CODE  0

#define TESSERAE_RELAY_MODEL   "esp32_client"
#define TESSERAE_RELAY_GAMUT   "inky_7colour"

/* Wake button: GPIO36, active-low (ext0 path in buttons.h). */
#define BOARD_BTN_REFRESH_PIN  36

/* Battery: ADC GPIO35 (ADC1 ch7), x2 divider, enabled via PCAL6416A P1-1
 * (expander pin 9; Soldered readBattery()). I2C is SDA 21 / SCL 22. */
#define BOARD_BATTERY_ADC_CHANNEL        ADC_CHANNEL_7
#define BOARD_BATTERY_DIVIDER            2
#define BOARD_VBAT_SWITCH_EXP_PIN        1      /* port-1 bit; expander pin 9 */
#define BOARD_VBAT_SWITCH_EXP_ADDR       0x20
#define BOARD_VBAT_SWITCH_EXP_I2C_PORT   0
#define BOARD_VBAT_SWITCH_EXP_I2C_SDA    21
#define BOARD_VBAT_SWITCH_EXP_I2C_SCL    22

#define MCU_TIER_ESP32_QUAD_PSRAM 1
#define PANEL_DRIVER_SPECTRA6_SPI_SINGLE 1
