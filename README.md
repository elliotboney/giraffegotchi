# Giraffegotchi

<img width="523" height="388" alt="image" src="https://github.com/user-attachments/assets/f9c86c1c-1b28-4a17-929a-1d7cde17333a" />

A Tamagotchi-style digital pet — a giraffe — running on an ESP32 **"Cheap Yellow Display" (CYD)**. Feed it, water it, play with it, and clean up after it on a 240×320 touchscreen, in a hand-drawn savanna that follows the real sun: the sky shifts through eight colors across the day, the sun and moon arc overhead, and the giraffe naps at night. It blinks, flicks its ears, daydreams, and remembers its state through a power cut.

## What it does

Tap five buttons to keep the giraffe happy. Its mood is driven by four care meters, and its world is driven by the actual time and your location — sunrise, sunset, and the day/night cycle are computed on-device.

### Care model
- **Four meters:** Hunger, Thirst, Fun, Hygiene. Hunger/Thirst/Fun drain slowly over time (~1 point/min); Hygiene only drops when the giraffe poops.
- **Five actions:** FEED, DRINK, PLAY, CLEAN, BOOK (read).
- **10 emotion sprites** — happy, hungry, thirsty, bored, dirty, sad, sick, sleepy, excited, reading — resolved by priority from the meters.
- **Poop** spawns on a timer and has to be cleaned, or Hygiene suffers.
- **Rotating PLAY mini-games:** butterfly, bubbles, kite, and a ball kick.

### Day/night cycle (synced to real time + your location)
- One-shot **WiFi + NTP** time sync at boot, then WiFi drops — the RTC keeps time.
- **Sunrise/sunset** computed locally from your lat/long (NOAA almanac algorithm), recomputed daily. No API needed.
- **Eight sky phases** — night, dawn, sunrise, morning, day, afternoon, sunset, dusk — each with its own sky/ground palette.
- **Sun and moon** ride a left-to-right arc, rising and setting at the horizon and passing *behind* the giraffe and the trees at their peak. Stars and fireflies come out at night.
- Offline-safe: no WiFi → it just stays daytime.

### A living world
- **Idle tics:** the giraffe blinks, flicks its ears, and swishes its tail between actions, so it's never frozen.
- **Daydream bubbles:** a thought bubble pops up beside its head now and then with a little wish (apple, heart, music note, butterfly).
- **Ambient critters:** clouds and birds drift by, butterflies wander the daytime sky, and fireflies blink at dusk.
- **Night sleep:** ~30 minutes after sunset the giraffe goes to sleep until sunrise. While it sleeps, decay/poop/sickness all pause — so you won't wake up to a sick giraffe. A care action wakes it for a few minutes, then it dozes off again.

### Quality-of-life
- **Save state:** care stats + poop count are persisted to NVS flash and restored on boot, so a power loss picks up right where it left off.
- **Auto-dim:** the backlight dims after 5 minutes with no touch and snaps back to full on any tap.
- **Mountable either way:** the display flips 180° in firmware for upside-down mounts.

### Easter egg 🥚
Mash the care buttons really fast and the giraffe "dies" (it's a prank). The dead state even survives a power cycle — the only way to bring it back is to mash the **BOOK** button fast.

## Hardware

- **[ESP32-2432S028R "Cheap Yellow Display" (CYD)](https://amzn.to/4vJQgwv)** — ILI9341 240×320 TFT with resistive XPT2046 touch.

That's the whole bill of materials — the board has the display, touch, USB, and power built in.

## Configuration

WiFi credentials and your location live in a `.env` file (gitignored) that a PlatformIO pre-build hook (`tools/load_env.py`) injects as build flags. Create `.env` in the project root:

```ini
WIFI_SSID="Your Network"
WIFI_PW="your-password"
LAT=30.0858                  # your latitude
LON=-97.8403                 # your longitude
TZ=CST6CDT,M3.2.0,M11.1.0    # POSIX timezone string (handles DST automatically)
```

Without a `.env`, the firmware still builds and runs — it just skips WiFi/time and stays in daytime.

## Build & flash

Requires [PlatformIO](https://platformio.org/). Find your serial port with `ls /dev/cu.*usbserial*`.

```bash
pio test -e native                                   # run pet-logic unit tests (37 tests)
pio run -e esp32dev                                  # compile firmware
pio run -e esp32dev -t uploadfs --upload-port <PORT> # flash sprites (data/) to LittleFS
pio run -e esp32dev -t upload   --upload-port <PORT> # flash firmware
```

`uploadfs` is only needed when sprites in `data/` change; firmware-only changes just need `upload`.

## Art pipeline

The giraffe is illustrated art, not procedural. High-res source PNGs live in `img/`; `tools/prep_sprite.py` keys out the background to magenta (the firmware's transparency color), autocrops, and downsamples them into the `data/` sprites that ship to LittleFS.

```bash
python3 -m venv /tmp/venv && /tmp/venv/bin/pip install Pillow   # one-time (Pillow isn't system-wide)
/tmp/venv/bin/python tools/prep_sprite.py                        # img/ -> data/
```

To add a new pose: drop `img/<name>.png`, add `<name>` to the `FRAMES` list in `prep_sprite.py`, regenerate, then `uploadfs`.

## Project layout

| Path | What's there |
|---|---|
| `src/pet.{h,cpp}` | Hardware-free pet logic (meters, emotions, decay, night sleep, save/load). Unit-tested on `native`. |
| `src/ui.{h,cpp}` | Scene rendering — sky palettes, sun/moon arc, trees, grass, clouds, critters, sprite decode. |
| `src/main.cpp` | Glue: touch input, WiFi/NTP, day/night driver, animations, NVS persistence, backlight. |
| `test/` | Unity tests for the pet logic. |
| `tools/` | `prep_sprite.py` (art pipeline), `load_env.py` (.env → build flags). |
| `img/` → `data/` | Source art → processed LittleFS sprites. |

## Docs

See [`docs/STATUS.md`](docs/STATUS.md) for the full architecture, the sprite pipeline, and the hard-won display/transparency gotchas.
