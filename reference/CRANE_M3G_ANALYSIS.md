# Crane / M3G presentation analysis (Fix 13)

Canonical source: `Tower-Bloxx_v1522.jar`, primarily `House.e(Graphics)`, the House slip branch, `f.class`, and `n.class`.

## Mesh identity and draw branches

The gameplay M3G loader binds the following House fields:

- House `e:Lf` -> mesh 9: platform
- House `f:Lf` -> mesh 8: normal long rope + hook
- House `g:Lf` -> mesh 7: special/roof V-strap rig

`House.e(Graphics)` draws mesh 8 only while `K == 0`. It resets the mesh transform, translates it to `(p, q, 0)`, then calls `f.a(r / 540.0f, 0, 0, 1)`. `f.a(float,float,float,float)` multiplies its first argument by 360 degrees before applying `postRotate`, so the actual Z rotation is `r * 2 / 3` degrees. House derives `r` from the crane horizontal position (`p >> 4`). The GBA snapshots already expose the equivalent `crane_angle_degrees` and Fix 13 keeps that recovered transform for mesh 8.

The special branch uses mesh 7 at `(p, q, 0)` without the mesh-8 Z rotation. In the finite GBA Build City construction model the corresponding player-visible state is `roof_phase`, so Fix 13 selects mesh 7 while `roof_phase` is active and restores mesh 8 otherwise.

## Separate special cable

The V-strap mesh is not the entire cable. In the same special branch, House projects world point `(p, q + 528, 0, 1)` through the active M3G camera and draws a black Java2D line to it twice, one pixel apart. The start point is derived from the current camera/world anchor:

```text
startX = v + 22 * (n - x) / 256
startY = w - 22 * (o - y) / 256
```

The original therefore has a two-pixel black cable independent of mesh 7. Fix 13 preserves that separation on GBA with a dedicated 2 px cable-segment sprite chain. The endpoint is aligned to the recovered mesh-7 attachment region. This reproduces the visible source structure, but it is not claimed to be a bit-identical M3G camera projection; final emulator screenshot tuning can still adjust the anchor by a few pixels if the canonical capture proves necessary.

## Bad-placement secondary tumble

The original slip branch makes two independent presentation targets:

```text
Z target: direction-dependent +/-45 degrees
Y target: (1 - 2 * random(2)) * 60 degrees
```

So every bad placement also receives a random `+60` or `-60` degree Y-axis target. House initializes the current Y angle to zero and linearly converges both the Z and Y presentation angles over 500 ms.

Fix 13 adds this second target to both Quick Game and Tower Construction using the existing deterministic Java-style visual PRNG. Because the original consumes this random draw before later impact jitter, adding it intentionally advances the visual RNG stream and changes the pinned replay presentation hashes without changing gameplay results.

## GBA projection treatment

The original passes the recovered Y angle through the M3G object transform. The GBA port renders pre-extracted mesh composites as 2D sprites, so it cannot ask a hardware M3G renderer to rotate the source mesh at runtime. Fix 13 applies the recovered angle as an affine horizontal foreshortening (`abs(cos(Y))`) to each mesh part and its X offset while retaining the existing Z rotation. At the original +/-60-degree target this gives a 0.5 horizontal scale.

This is intentionally documented as a presentation approximation rather than a claim of bit-identical 3D rasterization. The state, target angle, interpolation duration, RNG ordering and scene timing are recovered behavior; only the final 3D-to-2D rasterization is adapted to the GBA sprite renderer.

## Runtime integration

- Quick Game: normal mesh 8 remains the crane rig; bad-slip blocks receive the recovered secondary Y tumble.
- Tower Construction: normal construction uses mesh 8; roof phase switches to unrotated mesh 7 and the separate two-pixel cable; bad-slip blocks receive the same recovered secondary Y tumble.
- Suspend/resume does not alter simulation angles. Presentation sprites/matrices are rebuilt from the snapshot on resume.

## Remaining visual-only verification

The remaining crane work is screenshot tuning rather than missing state-machine behavior: compare a canonical roof-phase capture against the GBA output for the final cable/mesh attachment pixel anchor, and compare a bad-slip capture against the affine GBA foreshortening. If the 2D approximation is visibly insufficient, pre-rendered angle variants can replace it without changing the recovered core state.
