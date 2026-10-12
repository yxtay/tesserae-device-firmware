# Plan: port Inkplate 13SPECTRA into tesserae-device-firmware

## Context

This repo (`inkplate-tesserae-driver`) is a separate Arduino firmware of about 700 lines. It is built on the Soldered Inkplate library and talks the same Tesserae REST protocol as upstream `tesserae-device-firmware`, which is ESP-IDF 5.5 with a board-header and panel-vtable architecture. The reTerminal E1004 is supported upstream.

Research findings:
- **Same glass.** The 13SPECTRA (EL133UF1) and the E1004 (T133A01) are the same 13.3" Spectra-6 panel at 1200×1600 with two controllers. They share:
  - the UC81xx command set;
  - chip routing: CS_M takes the master-only commands and both chips take the rest;
  - TRES `04 B0 03 20`;
  - the 300/300-byte column split per 600-byte row;
  - nibble order (high nibble = even x);
  - colour codes 0,1,2,3,5,6;
  - raw BUSY polarity (LOW = busy).
- **Driver already shared.** Upstream's `src/panel/drivers/spectra6_t133a01_dual.c` serves the E1004, the EE02 and the Good Display 133C02. The Good Display board already switches init blobs with `#ifdef EPD_T133_GDEP133C02_INIT`.
- **Not harmonised today.** This repo has no OTA, no provisioning (credentials are compile-time), no wake button, no BUSY timeouts, no low-battery cutoff, no clock discipline, a thin status payload, and it duplicates the REST client.

Decision (user): port the 13SPECTRA into upstream as a new board. Leave the 6COLOR on this repo for now.

## Byte-level deltas: 13SPECTRA vs the E1004 driver

Sources: `~/git/Soldered-Inkplate-ESPHome/components/inkplate_spi/models/inkplate13.py:25-41`, `inkplate13.cpp`, and the Soldered Arduino lib.

| Item | E1004 (current driver) | 13SPECTRA |
|---|---|---|
| `0x74` AN_TM (master only) | `00 0C 0C D9 DD DD 15 15 55` | `C0 1C 1C CC CC CC 15 15 55` (same as the GDEP133C02 blob) |
| PSR `0x00` | `DF 69` | `DF 6B` |
| PLL `0x30` | not sent | `08` |
| DCDC `0xA5` | `44 54 00` | not sent (same as GDEP133C02) |
| CDI `0x50` | `37` | `F7` (same as GDEP133C02) |
| BTST_P / BTST_N `0x06` / `0x05` | `E0 20` | `D8 18` |
| DRF `0x12` | `01` | `00` |
| Reset timing | RST low 10 ms | PWR_EN high, wait 100 ms; RST low 100 ms; RST high, wait 100 ms |
| Strap pins | none | BS0 = GPIO6 driven LOW, BS1 = GPIO5 driven HIGH, both before power-up |
| Sleep | `07 A5`, then PWR and RST low | POF, PWR_EN off, RST held low (`07 A5` should also be harmless) |

The 13SPECTRA blob is close to the existing GDEP133C02 variant. Only PSR, PLL, BTST and DRF differ from that blob.

## Changes (all in `~/git/tesserae-device-firmware`, on branch `feat/inkplate-13spectra`)

1. **`boards/soldered_inkplate_13spectra.h`** (new). Copy the structure of `boards/gooddisplay_esp32_133c02.h`.
   - Panel pins:
     - SCLK 38, MOSI 40, CS_M 42, CS_S 39, DC 14, RST 4, BUSY 7, PWR 21
     - `EPD_PIN_BS0 6`, `EPD_PIN_BS1 5`
     - SPI2 at 10 MHz
   - Geometry and palette: same as the E1004.
   - Identity:
     - `TESSERAE_DEVICE_KIND "soldered_inkplate_13spectra"` (already in the server catalog since 0.443.1)
     - `TESSERAE_DEVICE_MODEL "Soldered_Inkplate_13SPECTRA"`
     - next free `TESSERAE_BLE_HARDWARE_CODE`
     - relay `esp32_client` / `spectra_6`
   - Init variant: define `EPD_T133_INKPLATE13_INIT 1`.
   - Wake button: `BOARD_BTN_REFRESH_PIN 18`, active-low. The S3 ext1 path in `include/buttons.h` handles it.
   - Tier and driver: `MCU_TIER_S3_OCTAL_PSRAM`, `PANEL_DRIVER_SPECTRA6_T133A01_DUAL`.
   - Battery: leave undefined in the first pass, so the module compiles to a stub (see step 5).

2. **`src/panel/drivers/spectra6_t133a01_dual.c`**: add an `EPD_T133_INKPLATE13_INIT` blob branch next to the GDEP133C02 one.
   - Blob values: AN_TM, PSR, CDI and BTST from the table, plus `PLL_V {0x08}` sent to both chips. No DCDC.
   - `DRF_V`: `{0x00}`.
   - `t133_port_init`: if `EPD_PIN_BS0` and `EPD_PIN_BS1` are defined, configure them as outputs and drive them LOW and HIGH.
   - Reset: make the reset and power-on delays macros, `EPD_T133_RST_MS` defaulting to the current value. The Inkplate header sets them to 100.
   - Keep the existing bounded BUSY waits. Neither vendor driver has timeouts, so these are an improvement.

3. **`boards/board.h`**: add an `#elif defined(TESSERAE_BOARD_SOLDERED_INKPLATE_13SPECTRA)` entry.

4. **`platformio.ini`**: add `[env:soldered-inkplate-13spectra]` and a `-selftest` variant, cloned from the E1004 env at L94/L109.
   - Board: `esp32-s3-devkitc1-n16r8`.
   - Fragments: `sdkconfig.defaults` and `sdkconfig.ble.defaults`.
   - Flash and partitions: the module is an ESP32-S3-WROOM-2-N32R16V (32 MB flash, 16 MB octal PSRAM, per the Crowd Supply listing). `sdkconfig.inkplate13spectra.defaults` sets 32 MB; the existing `partitions_ota.csv` A/B layout already fits (nothing past 0x830000), so no new table. The env still builds against the n16r8 PlatformIO board variant, as the E1004 does.
   - Add the board to the `.github/workflows/firmware.yml` build matrix and the `release.yml` matrix. Add the BLE code to `docs/ble-setup-protocol.md`.

5. **Battery (done, separate commit).**
   - Hardware: battery ADC on GPIO1 with a ×2 divider. The measure enable is expander P1-1 on a PCAL6416A at 0x20, not a GPIO, and it only works with jumper JP5 shorted.
   - Reuse: the PCA9535 helper in `src/panel/drivers/parallel_epd_gray.c:~302-361` has the same register map (in 0/1, out 2/3, cfg 6/7). Lift it into a small shared `src/io_expander.c` that both the parallel driver and battery code use.
   - `src/battery.c`: add a `BOARD_VBAT_SWITCH_EXP_PIN` hook next to `BOARD_VBAT_SWITCH_PIN`.
   - Polarity: two board revisions. Soldered's `readBattery()` reads expander pin 9 as an input first (HIGH = older PMOS-only, enable by driving LOW; LOW = newer PMOS+NMOS, enable HIGH). `battery.c` mirrors that.
   - I2C pins: SDA 8 / SCL 9. Soldered calls `Wire.begin()` with no pins and the working Arduino build uses the generic `esp32-s3-devkitc-1` variant, whose default Wire pins are 8/9. Bench: if the expander never answers, battery reads 0 mV = "unknown".

**Deferred (YAGNI until asked):**
- PCF85063A RTC: a different chip from the PCF8563, so a new driver. The HTTP Date header covers the clock meanwhile.
- SD card: needs jumper JP4.
- Partial refresh: not supported by the panel (Soldered docs: full refresh only), so not planned.
- 6COLOR port: needs a new ACeP 5.65" init block in `spectra6_spi_single`.

## Soldered docs review (docs.soldered.com/inkplate/13spectra)

The docs confirm the pins, init order and battery logic above; they add no contradictions. SDA/SCL, flash/PSRAM, RTC address and refresh time are not stated, and the schematic/KiCad files are "not available yet". Extra facts worth keeping:

- **Console:** USB-C goes through a CH340 UART bridge, not native USB CDC. The base `sdkconfig.defaults` already makes UART the primary console, so no change is needed.
- **Panel:** full refresh only (no partial update, no grayscale); operating range 0-50 C. A large refresh can brown out a weak USB port.
- **Sleep:** cut PWR_EN and float the panel pins before sleep; the panel needs a reset after wake.
- **Jumpers** (defaults): JP1 closed, expander INT to IO16; JP2 open, RTC clock out to IO2; JP3 open, SD at 3.3 V permanently; JP4 open, SD CS to GPIO15; JP5 open, P1_1 to the battery circuit; JP6 closed, P1_2 to the SD power MOSFET.
- **SD (deferred):** SPI MOSI 11 / SCK 12 / MISO 13. The docs say CS is IO15 via JP4, but the Arduino library uses GPIO10, so check the schematic. Power it by driving expander pin 10 LOW, then wait 200 ms before init.
- **Battery connector:** the battery page says + is on the right (notch up), the FAQ says left. Check the silkscreen before plugging one in.
- **Colours:** the docs table lists blue/green as 4/5, but the library sends 5/6, which matches our 0,1,2,3,5,6 codes.
- **Free GPIO:** IO17 is free.

## This repo after the port

Once the upstream env is proven on hardware:
- **README:** point 13SPECTRA users to upstream. Keep the 6COLOR firmware.
- **13SPECTRA paths:** remove `board.h`, `frame_painter.cpp` and the selftest env in a separate PR.
- **Tooling:** keep `tools/tesserae_dryrun.py`.

Do none of this until the bench test passes.

## Risks to check on the bench

1. **DC line.** The Soldered code never drives DC low, because the BS0=0/BS1=1 strap may select a mode that ignores DC. The upstream driver toggles DC, which should be fine in 4-wire mode. The selftest shows whether it works.
2. **Frame orientation.** Upstream `image_decoder.c` checks only the length, and 960000 bytes is the same in both orientations. Check that the server renders the `soldered_inkplate_13spectra` kind as native portrait 1200×1600 like the E1004; this repo currently paints landscape frames with rotation 1. If the server sends landscape, fix the server catalog to native portrait. Do not add rotation to the firmware.
3. **Order of operations.** Inkplate does PON before DTM and waits on BUSY after each half; the E1004 driver does DTM, then PON, then DRF. If the selftest shows a blank or garbled half, switch to the Inkplate order under the variant macro.
4. **Flash size.** Settled at 32 MB (Crowd Supply). `esptool flash_id` is still a quick sanity check before first flash.

## Verification

1. `pio run -e soldered-inkplate-13spectra` and the `-selftest` env both build. `pio run -e seeed-reterminal-e1004` still builds, so the E1004 blob is unchanged.
2. Host tests pass: `tools/test_*.sh` and the CI steps in `firmware.yml` L40-87.
3. Flash the selftest to a 13SPECTRA, then check:
   - the colour bars and palette sweep are correct;
   - the left and right halves line up;
   - nothing is left on BUSY timeout in the log.
4. Flash the main env and go through the captive portal (AP "Tesserae-Setup"). Pair with the server, then check:
   - the frame paints in the correct orientation;
   - the status shows a plausible battery voltage (JP5 shorted; ~0 / unknown if open);
   - the GPIO18 button wakes the device and triggers a refresh;
   - deep sleep then wakes on the timer.
5. Optional: test OTA from the server to the inactive slot.
