"""
Shared constants for the ArcticSense Buoy Monitor UI.

Centralises the colour palette, fonts, layout dimensions, metric lists,
and time-range configuration so all panels and widgets pull from one
source. Changing a value here propagates to every file that imports it.
"""

# =====================================================================
# Colour palette
# =====================================================================
BG_DARK        = "#111214"
BG_PANEL       = "#1a1b1f"
BG_CARD        = "#222328"
BG_CARD_HVR    = "#2e2f36"
BG_SELECTED    = "#2d1f3a"

PURPLE         = "#003699"
PURPLE_LT      = "#599DB6"
PURPLE_DIM     = "#222c55"
PURPLE_LINE    = "#4481AD"

YELLOW         = "#FFB81C"
YELLOW_DIM     = "#a37812"

TEXT_PRI       = "#eae8f0"
TEXT_SEC       = "#9590a8"
TEXT_MUT       = "#5a5770"
BORDER         = "#36304a"

RED            = "#e5484d"
GREEN          = "#46a758"

BTN_BG         = "#2a2b32"
BTN_BG_HVR     = "#3a3b44"
BTN_PURPLE_HVR = "#7B2FBE"

# Map-specific colours (GPS panel)
MAP_OCEAN      = "#141820"
MAP_LAND       = "#2a2540"
MAP_LAND_EDGE  = "#44396a"
MAP_GRID       = "#222040"

# =====================================================================
# Fonts
# =====================================================================
FM = "Courier New"   # monospace: metric codes, values, timestamps, IDs
FU = "Arial"         # proportional: labels, buttons, section titles

# =====================================================================
# Layout dimensions (px)
# =====================================================================
RIGHT_W  = 270   # width of the right sensor-card panel
BOTTOM_H = 180   # height of the bottom sensor-card strip
TOP_H    = 45    # height of the top toolbar

# =====================================================================
# Metric lists
# =====================================================================
# RIGHT_METRICS appear in the scrollable right panel (taller cards).
# BOTTOM_METRICS appear in the horizontally-scrollable bottom strip.
RIGHT_METRICS  = ["Camera 1", "Camera 2", "EMI", "Turbidity", "Salinity", "Battery"]
BOTTOM_METRICS = ["GPS", "Temp External", "Temp Internal (Static)", "Temp Internal (Dynamic)", "IMU", "UV Index", "Tilt"]
ALL_METRICS = RIGHT_METRICS + BOTTOM_METRICS

# Sub-channel names for the 4-channel thermistor array.
# Used when splitting a Temp External payload into per-channel CSV rows.
TEMP_EXTERNAL_CHANNELS = [
    "Temp External CH1",
    "Temp External CH2",
    "Temp External CH3",
    "Temp External CH4",
]

# =====================================================================
# Time range configuration
# =====================================================================
TIME_RANGES = ["30 min", "1 hr", "1 week", "All"]
# TIME_SECS maps each label to its cutoff in seconds (None = no cutoff).
TIME_SECS: dict = {
    "30 min": 1800,
    "1 hr":   3600,
    "1 week": 604800,
    "All":    None,
}
REFRESH_MS = 2500   # auto-refresh interval for sensor card values (ms)

# Maximum number of GPS fixes shown simultaneously on the map.
# Fixes are coloured oldest→newest as yellow→orange→red.
GPS_MAX_POINTS = 3