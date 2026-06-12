"""Graph panel — time-series matplotlib graphs for all sensor metrics.

GraphPanel owns a tk.Frame and renders into it on demand. All drawing
runs on background threads; results are marshalled back to the Tk thread
via root.after(0, ...) before any widget touches happen.
"""

import tkinter as tk
import threading
from datetime import datetime, timedelta

from ui import mpl_loader
from ui.constants import (
    BG_DARK, BG_PANEL, BG_CARD,
    BORDER, PURPLE_LT, PURPLE_LINE,
    YELLOW, TEXT_SEC, TEXT_MUT,
    GREEN,
    FM, FU,
    TIME_SECS, TEMP_EXTERNAL_CHANNELS,
)
from data_store import load_history, METRIC_RANGES, TILT_CHANNELS, IMU_CHANNELS


# ---------------------------------------------------------------------------
# Shared axis-styling helpers (eliminate ~9-line copy-paste per graph)
# ---------------------------------------------------------------------------

def _apply_dark_axes(ax, fig, cut_secs) -> None:
    """Apply the standard dark-theme spine, grid, and date-formatter styling.

    Removes top/right spines, colours the remaining spines and tick labels,
    sets an appropriate date format (HH:MM:SS for short windows, MM/DD HH:MM
    for windows longer than one day), and rotates x-axis labels by 25°.
    """
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)
    for sp in ("left", "bottom"):
        ax.spines[sp].set_color(BORDER)
    ax.tick_params(colors=TEXT_MUT, labelsize=10)
    ax.tick_params(axis="x", labelsize=9)
    fmt = "%m/%d %H:%M" if (cut_secs or 0) > 86400 else "%H:%M:%S"
    ax.xaxis.set_major_locator(mpl_loader.AutoDateLocator(maxticks=6))
    ax.xaxis.set_major_formatter(mpl_loader.DateFormatter(fmt))
    fig.autofmt_xdate(rotation=25)
    ax.grid(True, color=BORDER, alpha=0.3, linewidth=0.5)


def _style_legend(leg) -> None:
    """Apply dark-theme colours to an axes legend."""
    leg.get_frame().set_facecolor(BG_CARD)
    leg.get_frame().set_edgecolor(BORDER)
    for text in leg.get_texts():
        text.set_color("#eae8f0")  # TEXT_PRI — avoid circular import from constants


def _load_series(channels: list, cut_secs) -> dict:
    """Load (times, values) pairs for each channel name, applying the time cutoff.

    Returns {channel_name: ([datetime, ...], [float, ...])}. Rows that fail
    float conversion or fall outside the cutoff window are silently skipped.
    """
    result = {}
    for ch in channels:
        times, vals = [], []
        for row in load_history(ch):
            try:
                ts = datetime.strptime(row["timestamp"], "%Y-%m-%d %H:%M:%S")
                v  = float(row["value"])
            except Exception:
                continue
            if cut_secs is not None:
                if ts < datetime.now() - timedelta(seconds=cut_secs):
                    continue
            times.append(ts)
            vals.append(v)
        result[ch] = (times, vals)
    return result


# ---------------------------------------------------------------------------
# GraphPanel
# ---------------------------------------------------------------------------

class GraphPanel:
    """Manages graph rendering for all non-GPS, non-camera metrics.

    Parameters
    ----------
    container : tk.Frame
        Frame to pack the graph into (already in the window hierarchy).
    root : tk.Tk
        Root window — needed for root.after() thread-safe callbacks.
    get_time_range : callable -> str
        Returns the currently selected time-range label (e.g. "1 hr").
    """

    def __init__(self, container: tk.Frame, root: tk.Tk, get_time_range) -> None:
        self._frame = container
        self._root = root
        self._get_time_range = get_time_range
        self._busy = False
        self._pending: str | None = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def draw(self, metric_name: str) -> None:
        """Kick off a background render for *metric_name*.

        If a render is already in flight the new request is queued as
        _pending and executed immediately when the current one finishes.
        Only one request can be pending at a time; rapid successive calls
        for different metrics keep only the most recent pending request.
        """
        if self._busy:
            self._pending = metric_name
            return

        self._busy = True
        self._pending = None

        for w in self._frame.winfo_children():
            w.destroy()
        tk.Label(self._frame, text="Rendering…", bg=BG_DARK,
                 fg=TEXT_MUT, font=(FU, 12)).pack(expand=True)

        if not mpl_loader.HAS_MPL:
            self._busy = False
            for w in self._frame.winfo_children():
                w.destroy()
            tk.Label(self._frame, text="[pip3 install matplotlib]",
                     bg=BG_DARK, fg=TEXT_MUT, font=(FU, 13)).pack(expand=True)
            return

        cut = TIME_SECS.get(self._get_time_range())
        threading.Thread(
            target=self._build, args=(metric_name, cut), daemon=True
        ).start()

    def clear(self) -> None:
        for w in self._frame.winfo_children():
            w.destroy()

    @property
    def is_busy(self) -> bool:
        return self._busy

    # ------------------------------------------------------------------
    # Thread-safe attach / message helpers
    # ------------------------------------------------------------------

    def _show_msg(self, msg: str) -> None:
        """Replace the graph area with a plain text message (must be called on the main thread)."""
        if not self._frame.winfo_exists():
            return
        for w in self._frame.winfo_children():
            w.destroy()
        tk.Label(self._frame, text=msg, bg=BG_DARK, fg=TEXT_MUT,
                 font=(FU, 13), justify="center").pack(expand=True)
        self._finish()

    def _attach(self, fig) -> None:
        """Embed a matplotlib Figure into the graph frame (must be called on the main thread)."""
        if not self._frame.winfo_exists():
            self._finish()
            return
        for w in self._frame.winfo_children():
            w.destroy()
        canvas = mpl_loader.FigureCanvasTkAgg(fig, master=self._frame)
        canvas.draw()
        canvas.get_tk_widget().pack(fill="both", expand=True)
        self._finish()

    def _finish(self) -> None:
        """Mark the current render complete and start the pending one if any."""
        self._busy = False
        if self._pending:
            nxt = self._pending
            self._pending = None
            self.draw(nxt)

    # ------------------------------------------------------------------
    # Background build dispatcher (runs on worker thread)
    # ------------------------------------------------------------------

    def _build(self, metric_name: str, cut) -> None:
        """Dispatch to the appropriate per-metric graph builder.

        Runs on a background thread. Results are posted to the main thread
        via root.after(0, ...) inside each builder.
        """
        mpl_loader.load_mpl()

        if metric_name == "UV Index":
            self._draw_uv_lux(cut)
        elif metric_name == "Battery":
            self._draw_battery(cut)
        elif metric_name == "Tilt":
            self._draw_tilt(cut)
        elif metric_name == "IMU":
            self._draw_imu(cut)
        elif metric_name == "Temp External":
            self._draw_temp_external(cut)
        else:
            self._draw_scalar(metric_name, cut)

    # ------------------------------------------------------------------
    # Individual graph builders (all run on background thread)
    # ------------------------------------------------------------------

    def _draw_uv_lux(self, cut) -> None:
        series = {}
        for name in ("UV Index", "Lux"):
            times, values = [], []
            for row in load_history(name):
                try:
                    ts = datetime.strptime(row["timestamp"], "%Y-%m-%d %H:%M:%S")
                    v  = float(row["value"])
                except Exception:
                    continue
                if cut is not None and ts < datetime.now() - timedelta(seconds=cut):
                    continue
                times.append(ts); values.append(v)
            series[name] = (times, values)

        if not series["UV Index"][1] and not series["Lux"][1]:
            self._root.after(0, lambda: self._show_msg("No numeric data."))
            return

        fig = mpl_loader.Figure(figsize=(8, 4), dpi=100, facecolor=BG_DARK)
        ax_uv  = fig.add_subplot(111)
        ax_lux = ax_uv.twinx()
        ax_uv.set_facecolor(BG_PANEL)
        fig.subplots_adjust(left=0.08, right=0.88, top=0.90, bottom=0.18)

        uv_t, uv_v   = series["UV Index"]
        lux_t, lux_v = series["Lux"]
        if uv_v:
            ax_uv.plot(uv_t, uv_v, color=PURPLE_LINE, linewidth=2,
                       marker="o", markersize=4, label="UV Index")
        if lux_v:
            ax_lux.plot(lux_t, lux_v, color=YELLOW, linewidth=2,
                        marker="o", markersize=4, label="Lux")

        ax_uv.set_title("UV Index + Lux", pad=10, color=PURPLE_LT,
                        fontsize=14, fontweight="bold", fontfamily="Arial")
        ax_uv.set_ylabel("UV",   color=PURPLE_LINE, fontsize=11, fontfamily="Arial")
        ax_lux.set_ylabel("lux", color=YELLOW,      fontsize=11, fontfamily="Arial")
        ax_uv.tick_params(colors=TEXT_MUT, labelsize=10)
        ax_lux.tick_params(colors=TEXT_MUT, labelsize=10)
        ax_uv.tick_params(axis="x", labelsize=9)

        for sp in ("top",):
            ax_uv.spines[sp].set_visible(False)
            ax_lux.spines[sp].set_visible(False)
        for sp in ("left", "bottom"):
            ax_uv.spines[sp].set_color(BORDER)
        ax_lux.spines["right"].set_color(BORDER)

        fmt = "%m/%d %H:%M" if (cut or 0) > 86400 else "%H:%M:%S"
        ax_uv.xaxis.set_major_locator(mpl_loader.AutoDateLocator(maxticks=6))
        ax_uv.xaxis.set_major_formatter(mpl_loader.DateFormatter(fmt))
        fig.autofmt_xdate(rotation=25)
        ax_uv.grid(True, color=BORDER, alpha=0.3, linewidth=0.5)

        self._root.after(0, lambda: self._attach(fig))

    def _draw_battery(self, cut) -> None:
        names = ("Battery", "Voltage", "Current Draw")
        series = {}
        for name in names:
            times, values = [], []
            for row in load_history(name):
                try:
                    ts = datetime.strptime(row["timestamp"], "%Y-%m-%d %H:%M:%S")
                    v  = float(row["value"])
                except Exception:
                    continue
                if cut is not None and ts < datetime.now() - timedelta(seconds=cut):
                    continue
                times.append(ts); values.append(v)
            series[name] = (times, values)

        if not any(v for _, v in series.values()):
            self._root.after(0, lambda: self._show_msg("No numeric data."))
            return

        fig    = mpl_loader.Figure(figsize=(8, 4), dpi=100, facecolor=BG_DARK)
        ax_pct = fig.add_subplot(111)
        ax_volt    = ax_pct.twinx()
        ax_current = ax_pct.twinx()
        ax_current.spines["right"].set_position(("axes", 1.12))
        ax_pct.set_facecolor(BG_PANEL)
        fig.subplots_adjust(left=0.08, right=0.78, top=0.90, bottom=0.18)

        pct_t, pct_v   = series["Battery"]
        volt_t, volt_v = series["Voltage"]
        cur_t, cur_v   = series["Current Draw"]

        lines = []
        if pct_v:
            lines += ax_pct.plot(pct_t, pct_v, color=PURPLE_LINE, linewidth=2,
                                 marker="o", markersize=4, label="Battery %")
        if volt_v:
            lines += ax_volt.plot(volt_t, volt_v, color=YELLOW, linewidth=2,
                                  marker="o", markersize=4, label="Voltage")
        if cur_v:
            lines += ax_current.plot(cur_t, cur_v, color=GREEN, linewidth=2,
                                     marker="o", markersize=4, label="Current Draw")

        ax_pct.set_title("Battery + Voltage + Current Draw", pad=10,
                         color=PURPLE_LT, fontsize=14, fontweight="bold",
                         fontfamily="Arial")
        ax_pct.set_ylabel("%",  color=PURPLE_LINE, fontsize=11, fontfamily="Arial")
        ax_volt.set_ylabel("V", color=YELLOW,      fontsize=11, fontfamily="Arial")
        ax_current.set_ylabel("A", color=GREEN,    fontsize=11, fontfamily="Arial")
        for ax in (ax_pct, ax_volt, ax_current):
            ax.tick_params(colors=TEXT_MUT, labelsize=10)
            ax.spines["top"].set_visible(False)
        for sp in ("left", "bottom"):
            ax_pct.spines[sp].set_color(BORDER)
        ax_volt.spines["right"].set_color(BORDER)
        ax_current.spines["right"].set_color(BORDER)
        ax_pct.tick_params(axis="x", labelsize=9)

        if lines:
            ax_pct.legend(lines, [l.get_label() for l in lines],
                          loc="upper left", facecolor=BG_PANEL,
                          edgecolor=BORDER, labelcolor="#eae8f0", fontsize=9)

        fmt = "%m/%d %H:%M" if (cut or 0) > 86400 else "%H:%M:%S"
        ax_pct.xaxis.set_major_locator(mpl_loader.AutoDateLocator(maxticks=6))
        ax_pct.xaxis.set_major_formatter(mpl_loader.DateFormatter(fmt))
        fig.autofmt_xdate(rotation=25)
        ax_pct.grid(True, color=BORDER, alpha=0.3, linewidth=0.5)

        self._root.after(0, lambda: self._attach(fig))

    def _draw_tilt(self, cut) -> None:
        series = _load_series(TILT_CHANNELS, cut)

        if not any(v for _, v in series.values()):
            self._root.after(0, lambda: self._show_msg("No data – press ► Request Data"))
            return

        (tx_t, tx_v), (ty_t, ty_v) = (
            series["Tilt X"], series["Tilt Y"]
        )

        fig   = mpl_loader.Figure(figsize=(8, 4), dpi=100, facecolor=BG_DARK)
        ax_x  = fig.add_subplot(111)
        ax_y  = ax_x.twinx()
        ax_x.set_facecolor(BG_PANEL)
        fig.subplots_adjust(left=0.08, right=0.88, top=0.90, bottom=0.18)

        lines = []
        if tx_v:
            lines += ax_x.plot(tx_t, tx_v, color=PURPLE_LINE, linewidth=2,
                               marker="o", markersize=4, label="Tilt X")
        if ty_v:
            lines += ax_y.plot(ty_t, ty_v, color=YELLOW, linewidth=2,
                               marker="o", markersize=4, label="Tilt Y")

        ax_x.set_title("Tilt X / Y", pad=10, color=PURPLE_LT,
                       fontsize=14, fontweight="bold", fontfamily="Arial")
        ax_x.set_ylabel("Tilt X (°)", color=PURPLE_LINE, fontsize=11, fontfamily="Arial")
        ax_y.set_ylabel("Tilt Y (°)", color=YELLOW,      fontsize=11, fontfamily="Arial")

        for ax in (ax_x, ax_y):
            ax.tick_params(colors=TEXT_MUT, labelsize=10)
            ax.spines["top"].set_visible(False)
        for sp in ("left", "bottom"):
            ax_x.spines[sp].set_color(BORDER)
        ax_y.spines["right"].set_color(BORDER)
        ax_x.tick_params(axis="x", labelsize=9)

        fmt = "%m/%d %H:%M" if (cut or 0) > 86400 else "%H:%M:%S"
        ax_x.xaxis.set_major_locator(mpl_loader.AutoDateLocator(maxticks=6))
        ax_x.xaxis.set_major_formatter(mpl_loader.DateFormatter(fmt))
        fig.autofmt_xdate(rotation=25)
        ax_x.grid(True, color=BORDER, alpha=0.3, linewidth=0.5)

        if lines:
            leg = ax_x.legend(lines, [l.get_label() for l in lines],
                              loc="upper left", facecolor=BG_PANEL,
                              edgecolor=BORDER, labelcolor="#eae8f0", fontsize=9)

        self._root.after(0, lambda: self._attach(fig))

    def _draw_imu(self, cut) -> None:
        series = _load_series(IMU_CHANNELS, cut)

        if not any(v for _, v in series.values()):
            self._root.after(0, lambda: self._show_msg("No data – press ► Request Data"))
            return

        p_t, p_v = series["IMU Pitch"]
        r_t, r_v = series["IMU Roll"]
        y_t, y_v = series["IMU Yaw"]

        fig   = mpl_loader.Figure(figsize=(8, 4), dpi=100, facecolor=BG_DARK)
        ax_p  = fig.add_subplot(111)
        ax_r  = ax_p.twinx()
        ax_y  = ax_p.twinx()
        ax_y.spines["right"].set_position(("axes", 1.12))
        ax_p.set_facecolor(BG_PANEL)
        fig.subplots_adjust(left=0.08, right=0.78, top=0.90, bottom=0.18)

        lines = []
        if p_v:
            lines += ax_p.plot(p_t, p_v, color="#FF6B9D", linewidth=2,
                               marker="o", markersize=4, label="Pitch")
        if r_v:
            lines += ax_r.plot(r_t, r_v, color="#45D1A3", linewidth=2,
                               marker="o", markersize=4, label="Roll")
        if y_v:
            lines += ax_y.plot(y_t, y_v, color=YELLOW, linewidth=2,
                               marker="o", markersize=4, label="Yaw")

        ax_p.set_title("IMU — Pitch / Roll / Yaw", pad=10, color=PURPLE_LT,
                       fontsize=14, fontweight="bold", fontfamily="Arial")
        ax_p.set_ylabel("Pitch (°)", color="#FF6B9D", fontsize=11, fontfamily="Arial")
        ax_r.set_ylabel("Roll (°)",  color="#45D1A3", fontsize=11, fontfamily="Arial")
        ax_y.set_ylabel("Yaw (°)",   color=YELLOW,    fontsize=11, fontfamily="Arial")

        for ax in (ax_p, ax_r, ax_y):
            ax.tick_params(colors=TEXT_MUT, labelsize=10)
            ax.spines["top"].set_visible(False)
        for sp in ("left", "bottom"):
            ax_p.spines[sp].set_color(BORDER)
        ax_r.spines["right"].set_color(BORDER)
        ax_y.spines["right"].set_color(BORDER)
        ax_p.tick_params(axis="x", labelsize=9)

        fmt = "%m/%d %H:%M" if (cut or 0) > 86400 else "%H:%M:%S"
        ax_p.xaxis.set_major_locator(mpl_loader.AutoDateLocator(maxticks=6))
        ax_p.xaxis.set_major_formatter(mpl_loader.DateFormatter(fmt))
        fig.autofmt_xdate(rotation=25)
        ax_p.grid(True, color=BORDER, alpha=0.3, linewidth=0.5)

        if lines:
            ax_p.legend(lines, [l.get_label() for l in lines],
                        loc="upper left", facecolor=BG_PANEL,
                        edgecolor=BORDER, labelcolor="#eae8f0", fontsize=9)

        self._root.after(0, lambda: self._attach(fig))

    def _draw_temp_external(self, cut) -> None:
        """Plot all 4 thermistor channels on a shared y-axis plus a computed average.

        The average is computed only at timestamps where all 4 channels reported,
        so it never mixes readings from different request cycles.
        """
        ch_colors = {
            "Temp External CH1": PURPLE_LINE,
            "Temp External CH2": YELLOW,
            "Temp External CH3": "#FF6B9D",
            "Temp External CH4": "#45D1A3",
        }
        series = _load_series(TEMP_EXTERNAL_CHANNELS, cut)

        if not any(v for _, v in series.values()):
            self._root.after(0, lambda: self._show_msg("No data – press ► Request Data"))
            return

        # Compute average at timestamps where all 4 channels reported
        ts_buckets: dict = {}
        for ch, (times, vals) in series.items():
            for t, v in zip(times, vals):
                ts_buckets.setdefault(t, []).append(v)
        full_ts = sorted(t for t, vs in ts_buckets.items()
                         if len(vs) == len(TEMP_EXTERNAL_CHANNELS))
        avg_vals = [sum(ts_buckets[t]) / len(ts_buckets[t]) for t in full_ts]

        fig = mpl_loader.Figure(figsize=(8, 4), dpi=100, facecolor=BG_DARK)
        ax  = fig.add_subplot(111)
        ax.set_facecolor(BG_PANEL)
        fig.subplots_adjust(left=0.08, right=0.88, top=0.90, bottom=0.18)

        for ch, (times, vals) in series.items():
            if vals:
                label = ch.replace("Temp External ", "")
                ax.plot(times, vals, color=ch_colors[ch], linewidth=1.5,
                        marker="o", markersize=3, label=label, zorder=3)

        if avg_vals:
            ax.plot(full_ts, avg_vals, color="#eae8f0", linewidth=2,
                    linestyle="--", marker="D", markersize=4,
                    label="Average", zorder=5)

        ax.set_title("Temp External (4 Channels + Average)", pad=10,
                     color=PURPLE_LT, fontsize=14, fontweight="bold",
                     fontfamily="Arial")
        ax.set_ylabel("°C", color=TEXT_SEC, fontsize=11, fontfamily="Arial")
        _apply_dark_axes(ax, fig, cut)
        _style_legend(ax.legend(loc="upper right", frameon=True, fontsize=9))

        self._root.after(0, lambda: self._attach(fig))

    def _draw_scalar(self, metric_name: str, cut) -> None:
        """Plot a single time-series line with a dashed average annotation.

        For vector metrics (GPS, IMU) the displayed value is the mean of the
        comma-separated components, which avoids trying to plot multi-valued
        strings as a single line.
        """
        history = load_history(metric_name)

        if not history:
            self._root.after(0, lambda: self._show_msg("No data – press ► Request Data"))
            return

        def _scalar_value(raw: str) -> float:
            if "," in raw:
                parts = [float(x) for x in raw.split(",")]
                return sum(parts) / len(parts)
            return float(raw)

        times, values = [], []
        for row in history:
            try:
                ts = datetime.strptime(row["timestamp"], "%Y-%m-%d %H:%M:%S")
                v  = _scalar_value(row["value"])
            except Exception:
                continue
            times.append(ts); values.append(v)

        if not values:
            self._root.after(0, lambda: self._show_msg("No numeric data."))
            return

        if cut is not None:
            cutoff = datetime.now() - timedelta(seconds=cut)
            pairs  = [(t, v) for t, v in zip(times, values) if t >= cutoff]
            if pairs:
                times, values = map(list, zip(*pairs))

        avg  = sum(values) / len(values)
        unit = METRIC_RANGES.get(metric_name, (None, None, None, ""))[3]

        fig = mpl_loader.Figure(figsize=(8, 4), dpi=100, facecolor=BG_DARK)
        ax  = fig.add_subplot(111)
        ax.set_facecolor(BG_PANEL)
        fig.subplots_adjust(left=0.08, right=0.88, top=0.90, bottom=0.18)

        ax.plot(times, values, color=PURPLE_LINE, linewidth=2,
                marker="o", markersize=4,
                markerfacecolor=YELLOW, markeredgecolor=YELLOW,
                markeredgewidth=0.5, zorder=3)
        ax.fill_between(times, values, alpha=0.06, color=PURPLE_LINE)
        ax.axhline(y=avg, color=YELLOW, linestyle="--", linewidth=1, alpha=0.55)
        ax.annotate(f"avg {avg:.2f}", xy=(1.01, avg),
                    xycoords=("axes fraction", "data"),
                    color=YELLOW, fontsize=9, fontfamily="monospace",
                    va="center", annotation_clip=False)

        ax.set_title(metric_name, pad=10, color=PURPLE_LT,
                     fontsize=14, fontweight="bold", fontfamily="Arial")
        ax.set_ylabel(unit, color=TEXT_SEC, fontsize=11, fontfamily="Arial")
        _apply_dark_axes(ax, fig, cut)

        self._root.after(0, lambda: self._attach(fig))