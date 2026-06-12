"""Camera detail view and image-quality metrics panel."""

import os
import tkinter as tk

try:
    from PIL import Image as PILImage, ImageTk
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

from ui.constants import (
    BG_DARK, BORDER, PURPLE_LT,
    TEXT_MUT, TEXT_PRI, TEXT_SEC,
    YELLOW, RED, GREEN,
    FM, FU,
)
from ui.widgets.flat_button import FlatButton
from data_store import load_history, IMAGE_HISTORY_DIR
from ble_manager import IMAGE_METRIC_THRESHOLDS


def show_camera(
    container: tk.Frame,
    metric_name: str,
    cam_metrics: dict | None,
    on_open_images,
) -> None:
    """Render the camera detail view into *container*.

    Shows the most recent captured image, its metadata, and (if available)
    the firmware image-quality metrics panel below the image.
    """
    cf = tk.Frame(container, bg=BG_DARK)
    cf.pack(fill="both", expand=True, padx=20, pady=14)

    vsb = tk.Scrollbar(cf, orient="vertical")
    vsb.pack(side="right", fill="y")
    cam_canvas = tk.Canvas(cf, bg=BG_DARK, bd=0,
                           highlightthickness=0, yscrollcommand=vsb.set)
    cam_canvas.pack(side="left", fill="both", expand=True)
    vsb.config(command=cam_canvas.yview)

    inner = tk.Frame(cam_canvas, bg=BG_DARK)
    inner_id = cam_canvas.create_window((0, 0), window=inner, anchor="nw")

    def _on_canvas_resize(e):
        cam_canvas.itemconfig(inner_id, width=e.width)
    cam_canvas.bind("<Configure>", _on_canvas_resize)
    inner.bind("<Configure>",
               lambda e: cam_canvas.configure(scrollregion=cam_canvas.bbox("all")))

    def _on_wheel(e):
        cam_canvas.yview_scroll(int(-1 * (e.delta // 120)), "units")
    cam_canvas.bind("<Enter>", lambda e: cam_canvas.bind_all("<MouseWheel>", _on_wheel))
    cam_canvas.bind("<Leave>", lambda e: cam_canvas.unbind_all("<MouseWheel>"))

    history = load_history(metric_name)
    if not history:
        tk.Label(inner, text="No images captured yet.", bg=BG_DARK,
                 fg=TEXT_MUT, font=(FU, 13)).pack(pady=(20, 4))
    else:
        latest = history[-1]
        ip = latest["value"]
        if HAS_PIL and os.path.exists(ip) and os.path.getsize(ip) > 0:
            try:
                pi = PILImage.open(ip).resize((520, 390), PILImage.LANCZOS)
                ti = ImageTk.PhotoImage(pi)
                lbl = tk.Label(inner, image=ti, bg=BG_DARK, bd=0, highlightthickness=0)
                lbl.image = ti   # keep reference so GC doesn't drop the image
                lbl.pack(pady=12)
            except Exception:
                tk.Label(inner, text="[Preview unavailable]",
                         bg=BG_DARK, fg=TEXT_MUT, font=(FU, 12)).pack(pady=12)
        else:
            tk.Label(inner, text="[Install Pillow: pip3 install Pillow]",
                     bg=BG_DARK, fg=TEXT_MUT, font=(FU, 12)).pack(pady=12)
        tk.Label(inner, text=f"ID:  {latest['metric_id']}",
                 bg=BG_DARK, fg=PURPLE_LT, font=(FM, 11)).pack()
        tk.Label(inner, text=f"Captured:  {latest['timestamp']}",
                 bg=BG_DARK, fg=TEXT_MUT, font=(FM, 11)).pack(pady=(2, 0))
        FlatButton(inner, "\U0001f4c2  Open Images Folder", on_open_images,
                   fg=YELLOW, hover_fg=YELLOW, font=(FU, 11)).pack(pady=14)

    if cam_metrics:
        _show_camera_metrics(inner, cam_metrics)


def _show_camera_metrics(parent: tk.Frame, metrics: dict) -> None:
    """Render the image-quality breakdown below the camera image."""
    status = metrics.get("status", 255)
    passed = status == 0

    tk.Frame(parent, bg=BORDER, height=1).pack(fill="x", pady=(6, 10))

    verdict_text = (
        "Quality Check:  PASSED" if passed else
        f"Quality Check:  REJECTED  —  {metrics.get('status_name', '?')}"
    )
    tk.Label(parent, text=verdict_text, bg=BG_DARK,
             fg=GREEN if passed else RED, font=(FU, 12, "bold")).pack()

    tbl = tk.Frame(parent, bg=BG_DARK)
    tbl.pack(pady=(8, 4))

    for row_i, (key, (threshold, direction, label, unit)) in \
            enumerate(IMAGE_METRIC_THRESHOLDS.items()):
        val = metrics.get(key, 0)
        metric_passed = (val > threshold) if direction == ">" else (val < threshold)
        val_str  = f"{val:.1f}" if isinstance(val, float) else str(val)
        unit_str = f" {unit}" if unit else ""

        tk.Label(tbl, text="●", bg=BG_DARK,
                 fg=GREEN if metric_passed else RED,
                 font=(FU, 15)).grid(row=row_i, column=0, padx=(0, 6), sticky="w")
        tk.Label(tbl, text=f"{label}:", bg=BG_DARK, fg=TEXT_SEC,
                 font=(FM, 12), width=20, anchor="w").grid(row=row_i, column=1, sticky="w")
        tk.Label(tbl, text=f"{val_str}{unit_str}", bg=BG_DARK,
                 fg=TEXT_PRI if metric_passed else RED,
                 font=(FM, 12), width=12, anchor="e").grid(row=row_i, column=2,
                                                            padx=(8, 4), sticky="e")
        tk.Label(tbl, text=f"(thresh {direction} {threshold:.3g})",
                 bg=BG_DARK, fg=TEXT_MUT,
                 font=(FM, 12)).grid(row=row_i, column=3, sticky="w")