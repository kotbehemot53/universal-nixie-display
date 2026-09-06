# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Monorepo for the Universal Nixie Display: a modular nixie + VFD display. Two ATmega328P boards
(nixie board, VFD board) are I2C slaves; a Raspberry Pi on a custom hat is the I2C master and
runs the user-facing logic. Firmware, Pi software and KiCad hardware all live here; each part
must keep working on its own (a board with a different master, a board without the other).

```
firmware/nixie-controller/   PlatformIO, ATmega328P, I2C slave 0x4  (single main.cpp)
firmware/vfd-controller/     PlatformIO, ATmega328P, I2C slave 0x5  (layered libs under lib/)
software/raspberry/          Python 3 on the Pi (smbus, RPi.GPIO, gpiozero) + systemd unit
hardware/                    KiCad 6/7-format projects, one dir per board, shared libs in lib/
scratch/                     throwaway KiCad experiments, not part of the product
```

`hardware/README.md` and `hardware/lib/README.md` are the authoritative hardware docs (naming
history, variants, fab files, library origins, known upstream-rename warnings). Read them before
touching anything under `hardware/`.

## Commands

Firmware (run inside the firmware project directory):

```
pio run                      # build
pio run -t upload            # flash (board = ATmega328P via arduino framework)
pio device monitor           # 115200 baud
```

There are no automated tests. `firmware/nixie-controller/test/` only holds the PlatformIO README.

Pi software has no build step. It runs on the Pi against real hardware (`smbus`, `RPi.GPIO`,
`gpiozero`, `Encoder`); on a dev machine the most you can do is `python -m py_compile`.
`systemctl/lampsOnOff.service` runs `i2cnums/start.sh`, which starts `doStuff.py` in the
background. Both scripts still hard-code the pre-monorepo path `/home/pi/src/piclock/i2cnums`.

Hardware verification is headless with `kicad-cli` (KiCad 9 or newer). Run from the board's
directory so `${KIPRJMOD}` resolves:

```
kicad-cli pcb drc --severity-all --format json -o drc.json <board>.kicad_pcb
kicad-cli sch erc --severity-all --format json -o erc.json <board>.kicad_sch
kicad-cli pcb export step --no-dnp -o /tmp/x.step <board>.kicad_pcb   # "File not found" = missing 3D model
kicad-cli fp export svg -o /tmp/x hardware/lib/<lib>.pretty            # one bad file fails the whole lib
```

In the JSON, only `lib_footprint_issues` / `lib_symbol_issues` matter for library resolution.
`*_mismatch` entries are expected (embedded 2022 copies vs. current KiCad libs) and so are the
handful of upstream-renamed parts and five missing 3D models listed in `hardware/README.md`.
Anything else is a regression.

## I2C protocol (the contract between all three parts)

One command = one byte; high nibble is the command, low nibble is usually the argument
(digit index, duty cycle, comma position). Some commands take a second "following" byte.
The Python wrappers `software/raspberry/i2cnums/piToNixie.py` and `piToVFD.py` mirror the
constants one-to-one, so a protocol change touches three places: the firmware constants, the
Python wrapper, and `doStuff.py` if the semantics changed.

- Nixie board: constants `CMD_*` at the top of `firmware/nixie-controller/src/main.cpp`.
  `CMD_NUM` (0x80) flags a digit value; everything else is matched on the high nibble.
  Commands fill buffers; `CMD_FIN` (0x20) renders them as the next frame.
- VFD board: constants in `firmware/vfd-controller/lib/IV18Display/IV18I2CCommandExecutor.h`.
  "Instant" commands act in the frame they arrive in. "Bunchable" commands queue until
  `CMD_MULTI_FINISH` (0x20) applies them all at once, in order. Bunchable commands listed in
  `FOLLOWED_COMMANDS` need the following byte; sending them without it is undefined behaviour.

## Firmware architecture

Both firmwares are frame loops that multiplex the tube and service the I2C buffer once per frame.
Timing constants (`FRAME_US`, `AFTER_IMAGE_US`, dimming curve) sit next to the pin maps at the top
of the nixie `main.cpp`.

The VFD firmware is the reference architecture (the nixie README's TODO is to refactor towards it):

- `I2CComms` (all static, because the Wire receive callback must be static) double-buffers
  incoming bytes: the ISR writes one buffer while the frame loop drains the other.
- `IV18I2CCommandExecutor` decodes commands from that buffer within `MAX_EXECUTION_TIME_US`
  per frame and holds the bunched-command buffer.
- `DeviceAnimator` is a device-agnostic per-frame scheduler: independent "threads" of
  `DeviceAnimatorStep`s (command handling, digit fades, status LED, intro sequence) advance once
  per `doFrame()`. `IV18Animator` is its IV-18 specialisation; `IV18IntroSequencer` drives the
  intro on top of it; `DutyCycleGenerator` supplies non-linear fade curves.
- `IV18Display` owns the segment/grid output. `AnimatorFailureListener` reports frames that
  overran their budget (debug only, disabled in `main.cpp`).

## Hardware conventions

- Board directories are `nixie-board`, `vfd-board`, `pi-hat`, `sockets/{in14,z573m,neon}`.
  Old names (`nixiecock1`, `nixiecockB`, `pihat_smd`) survive inside the files as title blocks,
  net names and library nicknames. Do not rename library nicknames (`Nixiecock1`, `nixieCockB`,
  `SamacSys_Parts`, ...) - every schematic and PCB references them.
- All library tables point into `hardware/lib` via `${KIPRJMOD}/../lib` (`../../lib` for sockets,
  one more `../` inside `variants/`). `variants/` dirs need their own tables because KiCad uses the
  `.kicad_pro` directory as project root. Keep tables relative; never commit absolute paths.
- 3D models: standard parts use `${KICAD6_3DMODEL_DIR}/....step` (KiCad remaps the version
  number itself; it does not fall back from `.wrl` to `.step`, so keep `.step`). Vendored models
  use `${KIPRJMOD}/../lib/SamacSys_Parts.3dshapes/`.
- The file next to the `.kicad_sch` is the current layout. `variants/` are frozen references.
  `fab/` zips are what was actually ordered; never regenerate over them, add a new one.
- Symbols and footprints are embedded, so "Update from library" is a design change, not a
  cleanup: it pulls in years of upstream library drift and can move pads. Only do it as part of a
  real board revision and diff the result.
- Saving from KiCad 10 rewrites files to the new format (tens of thousands of lines of diff).
  Commit such resaves separately from real changes, and avoid opening-and-saving `variants/`.
- Ignored on purpose: `*-backups/`, `.history/`, `fp-info-cache`, `*.kicad_prl`, lock files.

## Git conventions

- `master` is the integration branch. Imported histories keep their old branches under the
  prefixes `nixie-controller/`, `vfd-controller/`, `raspberry/`; VFD releases are tagged
  `vfd-controller/<version>`. New tags should follow `<component>/<version>`.
- The repo was assembled from `universal-nixie-display-legacy` (+ submodules `und-nixie-controller`,
  `und-vfd-controller`, `und-raspberry`) and the private `piclock` repo. Those are archives now;
  do not push to them.
- `.gitattributes` normalises line endings and marks binaries; leave KiCad files as text.
