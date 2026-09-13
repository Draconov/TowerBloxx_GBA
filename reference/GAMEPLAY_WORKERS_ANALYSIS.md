# Tower Bloxx v1.5.22 gameplay worker analysis

This note records the clean-room reconstruction of the ambient worker layer used by `House` during tower gameplay. It is intentionally separate from the three-worker main-menu compositor: the gameplay renderer owns **8 worker slots** stored in the original `int[11][8]` table.

## Evidence anchors

The canonical source is `Tower-Bloxx_v1522.jar`, especially these recovered `House.class` methods:

- `House.o()` — loads worker resources 11/12, initializes the fixed-point viewport constants and the two curved-flight lookup tables.
- `House.d(int,int)` — spawns incoming workers after a successful landing.
- `House.i(int)` — scatters workers attached to the just-covered floor and may reuse inactive slots for thrown workers.
- `House.j(int)` — advances the worker state machine.
- `House.j(Graphics)` — draws the 8 gameplay-worker records.
- `House.a(Graphics,int,int,int,int,int)` — selects resource 11/12 and reverses source-frame numbers for the opposite facing direction.

## Storage layout

The Java runtime allocates `b = new int[11][8]`. The recovered row meanings used by the GBA port are:

| Java row | Meaning |
| --- | --- |
| `b[0]` | state / active flag |
| `b[1]` | origin X, fixed point |
| `b[2]` | origin Y, fixed point |
| `b[3]` | source animation frame |
| `b[4]` | state start time |
| `b[5]` | target floor number / state-specific speed value |
| `b[6]` | draw direction |
| `b[7]` | current X, fixed point |
| `b[8]` | current Y, fixed point |
| `b[9]` | target X |
| `b[10]` | sprite variant: resource 11 or 12 |

The GBA representation uses a named `GameplayWorker` struct rather than retaining this opaque row-indexed table, but preserves the recovered behavior.

## Assets and frame selection

- Resource **11** is the blue 190x23 worker strip.
- Resource **12** is the red 190x23 worker strip.
- Gameplay addresses source frames 0..7.
- Variant `1` selects resource 11; variant `0` selects resource 12.
- The original does **not** horizontally flip the sprite. When draw direction is negative it changes the source frame to `7 - frame`.
- `House.i(int)` does not rewrite `b[10]` when it reuses an inactive slot for a thrown worker. The slot's prior blue/red variant therefore persists; a never-used zeroed slot starts red.

## View transform

For the 240x160 GBA-sized viewport the original fixed-point constants resolve to:

- `H = 256 * 240 / 22 = 2792`
- `I = 256 * 160 / 22 = 1861`

The Java renderer places the 19x23 image from its top-left corner as:

```text
screen_x = screen_center_x + ((22 * (worker_x - camera_x)) >> 8) - 9
screen_y = screen_center_y - ((22 * (worker_y - camera_y)) >> 8) - 11
```

Butano sprite positions are center-based, so the GBA compositor uses the same world-to-screen conversion without the final `-9/-11` half-size correction.

## Spawn bands

After a successful normal-floor landing, `House.d(int,int)` chooses the number of incoming workers from absolute horizontal landing error:

| Absolute offset | Workers |
| ---: | ---: |
| `< 25` | 4 |
| `< 50` | 3 |
| `< 80` | 2 |
| otherwise | 1 |

Special/roof phase suppresses this normal worker spawn path (`K == 1` in the original method).

An incoming worker is initialized off the left or right side of the viewport, targets the new top-floor center, starts with a randomized phase offset, and chooses a blue/red resource variant.

## Curved arrival path

`House.o()` installs the lookup tables:

```text
y curve q = [0, 5, 9, 12, 14, 15, 15]
x curve r = [0, 5, 11, 18, 26, 35, 45]
```

State 1 advances in 500 ms curve segments, interpolating X by the x curve over 45 and Y by the y curve over 15 toward the target floor center. Its walking/falling-style frame sequence is derived from the original timing and resolves through frames 1..5 with the source strip's bounce ordering. After roughly 2000 ms, a worker close enough to the target floor transitions to the floor-walk state.

## Recovered states

- **State 0:** inactive slot.
- **State 1:** incoming curved flight toward the newly landed floor.
- **State 2:** walk along the floor toward the +/-64 fixed-point resting offset; source frame toggles between 6 and 7.
- **State 3:** falling/drifting worker after the supporting floor is displaced/covered.
- **State 4:** short thrown/scatter launch. It lasts about 300 ms before becoming state 3.
- **State 5:** short standing state at the floor-side offset; frame 7, then the slot deactivates after about 500 ms.

The update loop is gated at approximately **25 ms**, with caller delta clamped to 0..150 ms.

## Scatter behavior

`House.i(int floor)` derives a scatter budget from the just-covered floor's placement quality, then halves it:

```text
<25 -> 4 >> 1 = 2
<50 -> 3 >> 1 = 1
<80 -> 2 >> 1 = 1
else -> 1 >> 1 = 0
```

Existing workers targeting that floor are first converted to state 3. If budget remains, inactive worker slots are reused in state 4 at the floor position. As noted above, this reuse preserves `b[10]`, the slot's previous blue/red variant.

## Clipping

Falling/scattered workers deactivate when they leave the recovered horizontal/bottom view bounds:

```text
y < camera_y - I/2
x < camera_x - H/2 - 256
x > camera_x + H/2 + 256
```

The bytecode does not apply a corresponding upper-Y cutoff in this path.

## GBA integration

Fix 12 gives Quick Game and Tower Construction independent 8-slot `GameplayWorkerField` instances. They:

- spawn workers from the exact 4/3/2/1 landing-quality bands;
- scatter workers on a covered previous top floor;
- preserve worker simulation state when gameplay is suspended and rebuild only presentation sprites on Continue;
- draw resource 11/12 frames behind tower/crane sprites and ahead of the static background;
- suppress normal worker spawning on the special roof landing path.

The existing resource-31..34 construction foreground/procedural skyline compositor is unchanged in this phase. The missing gameplay element proved by the JAR was the separate 8-worker House layer, not a second background asset system.

## Determinism note

The exact J2ME runtime random seed is not recoverable as a stable cross-platform input. The GBA port therefore uses a deterministic cosmetic PRNG seed for worker side/phase/variant choices while keeping the recovered state transitions, timing, spawn counts, paths and resource selection behavior. This is an intentional clean-room determinism substitution, not a claim of random-sequence identity with one historical phone run.
