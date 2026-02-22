# 1.0 About

This directory contains code for the gps module used on the buoy project. The need for GPS alows the team to monitor the buoys location that coould change
rapidly if the buoy is in the sea. It also allows the buoy to get an upto date time measurement which it will then uses with a timer to periodically
poll different sensor to get different readings. somre extra fetures is that we can also get the speed of the buoy , altitude and more.


# 2.0 Part information

The GPS module used was the Adafruit Ultimate GPS module which can be brought in the link below. Additionally the default anntenna can be used to
get GPS readings or an external one can be use. The ceramic anntenna prebuilt is okay with ideally conditions but on this project the extension
antenna will be used . Note to uses the extension antenna , an uFL to SMA adapter will be needed. All parts are listed below.

+ Adafruit GPS Module : https://www.adafruit.com/product/746?srsltid=AfmBOoqGP10HA6yJnmVEq3yWMIVExClqFVGdHWk3Em1MeD_o17Q-r2RR
+ GPS External Antenna: https://www.adafruit.com/product/960
+ Antenna Adapter: https://www.adafruit.com/product/851


# 4.0 Code Flow
For in depthh information of the functions look at the gps.h file which is commented and tell the user what the function does and what 
parameters its take and what it returns. There is an example folder which was done STM32F401RCT6, which was coded in HAL. Usart 1 was
uterlised to talk to the module with Pin B9 to enable it , Pin C13 was used as debugging as this was onboard led.

The code follows a simple sturcture which does not need more explaining then the figure below:

<p align="center">
  <img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/GPS_Module/GPS%20Flow%20diagam.jpeg"
    alt="Figure 1 shows the GPS code structure" width="400">
</p>
<p align="center">Figure 1 shows the GPS code structure</p>

note that each block in Figure 1 is a function (see gps.h and example code for more detail). Thus multiple functions will have to be called
 to obtain the GPS information which is stored in a globle varaible.


