Shared KiCad libraries
======================

Library nicknames match what the board files reference, do not rename them without also updating the
`lib_id` / `footprint` references in the schematics and PCBs.

| Nickname             | Files                                              | Used by                           | Origin                                                                |
|----------------------|----------------------------------------------------|-----------------------------------|-----------------------------------------------------------------------|
| `Nixiecock1`         | `Nixiecock1.pretty/`                               | nixie-board, all socket boards    | own footprints (IDC header with alternate pins, double mounting hole) |
| `nixieCockB`         | `nixieCockB.kicad_sym`, `nixieCockB.pretty/`       | vfd-board                         | own (VFD boost converter module symbol + footprint)                   |
| `CableClamps`        | `CableClamps.pretty/`                              | pi-hat                            | own                                                                   |
| `CustomComponents`   | `CustomComponents.lib` / `.dcm` (legacy format)    | nixie-board (K155NA1 symbol)      | third party "KiCadLibs-master", see `CustomComponents-README.md`      |
| `SamacSys_Parts`     | `SamacSys_Parts.lib` / `.dcm` / `.pretty/` / `.3dshapes/` | nixie-board, vfd-board     | Mouser / SamacSys ECAD downloads (SFH619A, 3386F trimmer, TL1105 switch, NE555, Pi Zero W) |
| `nixies-us`          | `nixies-us.lib` / `.mod` / `.lbr` (+ `mynixies`, `nixiemisc`) | sockets/in14           | third party "Eagle-and-KiCAD-Nixie-Libs", see `Eagle-and-KiCAD-Nixie-Libs-README.md` |
| `nixietubes-western` | `nixietubes-western.lbr` (Eagle)                   | sockets/z573m                     | third party Eagle library                                             |

`SamacSys_Parts` used to live in a *global* library table entry pointing at `H:\...\KiCad\6.0\libraries\mouser`.
It is vendored here so the project tables resolve it on their own; if your global table still has an
entry with the same nickname, KiCad will prefer the project one.

The 3D models in `SamacSys_Parts.3dshapes/` are referenced from the PCBs as
`${KIPRJMOD}/../lib/SamacSys_Parts.3dshapes/<file>`; standard KiCad models use `${KICAD6_3DMODEL_DIR}`
and need a KiCad installation with the official 3D library.
