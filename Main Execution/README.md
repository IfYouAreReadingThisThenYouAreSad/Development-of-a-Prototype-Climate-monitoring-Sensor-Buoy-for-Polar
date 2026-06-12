# ArcticSense

A prototype autonomous climate-monitoring sensor buoy designed for polar deployment. The system collects Essential Climate Variables — including ocean temperature, turbidity, salinity, UV radiation, and electromagnetic interference — and transmits them remotely to a ground-station GUI.

Developed as a final-year engineering project in partnership with the **British Antarctic Survey**.

---

## System Overview

```
Ground Station (Python GUI)
        │
        │  Bluetooth Low Energy (HM-10)
        │  [target: Iridium 9603N satellite modem]
        ▼
┌─────────────────────────────────────┐
│        Motherboard  (STM32F405RGT6) │
│  UV · GPS · Tilt · Battery          │
│  EMI · Cameras · Temp · Humidity    │
│                      UART ↕         │
│      Daughterboard  (STM32F405RGT6) │
│  Thermistors · Turbidity · Salinity │
│  Temp · Humidity· IMU               │
└─────────────────────────────────────┘
```

The **Motherboard (MB)** receives commands from the GUI, forwards the request to the **Daughterboard (DB)**, reads its own sensors, assembles the combined payload, and returns it to the GUI as 20-byte fragments. The DB is a silent slave — it never initiates communication.

The **HM-10 BLE module** is a development stand-in for the target **Iridium 9603N** satellite modem. The 20-byte fragment size and 5-byte command frame were chosen to fit within the Iridium SBD 340-byte message limit, so migrating to satellite requires only a transport-layer swap.

---

## Quick Start

To run the ground station on a laptop with BLE:

```bash
cd GUIV3
pip3 install bleak Pillow tkintermapview matplotlib
python3 run.py
```

See [`GUI/README.md`](GUI/README.md) for full setup instructions, file layout, and GUI feature reference.

To flash firmware, open either the `Motherboard/` or `Daughterboard/` project in **STM32CubeIDE** and flash via ST-Link.
