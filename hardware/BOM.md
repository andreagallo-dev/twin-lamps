# Bill of materials

Everything below is **per lamp**: buy two of each for the pair.

## Electronics

| Qty | Part | Notes |
| --- | --- | --- |
| 1 | ESP32 devkit, WROOM-32, USB-C | It must be the classic ESP32, which has capacitive touch pins. The boards used here have a CP2102 USB chip (`/dev/ttyUSB0` on Linux). They often come in packs of three: the third one is a handy spare. |
| 1 | WS2812B LED ring, 16 LEDs | 68 mm outer, 54 mm inner diameter. The DI, 5V, GND and DO pads are on the back, near the outer edge. |
| 1 | 330 Ω resistor | In series on the data line. |
| 1 | 100 µF 50 V electrolytic capacitor | Across 5V and GND at the ring, stripe (−) to GND. |
| 1 | Photoresistor (LDR) | Automatic dimming in the dark. |
| 1 | 10 kΩ resistor | Voltage divider with the photoresistor. |
| 2 | M2 bolts with nuts | Connect the touch wire to the metal latch of the box. |
| 1 | 5 V 1 A USB charger | At full brightness the ring draws up to about 0.5 A, plus about 0.25 A for the ESP32: a PC USB port (0.5 A) can make the board reset. |
| 1 | USB-A to USB-C cable, about 1.5 m | A data-capable cable also lets you flash the board without opening anything. |

## Enclosure and printed parts

| Qty | Part | Notes |
| --- | --- | --- |
| 1 | Pine box 12 × 12 × 5 cm with hinged lid and metal latch | Walls about 1 cm thick. The latch doubles as the touch sensor. |
| 1 | Shade (tube), cap and base, printed | [3d-models/](../3d-models/). The shade takes about 50 g of filament. |
| — | Bambu PLA Matte, white | ABS was tried first and cracked between layers. |

## Consumables

- Thin stranded wire in a few colors
- Heat-shrink tubing or electrical tape
- Thick double-sided mounting tape
- Wood glue (PVA) or double-sided mounting tape to fix the base to the lid. **No hot glue on PLA.**

## Tools

- Soldering iron and solder
- Drill with 5 mm and 12 mm bits (a step drill bit works well on thin pine), plus a small bit for the M2 bolts
- Small file or sandpaper (to clean the edge of the LED ring and to adjust the fit of the printed parts)
- Multimeter (optional, for checking the wiring before powering on)
