# Review — Version / Stack Reality Check

**Reviewer lens:** Verify every committed technology decision in the architecture spine is reality-checked against `platformio.ini`, not asserted from memory. Brownfield standard: does the spine faithfully ratify what `platformio.ini` already pins? (No upgrade recommendations.)

**Files reviewed:**
- Spine: `_bmad-output/planning-artifacts/architecture/architecture-giraffegotchi-2026-06-30/ARCHITECTURE-SPINE.md`
- Ground truth: `platformio.ini`

**Verdict: PASS-WITH-FIXES**

The Stack table is mostly faithful — the two real library pins (TFT_eSPI, PNGdec) match exactly. But three rows use vague placeholders where `platformio.ini` has (or lacks) a concrete fact that should be cited instead, one row is actively misleading, and the Board row's identifier does not match the actual `board =` value.

---

## Line-by-line: Stack table vs platformio.ini

Spine Stack table (lines 108–116):

| Spine Name | Spine Version | platformio.ini ground truth | Result |
| --- | --- | --- | --- |
| `platform: espressif32` | `as-pinned` | `platform = espressif32` (line 8) — **no version specifier** | **MISLEADING** — nothing is pinned |
| `framework: arduino` | `—` | `framework = arduino` (line 10) — no version | OK |
| `TFT_eSPI` | `^2.5.43` | `bodmer/TFT_eSPI@^2.5.43` (line 15) | **MATCH** |
| `XPT2046_Touchscreen` | `PaulStoffregen git head` | `https://github.com/PaulStoffregen/XPT2046_Touchscreen.git` (line 16) | Accurate; citable more precisely |
| `PNGdec` | `^1.1.0` | `bitbank2/PNGdec@^1.1.0` (line 17) | **MATCH** |
| `Unity (test, env:native)` | `bundled` | `test_framework = unity` (line 44) | Vague; concrete token exists |
| `Board` | `ESP32-2432S028R (CYD) — ILI9341 + XPT2046` | `board = esp32dev` (line 9); `-DILI9341_2_DRIVER=1` (line 22) | **MISMATCH on board id** |

---

## Findings

### F1 — `platform: espressif32 = as-pinned` is misleading (there is no pin) `[FIX]`
Spine line 110 lists the version as `as-pinned`. `platformio.ini` line 8 is `platform = espressif32` with **no `@version` specifier at all** — the platform floats to whatever PlatformIO resolves at build time. "as-pinned" asserts a pin that does not exist. This is exactly the "asserted from memory" failure this review guards against.

- **Cite instead:** `espressif32 (unpinned — floats)` or `espressif32 (no version constraint)`.
- Optional note for the team (not an upgrade rec): the build is reproducible only to the extent PlatformIO caches the resolved platform; the spine should not imply otherwise.

### F2 — Board row identifier does not match `board =` `[FIX]`
Spine line 116 names the board `ESP32-2432S028R (CYD)`. The actual PlatformIO board id is `board = esp32dev` (line 9). The `ESP32-2432S028R` name is a hardware/marketing descriptor for the CYD unit, **not** the value the build uses. The driver half of the row (`ILI9341`) *is* grounded — `-DILI9341_2_DRIVER=1` (line 22) — and the header comments (lines 1–2) confirm ILI9341 + XPT2046 on split SPI buses.

- **Cite instead:** `board: esp32dev (CYD ESP32-2432S028R hardware) — ILI9341 driver (ILI9341_2_DRIVER) + XPT2046 touch`.
- The `XPT2046` half of the row is not a "board" fact; it's the touch library (row above). Keeping it in the Board cell is harmless but redundant.

### F3 — `XPT2046_Touchscreen = PaulStoffregen git head` — cite the exact URL `[FIX]`
Spine line 113 says `PaulStoffregen git head`. `platformio.ini` line 16 has a concrete, copy-pasteable constraint: `https://github.com/PaulStoffregen/XPT2046_Touchscreen.git`. "git head" is a paraphrase; the URL is the ground truth and is unpinned (no `#tag`/`#commit`), so "head" is *directionally* correct but the citable form is the URL.

- **Cite instead:** `git: PaulStoffregen/XPT2046_Touchscreen.git (unpinned, default branch)`.

### F4 — `Unity = bundled` — cite the actual declaration `[MINOR]`
Spine line 114 says `bundled`. `platformio.ini` line 44 declares `test_framework = unity` under `[env:native]` (with `test_build_src = yes`, `build_src_filter = -<*> +<pet.cpp>`, lines 45–46). Unity is provided by PlatformIO's test runner, so "bundled" is not wrong — but the spine has a concrete token to cite.

- **Cite instead:** `test_framework = unity (env:native)`.

### F5 — No fabricated dependencies `[PASS]`
Every library named in the Stack table traces to a real `lib_deps` / config entry. The spine's Mermaid hardware node (line 40) also lists `LittleFS`, `NVS`, `WiFi` — these are **not** `lib_deps`, but they are legitimately grounded in `platformio.ini`: `board_build.filesystem = littlefs` (line 12) covers LittleFS, and NVS/WiFi are ESP32-Arduino core facilities implied by `framework = arduino` on `platform = espressif32`. No invented third-party dependency. `USER_SETUP_LOADED` / `TFT_*` pins in the Mermaid/prose are all present as `-D` flags (lines 20–39).

---

## Out-of-lens claims (not in platformio.ini — flagged, not failed)

These appear in the spine but cannot be confirmed or denied against `platformio.ini`; they belong to `src/*` / `docs/*`, which are outside this review's ground-truth file. Listed so the team knows they were **not** version-checked here:

- **`GPIO 26 speaker`** (Deferred, line 144) — no GPIO 26 flag in `platformio.ini`. Verify against `src/*` before treating as fixed.
- **Box coords `x 85–235, y 34–194`** (AD-4, line 68), **magenta `0xF81F`** (AD-6, line 79), **NVS `SAVE_MAGIC` / `PetSave`** (AD-8) — all `src/*` facts, not build config. Not checked.
- **`.env` → `tools/load_env.py`** (AD-9, line 94) — the pre-build hook *is* grounded: `extra_scripts = pre:tools/load_env.py` (line 13). ✓

---

## Recommended Stack table (drop-in)

| Name | Version / Constraint |
| --- | --- |
| platform: espressif32 | unpinned (no version constraint) |
| framework: arduino | — (core, no version) |
| TFT_eSPI | ^2.5.43 (bodmer/TFT_eSPI) |
| XPT2046_Touchscreen | git: PaulStoffregen/XPT2046_Touchscreen.git (unpinned) |
| PNGdec | ^1.1.0 (bitbank2/PNGdec) |
| Unity (test) | test_framework = unity (env:native) |
| Board | esp32dev — ILI9341 driver (ILI9341_2_DRIVER) + XPT2046 touch; CYD ESP32-2432S028R hardware |

---

## Summary

- **Matches (no change):** TFT_eSPI `^2.5.43`, PNGdec `^1.1.0`.
- **Fix — vague/misleading vs a concrete fact in the ini:** F1 (`as-pinned` → unpinned), F2 (board id `esp32dev`), F3 (exact git URL), F4 (`test_framework = unity`).
- **No fabricated dependencies.** Every named tech is real.
- Nothing here is an upgrade recommendation; all fixes make the spine *cite what the ini already says*.
