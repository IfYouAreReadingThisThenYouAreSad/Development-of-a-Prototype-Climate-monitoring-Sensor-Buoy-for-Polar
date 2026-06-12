import os
import tkinter as tk
from data_store import METRIC_CODES, METRIC_RANGES
from ui.constants import (
    BG_CARD, BG_CARD_HVR, BG_DARK, BG_SELECTED,
    BORDER, PURPLE, PURPLE_LT,
    YELLOW, TEXT_PRI, TEXT_SEC, TEXT_MUT,
    FM, FU,
)


class MetricCard:
    """A sensor display card with a checkbox, metric code, name, value, unit, and ID label.

    Cards are used in both the right panel (taller, fixed width) and the
    bottom strip (shorter, horizontally scrollable). Clicking the card body
    fires on_select; toggling the checkbox fires on_check.

    Parameters
    ----------
    parent : tk.Frame
        Container frame to pack into.
    metric_name : str
        Must appear in data_store.METRIC_CODES and METRIC_RANGES.
    on_select : callable(metric_name: str)
        Called when the card body is clicked (opens the detail view).
    on_check : callable(metric_name: str, checked: bool)
        Called when the checkbox is toggled (controls request mask inclusion).
    """

    def __init__(self, parent, metric_name: str, on_select, on_check):
        self.metric_name = metric_name
        self._on_select = on_select
        self._on_check = on_check
        self.is_checked = True
        self._selected = False

        code = METRIC_CODES.get(metric_name, "???")
        unit = METRIC_RANGES.get(metric_name, (None, None, None, ""))[3]

        self.frame = tk.Frame(parent, bg=BG_CARD, highlightbackground=BORDER, highlightthickness=1)
        inner = tk.Frame(self.frame, bg=BG_CARD)
        inner.pack(fill="both", expand=True, padx=8, pady=5)

        row1 = tk.Frame(inner, bg=BG_CARD)
        row1.pack(fill="x")

        self.chk_var = tk.IntVar(value=1)
        # tk.Checkbutton handles its own clicks via command=; extra <Button-1>
        # bindings on top broke it in earlier revisions, so we deliberately
        # avoid them here.
        self._chk_btn = tk.Checkbutton(
            row1, variable=self.chk_var,
            bg=BG_CARD, fg=YELLOW, selectcolor=BG_DARK,
            activebackground=BG_CARD, activeforeground=YELLOW,
            highlightthickness=0, bd=0, padx=0, pady=0,
            cursor="hand2", takefocus=0,
            command=self._toggle_check,
        )
        # Checkbutton handles its own click events via command=; binding
        # <Button-1> on top would fire twice per click, so we deliberately
        # avoid it here.
        self._chk_btn.pack(side="left", padx=(0, 2))

        self.code_lbl = tk.Label(row1, text=f"[{code}]", bg=BG_CARD, fg=PURPLE_LT, font=(FM, 11))
        self.code_lbl.pack(side="left", padx=(2, 6))
        self.name_lbl = tk.Label(row1, text=metric_name, bg=BG_CARD,
                                 fg=TEXT_PRI, font=(FU, 13, "bold"), anchor="w",
                                 wraplength=120, justify="left")
        self.name_lbl.pack(side="left", fill="x", expand=True)

        self.val_lbl = tk.Label(inner, text="---", bg=BG_CARD, fg=YELLOW,
                                font=(FM, 19, "bold"), anchor="w")
        self.val_lbl.pack(fill="x", padx=(26, 0), pady=(2, 0))

        self.unit_lbl = tk.Label(inner, text=unit, bg=BG_CARD, fg=TEXT_SEC,
                                 font=(FU, 13, "bold"), anchor="w")
        self.unit_lbl.pack(fill="x", padx=(26, 0))

        self.id_lbl = tk.Label(inner, text="", bg=BG_CARD, fg=TEXT_MUT,
                               font=(FU, 10), anchor="w")
        self.id_lbl.pack(fill="x", padx=(26, 0))

        for w in (self.frame, inner, self.code_lbl, self.name_lbl,
                  self.val_lbl, self.unit_lbl, self.id_lbl):
            w.bind("<Button-1>", lambda e: self._on_select(self.metric_name))
            w.bind("<Enter>", self._enter)
            w.bind("<Leave>", self._leave)
            w.configure(cursor="hand2")

    def _toggle_check(self):
        """Sync is_checked from the IntVar and notify the parent via on_check."""
        self.is_checked = bool(self.chk_var.get())
        self._on_check(self.metric_name, self.is_checked)

    def _cur_bg(self):
        """Return the correct background colour for the current selected state."""
        return BG_SELECTED if self._selected else BG_CARD

    def _set_bg(self, c: str) -> None:
        """Apply background colour *c* to the card frame and all child widgets."""
        self.frame.configure(bg=c, highlightbackground=PURPLE if self._selected else BORDER)
        self._walk(self.frame, c)
        try:
            self._chk_btn.configure(bg=c, activebackground=c)
        except tk.TclError:
            pass

    def _walk(self, w, c: str) -> None:
        """Recursively apply background colour *c* to *w* and all its descendants."""
        for ch in w.winfo_children():
            try:
                ch.configure(bg=c)
            except tk.TclError:
                pass
            self._walk(ch, c)

    def _enter(self, _=None):
        """Highlight the card on mouse-enter if it is not already selected."""
        if not self._selected:
            self._set_bg(BG_CARD_HVR)

    def _leave(self, _=None):
        """Restore the card background on mouse-leave."""
        self._set_bg(self._cur_bg())

    def set_selected(self, s: bool) -> None:
        """Set the selected (highlighted) state of the card."""
        self._selected = s
        self._set_bg(self._cur_bg())

    def update_value(self, value: str, unit: str = None, metric_id: str = "", secondary: str = "") -> None:
        """Update the displayed value, unit, and ID label.

        *value* is truncated to 24 characters for display. Camera image rows
        show only the filename, not the full path. *secondary* overrides
        *metric_id* for the small bottom line (used for Lux, Humidity, etc.).
        """
        display = os.path.basename(value) if (unit == "Image" and os.sep in value) else value
        disp = display if len(display) <= 24 else display[:22] + "…"
        self.val_lbl.configure(text=disp)
        if unit:
            self.unit_lbl.configure(text=unit)
        self.id_lbl.configure(text=secondary if secondary else metric_id)

    def set_checked(self, s: bool) -> None:
        """Set the checkbox state programmatically (Check All / Uncheck All)."""
        self.is_checked = bool(s)
        self.chk_var.set(1 if s else 0)