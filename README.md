Universal Nixie Display
=======================

This is the central project for the open source software and hardware for the Universal Nixie Display. 
The display is something more than a desktop nixie clock - it can be whatever you want it to be by 
displaying not only time but also weather data or any data you want it to display.


Physical Components
-------------------

- 4 nixie tubes + 2 neon lamps to display data (in the future number of nixies and neon lamps will be configurable);
- 1 8-digit 7-segment VFD to display data;
- 2 Atmega328p microcontrollers (one to control the VFD, another for the nixie tubes and neon lamps);
- 1 Raspberry Pi Zero to provide easy means of high-level control over the display;
- custom PCBs to make connect it all together.


Operating principles
--------------------

The microcontrollers provide low-level control over the displays - multiplexing, dimming, displaying
letters or digits. The Raspberry communicates with them using I2C protocol and sends simple commands
(what characters to display, at what brightness etc.). There are python scripts on the Raspberry that
can obtain various data (e.g. weather data from Ruuvi sensors, time from an RTC, whatever else the user wants)
and pass it to be displayed on the nixies and VFD using provided python libraries that wrap the
microcontroller commands in an easy-to-use API.

Be default the nixies are meant to be used as the "main" display, whereas the VFD is a "secondary"
display, used to show currently chosen options etc. But it's up to the user to decide how they want to use them.

**At the moment it is a work in progress.**
