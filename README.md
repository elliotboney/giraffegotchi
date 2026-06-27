# Giraffegotchi

A Tamagotchi-style digital pet (a giraffe) running on an ESP32 **"Cheap Yellow Display" (CYD)**. Feed it, water it, play with it, clean up after it — all on a 240×320 touchscreen sitting in a hand-drawn savanna with drifting clouds, flapping birds, and swaying acacia trees.

## Hardware

- **[ESP32-2432S028R "Cheap Yellow Display" (CYD)](https://amzn.to/4vJQgwv)** — ILI9341 240×320 TFT with resistive XPT2046 touch.

That's the whole bill of materials — the board has the display, touch, USB, and power built in.

## Build & flash

Requires [PlatformIO](https://platformio.org/). Find your serial port with `ls /dev/cu.*usbserial*`.

```bash
pio test -e native                                   # run pet-logic unit tests (32 tests)
pio run -e esp32dev                                  # compile firmware
pio run -e esp32dev -t uploadfs --upload-port <PORT> # flash sprites (data/) to LittleFS
pio run -e esp32dev -t upload   --upload-port <PORT> # flash firmware
```

`uploadfs` is only needed when sprites in `data/` change; firmware-only changes just need `upload`.

## Features

- **Care model:** Hunger / Thirst / Fun / Hygiene meters + on-screen poop to clean up.
- **5 actions:** FEED, DRINK, PLAY, CLEAN, BOOK (read).
- **10 emotion sprites** and **7 animations** (eat, drink, play, clean, sleep, reading, excited).
- **Savanna scene:** sky, golden ground, sun, acacia trees, layered swaying grass, drifting clouds, flapping birds — with the giraffe occluding scenery behind its true silhouette, flicker-free.

## Docs

See [`docs/STATUS.md`](docs/STATUS.md) for the full architecture, the sprite pipeline, and the hard-won display/transparency gotchas.
