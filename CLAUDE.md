# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Fronius GEN24 Symo inverter monitoring display. Reads live power data from the inverter's local REST API every 5 seconds and renders it on a Waveshare ESP32-S3 AMOLED 1.75" touch screen.

**Hardware**: Waveshare ESP32-S3R8 (8 MB OPI PSRAM, 16 MB flash) + 368×448 RM67162 AMOLED display + CST816S touch controller.

## Build & Flash

```bash
# Build
pio run

# Flash + open serial monitor
pio run --target upload && pio device monitor

# Build only (no upload)
pio run --target buildfs
```

First boot opens a WiFi AP named **Fronius-Monitor**. Connect to it, visit 192.168.4.1, enter your WiFi credentials and the inverter's local IP address. Credentials are saved to NVS and the portal does not appear again unless WiFi is lost.

## Architecture

```
main.cpp
├── display_driver_init()   — RM67162 QSPI + LVGL flush/touch callbacks
├── ui_init()               — build LVGL screen widgets
├── wifi_setup()            — WiFiManager + Preferences (NVS)
├── fronius_task [core 0]   — HTTP GET every 5 s → PowerData mutex
└── loop() [core 1]
    ├── lv_timer_handler()  — drives LVGL rendering (~200 Hz)
    ├── display_get_gesture() — swipe left/right switches screens
    └── ui_update() @ 2 Hz  — reads PowerData mutex → updates widgets
```

### Key files

| File | Purpose |
|---|---|
| `include/config.h` | All pin assignments and compile-time constants |
| `include/fronius.h` / `src/fronius.cpp` | `PowerData` struct + `fronius_fetch()` |
| `include/display_driver.h` / `src/display_driver.cpp` | LVGL display + CST816S touch driver |
| `include/ui.h` / `src/ui.cpp` | LVGL widget creation and `ui_update()` |
| `src/main.cpp` | Setup, WiFiManager, FreeRTOS task, loop |
| `lv_conf.h` | LVGL feature flags (root of project, picked up via `-I.`) |

### Fronius API

Single endpoint supplies all values needed:
```
GET http://<inverter_ip>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
```
- `P_PV` → solar watts (null at night → 0)
- `P_Load` → consumption watts (Fronius returns negative; code takes `abs()`)
- `P_Grid` → grid watts: **positive = export**, **negative = import**
- `rel_SOC` → battery % (GEN24 firmware ≥ 1.14; older firmware falls back to `GetStorageRealtimeData.fcgi`)

### UI layout (368×448 px)

- **Solar arc** — `lv_arc`, 270° sweep (`rotation=135`, `bg_angles 0→270`), green, 0–6600 W around the screen edge
- **Centre** — solar watts (yellow, large), consumption (blue), battery SOC (green)
- **Bottom bar** — grid watts: grey when balanced, red (import ▲) or green (export ▼)
- Touch swipe left/right cycles screens (only `SCREEN_MAIN` exists today; add new `lv_obj_t *` screens in `ui.cpp` and extend the `ScreenId` enum in `ui.h`)

### Adding a second screen

1. Add a new value to `ScreenId` in `include/ui.h` (before `SCREEN_COUNT`).
2. Add a `build_<name>_screen()` function in `src/ui.cpp`.
3. Call it from `ui_init()` and store the `lv_obj_t *` pointer.
4. In `ui_switch_screen()`, add `lv_scr_load_anim()` for the transition.

### Colour / display notes

- LVGL uses `LV_COLOR_DEPTH 16` (RGB565). If colours appear byte-swapped on the physical display, set `LV_COLOR_16_SWAP 1` in `lv_conf.h`.
- Pin definitions in `include/config.h` are based on the Waveshare ESP32-S3 AMOLED 1.75" published schematic. Verify against the board's wiki page if the display does not initialise.
- The arc start/end angles may need ±90° rotation adjustment depending on RM67162 panel orientation. Tune `lv_arc_set_rotation()` in `src/ui.cpp`.
