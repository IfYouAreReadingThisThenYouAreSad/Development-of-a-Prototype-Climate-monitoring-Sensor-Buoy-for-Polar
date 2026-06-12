import tkinter as tk
from ui.constants import BTN_BG, BTN_BG_HVR, BORDER, TEXT_PRI, FU


class FlatButton(tk.Frame):
    """Frame + Label button with full dark-theme colour control.

    tk.Button ignores bg/fg on some platforms. A Frame with a Label child
    bound to <Button-1> honours all colours and fires on every click."""

    def __init__(self, parent, text, command, bg=BTN_BG, fg=TEXT_PRI,
                 hover_bg=BTN_BG_HVR, hover_fg=None,
                 font=None, padx=14, pady=5, **kw):
        super().__init__(parent, bg=bg, cursor="hand2",
                         highlightthickness=1, highlightbackground=BORDER)
        self._bg, self._fg = bg, fg
        self._hvr_bg = hover_bg
        self._hvr_fg = hover_fg or fg
        self._cmd = command
        if font is None:
            font = (FU, 11)

        self.label = tk.Label(self, text=text, bg=bg, fg=fg,
                              font=font, padx=padx, pady=pady, cursor="hand2")
        self.label.pack()
        for w in (self, self.label):
            w.bind("<Button-1>", self._click)
            w.bind("<Enter>", self._enter)
            w.bind("<Leave>", self._leave)

    def _click(self, _=None):
        if self._cmd:
            try:
                self._cmd()
            except Exception:
                pass

    def _enter(self, _=None):
        self.configure(bg=self._hvr_bg)
        self.label.configure(bg=self._hvr_bg, fg=self._hvr_fg)

    def _leave(self, _=None):
        self.configure(bg=self._bg)
        self.label.configure(bg=self._bg, fg=self._fg)

    def set_text(self, t: str) -> None:
        self.label.configure(text=t)