# Tower Bloxx v1.5.22 — Quick Game bytecode contract

This document freezes the Java ME behavior used by the native GBA Quick Game simulation. Phase 6 corrects the earlier Phase-5 assumption that the 10/20/30/40 construction table was a Quick Game completion target. It is based on the canonical supplied JAR (SHA-256 `7aebc77593a68e8bf58ce6e633c2e3bc4fae0d873eb909a64628bb293f920cf0`) and the recovered `House` bytecode. Names below use the obfuscated Java method names only as evidence anchors; the GBA code uses descriptive names.

## Frame/update gate

`House.a(int delta, int rawDelta)` clamps the simulation delta to **150 ms**. Gameplay accumulates time and only runs the physics/update group once the accumulator reaches **25 ms**, passing the accumulated interval to the simulation and then clearing it. Phase 5 preserves that behavior instead of assuming a fixed 60 Hz physics step.

The gameplay update group advances the game clock and then calls the camera, crane, falling-block, environment/state, stable-stack, and effect routines. Random cosmetic effects are not allowed to feed back into the deterministic Phase-5 gameplay core.

## Input and drop gate

The Java key mapper treats Nokia key `5` and game action `8` as the primary gameplay action. The input latch ultimately sets the primary boolean used by `House.t()`.

When the primary action is fresh, the game is in normal gameplay, the current block is attached (`c[0] == 1`), and the camera has reached its target (`y == z`), `House.t()` starts the drop:

- current block state becomes `2` (falling),
- drop-start Y stores the current crane Y (`j[0] = q`),
- drop-local time/offset fields reset,
- drop start time stores the current gameplay clock.

The GBA maps this action to a **fresh A press**.

## Quick Game initialization (`House.u()`)

Recovered initial values used by Phase 5:

- gameplay clock: `0`
- floor count `Q`: `0`
- camera current/target: `y = z = 512`
- world/crane anchor `o`: `2432`
- current rope extension `m`: `0`
- current block state: `6` (raising/initial extension)
- current block world Y and crane Y baseline: `2432`
- construction chances `E`: `3`
- swing phase `u`: `2000`
- horizontal swing amplitude `aF`: `128`
- vertical swing amplitude `aG`: `64`
- vertical swing bias `aD`: `0`
- maximum normal rope extension: `1664`

The recovered construction target-height table is `[10, 20, 30, 40]`, but bytecode control flow proves it belongs to Build City/construction progression. Quick Game takes the non-Build-City branch, forces `L = 4`, starts with `aE = period[4] = 1550`, and has no height-completion transition.

## Integer trigonometry (`House.w()`, `House.b(int)`, `House.c(int)`)

The game builds a 360-entry integer sine table at runtime. It begins with `sin=0`, `cos=32768` and uses:

`step = (32768 * 31416 * 2) / 3600000 = 571`

For each entry it stores the current sine, updates cosine first, and then updates sine from the **new** cosine. Java uses signed right shift by 15.

`House.b(angle)` directly indexes that table, negating the positive lookup for a negative input. `House.c(angle)` is not a conventional named cosine helper: it returns `House.b(angle - 90)`. Therefore `c(0)` is approximately `-1.0` in Q15 (`-32771`). The native core preserves this exact sign convention.

## Crane motion (`House.e(int dt)`)

The swing phase advances by `dt`. While the initial block state is `6`, rope extension increases by `2*dt/3` until **1664** in normal gameplay, then becomes attached state `1`.

For each update:

- horizontal crane displacement: `p = aF * c((200*u/aE) % 360) >> 15`
- crane display/rotation term: `r = p >> 4`
- vertical swing component: `-(aG * b((200*u/aE) % 360) >> 15)`
- crane/block Y: `q = o - aD - m + vertical_component`

While attached and outside special scripted states, horizontal and vertical velocities are derived from the change in crane position and scaled by `256/dt`. Those velocities are inherited by a released block.

Recovered tuning arrays:

- horizontal amplitude table `k`: `[213, 256, 298, 341, 384]`
- vertical amplitude table `l`: `[85, 106, 128, 149, 170]`
- period table `m`: `[1670, 1700, 1650, 1600, 1550, 1500, 1450]`

## Falling physics (`House.h(int dt)`)

For falling/slipping states, elapsed time is measured from the drop/transition start. In ordinary Quick Game the falling Y equation is:

`y = start_y + initial_vy * t / 256 - t*t / 200`

The current X advances with inherited horizontal velocity:

`x += vx * dt / 512`

The Java routine also interpolates visual Z/Y rotations and contains special-script gravity (`/400`) paths. Those script-only paths are outside the normal Quick Game Phase-5 contract.

## Collision search (`House.a()`) and landing (`House.b()`)

The collision scan walks the recent stable floors from top toward bottom. A candidate overlaps when the falling block center is strictly inside both of these windows around the floor center:

- vertical: `floor_y - 256 < block_y < floor_y + 256`
- horizontal: `floor_x - 256 < block_x < floor_x + 256`

For a top landing, horizontal offset is `block_x - floor_x`.

- `abs(offset) > 127`: edge failure/slip/miss path.
- `abs(offset) <= 127`: successful placement.

Successful-placement accuracy bands are recovered as:

- `<25`: band 4 (Phase-5 name `Perfect`)
- `<50`: band 3 (`Great`)
- `<80`: band 2 (`Good`)
- otherwise through 127: band 1 (`Ok`)

The first floor uses a virtual ground collision path and establishes the initial horizontal tower center. Stable floors are separated by **256 fixed units** vertically. Placement offsets are retained because later sway/scoring code consumes their history.

A slipping block is kicked horizontally at magnitude `500` in the offset direction and receives the recovered tilt/bounce setup; Phase 5 needs the deterministic miss outcome/chance loss, while the exact cosmetic tumble pose can be refined with the later gameplay pose-atlas pass.

## Construction chances and next block

Quick Game starts with **3** construction chances. The miss handler subtracts the requested amount; reaching zero enters the game-over state. A settled/failed block waits approximately **400 ms** before the next attached block is made available when the run can continue.

## Floor progression (`House.l(int)`) in Quick Game

Quick Game takes the `B != 3` branch with fixed `L = 4`. After successful floor count `Q` changes:

- `amp_x = min(k[4], k[0] + Q * (k[4] - k[0]) / 60)`
- `amp_y = min(l[4], l[0] + Q * (l[4] - l[0]) / 60)`
- for `Q < 100`: `aD = -min(128, Q*256/200)` and `aE = max(m[6], m[0] - Q*(m[0]-m[6])/100)`
- for `Q >= 100`: `aE = m[5] - (Q-100)*100/150` and `aD = -min(256, 128 + (Q-100)*256/300)`

All division uses Java integer truncation. The discontinuity at `Q == 100` is present in the original bytecode and is preserved. Quick Game continues until all three construction chances are consumed.

## Population and combo accounting (`House.b()`, `House.k(int)`, `House.r()`)

A successful placement first updates combo state. The first placement starts combo count `X` at 1. While combo timer `Y > 0`, another successful placement increments `X`; `aa = max(aa, X)` records longest combo whenever `X > 1`. A Perfect placement sets/refills `Y = 6000`.

Accuracy maps directly to population points: OK=1, Good=2, Great=3, Perfect=4. For a positive award, `House.k(int)` immediately adds `Q/10 + points` to population `R`. If `Y > 0`, it also defers `X * (2 + 2*(Q/10))` into combo accumulator `Z`.

Each update drains an active combo by `dt + (X-1)*dt/6`. When the timer expires, or when a miss occurs while the combo is active, `House.r()` adds `Z` to `R`, resets `X=0` and `Y=0`, but deliberately leaves `Z` untouched until the next new combo resets it.

## End-of-run and Quick Game results

The third miss reduces construction chances `E` to zero and enters game-over state `K=2`. After **2000 ms** of gameplay-clock delay, Quick Game opens its result flow. It reports exactly three statistics using localization entries 93-95: population `R`, tower height `Q`, and longest combo `aa`.

Each is compared independently with strict `>` against record slots `v[3]`, `v[4]`, and `v[5]`; a new best appends localization entry 96 (`New record!`). The old Mobile League/network submission path is intentionally omitted on GBA.

## Camera (`House.d(int)`, `House.p()`)

A floor-count change starts a camera transition by storing the current gameplay clock. With more than one floor the target moves upward by the requested fixed amount; the opening case establishes the canonical 512 baseline. The current camera approaches the target over **500 ms**. The Java code also applies a short random camera shake after particular events; Phase 5 deliberately omits that cosmetic randomness so replay state stays deterministic. It does not affect placement/collision rules.

## Still deferred beyond Phase 6

- dynamic five-floor tower sway/rocking presentation,
- exact 3D tumble/tilt pose rendering for slips,
- random cosmetic camera shake,
- Build City-specific construction/progression logic.

Phase 6 now includes Quick Game population, combo timing/settlement, three persistent records, the 2000 ms terminal delay, and the localized result flow.
