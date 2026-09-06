Universal Nixie Display
=======================

This is the central project for the open source software and hardware for the Universal Nixie Display.
The display is something more than a desktop nixie clock - it can be whatever you want it to be by
displaying not only time but also weather data or any data you want it to display.

**At the moment it is a work in progress.**


Physical Components
-------------------

The display is modular. Each board works on its own (or with a different master), the default
configuration is:

- **Nixie board** (top): up to 6 nixie tubes (attached via pluggable socket boards with current-limiting
  trimmers, so various nixie models can be used) + up to 2 neon lamps. Driven by an ATmega328P that
  receives commands over I²C (address `0x4`).
- **VFD board** (bottom): one IV-18 8-digit 7-segment VFD tube. Driven by an ATmega328P that receives
  commands over I²C (address `0x5`).
- **Pi hat**: sits on a Raspberry Pi, sends commands to the boards above, takes power from an external
  supply and distributes it, and exposes connectors for control buttons / knobs and the power button.
- **Socket boards**: small per-tube-model boards (IN-14, Z573M) and a neon lamp board that plug into
  the nixie board.


Operating principles
--------------------

The microcontrollers provide low-level control over the displays - multiplexing, dimming, displaying
letters or digits, simple animations. The Raspberry communicates with them using I²C and sends simple
commands (what characters to display, at what brightness etc.). There are python scripts on the
Raspberry that can obtain various data (e.g. weather data from Ruuvi sensors, time from an RTC,
whatever else the user wants) and pass it to be displayed on the nixies and VFD using provided python
libraries that wrap the microcontroller commands in an easy-to-use API.

By default the nixies are meant to be used as the "main" display, whereas the VFD is a "secondary"
display, used to show currently chosen options etc. But it's up to the user to decide how they want to
use them.


Repository layout
-----------------

```
firmware/
  nixie-controller/   PlatformIO project, ATmega328P firmware for the nixie board
  vfd-controller/     PlatformIO project, ATmega328P firmware for the VFD board
software/
  raspberry/          Python scripts + systemd units for the Raspberry Pi master
hardware/
  nixie-board/        KiCad project of the nixie board
  vfd-board/          KiCad project of the VFD board
  pi-hat/             KiCad project of the Raspberry Pi hat
  sockets/            KiCad projects of the tube socket boards (in14, z573m, neon)
  lib/                Shared KiCad symbol / footprint / 3D libraries used by the boards above
scratch/
  kicad-experiments/  Throwaway KiCad experiments, not part of the product
```

See `hardware/README.md` for the board naming history, PCB variants and fabrication files.

Each firmware directory is a self-contained PlatformIO project - open it directly in PlatformIO / your
IDE. Everything in this repository is licensed under the GPLv3 (see `LICENSE`).


History
-------

This repository was assembled from several previous ones (the firmware and Raspberry code used to live
in separate repos pulled in as git submodules, the hardware lived in an older private repo). Their git
histories were rewritten into subdirectories and merged here, so `git log --follow` works on the
imported files. Old feature branches were kept under the `nixie-controller/`, `vfd-controller/` and
`raspberry/` branch prefixes, VFD firmware releases are tagged `vfd-controller/<version>`.
