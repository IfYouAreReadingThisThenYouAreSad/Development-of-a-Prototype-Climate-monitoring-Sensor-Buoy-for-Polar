# ArcticSense Buoy Monitor — Ground Station GUI

Python/Tkinter ground station for the ArcticSense polar climate monitoring buoy. Communicates with the buoy Motherboard over Bluetooth Low Energy via the HM-10 UART-transparent module, decodes the sensor payload, and persists readings to CSV.

---

## Setup

```bash
cd GUI
pip3 install bleak Pillow tkintermapview matplotlib
python3 run.py
```

`tkintermapview` is optional — the GPS panel falls back to a text label if it is not installed.  
`matplotlib` is optional — graphs are replaced with a placeholder if it is not installed.  
`Pillow` is required for camera image display.

---

## File layout

```
GUI/
├── run.py                          # entry point — calls buoy_monitor.main()
├── buoy_monitor.py                 # BuoyMonitorApp: main window, BLE callbacks, card updates
├── ble_manager.py                  # BLEManager: BLE scanning, command framing, payload decode
├── data_store.py                   # CSV persistence, metric metadata, ID generation
├── data/
│   └── sensor_readings.csv         # live sensor log (auto-created on first run)
├── images_history/                 # saved JPEG captures, named by metric_id
├── ui/
│   ├── constants.py                # colour palette, fonts, layout dimensions, metric lists
│   ├── mpl_loader.py               # deferred matplotlib import (avoids ~0.5 s startup cost)
│   ├── panels/
│   │   ├── graph_panel.py          # GraphPanel: time-series matplotlib graphs
│   │   ├── camera_panel.py         # camera detail view and image-quality metrics panel
│   │   ├── gps_panel.py            # GPSPanel: tkintermapview live map
│   │   └── data_log_panel.py       # DataLogPanel: By Instance and Flat Table views
│   └── widgets/
│       ├── flat_button.py          # FlatButton: cross-platform styled button
│       └── metric_card.py          # MetricCard: sensor card with checkbox, value, and ID label
└── ArcticSense.png / arcticsenselogo1.png   # window icon and top-bar logo
```

---

## Architecture

```
BuoyMonitorApp (buoy_monitor.py)
│
├─ BLEManager (ble_manager.py)          ← background asyncio loop + Bleak
│   ├─ BLE scanning / connection
│   ├─ Command framing (build_command)
│   ├─ Fragment reassembly (_on_notify)
│   ├─ Sensor payload decode (decode_sensor_payload)
│   └─ Image quality decode (decode_image_metrics)
│
├─ DataStore (data_store.py)            ← CSV on disk
│   ├─ record_reading / record_readings
│   ├─ load_history
│   └─ new_instance_id / build_metric_id
│
└─ UI panels (ui/panels/)               ← Tkinter, all on main thread
    ├─ GraphPanel         matplotlib time-series
    ├─ GPSPanel           tkintermapview live map
    ├─ CameraPanel        image + quality metrics
    └─ DataLogPanel       By Instance / Flat Table
```

### Threading model

| Thread | Role |
|--------|------|
| Main | Tkinter event loop (`root.mainloop()`) |
| BLE thread | asyncio event loop hosting Bleak coroutines |
| Graph thread | matplotlib figure build (one per draw call) |

BLE callbacks fire on the BLE thread. All Tkinter widget updates are marshalled back to the main thread with `root.after(0, callback)`. Direct cross-thread Tkinter calls are unsafe and are not used anywhere in the codebase.

---

## BLE wire protocol

The BLE device name scanned for is **`EnduranceMKII`** (configurable via `HM10_NAME` in `ble_manager.py`).

### Start-of-frame bytes

| Constant | Value | Direction | Meaning |
|----------|-------|-----------|---------|
| `STX_CMD` | `0xAC` | GUI → buoy | Sensor request command |
| `STX_ACK` | `0xFF` | GUI → buoy | Acknowledge after receiving response |
| `STX_SENSOR_MORE` | `0xAA` | buoy → GUI | Sensor data fragment, more follow |
| `STX_SENSOR_LAST` | `0xAF` | buoy → GUI | Sensor data fragment, last |
| `STX_IMG_MORE` | `0xA0` | buoy → GUI | JPEG image fragment, more follow |
| `STX_IMG_LAST` | `0xA1` | buoy → GUI | JPEG image fragment, last |
| `STX_META_MORE` | `0xA2` | buoy → GUI | Image quality metrics fragment, more follow |
| `STX_META_LAST` | `0xA3` | buoy → GUI | Image quality metrics fragment, last |

### Command packet (5 bytes, GUI → buoy)

```
Byte 0 : 0xAC  (STX_CMD)
Byte 1 : mask[7:0]    sensor bitmask, low byte
Byte 2 : mask[15:8]   sensor bitmask, mid byte
Byte 3 : mask[23:16]  sensor bitmask, high byte
Byte 4 : XOR checksum of bytes 0–3
```

### Fragment packet (20 bytes, buoy → GUI)

```
Byte 0     : STX (one of the buoy→GUI values above)
Bytes 1–2  : fragment index (big-endian uint16)
Bytes 3–18 : 16-byte payload chunk
Byte 19    : checksum of bytes 0–18
```

### Firmware sensor bitmask

```
Bit   GUI metric(s)                     Format   Bytes   Notes
---   -----------------------------------  ------  -----   --------------------------
 0    UV Index + Lux                      <ff       8     two float32
 1    Camera 1                            image     –     JPEG stream via STX_IMG_*;
                                                           preceded by STX_META_*
 2    Camera 2                            image     –     same structure as Camera 1
 3    GPS                                 <ii       8     two int32, scale ×1e-6 → °
 4    Tilt (roll, pitch)                  <ff       8     two float32 → °
 5    Battery + Voltage + Current Draw    <fff     12     float32: %, V, mA
 6    EMI I/Q/Magnitude/Phase/PhaseIQ    <fffff   20     five float32
 7    Temp Internal (Dynamic) +
      Humidity (Dynamic)                  <ff       8     two float32: °C, %
11    Temp Internal (Static) +
      Humidity (Static)                   <ff       8     two float32: °C, %
12    Temp External (4-channel)           <4f      16     four float32 → °C
13    Turbidity                           <f        4     float32 → NTU
14    IMU (pitch, roll, yaw)              <3f      12     three float32 → °
15    Salinity                            <H        2     uint16, scale ×0.01 → mS
23    STANDBY_MASK (0x800000)             –         –     keep-alive when mask = 0
```

Sensor fields are packed in the order they appear in `SENSORS` (ble_manager.py), lowest set bit first. Sensors sharing a bit (e.g. UV+Lux on bit 0, Battery trio on bit 5) are consecutive entries in `SENSORS` and are decoded by reading sequentially. Cameras (bits 1, 2) are not in `SENSORS` — they arrive as separate JPEG streams and are handled by `_handle_image_payload`.

---

## Camera image quality

Before each JPEG, the firmware sends a 22-byte quality-metrics packet. The GUI decodes it with `decode_image_metrics()` and displays a pass/fail panel below the image in the camera detail view.

| Metric | Key | Pass threshold |
|--------|-----|---------------|
| Laplacian Variance | `lapvar` | > 4297.0 |
| Histogram Entropy | `entropy` | > 6.66 bits |
| Pixel Range | `range` | > 206 |
| Dark Pixel Ratio | `dpr` | < 0.198 |
| Saturated Pixel Ratio | `spr` | < 0.088 |
| Quality Score | `score` | > 1800.0 |

If the metrics packet reports a non-zero status, the firmware omits the JPEG entirely and the GUI skips the image-receive phase.

---

## CSV format

All readings are written to `data/sensor_readings.csv`.

| Column | Description |
|--------|-------------|
| `instance_id` | Shared ID for every reading from one "Request Data" click, e.g. `INST_20260610_143022` |
| `metric_id` | Per-reading ID, e.g. `GPS_20260610_143022` |
| `metric_name` | GUI metric name, e.g. `"GPS"`, `"UV Index"` |
| `value` | Scalar string or comma-separated vector (GPS, IMU, Tilt, Temp External) |
| `unit` | Unit string matching `METRIC_RANGES[name][3]` in data_store.py |
| `timestamp` | `YYYY-MM-DD HH:MM:SS` |

Old CSVs without an `instance_id` column are migrated automatically on first load.

---

## GUI features

### Sensor cards
- **Right panel** (6 cards): Camera 1, Camera 2, EMI, Turbidity, Salinity, Battery
- **Bottom strip** (7 cards): GPS, Temp External, Temp Internal (Static), Temp Internal (Dynamic), IMU, UV Index, Tilt
- Click any card to open its detail view in the centre panel
- Checkbox on each card controls whether that sensor is included in the next request
- **Check All / Uncheck All** buttons in the toolbar

### Centre panel
- **Graph view**: time-series matplotlib plot for the selected metric; time range switchable (30 min / 1 hr / 1 week / All)
- **Camera view**: most recent JPEG + image-quality metrics panel
- **GPS map**: live tkintermapview map showing up to 3 most recent fixes; oldest→newest coloured yellow→orange→red
- **Data Log**: two-tab view (By Instance / Flat Table) with metric and time-range filtering

### Toolbar buttons
| Button | Action |
|--------|--------|
| ► Request Data | Sends a BLE command for all checked sensors |
| Check All / Uncheck All | Bulk-toggle all sensor checkboxes |
| Clear CSV | Wipes `sensor_readings.csv` after confirmation |
| Data Log | Opens the inline Data Log view |
| Lookup ID | Searchable history window across all metrics |
| Connect | Scans for `EnduranceMKII` and connects |

### Auto-refresh
Cards refresh from CSV every 2500 ms (`REFRESH_MS` in constants.py) when no request is pending. This keeps the display current even when data was loaded from a previous session.

---

## Notes

- The BLE transport is a development proxy for the **Iridium 9603N** satellite modem. The 20-byte fragment size and command framing were chosen to fit within the Iridium SBD 340-byte message limit. Migrating to satellite requires replacing the Bleak UART calls with Iridium AT command sequences; the payload parsing logic is unchanged.