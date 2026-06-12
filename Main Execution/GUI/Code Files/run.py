"""Entry point for the ArcticSense Buoy Monitor.

Adds the package root to sys.path so sibling imports (buoy_monitor,
ble_manager, data_store, ui.*) resolve correctly when this file is
invoked directly with `python3 run.py` from any working directory.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from buoy_monitor import main
if __name__ == "__main__":
    main()
