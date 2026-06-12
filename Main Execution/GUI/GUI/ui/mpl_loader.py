"""Lazy matplotlib loader.

matplotlib is expensive to import (~0.5s). This module performs a cheap
availability check at import time (importlib.import_module is fast) and
defers the heavy import until load_mpl() is first called.  load_mpl() is
safe to call multiple times — subsequent calls are no-ops once loaded.

Typical usage:
    # In app __init__, kick off background pre-load:
    import threading
    from ui.mpl_loader import load_mpl
    threading.Thread(target=load_mpl, daemon=True).start()

    # In graph-draw code (background thread), call again defensively:
    from ui import mpl_loader
    mpl_loader.load_mpl()
    fig = mpl_loader.Figure(...)
"""

try:
    import importlib
    importlib.import_module("matplotlib")   # cheap availability check
    HAS_MPL = True
except ImportError:
    HAS_MPL = False

# Module-level handles populated by load_mpl().
matplotlib = None
FigureCanvasTkAgg = None
Figure = None
DateFormatter = None
AutoDateLocator = None
MplPolygon = None
PatchCollection = None
fm = None
_MPL_LOADED = False


def load_mpl() -> None:
    """Import the heavy matplotlib objects. Idempotent."""
    global matplotlib, FigureCanvasTkAgg, Figure, DateFormatter, AutoDateLocator
    global MplPolygon, PatchCollection, fm, _MPL_LOADED
    if _MPL_LOADED or not HAS_MPL:
        return
    import matplotlib as _mpl
    _mpl.use("TkAgg")
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg as _Canvas
    from matplotlib.figure import Figure as _Figure
    from matplotlib.dates import DateFormatter as _DF, AutoDateLocator as _ADL
    from matplotlib.patches import Polygon as _Poly
    from matplotlib.collections import PatchCollection as _PC
    import matplotlib.font_manager as _fm
    matplotlib = _mpl
    FigureCanvasTkAgg = _Canvas
    Figure = _Figure
    DateFormatter = _DF
    AutoDateLocator = _ADL
    MplPolygon = _Poly
    PatchCollection = _PC
    fm = _fm
    _MPL_LOADED = True