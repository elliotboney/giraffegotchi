<!-- Fill in the two {{...}} slots, then paste this whole file into a ChatGPT chat. -->

Generate a set of pixel-art sprites of a cute cartoon **{{ANIMAL — e.g. "zebra"}}**, in the style
of a 1990s handheld virtual pet / 16-bit Game Boy Color game. The animal looks like:
**{{body color, markings, distinctive features — e.g. "white body with black stripes, short
upright mane, little hooves"}}**.

Style, applied identically to every sprite:
- Full body, front-facing, standing, centered with even margin around it.
- Small ~64-pixel-tall sprite shown large with NO smoothing,  no anti-aliasing — chunky visible square pixels, hard
  edges, no anti-aliasing, no gradients.
- Limited retro palette of about 12 colors, bold 1-pixel dark outline, flat cell shading with
  simple dithering for shadows.

Hard rules so the sprites are usable:
1. Give each sprite a **fully transparent background** (transparent PNG). Keep the edges **hard**:
   every pixel is either fully the animal or fully transparent — no soft, feathered, blurred, or
   semi-transparent edge pixels, no drop shadow, no scenery.
2. **Keep the framing and scale identical in every pose** — the body and head stay in the same
   position across all sprites; only the part that moves changes (eyes for a blink, ears, a leg
   for a kick). The poses must line up exactly when overlaid, with no jump.
3. Front-facing, full body, standing in every pose except the "dead" one.
4. Consistent outline weight and palette across the whole set.
5. Output each sprite as its own separate image so I can save them individually.

Work in two steps:

**Step 1 — Generate ONLY the `happy` pose (#1 below), then STOP and wait for my approval.** Do
not generate any other pose yet. I want to check the style and the animal on one sprite first.

**Step 2 — After I say it's approved, generate the remaining 18 poses**, matching the approved
`happy` sprite's exact style, palette, framing, and scale. Generate **at most 10 per message**:
first poses #2–#10, then in your next message poses #11–#19.

The 19 poses. Use the given name for each:

Emotions:
1. happy — content, gentle smile, relaxed standing
2. hungry — droopy, mouth open, looking toward its belly
3. thirsty — parched, tongue out slightly, small sweat bead
4. bored — half-lidded eyes, slumped, looking away
5. dirty — grubby with a few dirt smudges and a little fly buzzing
6. sad — frown, head and features drooping down
7. sick — greenish tint, queasy expression, sweat drop
8. sleepy — eyes nearly closed, mid-yawn
9. excited — big open smile, sparkle in the eyes, perked up
10. reading — looking down, absorbed and calm (a small book is optional)

Happy variations (all still clearly "happy", for an idle animation):
11. happy2 — happy with a slight head tilt
12. happy3 — happy with an open-mouth grin or different eye shape

Idle twitches (tiny motions over the happy face):
13. blink — eyes fully closed
14. ears_up — ears (or fins/crest) perked up
15. ears_down — ears flattened or lowered
16. tail_left — tail swished to the left

Kick mini-game:
17. kick — a leg cocked back (wind-up / recovery)
18. kick2 — the same leg fully extended forward (the kick)

Prank death:
19. dead — lying on its back or side, X-shaped eyes, tongue out (the only non-standing pose)

If this animal genuinely lacks a body part (a snake has no ears, a fish no legs), skip that pose
rather than inventing one — but generate everything you reasonably can.

Also generate a small **icon** sprite for the on-device animal-select menu: a simple, cute badge of
this animal (its head/face reads best), same pixel-art style, transparent background, framed as a
small square (not at the body poses' scale). Name it `icon`. This is what shows in the picker grid.

Optional extra: if this animal should eat something other than the default apple, also generate a
small **food** sprite (the item it eats — e.g. a carrot for a groundhog), same pixel-art style,
transparent background, framed as a small standalone item (not at the body poses' scale). Name it
`food`. Skip it to keep the default apple.
