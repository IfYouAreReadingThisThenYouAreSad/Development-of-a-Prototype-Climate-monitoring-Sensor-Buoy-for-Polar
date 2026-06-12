"""Data Log panel — inline centre-area view with two tabs.

By Instance: readings grouped by Request Data click.
Flat Table:  sortable, filterable ttk.Treeview.
"""

import tkinter as tk
from tkinter import ttk
from datetime import datetime, timedelta

from ui.constants import (
    BG_DARK, BG_PANEL, BG_CARD, BG_CARD_HVR,
    BORDER, PURPLE, PURPLE_DIM, PURPLE_LT, PURPLE_LINE,
    BTN_BG, BTN_BG_HVR, BTN_PURPLE_HVR,
    YELLOW, TEXT_PRI, TEXT_SEC, TEXT_MUT,
    FM, FU,
    ALL_METRICS, TEMP_EXTERNAL_CHANNELS, TIME_RANGES, TIME_SECS,
)
from ui.widgets.flat_button import FlatButton
from data_store import load_history, TILT_CHANNELS, IMU_CHANNELS, IMAGE_QUALITY_METRICS


class DataLogPanel:
    """Builds and owns the Data Log view inside *container*.

    Parameters
    ----------
    container : tk.Frame
        The centre-panel frame to render into (already cleared by the caller).
    root : tk.Tk
        The root window, needed for after_idle() scroll restoration.
    on_close : callable
        Called when the user clicks the ✕ Close button.
    dispatch : callable(action_key, func)
        The app's _dispatch() method, threaded through so FlatButton
        actions are routed through the same debounce/flash mechanism.
    """

    def __init__(self, container: tk.Frame, root: tk.Tk, on_close, dispatch) -> None:
        self._container = container
        self._root = root
        self._on_close = on_close
        self._dispatch = dispatch

        self._tab: str = "instance"
        self._tab_btns: dict = {}
        self._count_lbl: tk.Label | None = None
        self._body: tk.Frame | None = None
        self._inst_canvas: dict | None = None
        self._tree: ttk.Treeview | None = None
        self._sort: tuple = ("timestamp", True)
        self._time_range: str = "All"
        self._time_btns: dict = {}
        self._filter_metric: tk.StringVar | None = None

        self._build(container)

    def _build(self, container: tk.Frame) -> None:
        hdr = tk.Frame(container, bg=BG_DARK)
        hdr.pack(fill="x", padx=20, pady=(16, 8))
        tk.Label(hdr, text="\U0001f4cb", bg=BG_DARK, fg=PURPLE_LT, font=(FU, 15)).pack(side="left")
        tk.Label(hdr, text="  Data Log", bg=BG_DARK, fg=YELLOW, font=(FU, 18, "bold")).pack(side="left")
        FlatButton(hdr, "✕  Close",
                   lambda: self._dispatch("close_log", self._on_close),
                   font=(FU, 10)).pack(side="right", pady=2)
        tk.Frame(container, bg=PURPLE_DIM, height=1).pack(fill="x", padx=20)

        tab_bar = tk.Frame(container, bg=BG_DARK)
        tab_bar.pack(fill="x", padx=20, pady=(10, 4))
        tk.Label(tab_bar, text="View:", bg=BG_DARK, fg=TEXT_MUT, font=(FU, 11)).pack(side="left", padx=(0, 8))

        self._body = tk.Frame(container, bg=BG_DARK)
        self._body.pack(fill="both", expand=True, padx=20, pady=(4, 14))

        for key, label in [("instance", "By Instance"), ("flat", "Flat Table")]:
            act = key == self._tab
            b = FlatButton(tab_bar, label,
                           lambda k=key: self._dispatch(f"logtab:{k}", lambda: self._switch_tab(k)),
                           bg=PURPLE if act else BTN_BG,
                           fg="#fff" if act else TEXT_SEC,
                           hover_bg=BTN_PURPLE_HVR if act else BTN_BG_HVR,
                           font=(FU, 10), padx=12, pady=3)
            b.pack(side="left", padx=3)
            self._tab_btns[key] = b

        self._count_lbl = tk.Label(tab_bar, text="", bg=BG_DARK, fg=TEXT_MUT, font=(FM, 10))
        self._count_lbl.pack(side="right")

        self._render_body()

    def _switch_tab(self, tab: str) -> None:
        self._tab = tab
        for name, btn in self._tab_btns.items():
            act = name == tab
            btn._bg     = PURPLE if act else BTN_BG
            btn._hvr_bg = BTN_PURPLE_HVR if act else BTN_BG_HVR
            btn._fg     = "#fff" if act else TEXT_SEC
            btn._hvr_fg = "#fff" if act else TEXT_SEC
            btn.configure(bg=btn._bg)
            btn.label.configure(bg=btn._bg, fg=btn._fg)
        self._render_body()

    def _render_body(self) -> None:
        for w in self._body.winfo_children():
            w.destroy()
        self._inst_canvas = None
        if self._tab == "instance":
            self._render_instance()
        else:
            self._render_flat()

    # ------------------------------------------------------------------
    # By-instance tab
    # ------------------------------------------------------------------
    def _render_instance(self) -> None:
        rows = load_history()
        groups: dict = {}
        order: list = []
        for row in rows:
            key = row.get("instance_id") or "(legacy)"
            if key not in groups:
                groups[key] = []
                order.append(key)
            groups[key].append(row)
        order.reverse()

        self._count_lbl.configure(
            text=f"{len(order)} instance(s) • {len(rows)} reading(s)")

        if not order:
            for w in self._body.winfo_children():
                w.destroy()
            self._inst_canvas = None
            tk.Label(self._body,
                     text="No data yet – press ► Request Data",
                     bg=BG_DARK, fg=TEXT_MUT, font=(FU, 13)).pack(expand=True)
            return

        existing = self._inst_canvas
        if existing is not None and existing["canvas"].winfo_exists():
            canvas = existing["canvas"]
            inner  = existing["inner"]
            try:
                y_top, _ = canvas.yview()
            except tk.TclError:
                y_top = 0.0
            for w in inner.winfo_children():
                w.destroy()
        else:
            for w in self._body.winfo_children():
                w.destroy()
            y_top = 0.0
            outer = tk.Frame(self._body, bg=BG_DARK)
            outer.pack(fill="both", expand=True)
            canvas = tk.Canvas(outer, bg=BG_DARK, highlightthickness=0)
            sb = ttk.Scrollbar(outer, orient="vertical", command=canvas.yview)
            inner = tk.Frame(canvas, bg=BG_DARK)
            inner_id = canvas.create_window((0, 0), window=inner, anchor="nw")
            canvas.configure(yscrollcommand=sb.set)
            canvas.pack(side="left", fill="both", expand=True)
            sb.pack(side="right", fill="y")

            def _size_inner(e, _id=inner_id):
                canvas.itemconfig(_id, width=e.width)
            canvas.bind("<Configure>", _size_inner)
            inner.bind("<Configure>",
                       lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

            def _on_wheel(e):
                canvas.yview_scroll(int(-1 * e.delta), "units")
            canvas.bind("<Enter>", lambda e: canvas.bind_all("<MouseWheel>", _on_wheel))
            canvas.bind("<Leave>", lambda e: canvas.unbind_all("<MouseWheel>"))

            self._inst_canvas = {"canvas": canvas, "inner": inner}

        total = len(order)
        for idx, inst_id in enumerate(order):
            instance_rows = groups[inst_id]
            first_ts = instance_rows[0]["timestamp"] if instance_rows else ""
            inst_num = total - idx

            block = tk.Frame(inner, bg=BG_PANEL,
                             highlightbackground=BORDER, highlightthickness=1)
            block.pack(fill="x", pady=(0, 8), padx=4)

            hdr = tk.Frame(block, bg=PURPLE_DIM)
            hdr.pack(fill="x")
            tk.Label(hdr, text=f"Instance {inst_num}", bg=PURPLE_DIM, fg=YELLOW,
                     font=(FU, 13, "bold")).pack(side="left", padx=10, pady=6)
            tk.Label(hdr, text=first_ts, bg=PURPLE_DIM, fg=TEXT_SEC,
                     font=(FM, 10)).pack(side="left", padx=(0, 10), pady=6)
            tk.Label(hdr, text=f"{len(instance_rows)} reading(s)", bg=PURPLE_DIM,
                     fg=TEXT_MUT, font=(FU, 10)).pack(side="right", padx=10, pady=6)

            col_hdr = tk.Frame(block, bg=BG_CARD)
            col_hdr.pack(fill="x")
            for txt, w in [("Metric ID", 22), ("Metric", 18), ("Value", 26),
                           ("Unit", 10), ("Timestamp", 20)]:
                tk.Label(col_hdr, text=txt, bg=BG_CARD, fg=TEXT_SEC,
                         font=(FM, 10, "bold"), width=w, anchor="w").pack(
                             side="left", padx=4, pady=3)

            for i, r in enumerate(instance_rows):
                bg = BG_PANEL if i % 2 == 0 else BG_CARD
                rf = tk.Frame(block, bg=bg)
                rf.pack(fill="x")
                vd = r["value"]
                if len(vd) > 34:
                    vd = vd[:32] + "…"
                for val, w in [(r["metric_id"], 22), (r["metric_name"], 18),
                               (vd, 26), (r["unit"], 10), (r["timestamp"], 20)]:
                    tk.Label(rf, text=val, bg=bg, fg=TEXT_PRI,
                             font=(FM, 10), width=w, anchor="w").pack(
                                 side="left", padx=4, pady=2)

        def _restore_scroll(target=y_top):
            if canvas.winfo_exists():
                canvas.configure(scrollregion=canvas.bbox("all"))
                canvas.yview_moveto(target)
        self._root.after_idle(_restore_scroll)

    # ------------------------------------------------------------------
    # Flat table tab
    # ------------------------------------------------------------------
    def _render_flat(self) -> None:
        filt = tk.Frame(self._body, bg=BG_DARK)
        filt.pack(fill="x", pady=(0, 8))

        tk.Label(filt, text="Metric:", bg=BG_DARK, fg=TEXT_MUT,
                 font=(FU, 11)).pack(side="left", padx=(0, 6))
        self._filter_metric = tk.StringVar(value="All")
        metric_choices = (
            ["All"] + ALL_METRICS + TEMP_EXTERNAL_CHANNELS
            + TILT_CHANNELS + IMU_CHANNELS + IMAGE_QUALITY_METRICS
        )
        ttk.OptionMenu(
            filt, self._filter_metric, "All", *metric_choices,
            command=lambda _=None: self._refresh_flat(),
        ).pack(side="left", padx=(0, 16))

        tk.Label(filt, text="Time:", bg=BG_DARK, fg=TEXT_MUT,
                 font=(FU, 11)).pack(side="left", padx=(0, 6))
        self._time_btns = {}
        for tr in TIME_RANGES:
            act = tr == self._time_range
            b = FlatButton(
                filt, tr,
                lambda t=tr: self._dispatch(f"logtime:{t}", lambda: self._set_time_range(t)),
                bg=PURPLE if act else BTN_BG,
                fg="#fff" if act else TEXT_SEC,
                hover_bg=BTN_PURPLE_HVR if act else BTN_BG_HVR,
                font=(FU, 10), padx=8, pady=2,
            )
            b.pack(side="left", padx=2)
            self._time_btns[tr] = b

        table_wrap = tk.Frame(self._body, bg=BG_DARK)
        table_wrap.pack(fill="both", expand=True)

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Log.Treeview",
                        background=BG_PANEL, foreground=TEXT_PRI,
                        fieldbackground=BG_PANEL, bordercolor=BORDER,
                        rowheight=22, font=(FM, 10))
        style.configure("Log.Treeview.Heading",
                        background=PURPLE_DIM, foreground=YELLOW,
                        font=(FM, 10, "bold"), relief="flat")
        style.map("Log.Treeview.Heading", background=[("active", PURPLE)])
        style.map("Log.Treeview",
                  background=[("selected", PURPLE)],
                  foreground=[("selected", "#ffffff")])

        cols = ("instance", "metric_id", "metric", "value", "unit", "timestamp")
        col_labels = {
            "instance": "Instance", "metric_id": "Metric ID",
            "metric": "Metric",     "value": "Value",
            "unit": "Unit",         "timestamp": "Timestamp",
        }
        col_widths = {
            "instance": 160, "metric_id": 170, "metric": 130,
            "value": 180,    "unit": 70,        "timestamp": 160,
        }

        tree = ttk.Treeview(table_wrap, columns=cols, show="headings",
                            style="Log.Treeview", selectmode="browse")
        for c in cols:
            tree.heading(c, text=col_labels[c] + "  ▴▾",
                         command=lambda col=c: self._sort_flat(col))
            tree.column(c, width=col_widths[c], anchor="w", stretch=True)

        vsb = ttk.Scrollbar(table_wrap, orient="vertical",   command=tree.yview)
        hsb = ttk.Scrollbar(table_wrap, orient="horizontal", command=tree.xview)
        tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)
        tree.grid(row=0, column=0, sticky="nsew")
        vsb.grid(row=0, column=1, sticky="ns")
        hsb.grid(row=1, column=0, sticky="ew")
        table_wrap.rowconfigure(0, weight=1)
        table_wrap.columnconfigure(0, weight=1)

        tree.tag_configure("odd",  background=BG_CARD)
        tree.tag_configure("even", background=BG_PANEL)

        self._tree = tree
        self._sort = ("timestamp", True)
        self._refresh_flat()

    def _set_time_range(self, tr: str) -> None:
        self._time_range = tr
        for name, btn in self._time_btns.items():
            act = name == tr
            btn._bg     = PURPLE if act else BTN_BG
            btn._hvr_bg = BTN_PURPLE_HVR if act else BTN_BG_HVR
            btn._fg     = "#fff" if act else TEXT_SEC
            btn._hvr_fg = "#fff" if act else TEXT_SEC
            btn.configure(bg=btn._bg)
            btn.label.configure(bg=btn._bg, fg=btn._fg)
        self._refresh_flat()

    def _sort_flat(self, col: str) -> None:
        cur_col, cur_desc = self._sort
        self._sort = (col, not cur_desc if col == cur_col else False)
        self._refresh_flat()

    def _refresh_flat(self) -> None:
        if self._tree is None or not self._tree.winfo_exists():
            return
        tree = self._tree
        rows = load_history()

        mf = self._filter_metric.get() if self._filter_metric else "All"
        if mf != "All":
            rows = [r for r in rows if r["metric_name"] == mf]

        cut = TIME_SECS.get(self._time_range)
        if cut is not None:
            cutoff = datetime.now() - timedelta(seconds=cut)
            kept = []
            for r in rows:
                try:
                    ts = datetime.strptime(r["timestamp"], "%Y-%m-%d %H:%M:%S")
                except ValueError:
                    continue
                if ts >= cutoff:
                    kept.append(r)
            rows = kept

        col, desc = self._sort

        def _sort_key(r):
            if col == "instance":  return r.get("instance_id") or ""
            if col == "metric_id": return r["metric_id"]
            if col == "metric":    return r["metric_name"]
            if col == "value":
                try:   return (0, float(r["value"]))
                except (ValueError, TypeError): return (1, r["value"])
            if col == "unit":      return r["unit"]
            if col == "timestamp": return r["timestamp"]
            return r.get(col, "")

        try:
            rows.sort(key=_sort_key, reverse=desc)
        except TypeError:
            rows.sort(key=lambda r: str(_sort_key(r)), reverse=desc)

        arrow_up, arrow_dn, arrow_neutral = " ▴", " ▾", "  "
        col_base = {
            "instance": "Instance", "metric_id": "Metric ID",
            "metric": "Metric",     "value": "Value",
            "unit": "Unit",         "timestamp": "Timestamp",
        }
        for c in col_base:
            suffix = (arrow_dn if desc else arrow_up) if c == col else arrow_neutral
            tree.heading(c, text=col_base[c] + suffix)

        tree.delete(*tree.get_children())
        for i, r in enumerate(rows):
            vd = r["value"]
            if len(vd) > 40:
                vd = vd[:38] + "…"
            tree.insert("", "end",
                        values=(r.get("instance_id") or "(legacy)",
                                r["metric_id"], r["metric_name"], vd,
                                r["unit"], r["timestamp"]),
                        tags=("odd" if i % 2 else "even",))

        if self._count_lbl:
            self._count_lbl.configure(text=f"{len(rows)} reading(s)")

    # ------------------------------------------------------------------
    # Public API called by BuoyMonitorApp
    # ------------------------------------------------------------------
    def on_new_data(self) -> None:
        """Refresh whichever tab is currently visible."""
        if self._body is None or not self._body.winfo_exists():
            return
        if self._tab == "flat":
            self._refresh_flat()
        else:
            self._render_instance()

    @property
    def is_alive(self) -> bool:
        """True while the panel's body frame still exists in the widget tree."""
        return self._body is not None and self._body.winfo_exists()