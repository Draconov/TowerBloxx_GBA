# Tower Bloxx v1.5.22 — menu worker and HUD texture evidence

This note records only behavior/assets that are directly backed by the canonical
`Tower-Bloxx_v1522.jar`, recovered `House` / `m` bytecode and captured JAR play.
It exists to stop the GBA renderer from replacing source graphics with invented
text or fixed-position animation.

## Menu background workers (`House`)

The main menu states render `House.b(Graphics)` before the menu overlay. The
worker seen to the right of the menu therefore belongs to the shared animated
background, not to `k`'s menu renderer.

`House.c(int,int)` maintains a static 18-int array: 3 workers × 6 integers:

1. X in 8.8-ish fixed-point screen units
2. Y in fixed-point screen units
3. animation state (0..7)
4. horizontal velocity
5. vertical velocity
6. worker color/strip selector

Recovered update rules:

- clamp supplied delta to 150 ms;
- accumulate time and skip motion until at least 25 ms is available;
- background scroll advances by `dt >> 1`;
- animation timer counts down and reloads to 300 ms;
- animation state advances `(state + 1) % 8`;
- position updates as `x += vx * dt >> 6`, `y += vy * dt >> 6`;
- when a worker reaches the bottom bound `I`, respawn it with:
  - `x = random(H)`;
  - `y = -267 - random(I)`;
  - `state = random(8)`;
  - `vx = random(16) - 8`;
  - `vy = 10 + random(10)`;
  - `variant = random(2)`.

For a 240×160 GBA viewport the corresponding fixed bounds are
`H = 256*240/22 = 2792` and `I = 256*160/22 = 1861`.

`House.b(Graphics)` maps animation state to visible source-strip frame as:

`1, 2, 3, 4, 5, 4, 3, 2`

for states 0..7. Resource 11 is the blue worker strip and resource 12 is the
red worker strip; both are 190×23 and contain ten 19×23 cells. The old Fix-5
`frame=(frame+1)%10` at a fixed coordinate is therefore not source behavior.

## Proven source HUD/UI resources

| Resource | Geometry | Proven use / export interpretation |
| --- | --- | --- |
| 13 | 55×12 | five 11×12 tower target badges (`10`,`20`,`30`,`40`, roof/target cell) |
| 14 | 70×7 | fourteen 5×7 white digit/symbol cells; Quick Game counter digits |
| 15 | 60×7 | twelve 5×7 brown digit/symbol cells; combo/top HUD digits |
| 16 | 55×7 | eleven 5×7 red digit/symbol cells; loaded by Build City `m` |
| 17 | 7×19 | construction/HUD status graphic; its top 6×9 clip is the Quick Game/tower population icon |
| 18 | 60×6 | ten 6×6 state-indicator cells used by construction HUD |
| 19 | 13×28 | Quick Game lower-left counter frame |
| 20 | 12×23 | Build City UI sprite loaded by `m` |
| 21 | 30×9 | five 6×9 Build City status/icon cells |
| 22 | 26×11 | two 13×11 Build City panel cells |
| 23 | 21×21 | Build City action icon |
| 24–27 | strips | four building-type animation/state strips (already used by GBA city) |
| 28 | strip | five city-lot/selection frames (already used by GBA city) |
| 29 | 132×22 | six 22×22 Build City effect/smoke cells |
| 30 | 90×16 | five 18×16 hook animation cells |
| 31–34 | environment | fence/barrier/container/tree; already composited into `construction_bg` |
| 35 | 27×10 | three 9×10 accuracy/star effect cells |
| 36 | font atlas | source UI font |

## Quick Game HUD (`House.i(Graphics)`)

For Quick Game (`B != 3`) resource 19 is drawn at the lower left and resource
14 is used to draw the floor number into its digit window. Resource 18 supplies
the three lower-left state/chance cells; the Quick Game family selects the
orange pair (base frame 6) and exhausted cells use frame 8. When at least one
floor exists, the top 6×9 clip of resource 17 is drawn at the lower right beside
a five-digit population value from resource 14. The combo meter is Java-drawn
geometry and resource 15 supplies the small brown `x`/combo digits. The GBA
Fix-5 `Floors:`, `Population:` and `***` text is presentation invented for
bring-up and must not remain as the normal-play HUD.

## Tower construction HUD (`House.i(Graphics)`, `B == 3`)

Construction selects resource-13 target badge frame `L-1`, where `L` is the
building type. The vertical status cells use resource 18 with the base frame
`2*(L-1)`; exhausted/blinking states use later cells in the same strip. This
matches the captured blue `10` badge + vertical cyan bars for a residential
construction run.

## Build City (`m`)

`m.a()` loads 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 and digit strips 14, 15,
16. Therefore a renderer that only has resources 24–28 cannot be visually
complete. Fix 6 exports the missing family, but placement of 20–23/29 remains
for the dedicated Build City compositor pass rather than guessing coordinates.

## Fix 7 Build City placement

The Build City renderer now places the already-extracted source families instead of leaving them as archive-only evidence: resource 21 status glyphs, brown population digits, resources 24-27 building frames, resource 28 placement outlines, resource 29 placement effects, and resource 23's action/discard icon. The background exporter also reproduces the recovered blue gradient, 88x88 road grid, selector shell, status band, top-right value boxes and bottom message panel.
