# Printing

Print one shade, one cap and one base per lamp, each part on its own plate. Print one set first, check the fits and the light with the ring on, then print the second set.

## Setup used

- Printer: Bambu Lab A1 mini (180 × 180 × 180 mm), 0.4 mm nozzle, Textured PEI plate. Any printer that fits about 112 × 101 × 149 mm will do.
- Filament: **Bambu PLA Matte, white**. ABS was tried first and cracked between layers.
- Slicer: Bambu Studio, filament profile "Bambu PLA Matte @BBL A1M", process "0.20mm Standard @BBL A1M", advanced settings shown.

## Before printing

- Wash the plate with water and dish soap.
- Do not rotate or scale the models: they are already oriented for printing.
- The shade is already hollow and open at both ends: do **not** use spiral vase mode.

## Settings per part

| Setting | Shade (`tube_v4.stl`) | Cap (`cap_v4.stl`) | Base (`base_v4_1.stl`) |
| --- | --- | --- | --- |
| Nozzle temperature | 225 °C | profile default | profile default |
| Wall generator | Arachne | Arachne | profile default |
| Wall loops | 2 | 3 | 3 |
| Seam position | Back | — | — |
| Wall speed | outer 60 mm/s, inner 100 mm/s | profile default | profile default |
| Acceleration | outer wall 2000, normal 4000 | profile default | profile default |
| Infill | — | 15% | 20% |
| Supports | none | none | none (the channel is open upwards, no overhangs) |
| Brim | 8 mm, outer **and** inner | none | none |
| Time and filament | about 3–4 h, about 50 g | about 30 min | about 40 min |

Check the shade in the slicer preview: names and dates outside with the holes of letters like a, e, d and 0 open, and the hidden message on the inside wall at half height (mirrored, seen from inside).

## After printing

- Let the plate cool down, then flex it to release the part.
- Remove the brim, inside the shade too.
- Test the fit: cap and shade should go together with a little push. If it is too tight, sand the rim lightly.

## Print history

| Version | What changed |
| --- | --- |
| 1 | One-piece heart dome, 11 cm, ABS: layer shift at one third of the height. |
| 2 | 17.5 cm tall, 1.2 mm wall: too little light through the wall. |
| 3 | 15 cm, 0.8 mm wall, ABS: cracked between layers after about 1 cm; the letters came out filled (bug). |
| 4 | PLA Matte, bigger lettering with open counters, text moved higher, above the LEDs. |
| 4.1 | Base only: channel for the ring pads and solder joints. Shade and cap stay v4. |
