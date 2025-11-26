![Title Banner](https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/TitleBanner.png)

![Build Status](https://img.shields.io/github/workflow/status/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/CI)
[![University of Manchester](https://img.shields.io/badge/University-of_Manchester-blue)](https://www.manchester.ac.uk)
![Version](https://img.shields.io/github/v/release/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar)
![Last Commit](https://img.shields.io/github/last-commit/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar)


# 1.0 About
This project aims to develop a prototype climate-monitoring buoy optimised for polar regions. The system will autonomously measure and communicate key environmental parameters such as sea-ice thickness, temperature, salinity, and local imagery. It is intended for deployment in both ice and open water, operating with minimal energy and maintenance. 

  

Existing polar-region monitoring systems are either satellite-based, providing low spatial resolution, or rely on manual field instruments requiring human intervention [1]. Drifting and tethered buoys have long been used to record oceanographic data, but most commercial systems focus on open-sea conditions and struggle in mixed ice-water environments [2]. Current Arctic buoys such as the Ice Mass-Balance Buoy (IMB) record ice growth and melt [3], but these methods are inherently destructive. Other systems focus on coastal or energy applications, providing power generation and communications but lacking the sensing capability needed for polar research [4] [5]. Consequently, there is still no single, low-cost platform that can autonomously measure sea-ice thickness without destructive drilling or complex human setup. 

 

Sea-ice thickness is a critical variable for understanding polar climate change. The IPCC reports that both the Arctic and Antarctic have experienced severe reductions in seasonal ice extent, and that local feedback processes amplify global temperature change [1] [6]. Despite this, consistent long-term thickness records are scarce because the measurement techniques are difficult to automate. Acoustic and mechanical methods require fixed installations, while airborne and satellite altimetry depend on periodic overflights. Recent studies have demonstrated that electromagnetic induction (EMI) can determine ice thickness non-destructively by measuring the induced secondary magnetic field over conductive seawater beneath resistive ice [7], [8]. However, these systems are typically handheld or sled-mounted, used during expeditions, and rely on operator calibration and manual alignment [9]. None yet function as fully autonomous, long-duration instruments. 

 

The innovation of this project is to integrate a compact EMI thickness sensor within a buoy that can self-level and operate without human supervision. The EMI module will use a horizontal coplanar transmitter-receiver coil arrangement to measure phase shifts corresponding to sub-surface conductivity changes [7] [8]. By coupling the sensor output with an onboard IMU, the system will compensate for orientation and tilt, allowing accurate readings regardless of how the buoy lands. The buoy will also host a dual-camera system to classify its surroundings and assess quality, with the overall system being able to distinguish between ice and open water. Environmental sensors for temperature, conductivity, and salinity will supplement the dataset, while a Bluetooth Low-Energy link will simulate constrained satellite communications for testing data transmission protocols. Power will be managed through scheduled wake-sleep cycles on a low-power microcontroller, prioritising sensor operation efficiency. Designed to fit a NATO Class B sonobuoy specification, it can be mass-deployed by air and operate autonomously on ice or water. This represents a scalable and energy-efficient approach to obtaining continuous ice-thickness data. 



The proposed system therefore advances current polar monitoring technologies by combining non-destructive EMI measurement, contextual imaging, and adaptive power management into a single, compact platform.  

# 2.0 Hardware Architecture 

This project utilises an STM32 microcontroller, along with additional peripherals and sensors. The hardware architecture is divided into five subsystems, where hardware is grouped based on similarity and dependency on other components, as shown in Figure 1.

The first sub-system is the STM32 and its associated peripherals, such as the clock, UART, and other minor components. The second sub-system is the power system, which includes solar panels, batteries, boost converters, and an H-bridge circuit used to power the EMI sensor.

The third sub-system is communications, which includes an HM-10 Bluetooth module used in conjunction with the microcontroller to replicate satellite communications. Bluetooth was chosen for prototyping the satellite system due to time and cost constraints.

The fourth subsystem is sensors, which are categorised into exteroceptive sensors, optical exteroceptive sensors, and interoceptive sensors.

The fifth subsystem, which Figure 1 does not detail extensively/show, is the data acquisition system, which is used with the EMI sensors to capture sea ice measurements.

A complete list of materials and hardware used in the hardware architecture and in the overall project can be found in Section 4.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/Hardware%20Artitecture.png"
    alt="Figure 1 shows the Hardware Architecture of the Project" width="900">
</p>
<p align="center">Figure 1 shows the Hardware Architecture of the Project</p>



# 3.0 Software Architecture 

This project was coded in C/C++ using the STM32 HAL library. The buoy operates using a simple main loop, as shown in Figure 3. A high-level abstraction of the code flow is as follows:

When the buoy first wakes up, it performs a health check of its subsystems, including battery status and verification that all components are functioning correctly. During the same process, the buoy reads data from multiple sensors, such as the IMU and GPS, to determine whether it is on ice, water, or snow.

Based on the results of these health and sensor checks, the buoy determines its current state (e.g., snow, ice, or water). It then assigns measurements to specific timers and enters a low-power sleep mode until one of the timers expires.

When a timer expires, or when an external interrupt occurs (e.g., from a camera or data request), the buoy wakes up, performs the required measurement, checks for other timers that may have expired, and then returns to sleep if the main timer has not yet elapsed.

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/images/Software%20Architecture.png"
    alt="Figure 2 shows the Software Architecture of the Project" width="900">
</p>
<p align="center">Figure 2 shows the Software Architecture of the Project</p>





# References

[1] IPCC, “Chapter 3: Polar regions,” Special Report on the Ocean and Cryosphere in a Changing Climate, 2022. [Online]. Available: https://www.ipcc.ch/srocc/chapter/chapter-3-2/ [2] NSIDC, “Arctic Weather and Climate,” National Snow and Ice Data Centre, 2024. [Online]. Available: [nidc.org](https://nsidc.org/learn/parts-cryosphere/arctic-weather-and-climate)

[2] NSIDC, “Arctic Weather and Climate,” National Snow and Ice Data Centre, 2024. [Online]. Available: [midc.org](https://nsidc.org/learn/parts-cryosphere/arctic-weather-and-climate)

[3] J. A. Richter-Menge, D. K. Perovich, and C. Polashenski, “Ice mass-balance buoys: a tool for measuring and attributing changes in the thickness of the Arctic sea-ice cover,” Annals of Glaciology, vol. 44, pp. 205–210, 2006. [Online]. Available: [cambridge.ord](https://www.cambridge.org/core/journals/annals-of-glaciology/article/ice-massbalance-buoys-a-tool-for-measur ing-and-attributing-changes-in-the-thickness-of-the-arctic-seaice-cover/EA4A48431ECBC368D854C3AC400A0C47)

[4] Kyocera, “Smart Buoy,” [Online]. Available: [global.kyocera.com](https://global.kyocera.com/we_love_engineers/series/trending-tech/smartbuoy.html)

[5] Ocean Power Technologies, “PowerBuoy Products,” [Online]. Available: [oceanpowertechnologies.com.com](https://oceanpowertechnologies.com/products/powerbuoys/)

[6] IPCC, “Regional Fact Sheet: Polar regions,” AR6 WGI, 2021. [Online]. Available: [ipcc.ch](https://www.ipcc.ch/report/ar6/wg1/downloads/factsheets/IPCC_AR6_WGI_Regional_Fact_Sheet_Polar_regions.pdf)
