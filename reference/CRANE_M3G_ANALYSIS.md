# Crane / M3G presentation analysis (Fix 13, corrected in Fix 14.6)

Canonical source: `Tower-Bloxx_v1522.jar`, primarily `House.e(Graphics)`, `House.u()`, the House landing/slip branches, `f.class`, and `n.class`.

## Mesh identity and first-floor state

The gameplay M3G loader binds the following House fields:

- House `g:Lf` -> mesh 7: V-shaped strap rig
- House `f:Lf` -> mesh 8: long rope + hook
- House `e:Lf` -> mesh 9: platform
- House `a:Lf` -> meshes 10..13: normal floor, one per building family
- House `d:Lf` -> meshes 20..23: initial/base floor, one per building family
- House `b:Lf` -> meshes 30..33: normal roof
- House `c:Lf` -> meshes 40..43: trophy roof

`House.u()` begins construction with `K == 4`.  While the first block is raising or attached (`c[0] != 2`), `House.e(Graphics)` draws mesh 7 and the separate two-pixel cable.  Releasing the first block sets `c[0] == 2` while `K` remains 4, so **no crane mesh is drawn during that first fall**.  On the first successful landing the House logic changes `K` from 4 to 0, after which normal construction uses mesh 8.

The first block itself is also special.  While `K == 4`, House marks it with the `999` presentation marker and selects field `d`, so the first hanging and first settled floor use meshes 20..23.  Later non-roof floors use meshes 10..13.  Quick Game uses family `L == 4`, therefore its first/base floor is mesh 23 and later floors are mesh 13.

## Normal mesh-8 branch

When `K == 0` and the block is not in miss state 3, `House.e(Graphics)` draws mesh 8. It resets the mesh transform, translates to `(p, q, 0)`, then calls `f.a(r / 540.0f, 0, 0, 1)`. `f.a(float,float,float,float)` multiplies its first argument by 360 degrees before applying `postRotate`, so the actual Z rotation is `r * 2 / 3` degrees. The GBA snapshots expose the equivalent `crane_angle_degrees` and the port keeps that recovered transform.

## Special mesh-7 branch

Mesh 7 is selected when any of the following recovered conditions is true:

- initial first-floor rig: `K == 4 && c[0] != 2`;
- miss presentation: `c[0] == 3`;
- Build City roof phase: `K == 1`.

Mesh 7 is translated to the crane position without the mesh-8 Z rotation.  This is state-driven behavior; resource 30 and the crane types are **not randomly selected**.

## Separate special cable

The V-strap mesh is not the entire cable. In every special branch above, House projects world point `(p, q + 528, 0, 1)` through the active M3G camera and draws a black Java2D line to it twice, one pixel apart. The GBA port preserves that separation with a dedicated two-pixel cable-segment sprite chain.

The endpoint is aligned to the recovered mesh-7 attachment region. This reproduces the visible source structure, but it is not claimed to be a bit-identical M3G camera projection; final emulator screenshot tuning can still adjust the anchor by a few pixels if a canonical capture proves necessary.

## Bad-placement secondary tumble

The original slip branch makes two independent presentation targets:

```text
Z target: direction-dependent +/-45 degrees
Y target: (1 - 2 * random(2)) * 60 degrees
```

So every bad placement also receives a random `+60` or `-60` degree Y-axis target. House initializes the current Y angle to zero and linearly converges both the Z and Y presentation angles over 500 ms.

The port applies this second target in both Quick Game and Tower Construction using the existing deterministic Java-style visual PRNG. Because the original consumes this random draw before later impact jitter, it intentionally advances the visual RNG stream without changing gameplay results.

## GBA projection treatment

The original passes the recovered Y angle through the M3G object transform. The GBA port renders pre-extracted mesh composites as 2D sprites, so it applies the recovered angle as an affine horizontal foreshortening (`abs(cos(Y))`) to each mesh part and its X offset while retaining Z rotation. At the original +/-60-degree target this gives a 0.5 horizontal scale.

This is a presentation approximation rather than a claim of bit-identical 3D rasterization. The state, target angle, interpolation duration, RNG ordering and scene timing are recovered behavior; only the final 3D-to-2D rasterization is adapted to the GBA sprite renderer.

## Runtime integration after Fix 14.6

- Quick Game: first block/base uses mesh 23; initial rig uses mesh 7 + cable; first release hides the crane; after first landing, normal construction uses mesh 8 and later floor mesh 13; miss state switches back to mesh 7 + cable.
- Build City construction: first block/base uses mesh 20..23 for the selected family; the same initial mesh-7 / hidden-first-fall / mesh-8 sequence applies; roof phase uses mesh 7 + cable; miss state also uses mesh 7 + cable.
- Suspend/resume does not alter simulation angles or crane state. Presentation objects are rebuilt from the snapshot.

## Remaining visual-only verification

With the state-machine mismatch removed, the remaining crane check is the normal mesh-8 rasterization itself. Compare a post-first-floor normal-hook capture against the GBA output. If affine rotation still produces visibly rough texture/edge artifacts, pre-rendered M3G-derived angle frames can replace the runtime affine approximation without changing the recovered state machine.
