"""
BLE link to the ArcticSense buoy.

Extracted from the standalone BT_Gui.py prototype: this module contains
only the protocol layer — BLE scanning, command framing, fragment
reassembly, sensor payload decoding, and image reassembly. The Tkinter
GUI from BT_Gui.py has been removed; buoy_monitor.py is the front end.

The SENSORS table is the contract with the STM32 firmware. It is aligned
with the sensor cards in buoy_monitor.py so the firmware's request mask,
payload layout, and the GUI's metric names all agree. See the firmware
contract below.

================================================================
Firmware contract — 24-bit request mask
================================================================
Sensor fields are packed in SENSORS declaration order; decode_sensor_payload()
walks the same list to deserialise. Only bits set in the request mask are
present in the payload; bits not set are skipped entirely.

Bit   GUI metric(s)                      Format   Bytes  Notes
---   ------------------------------------  ------  -----  ----------------------
 0    UV Index + Lux                       <ff       8    two float32
 1    Camera 1                             image     –    JPEG stream via STX_IMG_*
                                                          preceded by STX_META_*;
                                                          not in SENSORS
 2    Camera 2                             image     –    same structure as Camera 1
 3    GPS                                  <ii       8    two int32, scale ×1e-6 → °
 4    Tilt (roll, pitch)                   <ff       8    two float32 → °
 5    Battery + Voltage + Current Draw     <fff     12    float32: %, V, mA
 6    EMI I/Q/Magnitude/Phase/PhaseIQ     <fffff   20    five float32
 7    Temp Internal (Dynamic) +
      Humidity (Dynamic)                   <ff       8    two float32: °C, %
11    Temp Internal (Static) +
      Humidity (Static)                    <ff       8    two float32: °C, %
12    Temp External (4-channel)            <4f      16    four float32 → °C
13    Turbidity                            <f        4    float32 → NTU
14    IMU (pitch, roll, yaw)               <3f      12    three float32 → °
15    Salinity                             <H        2    uint16, scale ×0.01 → mS
23    STANDBY_MASK (0x800000)              –         –    keep-alive when mask = 0

Notes:
- Cameras (bits 1, 2) are NOT struct-packed. They arrive as JPEG byte
  streams via the image-fragment STX codes (STX_IMG_MORE / STX_IMG_LAST),
  always preceded by a 22-byte quality-metrics packet (STX_META_*).
  They do not appear in SENSORS.
- Sensors sharing a bit (UV+Lux on 0, Battery trio on 5, EMI quintet
  on 6, SHT45 pairs on 7 and 11) are consecutive SENSORS entries.
  decode_sensor_payload() reads them in declaration order; only the
  first entry for each bit clears the "bit seen" check, subsequent
  entries on the same bit continue reading the payload forward.
- "Temp External" (bit 12) decodes as a single <4f struct (four float32
  in one read), yielding a comma-separated value string that maps to
  the four TEMP_EXTERNAL_CHANNELS in data_store.
"""

import asyncio
import struct
import threading
from io import BytesIO

from bleak import BleakClient, BleakScanner
from PIL import Image, ImageFile

# Allow PIL to display images whose last fragment is zero-padded.
# The JPEG EOI marker (0xFF 0xD9) is always present; trailing zeros are
# harmless but PIL raises OSError without this flag.
ImageFile.LOAD_TRUNCATED_IMAGES = True


# ---------------------------------------------------------------------------
# BLE device / characteristic
# ---------------------------------------------------------------------------
# HM10_NAME is the BLE advertisement name the scanner looks for.
# Update this if the HM-10 module's name has been changed via AT+NAME.
HM10_NAME = "EnduranceMKII"
# Standard HM-10 UART-transparent service characteristic UUID.
CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"


# ---------------------------------------------------------------------------
# Protocol constants (must match STM32 firmware)
# ---------------------------------------------------------------------------
# Start-of-frame bytes identify the packet type and whether more fragments
# follow (MORE) or this is the final fragment (LAST).
STX_SENSOR_MORE = 0xAA   # sensor data fragment — more fragments follow
STX_SENSOR_LAST = 0xAF   # sensor data fragment — this is the last fragment
STX_IMG_MORE    = 0xA0   # JPEG image fragment — more fragments follow
STX_IMG_LAST    = 0xA1   # JPEG image fragment — this is the last fragment
STX_META_MORE   = 0xA2   # image quality metrics fragment — more follow
STX_META_LAST   = 0xA3   # image quality metrics fragment — last
STX_CMD         = 0xAC   # command packet from ground station to buoy
STX_ACK         = 0xFF   # acknowledgement from ground station to buoy

# Bit 23 is used as a keep-alive when the real sensor mask is zero,
# preventing the firmware from entering standby when nothing is requested.
STANDBY_MASK = 0x800000  # bit 23

# Camera bit positions in the 24-bit firmware mask. Must match the
# CAMERA1_BIT / CAMERA2_BIT definitions in the STM32 firmware.
CAMERA1_BIT  = 1
CAMERA2_BIT  = 2

FRAG_PAYLOAD = 16   # usable payload bytes in each 20-byte BLE packet
TIMEOUT_S    = 30.0 # seconds to wait for a complete response before giving up


# ---------------------------------------------------------------------------
# Sensor table — aligned with the sensor cards in buoy_monitor.py
# ---------------------------------------------------------------------------
# Each entry: (firmware_bit, gui_metric_name, struct_fmt, byte_width, unit, scale)
#
# struct_fmt and byte_width describe one complete field as it appears in the
# firmware payload. scale multiplies the raw decoded value into the displayed
# unit. Multi-element formats (<ii, <ff, <3f, <4f, <fffff) yield multiple
# values that are joined as a comma-separated string to match the CSV schema
# used by the graph and data-log code.
#
# Sensors that share a firmware bit (e.g. UV Index + Lux both on bit 0) are
# listed as consecutive entries. decode_sensor_payload() advances the payload
# offset for each entry in order, using the same bit-set check for all of them.
#
# Cameras (bits 1, 2) are intentionally absent — they are delivered as JPEG
# byte streams via the image-fragment STX codes, not as struct-packed fields.
#
# The unit strings MUST match the unit field in data_store.METRIC_RANGES so
# the card label and the per-reading unit recorded in the CSV stay consistent.
SENSORS = [
    (0,  "UV Index",          "<f",   4, "UV",           1),
    (0,  "Lux",               "<f",   4, "lux",          1),
    (3,  "GPS",               "<ii",  8, "lat/lon",      1e-6),
    (4,  "Tilt",              "<ff",  8, "\u00b0",       1),
    (5,  "Battery",           "<f",   4, "%",            1),
    (5,  "Voltage",           "<f",   4, "V",            1),
    (5,  "Current Draw",      "<f",   4, "mA",            1),
    (6,  "EMI I",             "<f",   4, "",              1),
    (6,  "EMI Q",             "<f",   4, "",              1),
    (6,  "EMI Magnitude",     "<f",   4, "",              1),
    (6,  "EMI Phase",         "<f",   4, "\u00b0",        1),
    (6,  "EMI Phase IQ",      "<f",   4, "\u00b0",        1),
    (7, "Temp Internal (Dynamic)", "<f",   4, "\u00b0C",      1),
    (7,  "Humidity (Dynamic)",     "<f",   4, "%",            1),


    (11,  "Temp Internal (Static)",   "<f",   4, "\u00b0C",      1),
    (11,  "Humidity (Static)",        "<f",   4, "%",            1),
    (12,  "Temp External",     "<4f", 16, "\u00b0C",      1),
    (13,  "Turbidity",         "<f",   4, "NTU",          1), 
    (14,  "IMU",               "<3f", 12, "\u00b0", 1),
    (15,  "Salinity",          "<H",   2, "mS",          0.01),

]

# Map every GUI metric name to its firmware bit position.
# Cameras are added explicitly because they do not appear in SENSORS
# (they are handled as JPEG streams rather than struct-packed fields).
# "EMI" is the card name; the five individual sub-metrics all share bit 6.
METRIC_TO_BIT = {name: bit for bit, name, *_ in SENSORS}
METRIC_TO_BIT["EMI"] = 6        # card-level name → bit 6 (same bit as EMI sub-metrics)
METRIC_TO_BIT["Camera 1"] = CAMERA1_BIT
METRIC_TO_BIT["Camera 2"] = CAMERA2_BIT


def metrics_to_mask(metric_names: list[str]) -> int:
    """Convert a list of GUI metric names into a 24-bit firmware mask.

    Unknown names are silently skipped, so it's safe to feed in the
    GUI's checked-cards list directly without filtering.
    """
    mask = 0
    for name in metric_names:
        bit = METRIC_TO_BIT.get(name)
        if bit is not None:
            mask |= (1 << bit)
    return mask


# ---------------------------------------------------------------------------
# Command framing helpers
# ---------------------------------------------------------------------------
def calc_crc(data: bytes) -> int:
    """XOR checksum over all bytes (matches STM firmware calc_crc)."""
    crc = 0
    for b in data:
        crc ^= b
    return crc


def build_command(mask: int) -> bytes:
    """Build a 5-byte command packet: [STX_CMD][MASK_L][MASK_M][MASK_H][CRC]."""
    body = bytes([
        STX_CMD,
        (mask >> 0)  & 0xFF,
        (mask >> 8)  & 0xFF,
        (mask >> 16) & 0xFF,
    ])
    return body + bytes([calc_crc(body)])


# ---------------------------------------------------------------------------
# Sensor payload decode
# ---------------------------------------------------------------------------
def _format_value(unpacked: tuple, scale: float, unit: str | None = None) -> str:
    """Format struct.unpack_from output into the string the GUI expects.

    Single-value sensors produce a plain numeric string ("23.5", "85").
    Multi-value sensors produce a comma-separated string ("12.5,-3.25,181").

    GPS is a special case: the two int32 lat/lon values are formatted as
    directional strings ("55.123456°N,3.456789°W") rather than plain numbers.

    Trailing-zero stripping keeps short values compact ("85" not "85.0")
    while the .6f format preserves enough precision for GPS coordinates
    (1e-6 scale → exactly 6 decimal places, eliminating floating-point noise).
    """
    def _one(raw):
        v = raw * scale
        if isinstance(v, float):
            s = f"{v:.6f}".rstrip("0").rstrip(".")
            return s if s else "0"
        return str(v)

    if unit == "lat/lon" and len(unpacked) == 2:
        lat = unpacked[0] * scale
        lon = unpacked[1] * scale
        lat_dir = "N" if lat >= 0 else "S"
        lon_dir = "E" if lon >= 0 else "W"
        lat_s = f"{abs(lat):.6f}".rstrip("0").rstrip(".")
        lon_s = f"{abs(lon):.6f}".rstrip("0").rstrip(".")
        return f"{lat_s}°{lat_dir},{lon_s}°{lon_dir}"

    if len(unpacked) == 1:
        return _one(unpacked[0])
    return ",".join(_one(r) for r in unpacked)


def decode_sensor_payload(payload: bytes, mask: int) -> dict:
    """Unpack a reassembled sensor payload into {gui_metric_name: (value_str, unit)}.

    Iterates SENSORS in declaration order — the same order the firmware uses
    to pack the payload — so byte offsets stay aligned. Entries whose bit is
    not set in *mask* are skipped without advancing the offset.

    For sensors that share a firmware bit (UV+Lux on bit 0, Battery trio on
    bit 5, etc.) the first entry for a bit clears the "last bit seen" check;
    subsequent entries on the same bit continue reading the payload forward
    without re-checking the bit.

    Raises ValueError if the payload is shorter than the requested mask implies,
    which indicates a desync between firmware and ground-station SENSORS table.
    """
    result = {}
    offset = 0
    last_bit_seen = None
    for bit, name, fmt, width, unit, scale in SENSORS:
        if not (mask & (1 << bit)):
            last_bit_seen = None
            continue
        # For sensors sharing a bit (UV + Lux on bit 6), only the
        # first entry resets the "bit seen" check; subsequent entries
        # on the same bit just continue reading the payload forward.
        if bit == last_bit_seen:
            pass   # same bit, keep reading
        last_bit_seen = bit
        if offset + width > len(payload):
            raise ValueError(
                f"Payload too short for {name}: need {width} B at "
                f"offset {offset}, have {len(payload)}"
            )
        unpacked = struct.unpack_from(fmt, payload, offset)
        result[name] = (_format_value(unpacked, scale), unit)
        offset += width
    return result


# ---------------------------------------------------------------------------
# Image quality metrics decode
# ---------------------------------------------------------------------------

# Maps the ImageStatus enum values from firmware image_quality.h to
# human-readable strings. Must stay in sync with that enum definition.
_IMAGE_STATUS_NAMES = {
    0: "OK",
    1: "Decode Error",
    2: "Blurry (LapVar too low)",
    3: "Low Entropy (flat histogram)",
    4: "Flat/Mono (Range too low)",
    5: "Overexposed (SPR too high)",
    6: "Underexposed (DPR too high)",
    7: "Score too low",
}

# Pass/fail thresholds from firmware image_quality.h.
# Used by the GUI camera panel to colour each metric row green or red
# without reimplementing the firmware decision logic.
IMAGE_METRIC_THRESHOLDS = {
    #  key       threshold  direction  display label         unit
    "lapvar":  (4297.0,    ">",  "Laplacian Variance", ""),
    "entropy": (6.66,      ">",  "Histogram Entropy",  "bits"),
    "range":   (206,       ">",  "Pixel Range",        ""),
    "dpr":     (0.198,     "<",  "Dark Pixel Ratio",   ""),
    "spr":     (0.088,     "<",  "Sat. Pixel Ratio",   ""),
    "score":   (1800.0,    ">",  "Quality Score",      ""),
}


def decode_image_metrics(payload: bytes) -> dict:
    """Decode the 22-byte image quality metrics packet from the firmware.

    Wire layout (matches firmware camera.c send_image_metrics()):
      bytes  0–3  : lapvar_val   float32  Laplacian variance
      bytes  4–7  : entropy_val  float32  Histogram entropy
      bytes  8–11 : dpr_val      float32  Dark pixel ratio
      bytes 12–15 : spr_val      float32  Saturated pixel ratio
      ---- 16-byte fragment boundary ----
      bytes 16–19 : score_val    float32  Weighted quality score
      byte  20    : range_val    uint8    Pixel range (max − min)
      byte  21    : status       uint8    ImageStatus rejection code

    The reassembled payload is always 32 bytes (2 × 16-byte fragments)
    with bytes 22–31 zero-padded; only bytes 0–21 are read.

    Returns a dict with keys: status, status_name, lapvar, entropy,
    dpr, spr, score, range.  On decode failure returns a minimal dict
    with status=255 and an 'error' key.
    """
    if len(payload) < 22:
        return {"status": 255, "status_name": "Payload too short", "error": True}

    lapvar, entropy, dpr, spr, score = struct.unpack_from("<5f", payload, 0)
    range_val = payload[20]
    status    = payload[21]

    return {
        "status":      status,
        "status_name": _IMAGE_STATUS_NAMES.get(status, f"Unknown ({status})"),
        "lapvar":      lapvar,
        "entropy":     entropy,
        "dpr":         dpr,
        "spr":         spr,
        "score":       score,
        "range":       range_val,
    }


# ---------------------------------------------------------------------------
# BLE manager
# ---------------------------------------------------------------------------
class BLEManager:
    """Manages BLE scanning, connection, command transmission, and fragment reassembly.

    The class is callback-driven: every event of interest is delivered via one
    of the callables passed to __init__. Callbacks fire on the BLE event loop's
    background thread; callers that touch Tkinter widgets must marshal back onto
    the GUI thread (e.g. via root.after(0, ...)).

    Callbacks
    ---------
    on_log(msg: str)
        Called for every protocol event line — useful for a debug pane or console.
    on_data(result: dict)
        Called when a sensor payload is fully decoded.
        result maps GUI metric name → (value_str, unit). Value strings are already
        in the form expected by data_store.record_reading() and the graphing code
        (plain number, "lat°N,lon°E", "pitch,roll,yaw", etc.).
    on_image(cam_id: int, image: PIL.Image.Image)
        Called when a JPEG is fully assembled and decoded. cam_id is 1 or 2.
        The caller is responsible for saving the image and recording its path
        via data_store.record_reading().
    on_image_metrics(cam_id: int, metrics: dict)  [optional]
        Called when an image quality metrics packet is decoded, before the JPEG
        is received (or instead of it, if the image was rejected).
    """

    def __init__(self, on_log, on_data, on_image, on_image_metrics=None):
        self.client    = None
        self.connected = False
        self.on_log            = on_log
        self.on_data           = on_data
        self.on_image          = on_image
        self.on_image_metrics  = on_image_metrics  # optional; fired on every camera request

        self._fragments           = {}    # {frag_idx: payload_bytes} for current receive
        self._last_idx            = None  # fragment index of the last fragment received
        self._frag_event          = None  # asyncio.Event set by _on_notify when a frag arrives
        self._active_mask         = 0
        self._active_kind         = None  # "sensor" | "image" | "meta"

        # Buffer for JPEG fragments that arrive while _active_kind is still "meta".
        # The firmware sends the JPEG immediately after the meta last-fragment with
        # no inter-packet pause, so fragments 0 and 1 of the JPEG typically arrive
        # before _reset_transfer can switch _active_kind to "image". Without this
        # buffer those fragments would be dropped.
        self._early_image_fragments  = {}
        self._early_image_last_idx   = None

        # BLE work runs on its own asyncio event loop in a background thread.
        # Public methods schedule coroutines onto it via _run().
        self.loop   = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self._run_loop, daemon=True)
        self.thread.start()

    # ------------------------------------------------------------------
    # Public API (called from GUI thread)
    # ------------------------------------------------------------------
    def connect(self) -> None:
        self._run(self._scan_and_connect())

    def disconnect(self) -> None:
        self._run(self._disconnect())

    def request(self, mask: int) -> None:
        """Send a raw 24-bit mask to the firmware. Sensor and camera bits should
        not be mixed in one call — use request_metrics() for that, which sequences
        the two transactions so they cannot race on the BLE loop."""
        self._run(self._request(mask))

    def request_metrics(self, metric_names: list[str]) -> int:
        """Convert a list of GUI metric names to mask(s) and send.

        Sensor and camera requests are sequenced inside one coroutine so the
        camera command is never sent before the sensor ACK is complete. Returns
        the combined mask (sensor | camera) that was transmitted.
        """
        sensor_metrics = [m for m in metric_names
                        if m not in ("Camera 1", "Camera 2")]
        camera_metrics = [m for m in metric_names
                        if m in ("Camera 1", "Camera 2")]

        sensor_mask = metrics_to_mask(sensor_metrics)
        camera_mask = metrics_to_mask(camera_metrics)

        self._run(self._request_sequence(sensor_mask, camera_mask))
        return sensor_mask | camera_mask

    def request_image1(self) -> None:
        """Request a JPEG capture from Camera 1 (bit 5)."""
        self._run(self._request(1 << CAMERA1_BIT))

    def request_image2(self) -> None:
        """Request a JPEG capture from Camera 2 (bit 7)."""
        self._run(self._request(1 << CAMERA2_BIT))

    def request_both_images(self) -> None:
        """Request JPEG captures from both cameras. Firmware sends
        Camera 1 first, waits for ACK, then Camera 2."""
        self._run(self._request((1 << CAMERA1_BIT) | (1 << CAMERA2_BIT)))

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    def _run_loop(self):
        asyncio.set_event_loop(self.loop)
        self._frag_event = asyncio.Event()
        self.loop.run_forever()

    def _run(self, coro):
        """Schedule an async coroutine onto the BLE event loop thread."""
        asyncio.run_coroutine_threadsafe(coro, self.loop)

    def _reset_transfer(self, kind, mask, clear_fragments=True):
        """Reset fragment state before a new receive phase.

        clear_fragments=False keeps the _fragments dict intact for the
        cam2-after-cam1 case where we switch from "meta" to "image" kind
        without discarding already-received meta bytes.

        When switching to kind="image" with clear_fragments=False, any
        JPEG fragments buffered in _early_image_fragments (those that
        arrived while _active_kind was still "meta") are merged into
        _fragments, overwriting the stale meta payloads at the same indices.
        """
        if clear_fragments:
            self._fragments              = {}
            self._early_image_fragments  = {}
            self._early_image_last_idx   = None

        # Merge any early-buffered JPEG fragments into the main dict.
        # Their indices (0, 1, …) overwrite the stale meta payloads at the
        # same positions so _receive_one_response assembles the correct JPEG.
        early_last = None
        if kind == "image" and self._early_image_fragments:
            self._fragments.update(self._early_image_fragments)
            early_last = self._early_image_last_idx
            self._early_image_fragments = {}
            self._early_image_last_idx  = None

        self._last_idx    = early_last   # None in the common case
        self._active_mask = mask
        self._active_kind = kind
        self._frag_event.clear()

    # ------------------------------------------------------------------
    # BLE notification handler (called from Bleak's internal thread)
    # ------------------------------------------------------------------
    def _on_notify(self, sender, raw):
        data = bytes(raw)
        self.on_log(f"RX raw: {data.hex()}")

        if len(data) != 20:
            self.on_log("RX wrong length - dropping")
            return

        stx       = data[0]
        is_sensor = stx in (STX_SENSOR_MORE, STX_SENSOR_LAST)
        is_image  = stx in (STX_IMG_MORE,    STX_IMG_LAST)
        is_meta   = stx in (STX_META_MORE,   STX_META_LAST)

        if not (is_sensor or is_image or is_meta):
            # Check for optional ASCII debug string from firmware
            text = data[3:19].rstrip(b"\x00")
            try:
                decoded = text.decode("ascii")
                if decoded.startswith("DBG:"):
                    self.on_log(f"STM debug: {decoded}")
                    return
            except UnicodeDecodeError:
                pass
            self.on_log(f"RX unknown STX {stx:#04x} - dropping")
            return

        # CRC covers bytes 0-18; byte 19 is the CRC field
        if data[19] != calc_crc(data[:19]):
            self.on_log("RX CRC mismatch - dropping")
            return

        frag_idx = (data[1] << 8) | data[2]
        payload  = data[3:19]
        is_last  = stx in (STX_SENSOR_LAST, STX_IMG_LAST, STX_META_LAST)
        kind     = "meta" if is_meta else ("image" if is_image else "sensor")

        if self._active_kind is None:
            self._active_kind = kind
        elif self._active_kind != kind:
            # Special case: JPEG fragments arriving during the meta receive phase
            # are buffered separately and merged when we switch to image mode.
            # The firmware sends the JPEG immediately after the meta last-fragment
            # with no pause, so fragments 0 and 1 typically arrive before
            # _reset_transfer can switch _active_kind to "image". Buffering them
            # here prevents those early fragments from being silently dropped.
            if self._active_kind == "meta" and kind == "image":
                self._early_image_fragments[frag_idx] = payload
                if is_last:
                    self._early_image_last_idx = frag_idx
                self.on_log(f"RX early image frag {frag_idx} (buffered during meta)")
                return
            self.on_log(
                f"RX type mismatch (expected {self._active_kind}, "
                f"got {kind}) - dropping fragment"
            )
            return

        self._fragments[frag_idx] = payload
        if is_last:
            self._last_idx = frag_idx

        self.on_log(f"RX {kind} frag {frag_idx}{' (last)' if is_last else ''}")
        self.loop.call_soon_threadsafe(self._frag_event.set)

    # ------------------------------------------------------------------
    # Async internals
    # ------------------------------------------------------------------
    async def _send_ack(self):
        await self.client.write_gatt_char(CHAR_UUID, bytes([STX_ACK]), response=False)
        self.on_log("TX ACK")

    async def _scan_and_connect(self):
        self.on_log(f"Scanning for '{HM10_NAME}'...")
        device = await BleakScanner.find_device_by_name(HM10_NAME, timeout=6.0)
        if device is None:
            self.on_log("Device not found")
            return

        self.client = BleakClient(device.address)
        await self.client.connect()
        self.connected = self.client.is_connected

        if self.connected:
            await self.client.start_notify(CHAR_UUID, self._on_notify)
            self.on_log(f"Connected to {device.name}")
        else:
            self.on_log("Connection failed")

    async def _disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()
        self.connected = False
        self.on_log("Disconnected")

    async def _receive_one_response(self):
        """Block until all fragments of a single response are collected.

        Returns the reassembled raw payload (bytes), or None on timeout.
        The caller is responsible for sending ACK and resetting state
        before starting the next response.
        """
        while True:
            self._frag_event.clear()
            try:
                await asyncio.wait_for(self._frag_event.wait(), TIMEOUT_S)
            except asyncio.TimeoutError:
                self.on_log("Timeout waiting for fragment - transmission failed")
                return None

            if self._last_idx is not None:
                missing = [
                    i for i in range(self._last_idx + 1)
                    if i not in self._fragments
                ]
                if not missing:
                    break   # all fragments received

        return b"".join(self._fragments[i] for i in range(self._last_idx + 1))
    
    async def _request_sequence(self, sensor_mask, camera_mask):
        """Run sensor and camera requests back-to-back on the BLE loop so
        the camera command is never sent until the sensor ACK is done."""
        if sensor_mask:
            await self._request(sensor_mask)
        if camera_mask:
            await self._request(camera_mask)


    async def _request(self, mask):
        if not self.client or not self.client.is_connected:
            self.on_log("Not connected")
            return

        # bit 23 keeps the firmware out of standby when the real mask is zero
        tx_mask = mask if mask != 0 else STANDBY_MASK

        # Determine which cameras (if any) are requested.
        # The firmware always processes CAMERA1 first, then CAMERA2.
        cam_bits = [b for b in (CAMERA1_BIT, CAMERA2_BIT) if mask & (1 << b)]
        is_sensor = len(cam_bits) == 0

        # Camera requests always begin with a meta (quality-metrics) packet;
        # sensor requests begin directly with a sensor data payload.
        kind = "sensor" if is_sensor else "meta"
        self._reset_transfer(kind=kind, mask=mask)

        cmd = build_command(tx_mask)
        self.on_log(f"TX command mask={tx_mask:024b}")
        await self.client.write_gatt_char(CHAR_UUID, cmd, response=False)

        if mask == 0:
            await self._send_ack()
            self.on_log("Zero mask - standby ping sent, no response expected")
            return

        # ---- Sensor response (single payload) ----
        if is_sensor:
            payload = await self._receive_one_response()
            if payload is None:
                return
            await self._send_ack()
            try:
                result = decode_sensor_payload(payload, mask)
                self.on_data(result)
            except Exception as exc:
                self.on_log(f"Sensor decode error: {exc}")
            return

        # ---- Camera response(s) ----
        # For each requested camera the firmware sends:
        #   Phase 1 — a 22-byte image quality metrics packet (STX_META_*)
        #   Phase 2 — the JPEG byte stream (STX_IMG_*), only if quality passed
        # If quality checks failed the JPEG is omitted; the GUI learns this from
        # the status byte in the metrics packet and skips the JPEG receive phase.
        for cam_idx, cam_bit in enumerate(cam_bits):
            cam_id = 1 if cam_bit == CAMERA1_BIT else 2

            if cam_idx > 0:
                # Reset to receive the next camera's meta packet.
                # clear_fragments=False preserves the fragment dict; stale entries
                # from the previous camera's meta/image are overwritten by kind mismatch
                # dropping in _on_notify, or by the early-image merge in _reset_transfer.
                self._reset_transfer(kind="meta", mask=mask, clear_fragments=False)

            # Phase 1 — receive the metrics packet (always present)
            meta_payload = await self._receive_one_response()
            if meta_payload is None:
                return

            metrics = decode_image_metrics(meta_payload)
            self.on_log(
                f"Cam{cam_id} quality: {metrics['status_name']} "
                f"(lapvar={metrics.get('lapvar', 0):.0f}, "
                f"entropy={metrics.get('entropy', 0):.2f}, "
                f"score={metrics.get('score', 0):.0f})"
            )
            if self.on_image_metrics:
                self.on_image_metrics(cam_id, metrics)

            # Phase 2 — receive the JPEG only if the image passed
            if metrics["status"] == 0:  # IMAGE_OK
                self._reset_transfer(kind="image", mask=mask, clear_fragments=False)
                payload = await self._receive_one_response()
                if payload is None:
                    await self._send_ack()
                    return
                await self._send_ack()
                try:
                    self._handle_image_payload(payload, cam_id)
                except Exception as exc:
                    self.on_log(f"Image decode error (cam{cam_id}): {exc}")
            else:
                self.on_log(f"Cam{cam_id} image rejected — no JPEG expected")
                await self._send_ack()

    def _handle_image_payload(self, payload: bytes, cam_id: int) -> None:
        """Decode a reassembled JPEG payload and fire on_image.

        Expected layout: b'JPEG' magic header followed by raw JPEG bytes.
        The last fragment may be zero-padded; PIL's decoder stops at the
        EOI marker (0xFF 0xD9) so trailing zeros are harmless, and
        ImageFile.LOAD_TRUNCATED_IMAGES=True is set at module level to
        suppress the OSError that PIL would otherwise raise.
        """
        if len(payload) < 4 or payload[:4] != b"JPEG":
            raise ValueError("Image header missing 'JPEG' tag")

        jpeg_bytes = payload[4:]
        image = Image.open(BytesIO(jpeg_bytes))
        self.on_log(
            f"Cam{cam_id} image complete: "
            f"{image.width}x{image.height}, {len(jpeg_bytes)} bytes"
        )
        self.on_image(cam_id, image)
