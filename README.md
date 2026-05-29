# Fronius GEN24 Monitor

Live solar power dashboard for the **Waveshare ESP32-S3-Touch-AMOLED-1.75** display board. Polls the Fronius GEN24 Symo inverter's local REST API every 5 seconds and renders power data across five touch-navigable screens.

## Hardware

| Component | Detail |
|---|---|
| MCU | ESP32-S3R8 (240 MHz, 8 MB OPI PSRAM, 16 MB flash) |
| Display | CO5300 QSPI AMOLED, 466×466 px, 1.75" |
| Touch | CST9217 I2C capacitive |
| PMIC | AXP2101 (board LiPo management) |

## Screens

Navigation is by **swipe gestures** on the touch screen. The screen layout is:

```
         [STATUS]
            ↕  swipe up/down
         [PHASES]
            ↕  swipe up/down
[CLOCK] ←→ [MAIN]              (left/right swipe)
            ↕  swipe up/down
           [PV]
```

### Main screen

The default screen after boot.

- **Green arc** (270° sweep around the edge) — solar PV generation. Full scale is the configured solar maximum; the arc fills proportionally.
- **Yellow** — current PV output in watts (top).
- **Yellow, smaller** — inverter AC output watts.
- **Blue** — household consumption in watts.
- **Blue** — battery state of charge (%). Colour changes to green while charging and red while discharging.
- **Grid** — net grid power (centre bottom). Grey when balanced (< 20 W), green when importing, red when exporting.
- **"No data"** — shown in red if the inverter has not responded after 3 consecutive poll failures.

### Clock screen

Swipe left or right from Main.

- **Analog clock face** — hour, minute (white), and second (orange) hands with 12 hour-marker dots.
- **Blue arc** — battery state of charge, same 270° geometry as the solar arc. Arc and the small SOC label at the bottom change colour to match charge state (green = charging, red = discharging).

### PV screen

Swipe down from Clock.

- Identical analog clock face.
- **Green arc** — solar PV output (same scale as the Main screen arc).
- **Green label** at the arc gap — current PV watts.

### Phases screen

Swipe up from Main.

- **Green arc** — solar PV output (same scale and colour as Main).
- **Yellow label** — current PV watts.
- **L1 / L2 / L3** — per-phase grid power in watts, signed. Colour per phase:
  - Grey — near zero (< 20 W)
  - Green — importing from grid
  - Red — exporting to grid
- Data comes from the Fronius Smart Meter via `GetMeterRealtimeData.cgi`. If the meter is unreachable, all phase labels show `-- W`.

### Status screen

Swipe up from Phases.

- **Arc** — board LiPo battery level (0–100%). Colour: blue = idle/full, green = charging, red = low.
- **Battery %** and charge state label (Charging / Discharging / Low battery / No battery).
- **Wi-Fi IP address** — the address to use for OTA updates.
- **RSSI** — signal strength in dBm with quality rating (Excellent / Good / Fair / Poor).

## First-time installation

### Prerequisites

- [PlatformIO](https://platformio.org/) — install the VS Code extension or the CLI (`pip install platformio`).
- USB-C cable connected to the board's USB port.

### Steps

**1. Clone the repository**

```bash
git clone https://github.com/pmajor021/fronius_gen24_v2.git
cd fronius_gen24_v2
```

**2. Build and flash**

```bash
pio run --target upload
```

PlatformIO downloads all dependencies on the first run. Upload speed is 921600 baud; the board is detected automatically via USB CDC.

**3. Open the serial monitor** (optional but useful on first boot)

```bash
pio device monitor
```

**4. Connect to the setup portal**

On first boot — or whenever saved WiFi credentials are missing — the board opens a WiFi access point named **Fronius-Monitor**.

1. On your phone or laptop, connect to the **Fronius-Monitor** WiFi network.
2. A captive portal will open automatically, or navigate to **http://192.168.4.1**.
3. Click **Configure WiFi**.
4. Fill in:
   - **WiFi network** — your home network SSID and password.
   - **Inverter IP Address** — the local IP of your Fronius GEN24 inverter (e.g. `192.168.1.120`). Find it in the Fronius Solar.web app or your router's DHCP table.
   - **Solar Max Watts** — the peak DC input capacity of your inverter (e.g. `6000`). This sets the full-scale value of the solar arc.
5. Click **Save**.

The board reboots, connects to your WiFi, and begins polling the inverter. The boot screen displays the assigned IP address for 4 seconds before switching to the Main screen.

The portal does not reappear on subsequent boots unless credentials are lost or a factory reset is performed.

### Factory reset

To clear all saved settings and re-enter the setup portal:

1. Hold a finger on the touch screen.
2. Power on (or reset) the board while keeping the touch held.
3. Hold for **3 seconds** after the board starts.
4. Release — the board clears NVS storage and WiFi credentials, then opens the **Fronius-Monitor** portal.

### Timezone

Edit `include/config.h` and rebuild:

```cpp
#define TZ_OFFSET_SEC   3600   /* seconds east of UTC — e.g. 3600 for CET  */
#define DST_OFFSET_SEC  3600   /* daylight saving offset — e.g. 3600 for CEST */
```

## OTA firmware update

After the board is on your network, you can update the firmware without a USB cable.

**1. Build the new firmware:**

```bash
pio run
```

The output binary is at `.pio/build/esp32s3_amoled/firmware.bin`.

**2. Open the OTA page in a browser:**

```
http://<device-ip>/update
```

The device IP is shown on the Status screen, or in the serial monitor at boot:
```
[ota] update page at http://192.168.x.x/update
```

**3.** Click **Firmware**, select `.pio/build/esp32s3_amoled/firmware.bin`, then click **Update**.

The board reboots automatically after a successful upload. All NVS settings (WiFi credentials, inverter IP, solar max) are preserved across OTA updates.

> Navigating to the root address `http://<device-ip>/` also redirects to the update page.

## Build reference

```bash
# Build only
pio run

# Build + flash + open serial monitor
pio run --target upload && pio device monitor

# Build filesystem image only
pio run --target buildfs
```

## Serial monitor diagnostics

Open at 115200 baud. Normal startup looks like:

```
[boot] Fronius Gen24 Monitor starting
[boot] display init done
[boot] UI init done
[wifi] connected, local IP: 192.168.x.x
[ota] update page at http://192.168.x.x/update
[fronius] task started, polling 192.168.x.x every 5000 ms
[fronius] response code: 200
[fronius] OK  solar=2340 W  inv=2290 W  load=850 W  grid=+1490 W  soc=78%
[fronius] phases: L1=500 L2=-300 L3=-450 W
```

| Log line | Diagnosis |
|---|---|
| `response code: -1` or `connection error` | Wrong inverter IP, inverter unreachable, or WiFi dropped |
| `response code: 200` but `JSON parse error` | Fronius returned an HTML error page instead of JSON |
| `Body.Data.Site not found` | API response structure differs — keys are printed to help diagnose |
| `phases response: 404` | Smart meter endpoint not available on this inverter |
| `phases: Body.Data[0] not found` | Meter found but indexed differently — check meter DeviceId |
| `data state changed → VALID never appears` | All fetch attempts failing — check the fronius lines above |

## Fronius API endpoints used

| Endpoint | Data |
|---|---|
| `GET /solar_api/v1/GetPowerFlowRealtimeData.fcgi` | PV watts, consumption, grid, battery SOC |
| `GET /solar_api/v1/GetMeterRealtimeData.cgi?Scope=System` | Per-phase grid power (L1/L2/L3) |
| `GET /solar_api/v1/GetStorageRealtimeData.fcgi?Scope=System` | Battery SOC fallback (older firmware) |

The inverter is polled every 5 seconds. Per-phase data requires a Fronius Smart Meter connected to the inverter.
