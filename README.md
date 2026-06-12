# Development of a Prototype Climate-Monitoring Sensor Buoy for Polar Regions

---

# 1.0 About

This project aims to develop a prototype of a mass-deployable climate-monitoring buoy optimised for polar regions. The system autonomously measures and communicates key environmental parameters such as sea ice thickness, temperature, salinity, and local imagery. It is intended for deployment in both ice and open water, operating with minimal energy and maintenance.

[![Project Demo](https://img.youtube.com/vi/G8KJ4cSrFfc/maxresdefault.jpg)](https://www.youtube.com/watch?v=G8KJ4cSrFfc)

---

# 2.0 Sensors

The buoy contains sensors that help measure essential climate variables and the overall health of the buoy. Sensors include:

- Sea ice thickness (EMI)
- Turbidity
- Salinity (and possible antifouling monitoring)
- UV and ambient light exposure
- Temperature (external)
- Temperature (internal)
- Tilt (compensates for angle for the sea ice measurement system)
- IMU (wave spectrum)
- GPS
- Bluetooth (for the prototype only, will be replaced by satellite communication)
- Battery monitoring
- SD card (onboard storage)
- Camera (with onboard image processing)

The image below shows the buoy's sensors and their locations.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/CAD%20Assembly%20of%20Buoy.png"
    alt="Figure 1 shows CAD View of Buoy" width="900">
</p>
<p align="center">Figure 1: CAD view of the buoy</p>

---

# 3.0 Mechanical Design

The buoy has a rotating side and a static side. The static side contains sensors such as turbidity and salinity, the external temperature sensors, and the IMU. These need to stay fixed, and this side is weighted slightly so that, in water, they can work effectively without being affected by the rotating part of the system.

The buoy has a rotating side, where a rotating I-beam is used to ensure proper orientation of the buoy. Due to the buoy being mass-deployed and dropped anywhere in the Arctic, this allows the system to always align orientation-sensitive sensors, such as the EMI sensor, which must always be horizontal to the sea ice. It also keeps the GPS, UV sensor, and Bluetooth module pointing towards the sky, and the camera pointing perpendicular to the ice for the best view.

For both sides to work, two PCBs (shown below) were made, connected through a slip ring. The daughter board digitises the data on the static side and adds a checksum. The mother PCB then reads this data and sends it via satellite. This is a very brief explanation — for more detail, please read the attached report in this repository.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/Mother%20and%20Daughter%20Board.png"
    alt="Figure 2 shows the Mother and Daughter board" width="900">
</p>
<p align="center">Figure 2: Mother and daughter boards</p>

---

# 4.0 GUI

A GUI was designed to interact with the system. This allows for monitoring of the buoy and for manual requests of data. In this prototype, data must be requested manually, but in the real system data will be sent periodically, with the frequency changing depending on whether the buoy is on ice, in water, or on land. The GUI in the real design will still allow specific data to be requested, but will also allow monitoring of the health, position, and data of multiple buoys. An image of the GUI is shown below.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/GUI%20Screenshot.png"
    alt="Figure 3 shows GUI Interface" width="900">
</p>
<p align="center">Figure 3: GUI interface</p>

---

# 5.0 System Architecture

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/System%20Architecture.png"
    alt="Figure 4 shows simplified System Architecture of Buoy" width="900">
</p>
<p align="center">Figure 4: Simplified system architecture of the buoy</p>

---

# 6.0 Repository Structure

This repository is organised into folders, each corresponding to a specific sensor, hardware module, or subsystem. Code for each module was developed, compiled, and tested independently within its respective folder. Once verified, each module's code is integrated into a single project that combines all sensors, hardware, and subsystems into a fully functioning system.

This structure is designed to make it easier for external users to:

- Understand how each module works individually
- Navigate to specific hardware or sensor code
- Contribute to or modify individual subsystems without affecting the rest of the project

## Folder Overview

| Folder | Contents |
|--------|----------|
| `README` | This top-level overview of the project. |
| `Intergration` | The integrated mother and daughter board projects (`main.c` for each), the GUI application, and their associated READMEs. |
| `Sensors` | One folder per sensor/module, each containing its driver library and a README explaining its hardware, API, and usage. |
| `Report` | The full project report and the project poster. |
| `PCB` *(planned)* | PCB design files for the mother and daughter boards. This may be sent separately if it isn't ready in time for handover. |

---

# 7.0 More Information

Each README in this repository is intentionally short, providing baseline information on the hardware and how to use the code associated with each sensor or section. For more information, calibration data, and results, please read the report attached in this repository.
