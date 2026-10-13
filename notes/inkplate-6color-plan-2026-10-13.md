# Add Soldered Inkplate 6COLOR to tesserae-device-firmware

## Context
`inkplate-tesserae-driver` (Arduino, community) already runs the 6COLOR against Tesserae
and has checked its colours on glass. This repo (ESP-IDF) supports the 13SPECTRA
(#45/#46) but not the 6COLOR. Goal: a native `soldered-inkplate-6color` env that uses this
repo's existing panel, battery, button and OTA machinery.

**Effort: small to medium.** It is about one board header, one init variant in an
existing driver (around 60 lines), one env plus a sdkconfig fragment, and docs/CI. Most of
the hardware is already covered:

| Need | Existing piece to reuse |
|---|---|
| UC81xx SPI ACeP 4bpp driver, BUSY high = idle, 0x10/04/12/02/07 flow | `src/panel/drivers/spectra6_spi_single.c` (`EPD_INIT_ACEP_7IN3F` variant at ~:324) |
| 180° flip | `EPD_ROTATE_180` → `send_buffer_rot180` |
| PCAL6416A at 0x20, battery switch on B1 (port1 bit1) with revision-polarity detection | `src/io_expander.c` + `BOARD_VBAT_SWITCH_EXP_*` path in `src/battery.c` (same pin as the 13SPECTRA) |
| Classic ESP32 + quad PSRAM | `sdkconfig.esp32-psram.defaults` (M5Paper) |
| Wake button GPIO36 via ext0 | `include/buttons.h` classic-ESP32 ext0 path on `BOARD_BTN_REFRESH_PIN` |
| `inky_7colour` gamut, orange band in selftest | `boards/waveshare_epaper_73f.h`, `EPD_COL_ORANGE` |

## Hardware facts (Arduino lib `src/boards/Inkplate6COLOR/*`, ESPHome `inkplate_spi`)
- ESP32 (WROVER), quad PSRAM at 40 MHz. The flash size is not in the repos; it is probably 4 MB.
- Panel: 600x448 ACeP 7-colour, 5.85". Controller looks like UC8159c (inferred). No power-enable pin, no PMIC.
- SPI at 2 MHz: SCLK 18, MOSI 23, CS 27, DC 33, RST 19, BUSY 32 (high = ready, pull-up).
- I2C on SDA 21, SCL 22: PCAL6416A at 0x20, PCF85063A RTC at 0x51 (RTC INT on GPIO39).
- Battery ADC on GPIO35 (ADC1_CH7), divider ×2. Wake button on GPIO36, active low.
- Init sequence:
  - `00:EF 08`, `01:37 00 05 05`, `03:00`, `06:C7 C7 1D`, `41:00`, `50:37`, `60:20`, `61:02 58 01 C0`, `E3:AA`
  - wait 100 ms, then `50:37` again
- Reset: RST low 1–10 ms, then wait 200 ms. Keep the ≥100 ms settle before reset (ESPHome says it fails without it).
- Display: `61` again, `10` + 134,400 B, `04`/busy, `12` (no parameter)/busy, `02`/busy, then deep sleep `07:A5`.
- A refresh takes about 25–30 s.

## Changes

1. **`boards/soldered_inkplate_6color.h`** (new). Model it on `soldered_inkplate_13spectra.h` and `waveshare_epaper_73f.h`.
   - Pins as listed above; `EPD_SPI_HOST SPI3_HOST`; `EPD_SPI_HZ 2000000`.
   - `EPD_WIDTH 600`, `EPD_HEIGHT 448`, `EPD_BUF_BYTES W*H/2`; `EPD_ROTATE_180`.
   - `EPD_INIT_INKPLATE6COLOR 1`, `PANEL_DRIVER_SPECTRA6_SPI_SINGLE 1`.
   - `EPD_COL_*` in Inkplate order (0 black, 1 white, 2 green, 3 blue, 4 red, 5 yellow, 6 orange).
   - Identity:
     - `TESSERAE_DEVICE_KIND "soldered_inkplate_6color"`, which the server catalog already knows (the driver repo registers it)
     - `TESSERAE_DEVICE_MODEL "Soldered_Inkplate_6COLOR"`
     - `TESSERAE_RELAY_MODEL "esp32_client"`, `TESSERAE_RELAY_GAMUT "inky_7colour"`
     - `TESSERAE_BLE_HARDWARE_CODE 0`, with no BLE (same as M5Paper: classic ESP32 + PSRAM is at ~97% IRAM)
   - Battery: `BOARD_BATTERY_ADC_CHANNEL ADC_CHANNEL_7`, `_DIVIDER 2`, `BOARD_VBAT_SWITCH_EXP_PIN 1`, `_ADDR 0x20`, `_SDA 21`, `_SCL 22`.
   - Button: `BOARD_BTN_REFRESH_PIN 36`.
   - Leave RTC, touchpads and SD undefined (YAGNI).
2. **`boards/board.h`**: add an `#elif TESSERAE_BOARD_SOLDERED_INKPLATE_6COLOR`.
3. **`src/panel/drivers/spectra6_spi_single.c`**:
   - Add an `#elif defined(EPD_INIT_INKPLATE6COLOR)` init (`run_init_sequence_inkplate6color`) to the `s6s_init` chain.
   - Build TRES from `EPD_WIDTH`/`EPD_HEIGHT` instead of the hardcoded 800x480. This is a no-op for existing boards.
   - Under the variant, send `0x12` with no data byte and use the longer reset settle.
   - Check the deep-sleep tail (`0x07 A5`) matches; it already does.
4. **Palette order (main risk).** The driver repo checked on glass, against the server's calibration swatches, that the server's `inky_7colour` frames arrive as 0 black, 1 white, 2 yellow, 3 red, 4 blue, 5 green, 6 orange. Its `frame_painter.cpp:21-32` remaps 2–5. This driver sends frames to DTM unchanged, so add a 16-entry nibble LUT in `send_buffer*`, used only when the board defines `EPD_NIBBLE_REMAP` (one table in the board header). The Waveshare 7.3F (`waveshare_epaper_73f.h`, unverified) very likely has the same bug; flag it in the PR, don't touch it.
5. **sdkconfig + env**:
   - `sdkconfig.inkplate6color.defaults`: 4 MB flash (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB`). It layers after `sdkconfig.esp32-psram.defaults` (which sets 16 MB).
   - `platformio.ini`:
     - `[env:soldered-inkplate-6color]`: `board = esp32dev`, `SDKCONFIG_DEFAULTS="sdkconfig.esp32-psram.defaults;sdkconfig.ota.defaults;sdkconfig.inkplate6color.defaults;sdkconfig.coredump.defaults"`, `partitions_4m_ota.csv`, 4 MB.
     - A `-selftest` env (`-DEPD_SELFTEST`).
   - Risk: the app must fit the 1.94 MiB OTA slot. Check the build size. If it doesn't fit, fall back to a non-OTA 4 MB layout.
6. **CI/docs**:
   - `.github/workflows/firmware.yml` matrix; `release.yml` `{env, kind: soldered_inkplate_6color, chip: esp32}`.
   - README device table and frame-format table; CHANGELOG entry marked unverified on hardware.
   - Update the driver repo's README later only if it should point people here; out of scope.

## Verification
1. Build both new envs, plus `waveshare-epaper-73f` and `seeed-reterminal-e1002`, to prove the TRES change is a no-op.
2. Check the app size is under 0x1F0000.
3. On hardware:
   - `esptool.py flash_id` to confirm 4 MB.
   - Flash the selftest: 7 colour bands in the right order, the right way up.
   - Flash the main env, pair, and paint the server's Calibration "Palette swatches". Colours must match the labels.
   - Check that a deep-sleep wake repaints.
   - Check that the battery reads about 3.7–4.2 V on a LiPo.
   - Check that the GPIO36 button wakes the board.
