"""
Data Store for ArcticSense Buoy Monitor.

Persists sensor readings and camera-image filepaths to a CSV log so the
GUI's graphs, Data Log view, history pop-ups, and Lookup ID dialog all
read from the same source.

This module replaces data_simulator.py from the prototype builds. The
random-data simulation paths have been removed; readings now arrive from
the BLE link (see ble_manager.py) and are persisted by callers via
record_reading() / record_readings().

Public API
----------
CSV_FILE_PATH, IMAGE_HISTORY_DIR
    Filesystem locations the GUI expects.

METRIC_CODES, METRIC_RANGES
    Lookup tables consumed by buoy_monitor.py to label cards.
    METRIC_RANGES is preserved with the same (lo, hi, decimals, unit)
    shape as the simulator built so existing GUI code that does
    METRIC_RANGES[name][3] keeps working unchanged. Only the unit slot
    is meaningful now; the lo/hi/decimals slots are placeholder values
    kept for backwards-compatibility.

load_history(metric_name=None)
    Returns the full CSV history as a list of dicts (newest last).

clear_csv()
    Wipes the CSV (header is rewritten so future appends still align).

new_instance_id()
    Generates a fresh INST<...> id; one per Request Data click. All
    readings captured during one click should share the same instance
    id so the Data Log view's "By Instance" tab can group them.

build_metric_id(metric_name, ts)
    Builds the per-reading id used for the GUI's "ID:" line on cards
    and as the camera-image filename stem.

record_reading(metric_name, value, unit, instance_id, timestamp=None)
    Appends one row to the CSV and returns the row dict.

record_readings(readings, instance_id, timestamp=None)
    Convenience batch-writer: takes an iterable of (name, value, unit)
    tuples (or dicts) and appends them all under the same instance id.
"""

import csv
import os
from datetime import datetime

# ---------------------------------------------------------------------------
# Filesystem locations
# ---------------------------------------------------------------------------
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CSV_FILE_PATH     = os.path.join(_SCRIPT_DIR, "data", "sensor_readings.csv")
IMAGE_HISTORY_DIR = os.path.join(_SCRIPT_DIR, "images_history")

CSV_HEADERS = ["instance_id", "metric_id", "metric_name", "value", "unit", "timestamp"]
# Add a buoy_id column here (and to record_reading) if deploying multiple buoys.


# ---------------------------------------------------------------------------
# Metric metadata (must match the 12 cards in buoy_monitor.py)
# ---------------------------------------------------------------------------
# Short codes used as the prefix of every metric_id and as the bracketed
# label on each sensor card, e.g. [GPS], [SIT], [CM1].
METRIC_CODES = {
    "GPS":               "GPS",
    "EMI":               "EMI",
    "EMI I":             "EMI",
    "EMI Q":             "EMI",
    "EMI Magnitude":     "EMI",
    "EMI Phase":         "EMI",
    "EMI Phase IQ":      "EMI",
    "Camera 1":          "CM1",
    "Camera 2":          "CM2",
    "Turbidity":         "TRB",
    "Salinity":          "SAL",
    "UV Index":          "UVI",
    "Lux":               "LUX",
    "Temp Internal":           "TIN",
    "Temp Internal (Static)":  "TIS",
    "Humidity (Static)":       "HMS",
    "Temp Internal (Dynamic)": "TID",
    "Humidity (Dynamic)":      "HMD",
    "Humidity":                "HUM",
    "Temp External":     "TEX",
    "Temp External CH1": "TE1",
    "Temp External CH2": "TE2",
    "Temp External CH3": "TE3",
    "Temp External CH4": "TE4",
    "Tilt":              "TLT",
    "Tilt X":            "TLX",
    "Tilt Y":            "TLY",
    "IMU":               "IMU",
    "IMU Pitch":         "IMP",
    "IMU Roll":          "IMR",
    "IMU Yaw":           "IMW",
    "Battery":           "BAT",
    "Voltage":           "VLT",
    "Current Draw":      "CUR",
    # Image quality metrics (one set per camera, logged on every request)
    "Cam1 Quality":      "C1Q",
    "Cam1 Score":        "C1S",
    "Cam1 LapVar":       "C1V",
    "Cam1 Entropy":      "C1E",
    "Cam1 Range":        "C1R",
    "Cam1 DPR":          "C1D",
    "Cam1 SPR":          "C1P",
    "Cam2 Quality":      "C2Q",
    "Cam2 Score":        "C2S",
    "Cam2 LapVar":       "C2V",
    "Cam2 Entropy":      "C2E",
    "Cam2 Range":        "C2R",
    "Cam2 DPR":          "C2D",
    "Cam2 SPR":          "C2P",
}

# METRIC_RANGES is kept in its original (lo, hi, decimals, unit) shape for
# backwards compatibility with buoy_monitor.py, which reads [name][3] for
# the card unit label. The lo/hi/decimals slots are placeholder values left
# over from the old data_simulator and are not read anywhere in the GUI.
# The unit strings MUST match the unit field in ble_manager.SENSORS so the
# card label and the CSV unit column stay consistent.
METRIC_RANGES = {
    "GPS":               ((0, 0), (0, 0), 5, "lat/lon"),
    "EMI I":             (-10.0, 10.0,     4, ""),
    "EMI Q":             (-10.0, 10.0,     4, ""),
    "EMI Magnitude":     (0.0,   10.0,     4, ""),
    "EMI Phase":         (-180.0, 180.0,   2, "\u00b0"),
    "EMI Phase IQ":      (-180.0, 180.0,   2, "\u00b0"),
    "Camera 1":          (None, None,      0, "Image"),
    "Camera 2":          (None, None,      0, "Image"),
    "Turbidity":         (0.0, 1000.0,     1, "NTU"),
    "Salinity":          (30.0, 40.0,      2, "mS"),
    "UV Index":          (0.0, 11.0,       1, "UV"),
    "Lux":               (0.0, 100000.0, 1, "lux"),
    "Temp Internal":           (-10.0, 45.0,     1, "\u00b0C"),
    "Temp Internal (Static)":  (-10.0, 45.0,     1, "\u00b0C"),
    "Temp Internal (Dynamic)": (-10.0, 45.0,     1, "\u00b0C"),
    "Humidity":           (0.0, 100.0,  1, "%"),
    "Humidity (Static)":  (0.0, 100.0,  1, "%"),
    "Humidity (Dynamic)": (0.0, 100.0,  1, "%"),
    "Temp External":     (-40.0, 10.0,     1, "\u00b0C"),
    "Temp External CH1": (-40.0, 10.0,     1, "\u00b0C"),
    "Temp External CH2": (-40.0, 10.0,     1, "\u00b0C"),
    "Temp External CH3": (-40.0, 10.0,     1, "\u00b0C"),
    "Temp External CH4": (-40.0, 10.0,     1, "\u00b0C"),
    "Tilt":              (0.0, 45.0,       1, "\u00b0"),
    "Tilt X":            (0.0, 45.0,       1, "\u00b0"),
    "Tilt Y":            (0.0, 45.0,       1, "\u00b0"),
    "IMU":               ((0, 0), (0, 0),  2, "\u00b0 (pitch,roll,yaw)"),
    "IMU Pitch":         (-180.0, 180.0,   2, "\u00b0"),
    "IMU Roll":          (-180.0, 180.0,   2, "\u00b0"),
    "IMU Yaw":           (-180.0, 180.0,   2, "\u00b0"),
    "Battery":           (0.0, 100.0,      1, "%"),
    "Voltage":           (0.0, 20.0,       2, "V"),
    "Current Draw":      (0.0, 10.0,       2, "A"),
    # Image quality metrics — logged on every camera request (pass or reject)
    "Cam1 Quality":      (None, None,      0, ""),
    "Cam1 Score":        (0.0, 50000.0,    1, ""),
    "Cam1 LapVar":       (0.0, 100000.0,   1, ""),
    "Cam1 Entropy":      (0.0, 8.0,        2, "bits"),
    "Cam1 Range":        (0,   255,         0, ""),
    "Cam1 DPR":          (0.0, 1.0,         4, ""),
    "Cam1 SPR":          (0.0, 1.0,         4, ""),
    "Cam2 Quality":      (None, None,      0, ""),
    "Cam2 Score":        (0.0, 50000.0,    1, ""),
    "Cam2 LapVar":       (0.0, 100000.0,   1, ""),
    "Cam2 Entropy":      (0.0, 8.0,        2, "bits"),
    "Cam2 Range":        (0,   255,         0, ""),
    "Cam2 DPR":          (0.0, 1.0,         4, ""),
    "Cam2 SPR":          (0.0, 1.0,         4, ""),
}

# Ordered list of image quality sub-metric names (both cameras).
# Used by buoy_monitor.py to populate the data-log filter dropdown.
IMAGE_QUALITY_METRICS = [
    "Cam1 Quality", "Cam1 Score", "Cam1 LapVar",
    "Cam1 Entropy", "Cam1 Range", "Cam1 DPR", "Cam1 SPR",
    "Cam2 Quality", "Cam2 Score", "Cam2 LapVar",
    "Cam2 Entropy", "Cam2 Range", "Cam2 DPR", "Cam2 SPR",
]


TILT_CHANNELS = ["Tilt X", "Tilt Y"]
IMU_CHANNELS  = ["IMU Pitch", "IMU Roll", "IMU Yaw"]

# ---------------------------------------------------------------------------
# ID helpers
# ---------------------------------------------------------------------------
def build_metric_id(metric_name: str, ts: datetime) -> str:
    """Format: <CODE>_<YYYYMMDD>_<HHMMSS>
    e.g.  GPS_20260515_170033
    add microseconds if sub-second needed"""
    code = METRIC_CODES.get(metric_name, "???")
    return f"{code}_{ts.strftime('%Y%m%d_%H%M%S')}"


def new_instance_id() -> str:
    """Format: INST_<YYYYMMDD>_<HHMMSS>
    e.g.  INST_20260515_170033"""
    return "INST_" + datetime.now().strftime("%Y%m%d_%H%M%S")


# ---------------------------------------------------------------------------
# Value formatting
# ---------------------------------------------------------------------------

# Number of decimal places to record per metric. Unlisted metrics fall back
# to _DEFAULT_DP. Sub-channel names are included so splits (e.g. Tilt X/Y,
# IMU Pitch/Roll/Yaw, Temp External CH1-4) inherit the same precision.
_METRIC_DP = {
    "Temp Internal":           1,
    "Temp Internal (Static)":  1,
    "Temp Internal (Dynamic)": 1,
    "Temp External":     1,
    "Temp External CH1": 1,
    "Temp External CH2": 1,
    "Temp External CH3": 1,
    "Temp External CH4": 1,
    "Humidity":           1,
    "Humidity (Static)":  1,
    "Humidity (Dynamic)": 1,
    "Tilt":              1,
    "Tilt X":            1,
    "Tilt Y":            1,
    "IMU":               1,
    "IMU Pitch":         1,
    "IMU Roll":          1,
    "IMU Yaw":           1,
    "UV Index":          1,
    "Lux":               1,
    "Turbidity":         1,
    "Battery":           1,
    "Voltage":           2,
    "Current Draw":      2,
    # Image quality metrics
    "Cam1 Score":        1,
    "Cam1 LapVar":       1,
    "Cam1 Entropy":      2,
    "Cam1 Range":        0,
    "Cam1 DPR":          3,
    "Cam1 SPR":          3,
    "Cam2 Score":        1,
    "Cam2 LapVar":       1,
    "Cam2 Entropy":      2,
    "Cam2 Range":        0,
    "Cam2 DPR":          3,
    "Cam2 SPR":          3,
}
_DEFAULT_DP = 1


def _format_recorded_value(metric_name: str, value, unit: str) -> str:
    """Round recorded numeric readings to the metric's configured precision.

    Sensor values arrive as either a scalar string ("23.456789") or a
    comma-separated vector ("1.234567,-2.345678"). Camera rows store file
    paths, so those are left exactly as-is.
    """
    if unit == "Image" or metric_name.startswith("Camera "):
        return str(value)

    dp = _METRIC_DP.get(metric_name, _DEFAULT_DP)

    def _format_part(part):
        text = str(part).strip()
        try:
            number = float(text)
        except (TypeError, ValueError):
            return text
        if abs(number) < 0.5 * 10 ** (-dp):
            number = 0.0
        return f"{number:.{dp}f}".rstrip("0").rstrip(".")

    text = str(value)
    if "," in text:
        return ",".join(_format_part(part) for part in text.split(","))
    return _format_part(text)


# ---------------------------------------------------------------------------
# CSV plumbing
# ---------------------------------------------------------------------------
def _ensure_csv() -> None:
    """Make sure the CSV exists with the current 6-column header.
    Migrates pre-instance_id CSVs by padding existing rows with an
    empty instance_id field."""
    os.makedirs(os.path.dirname(CSV_FILE_PATH), exist_ok=True)
    if not os.path.exists(CSV_FILE_PATH):
        with open(CSV_FILE_PATH, "w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(CSV_HEADERS)
        return

    with open(CSV_FILE_PATH, "r", newline="", encoding="utf-8", errors="replace") as f:
        rows = list(csv.reader(f))
    if not rows:
        with open(CSV_FILE_PATH, "w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(CSV_HEADERS)
        return
    if rows[0] != CSV_HEADERS:
        # Migrate a pre-instance_id CSV to the current 6-column schema.
        # Rows without an instance_id column get an empty string so
        # `row["instance_id"] or "(legacy)"` in downstream code still works.
        migrated = [CSV_HEADERS]
        old_headers = rows[0]
        for r in rows[1:]:
            row_dict = dict(zip(old_headers, r))
            migrated.append([
                row_dict.get("metric_id",   ""),
                row_dict.get("metric_name", ""),
                row_dict.get("value",       ""),
                row_dict.get("unit",        ""),
                row_dict.get("timestamp",   ""),
                row_dict.get("instance_id", ""),
            ])
            with open(CSV_FILE_PATH, "w", newline="", encoding="utf-8") as f:
                csv.writer(f).writerows(migrated)


def record_reading(metric_name: str, value, unit: str,
                   instance_id: str, timestamp: datetime | None = None) -> dict:
    """Append one reading to the CSV and return it as a dict.

    Parameters
    ----------
    metric_name : str
        One of the GUI metric names (must appear in METRIC_CODES).
    value : str
        Already formatted in the shape the GUI's graph/log code expects:
          - a plain number for scalar metrics, e.g. "23.5", "85"
          - a comma-separated pair for GPS, e.g. "22.178772,-72.150898"
          - a comma-separated triple for IMU, e.g. "12.5,-3.25,181"
          - an absolute filesystem path for Camera 1 / Camera 2 images
    unit : str
        Unit string (must match METRIC_RANGES[name][3] so the card and
        the recorded row agree).
    instance_id : str
        Shared across every reading from one Request Data click.
    timestamp : datetime, optional
        Defaults to now(). When BLE delivers sensor + camera responses
        from one mask the caller may want to pass a single timestamp so
        every row in that click is grouped tightly in time.
    """
    _ensure_csv()
    ts = timestamp or datetime.now()
    metric_id = build_metric_id(metric_name, ts)
    ts_str = ts.strftime("%Y-%m-%d %H:%M:%S")
    recorded_value = _format_recorded_value(metric_name, value, unit)
    row = {
        "metric_id":   metric_id,
        "metric_name": metric_name,
        "value":       recorded_value,
        "unit":        unit,
        "timestamp":   ts_str,
        "instance_id": instance_id,
    }
    with open(CSV_FILE_PATH, "a", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow([
            row["instance_id"], row["metric_id"], row["metric_name"],
            row["value"], row["unit"], row["timestamp"],
        ])
    return row


def record_readings(readings, instance_id: str,
                    timestamp: datetime | None = None) -> list[dict]:
    """Batch-append several readings under the same instance id.

    readings: iterable of either
      - (metric_name, value, unit) tuples, or
      - {"metric_name", "value", "unit"} dicts.

    Returns the list of recorded row dicts in the same order. All rows
    share one timestamp (default: now()) so they sort together in the
    Data Log view.
    """
    out = []
    ts = timestamp or datetime.now()
    for r in readings:
        if isinstance(r, dict):
            name, value, unit = r["metric_name"], r["value"], r["unit"]
        else:
            name, value, unit = r
        out.append(record_reading(name, value, unit, instance_id, timestamp=ts))
    return out


def load_history(metric_name: str | None = None) -> list[dict]:
    """Read the CSV. If metric_name is given, return only matching rows.
    Empty instance_id is tolerated for legacy rows."""
    _ensure_csv()
    with open(CSV_FILE_PATH, "r", newline="", encoding="utf-8", errors="replace") as f:
        rows = list(csv.DictReader(f))
    # Backward-compat: rows written before instance_id existed won't
    # have the key; fill it in with the empty string so downstream code
    # that does `row["instance_id"] or "(legacy)"` keeps working.
    for row in rows:
        if "instance_id" not in row or row["instance_id"] is None:
            row["instance_id"] = ""
    if metric_name is None:
        return rows
    return [row for row in rows if row["metric_name"] == metric_name]

def clear_csv() -> None:
    """Wipe all sensor readings and rewrite the CSV with just the header row.

    Bypasses _ensure_csv() intentionally — we want a clean slate, not migration.
    The data/ directory is created if it does not already exist.
    """
    os.makedirs(os.path.dirname(CSV_FILE_PATH), exist_ok=True)
    with open(CSV_FILE_PATH, "w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow(CSV_HEADERS)