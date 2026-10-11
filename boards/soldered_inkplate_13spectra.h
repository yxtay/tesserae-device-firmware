/*
 * Board: Soldered Inkplate 13SPECTRA
 *   - Module: ESP32-S3-WROOM-2 (octal PSRAM)
 *   - Panel:  E Ink EL133UF1 13.3" Spectra 6, dual-chip, 1200x1600
 *
 * Same glass family as the Seeed E1004 / Good Display 133C02, so this reuses
 * the T133A01 dual-controller driver with Soldered's init values
 * (EPD_T133_INKPLATE13_INIT) and the pin map below. Pins and init bytes come
 * from Soldered's Inkplate-Arduino-library (Inkplate13SPECTRA/pins.h) and the
 * Soldered-Inkplate-ESPHome inkplate_spi component.
 *
 * NOT YET VERIFIED ON HARDWARE. Open points: DC handling under the BS0/BS1
 * strap mode, flash size (16 MB table used until `esptool flash_id` confirms
 * more), I2C pins / battery measurement (see docs/13spectra-upstream-port-plan.md).
 */
#pragma once

/* Panel pin map (SPI2). */
#define EPD_PIN_SCLK   38
#define EPD_PIN_MOSI   40
#define EPD_PIN_CS_M   42   /* primary/left controller  */
#define EPD_PIN_CS_S   39   /* secondary/right controller */
#define EPD_PIN_DC     14
#define EPD_PIN_RST    4
#define EPD_PIN_BUSY   7    /* active low: 0 = busy */
#define EPD_PIN_PWR    21   /* active-high panel power enable */
#define EPD_PIN_BS0    6    /* interface-select strap, driven LOW  */
#define EPD_PIN_BS1    5    /* interface-select strap, driven HIGH */

#define EPD_SPI_HOST   SPI2_HOST
#define EPD_SPI_HZ     (10 * 1000 * 1000)

/* Panel geometry. Portrait-native 1200x1600, 4bpp packed dual-controller. */
#define EPD_WIDTH      1200
#define EPD_HEIGHT     1600
#define EPD_BUF_BYTES  ((EPD_WIDTH * EPD_HEIGHT) / 2)

/* Soldered's init values and slower power/reset timing. */
#define EPD_T133_INKPLATE13_INIT 1
#define EPD_T133_RST_MS          100

/* 6-colour Spectra palette indices (nibble values). */
#define EPD_COL_BLACK   0x0
#define EPD_COL_WHITE   0x1
#define EPD_COL_YELLOW  0x2
#define EPD_COL_RED     0x3
#define EPD_COL_BLUE    0x5
#define EPD_COL_GREEN   0x6

#define TESSERAE_DEVICE_MODEL  "Soldered_Inkplate_13SPECTRA"
#define TESSERAE_DEVICE_KIND   "soldered_inkplate_13spectra"
#define TESSERAE_BLE_HARDWARE_CODE  19

#define TESSERAE_RELAY_MODEL   "esp32_client"
#define TESSERAE_RELAY_GAMUT   "spectra_6"

/* Wake button: GPIO18, active-low (ext1 ANY_LOW path in buttons.h). */
#define BOARD_BTN_REFRESH_PIN  18

/* Battery (ADC GPIO1, x2 divider, enable via PCAL6416A expander P1-1) is not
 * wired up yet; battery.c compiles to a stub without BOARD_BATTERY_*. */

#define MCU_TIER_S3_OCTAL_PSRAM 1
#define PANEL_DRIVER_SPECTRA6_T133A01_DUAL 1
