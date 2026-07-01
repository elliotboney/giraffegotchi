# Development Guide — Giraffegotchi

_Generated: 2026-06-30_

## Prerequisites

- **[PlatformIO](https://platformio.org/)** (CLI or IDE).
- **Python 3 + Pillow** — only for the art pipeline (`tools/prep_sprite.py`). Not needed to
  build firmware.
- **A CYD board** (ESP32-2432S028R) + USB cable for on-device work. The CH340 serial number
  changes per USB slot — find it with `ls /dev/cu.*usbserial*`.

## Environments

`platformio.ini` defines two:

| Env | Target | What compiles | Use |
|---|---|---|---|
| `esp32dev` | ESP32 hardware | all of `src/` | build + flash firmware |
| `native` | host machine | **only `pet.cpp`** (`build_src_filter`) | run unit tests, no board |

## Commands

```bash
pio test -e native                                   # run the 37 pet-logic unit tests
pio run -e esp32dev                                  # compile firmware
pio run -e esp32dev -t upload   --upload-port <PORT> # flash firmware (auto-resets board)
pio run -e esp32dev -t uploadfs --upload-port <PORT> # flash sprites (data/) to LittleFS
```

- **`uploadfs` is only needed when sprites in `data/` change.** Firmware-only changes just
  need `upload`.
- After `uploadfs`, tap **RST** (or trigger a redraw) — the firmware reads each PNG from flash
  on draw.
- Port "busy or doesn't exist" almost always = board unplugged or a serial monitor holding it.

## Configuration (`.env`)

WiFi + location live in a gitignored `.env` at the project root, injected as build flags by
the `tools/load_env.py` pre-build hook:

```ini
WIFI_SSID="Your Network"
WIFI_PW="your-password"
LAT=30.0858
LON=-97.8403
TZ=CST6CDT,M3.2.0,M11.1.0    # POSIX TZ, handles DST
```

Without `.env` the firmware still builds — it skips WiFi/NTP and stays in daytime.

## Art pipeline

```bash
python3 -m venv /tmp/venv && /tmp/venv/bin/pip install Pillow   # one-time
/tmp/venv/bin/python tools/prep_sprite.py                        # img/ -> data/
pio run -e esp32dev -t uploadfs --upload-port <PORT>            # then flash the new sprites
```

To add a pose: drop `img/<name>.png`, add `<name>` to the `FRAMES` list in `prep_sprite.py`,
regenerate, then `uploadfs`. Sprites are 150×160 with a solid **magenta** background that the
firmware keys out as transparent — magenta must never appear inside the silhouette.

## Testing approach

- **`pet` is the only tested module** — 37 Unity tests in `test/test_pet/test_pet.cpp` cover
  decay, clamping, poop timing, emotion priority/tie-breaks, night sleep, and NVS load/clamp.
- Tests run on `native` (no hardware) because `pet` has no Arduino includes. **Keep it that
  way** — it's the project's main automated safety net.
- `ui`/`main` are hardware-bound and currently have no automated tests; changes there are
  verified by flashing and watching the device (and the serial log — most subsystems print
  `[daynight]`, `[save]`, `[prank]` traces).

## Conventions observed in the codebase

- Constants live as `static constexpr` in `Pet` or `static const` file-scope in `ui`/`main`,
  each with an inline comment explaining the tuning rationale.
- Comments are dense and explain **why** (especially rendering gotchas) — match that density.
- Erases always go through `restoreBg`, never a flat fill (see [architecture.md](./architecture.md) invariant #4).
