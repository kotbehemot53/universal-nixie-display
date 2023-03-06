Universal Nixie Display
=======================

Nixie microcontroller
---------------------

Hardware: Atmega328p

*This is a work in progress*

Controls the Board A of the Universal Nixie Display (the Nixie module).
Receives commands via I²C.

The current device address is `0x4`.

Available commands
------------------

See the `CMD_` constants in `main.cpp` and their comments.

TODO
----

- refactor the code based on the current VFD Display Firmware architecture ().