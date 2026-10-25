# Changelog

Firmware versions, from the first prototype to the version running on both lamps. Versions not listed were small intermediate builds. Hardware and print versions are at the end.

## v2.6 — 2026-10-25

- Softer "thinking of you": `GLOW_LEVEL` 0.45, about half the brightness of v2.5, adjustable live with `livello_respiro`.
- Both lamps updated over the air to v2.6.

## v2.5 — 2026-10-25

- Final colors, picked live on the finished lamp: green `{55, 200, 0}` for lamp A, sunflower yellow `{255, 115, 5}` for lamp B, full red `{255, 0, 0}` for the 30th. Each main color has a companion (lime, orange) and the breathing is a slow wave between the two.
- New heartbeat: lub-dub at 50 bpm, a strong first beat and a soft light between beats.
- The tap is sent as soon as the finger is lifted.
- Ghost-touch filter: a touch counts only after 50 ms, and the release has hysteresis. Before, random green flashes could appear while the LEDs were bright.

## v2.3 — 2026-10-25

- First successful remote update (v2.2 → v2.3), in about a minute.

## v2.2 — 2026-10-25

- Remote update made sturdier: detailed error reasons on `/log` (with downloaded KB and signal), up to 3 attempts, 30 s HTTP timeout.

## v2.1 — 2026-10-25

- First remote update attempt: aborted at a weak signal (−86 dBm). The lamp stayed on its old version and came back online by itself.

## v2 — 2026-10-24

- Double tap: "goodnight" sunset.
- Unseen "thinking of you": one LED stays on until you touch your lamp.
- Special dates with their own animations, on both lamps: anniversary, birthdays, Valentine's Day, Christmas, New Year.
- The 30th of every month: solid red all day, with an animation at 21:00.
- Remote updates (OTA) with automatic rollback.
- Retained status (`online`, `offline`), remote commands and replies over MQTT.

## v1 — 2026-10-24

- First firmware, loaded on both boards and tested without the LED ring: touch, `tap`, `heart_on`/`heart_off` in both directions, the setup network names.

## Hardware and 3D prints

- **Base v4.1** — 2026-10-24: channel under the ring pads, so the solder joints no longer lift the ring. Shade and cap stay v4.
- **v4**: PLA Matte instead of ABS, bigger lettering with open counters, text moved above the LEDs.
- **v3**: 15 cm with a 0.8 mm wall, in ABS: it cracked between layers, and the letters came out filled.
- **v2**: 17.5 cm with a 1.2 mm wall: too little light came through.
- **v1**: one-piece 11 cm heart dome in ABS: layer shift a third of the way up.
- The touch sensor moved from a touch plate to the metal latch of the box.
