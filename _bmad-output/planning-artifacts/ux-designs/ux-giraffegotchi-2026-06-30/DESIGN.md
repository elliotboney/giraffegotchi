---
name: giraffegotchi-selector
status: final
created: '2026-06-30'
updated: '2026-06-30'
scope: The on-device animal-selector screen only. Ratifies the existing firmware's visual language; does not introduce a new brand.
sources: [_bmad-output/planning-artifacts/architecture/architecture-giraffegotchi-2026-06-30/ARCHITECTURE-SPINE.md]
colors:
  panel_bg: '#101820'      # dark neutral behind the picker (RGB565 ~0x0865)
  tile_bg: '#37474F'       # tile fill, dark slate (RGB565 ~0x39C7)
  tile_selected: '#FFFFFF' # selected tile outline / highlight
  text: '#FFFFFF'
  text_dim: '#9E9E9E'      # inactive labels
  accent_back: '#01305F'   # 'back' affordance, reuses BOOK navy (RGB565 0x019F family)
typography:
  small: 'TFT_eSPI FONT2'  # tile labels, hints
  large: 'TFT_eSPI FONT4'  # screen title
rounded:
  button: 8                # matches existing fillRoundRect r8
spacing:
  screen: '320x240 landscape'
  tile: '64x72'            # >=44px touch target; 5 across fits 320w with gutters
  gutter: 4
components: [picker_tile, back_button, long_press_zone]
---

# Design — Giraffegotchi Animal Selector

## Brand & Style

Inherited, not invented. The selector lives inside an existing hand-drawn pixel-pet firmware: chunky rounded buttons, white 1px outlines, bright RGB565 fills, playful. The picker is a **quiet modal** — a dark neutral panel so the colorful animal thumbnails carry the screen. No new fonts, no new shapes; it reads as the same device.

## Colors

RGB565, drawn with TFT_eSPI primitives. The picker deliberately drops the biome sky/ground palette (which is per-animal and would clash with the thumbnails) for a fixed dark panel.

| Token | Hex | Use |
| --- | --- | --- |
| `{colors.panel_bg}` | #101820 | full-screen picker background |
| `{colors.tile_bg}` | #37474F | animal tile fill |
| `{colors.tile_selected}` | #FFFFFF | outline on the current animal's tile |
| `{colors.text}` | #FFFFFF | title, active labels |
| `{colors.text_dim}` | #9E9E9E | (reserved) unavailable/locked labels |
| `{colors.accent_back}` | #01305F | `back` affordance (reuses the BOOK navy) |

Low-meter red, care-button colors, and biome palettes are **out of scope** here — they belong to the pet screen (DESIGN owned by the firmware, not this spine).

## Typography

Two sizes only, both existing: `{typography.large}` (FONT4) for the `PICK YOUR PAL` title; `{typography.small}` (FONT2) for tile names and the hold hint. No custom glyphs.

## Layout & Spacing

- Title band: top ~y8–30, centered.
- Tile grid: `{spacing.tile}` tiles, `{spacing.gutter}`px gutters, 5 columns × up to 2 rows, centered in the body.
- `back` affordance: bottom row, reusing the button-row band (~y198–236).

## Shapes

Rounded rectangles at `{rounded.button}` radius (identical to the care buttons) — one shape system across the device.

## Components

- **`picker_tile`** — `{spacing.tile}` rounded rect, `{colors.tile_bg}` fill; the species' `icon` sprite centered (falls back to a scaled `happy` sprite), lowercase name in FONT2 below. Current animal gets a `{colors.tile_selected}` outline. Behavior in EXPERIENCE.md.
- **`back_button`** — rounded rect, `{colors.accent_back}`, white `back` label. Cancels with no change.
- **`long_press_zone`** — invisible; the existing BOOK button rect (x260,y198,60,38) doubles as the picker-open hold target. Behavior in EXPERIENCE.md.

## Do's and Don'ts

- **Do** keep the picker's panel neutral so the animal art is the focus.
- **Do** reuse the existing r8 rounded-rect + white-outline button idiom.
- **Don't** paint a biome sky/ground behind the grid — thumbnails span biomes and would clash.
- **Don't** add a new font, color ramp, or icon set; this screen must read as the same device.
