"""GPS panel — renders the last N GPS fixes as map markers."""

import tkinter as tk

try:
    import tkintermapview
    HAS_MAPVIEW = True
except ImportError:
    HAS_MAPVIEW = False

from ui.constants import BG_DARK, TEXT_MUT, FU, GPS_MAX_POINTS


class GPSPanel:
    """Wraps a tkintermapview widget inside *container*.

    Call update(gps_history) whenever new GPS data arrives.
    Call destroy() when the panel is being torn down (e.g. the user
    closes the GPS detail view) to reset internal state.
    """

    def __init__(self, container: tk.Frame) -> None:
        self._container = container
        self._map_widget = None
        self._map_markers: list = []
        self._map_initialised = False

        if not HAS_MAPVIEW:
            tk.Label(container, text="pip install tkintermapview",
                     bg=BG_DARK, fg=TEXT_MUT, font=(FU, 13)).pack(expand=True)

    def update(self, gps_history: list) -> None:
        """Refresh the map with *gps_history* (list of {lat, lon, ts} dicts)."""
        if not HAS_MAPVIEW:
            return

        if self._map_widget is None or not self._map_widget.winfo_exists():
            for w in self._container.winfo_children():
                w.destroy()
            self._map_widget = tkintermapview.TkinterMapView(
                self._container, corner_radius=0)
            self._map_widget.pack(fill="both", expand=True)
            self._map_markers = []

        mw = self._map_widget
        for m in self._map_markers:
            try:
                m.delete()
            except Exception:
                pass
        self._map_markers = []

        gps_points = list(gps_history)
        if not gps_points:
            if not self._map_initialised:
                mw.set_position(0, 0)
                mw.set_zoom(2)
            return

        # Colour scale: oldest fix = yellow, middle = orange, newest = red.
        # GPS_MAX_POINTS is 3, so offset maps index 0→yellow, 1→orange, 2→red.
        colors = ["yellow", "orange", "red"]
        offset = GPS_MAX_POINTS - len(gps_points)
        for i, pt in enumerate(gps_points):
            ns = "S" if pt["lat"] < 0 else "N"
            ew = "W" if pt["lon"] < 0 else "E"
            label = f"{abs(pt['lat']):.5f}° {ns},  {abs(pt['lon']):.5f}° {ew}"
            try:
                marker = mw.set_marker(
                    pt["lat"], pt["lon"],
                    text=label,
                    marker_color_circle=colors[i + offset],
                    marker_color_outside=colors[i + offset],
                )
                self._map_markers.append(marker)
            except Exception as e:
                pass  # marker placement failed silently; map remains functional

        latest = gps_points[-1]
        mw.set_position(latest["lat"], latest["lon"])
        if not self._map_initialised:
            mw.set_zoom(10)
            self._map_initialised = True

        mw.update_idletasks()

    def destroy(self) -> None:
        """Reset internal references when the detail view is closed."""
        self._map_widget = None
        self._map_markers = []
        self._map_initialised = False