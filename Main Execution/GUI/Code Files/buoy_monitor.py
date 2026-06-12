"""
ArcticSense Buoy Monitor — main application.

Builds the Tkinter window, wires BLE callbacks from ble_manager.BLEManager,
routes user actions to the appropriate panel, and maintains the sensor card
display. All panel rendering lives in ui/panels/ so this file stays focused
on orchestration.

Threading model
---------------
Main thread   — Tkinter event loop (root.mainloop())
BLE thread    — asyncio event loop running Bleak coroutines (inside BLEManager)
Graph threads — one per GraphPanel.draw() call; results posted via root.after()

BLE callbacks (on_log, on_data, on_image, on_image_metrics) are scheduled
back onto the main thread with root.after(0, ...) inside BLEManager, so this
file never calls Tkinter from the BLE thread.
"""

import tkinter as tk
from tkinter import ttk, messagebox
import os, platform, subprocess, threading, queue
from datetime import datetime

from data_store import (
    METRIC_CODES, IMAGE_HISTORY_DIR,
    load_history, clear_csv,
    new_instance_id, build_metric_id, record_reading, record_readings,
    TILT_CHANNELS, IMU_CHANNELS,
)
from ble_manager import BLEManager

from ui.constants import (
    BG_DARK, BG_PANEL, BG_CARD,
    BORDER, PURPLE, PURPLE_DIM, PURPLE_LT,
    BTN_BG, BTN_BG_HVR, BTN_PURPLE_HVR,
    YELLOW, RED, GREEN,
    TEXT_PRI, TEXT_SEC, TEXT_MUT,
    FM, FU,
    RIGHT_W, BOTTOM_H, TOP_H,
    RIGHT_METRICS, BOTTOM_METRICS, ALL_METRICS,
    TEMP_EXTERNAL_CHANNELS, TIME_RANGES, REFRESH_MS, GPS_MAX_POINTS,
)
from ui.widgets.flat_button import FlatButton
from ui.widgets.metric_card import MetricCard
from ui import mpl_loader
from ui.panels.camera_panel import show_camera
from ui.panels.gps_panel import GPSPanel
from ui.panels.data_log_panel import DataLogPanel
from ui.panels.graph_panel import GraphPanel

_DIR = os.path.dirname(os.path.abspath(__file__))


class BuoyMonitorApp:

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("ArcticSense Buoy Monitor")
        self.root.configure(bg=BG_DARK)

        sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
        ww = max(1200, int(sw * 0.85))
        wh = max(750,  int(sh * 0.82))
        root.geometry(f"{ww}x{wh}+{(sw-ww)//2}+{max(0,(sh-wh)//2-30)}")
        root.minsize(1000, 650)

        self.selected_metric: str | None = None
        self.cards: dict[str, MetricCard] = {}
        self.checked: dict[str, bool] = {m: True for m in ALL_METRICS}
        self.bt_on = False
        self.alive = True
        self.time_range = "All"
        self.range_btns: dict = {}

        self.req_queue: queue.Queue = queue.Queue()
        self.pending_count = 0

        self._last_click_ms: dict = {}
        self._current_instance_id: str | None = None
        self._cam_metrics: dict = {}
        self._gps_history: list = []

        # Panel handles — assigned when a detail view is opened,
        # reset to None by _close_detail / _clr.
        self._gps_panel: GPSPanel | None = None
        self._graph_panel: GraphPanel | None = None
        self._data_log_panel: DataLogPanel | None = None

        self.ble = BLEManager(
            on_log   = lambda msg: self.root.after(0, lambda m=msg: self._on_ble_log(m)),
            on_data  = lambda result: self.root.after(0, lambda r=result: self._on_ble_data(r)),
            on_image = lambda cam_id, img: self.root.after(
                0, lambda c=cam_id, i=img: self._on_ble_image(c, i)),
            on_image_metrics = lambda cam_id, m: self.root.after(
                0, lambda c=cam_id, mx=m: self._on_ble_image_metrics(c, mx)),
        )

        self.DEBOUNCE_MS = 200

        threading.Thread(target=self._worker, daemon=True).start()

        self._build()
        self._auto_refresh()

        if mpl_loader.HAS_MPL:
            threading.Thread(target=mpl_loader.load_mpl, daemon=True).start()

    # ------------------------------------------------------------------
    # Worker queue
    # ------------------------------------------------------------------

    def _worker(self) -> None:
        """Background worker thread: drains req_queue and executes tasks.

        Each task is a zero-argument callable. A None sentinel shuts the
        thread down (posted by on_close). Exceptions inside a task are
        caught here so the thread never dies silently.
        """
        while True:
            task = self.req_queue.get()
            if task is None:
                break
            try:
                task()
            except Exception:
                pass
            self.req_queue.task_done()

    def _enqueue(self, func) -> None:
        """Wrap *func* for execution on the worker thread.

        Increments the pending counter (shown in the status label) before
        queuing and decrements it when the task finishes, regardless of
        whether it raised an exception.
        """
        self.pending_count += 1
        self._upd_status()

        def wrap():
            try:
                func()
            except Exception as e:
                self.root.after(0, lambda: messagebox.showerror("Error", str(e)))
            finally:
                self.root.after(0, self._dec_pending)
        self.req_queue.put(wrap)

    def _dispatch(self, action_key, func, flash_widget=None, debounce_ms=None) -> None:
        """Central action dispatcher.

        Optionally flashes *flash_widget* to give visual feedback, then calls
        *func* directly on the main thread. action_key and debounce_ms are
        accepted for API compatibility but debouncing is not currently enforced
        here — rapid duplicate clicks are handled upstream by the BLE request
        queue's pending counter.
        """
        if flash_widget is not None:
            self._flash(flash_widget)
        try:
            func()
        except Exception:
            pass

    def _flash(self, widget) -> None:
        """Briefly highlight *widget*'s border in yellow to confirm a click."""
        try:
            orig = widget.cget("highlightbackground")
            widget.configure(highlightbackground=YELLOW, highlightthickness=1)
            self.root.after(120, lambda: widget.configure(highlightbackground=orig))
        except Exception:
            pass

    def _dec_pending(self) -> None:
        """Decrement the pending-request counter and refresh the status label."""
        self.pending_count = max(0, self.pending_count - 1)
        self._upd_status()

    def _upd_status(self) -> None:
        """Update the status label and Request Data button colour to reflect
        the current pending-request count."""
        if self.pending_count > 0:
            self.status_lbl.configure(text=f"⏳ {self.pending_count} pending", fg=YELLOW)
            if hasattr(self, "req_btn"):
                self.req_btn._bg = PURPLE_DIM; self.req_btn._hvr_bg = PURPLE_DIM
                self.req_btn.configure(bg=PURPLE_DIM)
                self.req_btn.label.configure(bg=PURPLE_DIM)
        else:
            self.status_lbl.configure(text="✓ Ready", fg=GREEN)
            if hasattr(self, "req_btn"):
                self.req_btn._bg = PURPLE; self.req_btn._hvr_bg = BTN_PURPLE_HVR
                self.req_btn.configure(bg=PURPLE)
                self.req_btn.label.configure(bg=PURPLE)

    # ------------------------------------------------------------------
    # Layout
    # ------------------------------------------------------------------

    def _build(self) -> None:
        tk.Frame(self.root, bg=PURPLE, height=3).pack(fill="x", side="top")

        top = tk.Frame(self.root, bg=BG_PANEL, height=TOP_H)
        top.pack(fill="x", side="top")
        top.pack_propagate(False)

        logo_f = tk.Frame(top, bg=BG_PANEL)
        logo_f.pack(side="left", padx=(12, 0))
        logo_f = tk.Frame(top, bg=BG_PANEL)
        logo_f.pack(side="left", padx=(16, 12))
        try:
            from PIL import Image, ImageTk
            _raw = Image.open("arcticsenselogo1.png").resize((160, 40), Image.LANCZOS)
            self._logo_top = ImageTk.PhotoImage(_raw)
            tk.Label(logo_f, image=self._logo_top, bg=BG_PANEL, bd=0).pack(side="left")
        except Exception:
            tk.Label(logo_f, text="ARCTICSENSE", bg=BG_PANEL, fg=PURPLE_LT,
                     font=(FU, 14, "bold")).pack(side="left", padx=(0, 4))
            tk.Label(logo_f, text="Buoy Monitor", bg=BG_PANEL, fg=TEXT_SEC,
                     font=(FU, 9)).pack(side="left")

        bt_f = tk.Frame(top, bg=BG_PANEL)
        bt_f.pack(side="right", padx=16)
        self.bt_dot = tk.Label(bt_f, text="●", bg=BG_PANEL, fg=RED, font=(FM, 14))
        self.bt_dot.pack(side="left")
        self.bt_txt = tk.Label(bt_f, text="Buoy Disconnected", bg=BG_PANEL,
                               fg=TEXT_SEC, font=(FU, 11))
        self.bt_txt.pack(side="left", padx=(4, 8))
        _bt_btn = FlatButton(bt_f, "Connect",
                             lambda: self._dispatch("bt", self._toggle_bt, _bt_btn),
                             fg=YELLOW, hover_fg=YELLOW)
        _bt_btn.pack(side="left")

        _lookup_btn = FlatButton(top, "Lookup ID",
                                 lambda: self._dispatch("lookup", self._lookup_id, _lookup_btn),
                                 font=(FU, 9))
        _lookup_btn.pack(side="right", padx=6, pady=4)
        _datalog_btn = FlatButton(top, "Data Log",
                                  lambda: self._dispatch("datalog", self._show_data_log, _datalog_btn),
                                  font=(FU, 9))
        _datalog_btn.pack(side="right", padx=6, pady=4)
        _clear_btn = FlatButton(top, "Clear CSV",
                                lambda: self._dispatch("clear", self._clear_csv, _clear_btn),
                                font=(FU, 9))
        _clear_btn.pack(side="right", padx=6, pady=4)
        self.req_btn = FlatButton(top, "►  Request Data",
                                  lambda: self._dispatch("request", self._request_data,
                                                         self.req_btn, debounce_ms=0),
                                  bg=PURPLE, fg="#fff", hover_bg=BTN_PURPLE_HVR, hover_fg="#fff",
                                  font=(FU, 10, "bold"))
        self.req_btn.pack(side="right", padx=10, pady=4)
        tk.Frame(top, bg=BORDER, width=1).pack(side="right", fill="y", pady=10, padx=8)
        _uncheck_btn = FlatButton(top, "Uncheck All",
                                  lambda: self._dispatch("uncheck_all", self._uncheck_all, _uncheck_btn),
                                  font=(FU, 9))
        _uncheck_btn.pack(side="right", padx=6, pady=4)
        _check_btn = FlatButton(top, "Check All",
                                lambda: self._dispatch("check_all", self._check_all, _check_btn),
                                font=(FU, 9))
        _check_btn.pack(side="right", padx=6, pady=4)

        body = tk.Frame(self.root, bg=BG_DARK)
        body.pack(fill="both", expand=True)

        right = tk.Frame(body, bg=BG_PANEL, width=RIGHT_W)
        right.pack(side="right", fill="y")
        right.pack_propagate(False)
        hdr_f = tk.Frame(right, bg=BG_PANEL)
        hdr_f.pack(fill="x", padx=10, pady=(12, 2))
        tk.Label(hdr_f, text="SENSORS", bg=BG_PANEL, fg=PURPLE_LT,
                 font=(FM, 10, "bold")).pack(side="left")
        tk.Frame(right, bg=YELLOW, height=1).pack(fill="x", padx=10)

        right_canvas = tk.Canvas(right, bg=BG_PANEL, highlightthickness=0)
        right_sb = tk.Scrollbar(right, orient="vertical", command=right_canvas.yview)
        right_canvas.configure(yscrollcommand=right_sb.set)
        right_sb.pack(side="right", fill="y")
        right_canvas.pack(side="left", fill="both", expand=True)
        ri = tk.Frame(right_canvas, bg=BG_PANEL)
        _ri_win = right_canvas.create_window((0, 0), window=ri, anchor="nw")
        right_canvas.bind("<Configure>",
                          lambda e: right_canvas.itemconfigure(_ri_win, width=e.width))
        ri.bind("<Configure>",
                lambda e: right_canvas.configure(scrollregion=right_canvas.bbox("all")))

        def _v_scroll(e):
            right_canvas.yview_scroll(-1 * (e.delta // 120), "units")
        right_canvas.bind("<Enter>",
                          lambda _: right_canvas.bind_all("<MouseWheel>", _v_scroll))
        right_canvas.bind("<Leave>",
                          lambda _: right_canvas.unbind_all("<MouseWheel>"))

        for n in RIGHT_METRICS:
            c = MetricCard(ri, n, self._select_metric, self._check_metric)
            c.frame.pack(fill="x", padx=6, pady=3)
            self.cards[n] = c

        mid = tk.Frame(body, bg=BG_DARK)
        mid.pack(side="left", fill="both", expand=True)
        tk.Frame(mid, bg=PURPLE_DIM, height=1).pack(side="bottom", fill="x")
        bot = tk.Frame(mid, bg=BG_PANEL, height=BOTTOM_H)
        bot.pack(side="bottom", fill="x")
        bot.pack_propagate(False)

        bot_canvas = tk.Canvas(bot, bg=BG_PANEL, highlightthickness=0)
        bot_sb = tk.Scrollbar(bot, orient="horizontal", command=bot_canvas.xview)
        bot_canvas.configure(xscrollcommand=bot_sb.set)
        bot_sb.pack(side="bottom", fill="x")
        bot_canvas.pack(side="top", fill="both", expand=True)
        bi = tk.Frame(bot_canvas, bg=BG_PANEL)
        _bi_win = bot_canvas.create_window((0, 0), window=bi, anchor="nw")

        def _on_canvas_resize(e):
            bot_canvas.itemconfigure(_bi_win,
                                     width=max(e.width, bi.winfo_reqwidth()),
                                     height=e.height)
        bot_canvas.bind("<Configure>", _on_canvas_resize)
        bi.bind("<Configure>",
                lambda e: bot_canvas.configure(scrollregion=bot_canvas.bbox("all")))

        def _h_scroll(e):
            bot_canvas.xview_scroll(-1 * (e.delta // 120), "units")
        bot_canvas.bind("<Enter>",
                        lambda _: bot_canvas.bind_all("<Shift-MouseWheel>", _h_scroll))
        bot_canvas.bind("<Leave>",
                        lambda _: bot_canvas.unbind_all("<Shift-MouseWheel>"))

        for i, n in enumerate(BOTTOM_METRICS):
            c = MetricCard(bi, n, self._select_metric, self._check_metric)
            c.frame.grid(row=0, column=i, sticky="nsew", padx=3, pady=3)
            bi.columnconfigure(i, weight=1)
            self.cards[n] = c
        bi.rowconfigure(0, weight=1)

        self.centre = tk.Frame(mid, bg=BG_DARK)
        self.centre.pack(fill="both", expand=True)
        self._show_placeholder()

    # ------------------------------------------------------------------
    # Centre-panel management
    # ------------------------------------------------------------------

    def _clr(self) -> None:
        """Destroy all widgets in the centre panel, ready for a fresh render."""
        for w in self.centre.winfo_children():
            w.destroy()

    def _show_placeholder(self) -> None:
        """Render the startup placeholder (logo + hint text) in the centre panel."""
        self._clr()
        f = tk.Frame(self.centre, bg=BG_DARK)
        f.place(relx=0.5, rely=0.42, anchor="center")
        try:
            from PIL import Image, ImageTk
            _raw = Image.open("arcticsenselogo1.png").resize((320, 90), Image.LANCZOS)
            self._logo_centre = ImageTk.PhotoImage(_raw)
            tk.Label(f, image=self._logo_centre, bg=BG_DARK, bd=0).pack(pady=(0, 6))
        except Exception:
            tk.Label(f, text="ARCTICSENSE", bg=BG_DARK, fg=PURPLE_DIM,
                     font=(FU, 38, "bold")).pack()
        tk.Label(f, text="Select a metric to view data",
                 bg=BG_DARK, fg=TEXT_SEC, font=(FU, 15)).pack(pady=(16, 0))
        tk.Label(f, text="Click a sensor card → ► Request Data",
                 bg=BG_DARK, fg=TEXT_MUT, font=(FU, 11)).pack(pady=(6, 0))

    def _show_detail(self, metric_name: str) -> None:
        """Open the detail view for *metric_name* in the centre panel.

        Camera metrics open camera_panel.show_camera().
        GPS opens a GPSPanel map.
        All other metrics open a GraphPanel time-series with time-range buttons.
        The selected card is highlighted; all others are deselected.
        """
        self._clr()
        self.selected_metric = metric_name
        self._gps_panel = None
        self._graph_panel = None
        self._data_log_panel = None
        for n, c in self.cards.items():
            c.set_selected(n == metric_name)

        code = METRIC_CODES.get(metric_name, "???")
        hdr = tk.Frame(self.centre, bg=BG_DARK)
        hdr.pack(fill="x", padx=20, pady=(16, 8))
        tk.Label(hdr, text=f"[{code}]", bg=BG_DARK, fg=PURPLE_LT,
                 font=(FM, 15)).pack(side="left")
        tk.Label(hdr, text=f"  {metric_name}", bg=BG_DARK, fg=YELLOW,
                 font=(FU, 18, "bold")).pack(side="left")
        FlatButton(hdr, "✕  Close", self._close_detail,
                   font=(FU, 10)).pack(side="right", pady=2)
        FlatButton(hdr, "⏳  History",
                   lambda: self._show_history(metric_name),
                   fg=YELLOW, hover_fg=YELLOW, font=(FU, 10)).pack(
                       side="right", padx=(0, 8), pady=2)
        tk.Frame(self.centre, bg=PURPLE_DIM, height=1).pack(fill="x", padx=20)

        if metric_name in ("Camera 1", "Camera 2"):
            cam_id = 1 if metric_name == "Camera 1" else 2
            show_camera(self.centre, metric_name,
                        self._cam_metrics.get(cam_id),
                        self._open_images)
            return

        if metric_name == "GPS":
            # Rebuild _gps_history from the CSV so previously recorded fixes
            # appear on the map immediately when the GPS card is opened —
            # not just fixes received in this session. Near-zero coordinates
            # (|lat| < 0.01 and |lon| < 0.01) are filtered out as likely
            # invalid/placeholder readings from a pre-fix state.
            self._gps_history = []
            for row in load_history("GPS"):
                try:
                    parts = row["value"].strip().split(",")
                    lat = float(parts[0].strip())
                    lon = float(parts[1].strip())
                    if not (abs(lat) < 0.01 and abs(lon) < 0.01):
                        self._gps_history.append(
                            {"lat": lat, "lon": lon, "ts": row["timestamp"]})
                except Exception:
                    pass
            self._gps_history = self._gps_history[-GPS_MAX_POINTS:]
            graph_f = tk.Frame(self.centre, bg=BG_DARK)
            graph_f.pack(fill="both", expand=True, padx=20, pady=(10, 14))
            self._gps_panel = GPSPanel(graph_f)
            self._gps_panel.update(self._gps_history)
            return

        rf = tk.Frame(self.centre, bg=BG_DARK)
        rf.pack(fill="x", padx=20, pady=(10, 4))
        tk.Label(rf, text="Time range:", bg=BG_DARK, fg=TEXT_MUT,
                 font=(FU, 11)).pack(side="left", padx=(0, 8))
        self.range_btns = {}
        for tr in TIME_RANGES:
            act = tr == self.time_range
            b = FlatButton(rf, tr,
                           lambda t=tr: self._dispatch(f"range:{t}",
                                                       lambda: self._set_range(t)),
                           bg=PURPLE if act else BTN_BG,
                           fg="#fff" if act else TEXT_SEC,
                           hover_bg=BTN_PURPLE_HVR if act else BTN_BG_HVR,
                           font=(FU, 10), padx=10, pady=3)
            b.pack(side="left", padx=3)
            self.range_btns[tr] = b

        graph_f = tk.Frame(self.centre, bg=BG_DARK)
        graph_f.pack(fill="both", expand=True, padx=20, pady=(4, 14))
        self._graph_panel = GraphPanel(graph_f, self.root, lambda: self.time_range)
        self._graph_panel.draw(metric_name)

    def _show_data_log(self) -> None:
        """Open the Data Log view in the centre panel.

        Deselects all sensor cards and replaces the current centre-panel
        content with a DataLogPanel, which auto-refreshes whenever new
        data arrives via _apply().
        """
        self._clr()
        for c in self.cards.values():
            c.set_selected(False)
        self.selected_metric = None
        self._gps_panel = None
        self._graph_panel = None
        self._data_log_panel = DataLogPanel(
            self.centre, self.root,
            on_close=self._close_detail,
            dispatch=self._dispatch,
        )

    # ------------------------------------------------------------------
    # History window (Toplevel)
    # ------------------------------------------------------------------

    # Sub-metrics to load for multi-channel sensors in the history window.
    _HISTORY_CHANNELS: dict = {
        "Temp External":          ["Temp External CH1", "Temp External CH2",
                                   "Temp External CH3", "Temp External CH4", "Temp External"],
        "Tilt":                   ["Tilt X", "Tilt Y"],
        "IMU":                    ["IMU Pitch", "IMU Roll", "IMU Yaw"],
        "UV Index":               ["UV Index", "Lux"],
        "Battery":                ["Battery", "Voltage", "Current Draw"],
        "EMI":                    ["EMI I", "EMI Q", "EMI Magnitude", "EMI Phase", "EMI Phase IQ"],
        "Temp Internal (Static)":  ["Temp Internal (Static)", "Humidity (Static)"],
        "Temp Internal (Dynamic)": ["Temp Internal (Dynamic)", "Humidity (Dynamic)"],

    }

    def _show_history(self, mn: str) -> None:
        channels = self._HISTORY_CHANNELS.get(mn, [mn])
        history = []
        for ch in channels:
            history.extend(load_history(ch))
        history.sort(key=lambda r: r["timestamp"])

        win = tk.Toplevel(self.root)
        win.title(f"History – {mn}")
        win.geometry("960x560")
        win.configure(bg=BG_DARK)
        tk.Label(win, text=f"⏳  {mn} History", bg=BG_DARK, fg=PURPLE_LT,
                 font=(FU, 15, "bold")).pack(pady=(16, 8))
        container = tk.Frame(win, bg=BG_DARK)
        container.pack(fill="both", expand=True, padx=16, pady=(0, 8))
        canvas = tk.Canvas(container, bg=BG_DARK, highlightthickness=0)
        sb = ttk.Scrollbar(container, orient="vertical", command=canvas.yview)
        sf = tk.Frame(canvas, bg=BG_DARK)
        sf.bind("<Configure>",
                lambda _: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=sf, anchor="nw")
        canvas.configure(yscrollcommand=sb.set)
        canvas.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

        hdr = tk.Frame(sf, bg=PURPLE_DIM)
        hdr.pack(fill="x", pady=(0, 4))
        for txt, w in [("Metric ID", 18), ("Name", 24), ("Value", 22), ("Unit", 10), ("Timestamp", 20)]:
            tk.Label(hdr, text=txt, bg=PURPLE_DIM, fg=YELLOW,
                     font=(FM, 10, "bold"), width=w, anchor="w").pack(side="left", padx=4)

        if not history:
            tk.Label(sf, text="No readings.", bg=BG_DARK, fg=TEXT_MUT,
                     font=(FU, 12)).pack(pady=24)
        else:
            for i, row in enumerate(reversed(history)):
                bg = BG_CARD if i % 2 == 0 else BG_PANEL
                rf = tk.Frame(sf, bg=bg)
                rf.pack(fill="x", pady=1)
                vd = row["value"]
                if len(vd) > 28:
                    vd = vd[:26] + "…"
                nd = row["metric_name"]
                if len(nd) > 28:
                    nd = nd[:26] + "…"
                for val, w in [(row["metric_id"], 18), (nd, 24), (vd, 22),
                               (row["unit"], 10), (row["timestamp"], 20)]:
                    tk.Label(rf, text=val, bg=bg, fg=TEXT_PRI,
                             font=(FM, 10), width=w, anchor="w").pack(side="left", padx=4)

        if mn in ("Camera 1", "Camera 2"):
            FlatButton(win, "\U0001f4c2  Open Images Folder",
                       self._open_images, fg=YELLOW, hover_fg=YELLOW,
                       font=(FU, 10)).pack(pady=(4, 16))

    # ------------------------------------------------------------------
    # Actions
    # ------------------------------------------------------------------

    def _select_metric(self, n: str) -> None:
        def _do():
            if n == self.selected_metric:
                return
            self._show_detail(n)
        self._dispatch(f"select:{n}", _do)

    def _check_metric(self, n: str, s: bool) -> None:
        self.checked[n] = s

    def _lookup_id(self) -> None:
        win = tk.Toplevel(self.root)
        win.title("Lookup Metric Table")
        win.geometry("900x600")
        win.configure(bg=BG_DARK)
        win.resizable(True, True)
        tk.Label(win, text="\U0001f50d  Lookup by Metric", bg=BG_DARK, fg=PURPLE_LT,
                 font=(FU, 15, "bold")).pack(pady=(16, 8))
        tk.Frame(win, bg=PURPLE_DIM, height=1).pack(fill="x", padx=16)

        search_f = tk.Frame(win, bg=BG_DARK)
        search_f.pack(fill="x", padx=16, pady=12)
        tk.Label(search_f, text="ID / partial match:", bg=BG_DARK, fg=TEXT_SEC,
                 font=(FU, 12)).pack(side="left", padx=(0, 8))
        entry_var = tk.StringVar()
        entry = tk.Entry(search_f, textvariable=entry_var, bg=BG_CARD, fg=YELLOW,
                         insertbackground=YELLOW, font=(FM, 12), relief="flat",
                         highlightthickness=1, highlightbackground=BORDER,
                         highlightcolor=PURPLE_LT, width=28)
        entry.pack(side="left", padx=(0, 8), ipady=5)
        entry.focus_set()

        results_outer = tk.Frame(win, bg=BG_DARK)
        results_outer.pack(fill="both", expand=True, padx=16, pady=(0, 12))
        canvas_r = tk.Canvas(results_outer, bg=BG_DARK, highlightthickness=0)
        sb_r = ttk.Scrollbar(results_outer, orient="vertical", command=canvas_r.yview)
        scroll_f = tk.Frame(canvas_r, bg=BG_DARK)
        scroll_f.bind("<Configure>",
                      lambda _: canvas_r.configure(scrollregion=canvas_r.bbox("all")))
        canvas_r.create_window((0, 0), window=scroll_f, anchor="nw")
        canvas_r.configure(yscrollcommand=sb_r.set)
        canvas_r.pack(side="left", fill="both", expand=True)
        sb_r.pack(side="right", fill="y")
        count_lbl = tk.Label(win, text="", bg=BG_DARK, fg=TEXT_MUT, font=(FM, 10))
        count_lbl.pack(pady=(0, 8))

        def _do_search(*_):
            for w in scroll_f.winfo_children():
                w.destroy()
            q = entry_var.get().strip()
            all_rows = load_history()
            hits = ([r for r in all_rows
                     if q.lower() in r["metric_id"].lower()
                     or q.lower() in r["metric_name"].lower()]
                    if q else all_rows)
            count_lbl.configure(text=f"{len(hits)} record(s) found")
            if not hits:
                tk.Label(scroll_f, text="No matching records.", bg=BG_DARK,
                         fg=TEXT_MUT, font=(FU, 12)).pack(pady=20)
                return
            h = tk.Frame(scroll_f, bg=PURPLE_DIM)
            h.pack(fill="x", pady=(0, 3))
            for txt, w in [("Metric ID", 22), ("Name", 18), ("Value", 22),
                           ("Unit", 10), ("Timestamp", 20)]:
                tk.Label(h, text=txt, bg=PURPLE_DIM, fg=YELLOW,
                         font=(FM, 10, "bold"), width=w, anchor="w").pack(side="left", padx=4)
            for i, row in enumerate(reversed(hits[-200:])):
                bg = BG_CARD if i % 2 == 0 else BG_PANEL
                rf = tk.Frame(scroll_f, bg=bg)
                rf.pack(fill="x", pady=1)
                vd = row["value"]
                if len(vd) > 30:
                    vd = vd[:28] + "…"
                for val, w in [(row["metric_id"], 22), (row["metric_name"], 18),
                               (vd, 22), (row["unit"], 10), (row["timestamp"], 20)]:
                    tk.Label(rf, text=val, bg=bg, fg=TEXT_PRI,
                             font=(FM, 10), width=w, anchor="w").pack(side="left", padx=4)

        FlatButton(search_f, "Search", _do_search, bg=PURPLE, fg="#fff",
                   hover_bg=BTN_PURPLE_HVR, hover_fg="#fff", font=(FU, 11)).pack(side="left")
        entry.bind("<Return>", _do_search)
        _do_search()

    def _check_all(self) -> None:
        for n, c in self.cards.items():
            c.set_checked(True)
            self.checked[n] = True

    def _uncheck_all(self) -> None:
        for n, c in self.cards.items():
            c.set_checked(False)
            self.checked[n] = False

    def _toggle_bt(self) -> None:
        if not self.bt_on:
            self.ble.connect()
            self.bt_dot.configure(fg=YELLOW)
            self.bt_txt.configure(text="Connecting to buoy...")
            self._poll_bt_state()
        else:
            self.ble.disconnect()
            self.bt_on = False
            self.bt_dot.configure(fg=RED)
            self.bt_txt.configure(text="Buoy Disconnected")

    def _poll_bt_state(self) -> None:
        if self.ble.connected:
            self.bt_on = True
            self.bt_dot.configure(fg=GREEN)
            self.bt_txt.configure(text="Buoy Connected")
            return
        if self.bt_txt.cget("text") != "Connecting to buoy...":
            return
        self.root.after(500, self._poll_bt_state)

    def _request_data(self) -> None:
        sel = [m for m, on in list(self.checked.items()) if on]
        if not sel:
            messagebox.showinfo("No Metrics", "Check at least one metric first.")
            return
        if not self.bt_on:
            messagebox.showinfo("Not Connected", "Connect to the buoy first.")
            return
        self._current_instance_id = new_instance_id()
        self.ble.request_metrics(sel)

    # ------------------------------------------------------------------
    # BLE callbacks (all called on the Tk thread via root.after)
    # ------------------------------------------------------------------

    def _on_ble_log(self, msg: str) -> None:
        print(msg)

    def _on_ble_data(self, result: dict) -> None:
        """Handle a decoded sensor payload delivered by BLEManager.on_data.

        Splits multi-channel sensors (Temp External, Tilt, IMU) into
        per-channel sub-readings before recording to CSV, so each channel
        gets its own row and can be graphed independently. The parent card
        (e.g. "Tilt") is also written as a single comma-separated row so
        the card display always has a value even if sub-channels are not
        individually selected.

        Secondary annotations (Lux under UV, Humidity under Temp Internal,
        Voltage/Current under Battery) are applied directly to the relevant
        card after recording.
        """
        inst = self._current_instance_id or new_instance_id()
        temp_external_card = None
        tilt_card = None
        imu_card = None
        triples = []

        for name, (value, unit) in result.items():
            if name == "Temp External":
                channels = [v.strip() for v in value.split(",") if v.strip()]
                if len(channels) == len(TEMP_EXTERNAL_CHANNELS):
                    triples.extend((ch, cv, unit)
                                   for ch, cv in zip(TEMP_EXTERNAL_CHANNELS, channels))
                    try:
                        avg = sum(float(cv) for cv in channels) / len(channels)
                        triples.append(("Temp External", f"{avg:.1f}", unit))
                    except (TypeError, ValueError):
                        pass
                    # temp_external_card stays None; the "Temp External" avg row in
                    # triples will update the card via _apply()
                    continue
            if name == "Tilt":
                parts = [v.strip() for v in value.split(",") if v.strip()]
                if len(parts) == len(TILT_CHANNELS):
                    triples.extend((ch, v, unit) for ch, v in zip(TILT_CHANNELS, parts))
                    tilt_card = {
                        "metric_name": "Tilt", "value": value, "unit": unit,
                        "metric_id": build_metric_id("Tilt", datetime.now()),
                    }
                    continue
            if name == "IMU":
                parts = [v.strip() for v in value.split(",") if v.strip()]
                if len(parts) == len(IMU_CHANNELS):
                    triples.extend((ch, v, unit) for ch, v in zip(IMU_CHANNELS, parts))
                    imu_card = {
                        "metric_name": "IMU", "value": value, "unit": unit,
                        "metric_id": build_metric_id("IMU", datetime.now()),
                    }
                    continue
            triples.append((name, value, unit))

        rows = record_readings(triples, instance_id=inst)
        extras = [c for c in (temp_external_card, tilt_card, imu_card) if c]
        self._apply(rows + extras)

        if "Lux" in result and "UV Index" in self.cards:
            lux_val, lux_unit = result["Lux"]
            self.cards["UV Index"].update_value(
                self.cards["UV Index"].val_lbl.cget("text"),
                secondary=f"Lux: {lux_val} {lux_unit}")

        if "Humidity (Static)" in result and "Temp Internal (Static)" in self.cards:
            hum_val, hum_unit = result["Humidity (Static)"]
            self.cards["Temp Internal (Static)"].update_value(
                self.cards["Temp Internal (Static)"].val_lbl.cget("text"),
                secondary=f"Humidity: {hum_val} {hum_unit}")

        if "Humidity (Dynamic)" in result and "Temp Internal (Dynamic)" in self.cards:
            hum_val, hum_unit = result["Humidity (Dynamic)"]
            self.cards["Temp Internal (Dynamic)"].update_value(
                self.cards["Temp Internal (Dynamic)"].val_lbl.cget("text"),
                secondary=f"Humidity: {hum_val} {hum_unit}")

        if "Battery" in self.cards and ("Voltage" in result or "Current Draw" in result):
            parts = []
            if "Voltage" in result:
                v_val, v_unit = result["Voltage"]
                parts.append(f"Voltage: {v_val} {v_unit}")
            if "Current Draw" in result:
                c_val, c_unit = result["Current Draw"]
                parts.append(f"Current: {c_val} {c_unit}")
            self.cards["Battery"].update_value(
                self.cards["Battery"].val_lbl.cget("text"),
                secondary=" | ".join(parts))

    def _on_ble_image(self, cam_id: int, pil_image) -> None:
        """Save a received PIL image to images_history/ and record its path in CSV.

        The filename is the metric_id (e.g. CM1_20260610_143022.jpg) so the
        camera detail panel can locate it by reading the CSV value field.
        """
        inst = self._current_instance_id or new_instance_id()
        metric_name = f"Camera {cam_id}"
        ts = datetime.now()
        metric_id = build_metric_id(metric_name, ts)
        os.makedirs(IMAGE_HISTORY_DIR, exist_ok=True)
        filepath = os.path.join(IMAGE_HISTORY_DIR, f"{metric_id}.jpg")
        try:
            pil_image.save(filepath, "JPEG")
        except Exception as exc:
            print(f"Cam{cam_id} save error: {exc}")
            return
        row = record_reading(metric_name, filepath, "Image", inst, timestamp=ts)
        self._apply([row])

    def _on_ble_image_metrics(self, cam_id: int, metrics: dict) -> None:
        """Handle an image quality metrics packet from BLEManager.

        Caches the metrics dict so the camera detail view can display them
        immediately when the card is selected. Each sub-metric is recorded
        as a separate CSV row (Cam1 Score, Cam1 LapVar, etc.) so the Data
        Log can filter on them. The camera card's ID label is updated with
        a pass/fail summary line. If the camera detail view is currently
        open it is refreshed in place.
        """
        self._cam_metrics[cam_id] = metrics
        metric_name = f"Camera {cam_id}"
        inst = self._current_instance_id or new_instance_id()
        prefix = f"Cam{cam_id}"
        record_readings([
            (f"{prefix} Quality", metrics.get("status_name", "?"),  ""),
            (f"{prefix} Score",   str(metrics.get("score",   0.0)), ""),
            (f"{prefix} LapVar",  str(metrics.get("lapvar",  0.0)), ""),
            (f"{prefix} Entropy", str(metrics.get("entropy", 0.0)), "bits"),
            (f"{prefix} Range",   str(metrics.get("range",   0)),   ""),
            (f"{prefix} DPR",     str(metrics.get("dpr",     0.0)), ""),
            (f"{prefix} SPR",     str(metrics.get("spr",     0.0)), ""),
        ], instance_id=inst)

        if metric_name in self.cards:
            status = metrics.get("status", 255)
            secondary = (f"✓  Score: {metrics.get('score', 0):.0f}"
                         if status == 0
                         else f"✗  {metrics.get('status_name', 'Rejected')}")
            self.cards[metric_name].id_lbl.configure(text=secondary)

        if self.selected_metric == metric_name:
            self._show_detail(metric_name)

    # ------------------------------------------------------------------
    # State update — called after every batch of new readings
    # ------------------------------------------------------------------

    # Metrics whose card display does not show an individual metric_id.
    # Temp External and Tilt display averaged/combined values that don't
    # map to a single CSV row, so showing a stale or mismatched ID is
    # more confusing than showing nothing.
    _NO_METRIC_ID = {"Temp External", "Tilt"}

    def _apply(self, readings: list) -> None:
        """Update sensor cards and the active centre panel with new readings.

        For each reading whose metric_name maps to a card, the card value
        is updated. If the selected metric is a camera, GPS, or graph panel,
        the relevant panel is also refreshed. The Data Log panel is notified
        so it can add the new rows without a full rebuild.
        """
        for r in readings:
            n = r["metric_name"]
            if n in self.cards:
                mid = "" if n in self._NO_METRIC_ID else r["metric_id"]
                self.cards[n].update_value(r["value"], r["unit"], mid)

        if self.selected_metric:
            if self.selected_metric in ("Camera 1", "Camera 2"):
                mn = self.selected_metric
                self.selected_metric = None
                self._show_detail(mn)
            elif self.selected_metric == "GPS":
                # Append any new GPS readings to the in-memory history list
                # and pass the updated list to GPSPanel. GPS data is NOT
                # round-tripped through CSV here because GPS value strings
                # contain a comma, which breaks CSV round-trip parsing.
                for r in readings:
                    if r["metric_name"] != "GPS":
                        continue
                    try:
                        parts = r["value"].strip().split(",")
                        lat = float(parts[0].strip())
                        lon = float(parts[1].strip())
                        if not (abs(lat) < 0.01 and abs(lon) < 0.01):
                            self._gps_history.append(
                                {"lat": lat, "lon": lon, "ts": r["timestamp"]})
                            self._gps_history = self._gps_history[-GPS_MAX_POINTS:]
                    except Exception:
                        pass
                if self._gps_panel:
                    self._gps_panel.update(self._gps_history)
            elif self._graph_panel:
                self._graph_panel.draw(self.selected_metric)

        if self._data_log_panel and self._data_log_panel.is_alive:
            self._data_log_panel.on_new_data()

    def _set_range(self, tr: str) -> None:
        """Switch the active time range and redraw the current graph if one is open."""
        self.time_range = tr
        for name, btn in self.range_btns.items():
            act = name == tr
            btn._bg = PURPLE if act else BTN_BG
            btn._hvr_bg = BTN_PURPLE_HVR if act else BTN_BG_HVR
            btn._fg = "#fff" if act else TEXT_SEC
            btn._hvr_fg = "#fff" if act else TEXT_SEC
            btn.configure(bg=btn._bg)
            btn.label.configure(bg=btn._bg, fg=btn._fg)
        if self._graph_panel and self.selected_metric:
            self._graph_panel.draw(self.selected_metric)

    def _close_detail(self) -> None:
        """Close the active detail view and return the centre panel to the placeholder."""
        for c in self.cards.values():
            c.set_selected(False)
        self.selected_metric = None
        if self._gps_panel:
            self._gps_panel.destroy()
            self._gps_panel = None
        self._graph_panel = None
        self._data_log_panel = None
        self._clr()
        self._show_placeholder()

    def _clear_csv(self) -> None:
        """Prompt for confirmation then wipe sensor_readings.csv."""
        if messagebox.askyesno("Clear Data", "Delete all recorded sensor data?"):
            try:
                clear_csv()
                self._after_clear()
            except Exception as e:
                messagebox.showerror("Clear CSV Failed", str(e))

    def _after_clear(self) -> None:
        """Reset all card values to '---' and refresh any open panel after a CSV clear."""
        for c in self.cards.values():
            c.update_value("---")
        if self.selected_metric:
            self._show_detail(self.selected_metric)
        if self._data_log_panel and self._data_log_panel.is_alive:
            self._data_log_panel.on_new_data()

    def _open_images(self) -> None:
        folder = os.path.abspath(IMAGE_HISTORY_DIR)
        os.makedirs(folder, exist_ok=True)
        try:
            if platform.system() == "Windows":
                os.startfile(folder)
            else:
                subprocess.Popen(["xdg-open", folder])
        except Exception:
            messagebox.showinfo("Images", f"Open manually:\n{folder}")

    # ------------------------------------------------------------------
    # Auto-refresh
    # ------------------------------------------------------------------

    def _auto_refresh(self) -> None:
        """Reload the most recent CSV value for every card on a 2500 ms timer.

        Runs only when no request is pending, to avoid overwriting live BLE
        data mid-flight. Multi-channel sensors (Temp External, Tilt, IMU,
        Battery) reconstruct their combined card display from the per-channel
        CSV rows. Secondary annotations (Lux, Humidity, Voltage/Current) are
        refreshed alongside their parent cards.
        """
        if not self.alive:
            return
        if self.pending_count == 0:
            try:
                for n in ALL_METRICS:
                    h = load_history(n)
                    if h:
                        l = h[-1]
                        if n in self.cards:
                            self.cards[n].update_value(l["value"], l["unit"], l["metric_id"])
                if "UV Index" in self.cards:
                    lux_h = load_history("Lux")
                    if lux_h:
                        lux = lux_h[-1]
                        self.cards["UV Index"].update_value(
                            self.cards["UV Index"].val_lbl.cget("text"),
                            secondary=f"Lux: {lux['value']} {lux['unit']}")
                if "Temp Internal (Static)" in self.cards:
                    hum_h = load_history("Humidity (Static)")
                    if hum_h:
                        hum = hum_h[-1]
                        self.cards["Temp Internal (Static)"].update_value(
                            self.cards["Temp Internal (Static)"].val_lbl.cget("text"),
                            secondary=f"Humidity: {hum['value']} {hum['unit']}")
                if "Temp Internal (Dynamic)" in self.cards:
                    hum_h = load_history("Humidity (Dynamic)")
                    if hum_h:
                        hum = hum_h[-1]
                        self.cards["Temp Internal (Dynamic)"].update_value(
                            self.cards["Temp Internal (Dynamic)"].val_lbl.cget("text"),
                            secondary=f"Humidity: {hum['value']} {hum['unit']}")
                if "Temp External" in self.cards:
                    latest_ch = []
                    for ch in TEMP_EXTERNAL_CHANNELS:
                        ch_h = load_history(ch)
                        if ch_h:
                            latest_ch.append(ch_h[-1])
                    if len(latest_ch) == len(TEMP_EXTERNAL_CHANNELS):
                        try:
                            avg = sum(float(r["value"]) for r in latest_ch) / len(latest_ch)
                            self.cards["Temp External"].update_value(
                                f"{avg:.1f}",
                                latest_ch[-1]["unit"],
                                "")
                        except (TypeError, ValueError):
                            pass
                if "Tilt" in self.cards:
                    tilt_parts = []
                    for ch in TILT_CHANNELS:
                        h = load_history(ch)
                        if h:
                            tilt_parts.append(h[-1])
                    if len(tilt_parts) == len(TILT_CHANNELS):
                        self.cards["Tilt"].update_value(
                            ",".join(r["value"] for r in tilt_parts),
                            tilt_parts[-1]["unit"],
                            "")
                if "IMU" in self.cards:
                    imu_parts = []
                    for ch in IMU_CHANNELS:
                        h = load_history(ch)
                        if h:
                            imu_parts.append(h[-1])
                    if len(imu_parts) == len(IMU_CHANNELS):
                        self.cards["IMU"].update_value(
                            ",".join(r["value"] for r in imu_parts),
                            imu_parts[-1]["unit"],
                            imu_parts[-1]["metric_id"])
                if "Battery" in self.cards:
                    parts = []
                    volt_h = load_history("Voltage")
                    curr_h = load_history("Current Draw")
                    if volt_h:
                        v = volt_h[-1]
                        parts.append(f"Voltage: {v['value']} {v['unit']}")
                    if curr_h:
                        c = curr_h[-1]
                        parts.append(f"Current: {c['value']} {c['unit']}")
                    if parts:
                        self.cards["Battery"].update_value(
                            self.cards["Battery"].val_lbl.cget("text"),
                            secondary=" | ".join(parts))
            except Exception:
                pass
        self.root.after(REFRESH_MS, self._auto_refresh)

    def on_close(self) -> None:
        """Shut down the application cleanly.

        Sets alive=False to stop the auto-refresh loop, posts a None sentinel
        to the worker queue to terminate its thread, then destroys the root window.
        """
        self.alive = False
        self.req_queue.put(None)
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    app = BuoyMonitorApp(root)
    root.iconphoto(False, tk.PhotoImage(file="ArcticSense.png"))
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()
