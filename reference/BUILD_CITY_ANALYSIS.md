# Tower Bloxx v1.5.22 Build City bytecode contract

This document records Build City behavior recovered from `m.class` and the Build City branches of `House.class` in the supplied Nokia/J2ME v1.5.22 JAR. It is a reference contract for the native GBA port, not a redesign.

## Persistent city record

The original RMS store is named `citymode`. `m.i()` loads and `m.j()` writes exactly 25 tile records followed by 46 booleans. Each tile is serialized as an unsigned byte building type, a 32-bit signed population integer, and a byte roof/special value. The canonical payload is therefore `25 * (1 + 4 + 1) + 46 = 196` bytes.

Building type zero means empty. Types 1–4 are Residential, Commercial, Office and Luxury. Replacing an occupied tile is legal; the old tile population is removed before the new tower population is added.

## Milestones and unlocks

The exact population thresholds are:

`0, 75, 150, 250, 400, 600, 800, 1000, 1400, 1800, 2200, 3000, 4000, 5000, 6500, 8000, 9500, 11500, 14000, 17000, 19000`.

Building unlock milestone indices are `0, 3, 6, 10`, giving Residential at 0 population, Commercial at 250, Office at 800 and Luxury at 2200.

The roof/trophy eligibility milestone indices are `8, 12, 14, 16`, corresponding to populations 1400, 4000, 6500 and 9500 for the four building families.

City-level milestone indices are `0, 1, 4, 7, 9, 11, 13, 15, 18, 20`. Localized labels map level 1 through 9 to Tiny Town, Small Town, Town, Small City, Medium City, Big City, Capital, Metropolis and Megalopolis. The level-zero placeholder remains `-` before population 75.

## Placement prerequisites

`m.g()` inspects only the four cardinal neighbors. It creates a capability rank from neighboring types 1, 2 and 3:

- rank 0: no Residential/type-1 neighbor;
- rank 1: at least a type-1 neighbor;
- rank 2: type-1 and type-2 neighbors;
- rank 3: type-1, type-2 and type-3 neighbors.

Placement accepts selected building type `1..4` when `capability >= type - 1`. Therefore Residential can go anywhere, Commercial needs Residential, Office needs Residential + Commercial, and Luxury needs Residential + Commercial + Office. Diagonals do not count.

The normal city cursor is a 5×5 grid and starts at column 2, row 2. Moving left from column 0 enters the separate discard/demolition selector (`column = -1`, row 4); moving right from it returns to column 0. During the recovered 3000 ms placement completion, `m` writes a city tile only when the column is non-negative. Choosing column `-1` therefore discards the newly constructed tower; it does **not** erase an existing saved city tile.

## Construction handoff

`m.a()` returns the selected tower family as 1–4, or 5–8 when that family is trophy-roof eligible. `House` subtracts four from values above four and remembers trophy eligibility. The construction target table in `House` is exactly `10, 20, 30, 40` floors for Residential, Commercial, Office and Luxury. On construction completion/failure `House` calls `m.a(type, population, roof_result)`, where roof result is 0 for failed target/no roof, 1 for normal roof and 2 for the trophy roof branch.


## City graphics contract

`m.a()` loads resources 24–27 into its four building-family images. Each image is exactly four equal-width frames. During normal city rendering the tile's stored building type selects resources 24–27 and the stored roof/special byte is used directly as the horizontal frame index (`imageWidth / 4`). Resource 28 is a five-frame red outline/placement strip and is drawn by the placement/highlight paths, not as empty-lot ground art.

The GBA UI exporter therefore emits 16 individual building-frame composites (`city_building_1_f0` through `city_building_4_f3`) plus five resource-28 outline composites. This preserves the original frame selection instead of replacing the city with generic colored blocks.

## Phase-8 replay

The deterministic host replay reaches population 2200/milestone 10/city level 4, observes construction targets 10, 20, 30 and 40 at the original unlock boundaries, and confirms the column `-1` discard path leaves all 25 persisted tiles unchanged. Its trace SHA-256 is `691a92771e2c6a4c0e08c477dfa7b0dc93555328f8e31e445ccec0b961b2b055`.

## Phase 9: finite construction and roof result contract

The Build City tower run is a distinct finite mode (`B == 3`) and is not Quick Game with a target bolted on. Building family `L` is 1..4 and selects target heights 10/20/30/40. A target-height `N` tower contains `N-1` ordinary floors; when `Q == J - 1` the game enters roof state (`K == 1`) and the next successful landing is the roof as floor `N`.

Construction starts with three misses, `R=0` tower population, `O=0` roof result, and the same 128/64 initial crane amplitudes. Initial swing period is `m[L]`, giving 1700/1650/1600/1550 ms for the four families. After ordinary placements, `House.l(int)` applies the construction-only progression:

- `period = m[L] - Q * (m[L] - m[L+2]) / J`
- `swingX = min(k[L], k[1] + Q * (k[L] - k[1]) / (J >> 1))`
- `swingY = min(l[L], l[1] + Q * (l[L] - l[1]) / (J >> 1))`
- `verticalBias = -min(128, Q * 256 / 100)`

with `k=[213,256,298,341,384]`, `l=[85,106,128,149,170]`, and `m=[1670,1700,1650,1600,1550,1500,1450]`, using Java integer truncation.

The city-level trophy flag is provisional. Immediately before roof state, the constructed tower must also have at least 70/250/550/1000 population for Residential/Commercial/Office/Luxury; otherwise trophy eligibility is revoked. A successful roof does not receive the ordinary 1/2/3/4 accuracy population. Instead, with `e = abs(offset)`:

- normal roof (`O=1`): `(128-e) / (5-L)`
- trophy roof (`O=2`): `(128-e) * L / 2`

A roof miss leaves `O=0` and consumes one of the same three construction chances. If chances remain, the roof is retried. Exhausting all chances anywhere in construction enters terminal state `K=2`; a successful roof does the same after reaching `Q==J`. After 2000 ms, the game calls the city handoff with `(L, R, O)`.

M3G mesh selection is also code-proven: user IDs 10..13 are the ordinary family floors, 30..33 are the normal roofs, and 40..43 are the trophy roofs. IDs 20..23 are selected by a separate `999` render marker and are not assigned an invented role in the Phase-9 port.

## Fix 7: recovered Build City compositor geometry

The old GBA scene centered city sprites on an invented tan/green 5x5 panel and overlaid large `Build City`, `Population`, and town-name text. Re-reading `m.a(Graphics, boolean)` proves that those elements do not belong in the playfield. On a 240x160 GBA viewport the recovered Java2D layout specializes to:

- board outer rectangle: `(89,32)` size `88x88`;
- 5x5 lot origin: `(92,35)`, 17-pixel pitch;
- browse selector: `(63,34)` size `19x67`, with 15x15 slots at y `36 + 16*n`;
- stored building source left: `94 + 17*column`;
- stored building baseline: `47 + 17*row`;
- bottom context panel starts at y=137;
- population icon is clipped at `(3,1)` and the five brown digits are right-aligned from x=58 with 8-pixel spacing;
- the two browse-mode top-right value boxes begin at x=189 and x=213.

The native scene now consumes resources 21, 24-29 and the original brown digit strip at those recovered anchors. Resources 24-27 use their stored roof byte as the frame selector. Resource 28 remains the red placement-outline family, resource 29 is the placement effect animation, and resource 23 is used for the separate discard selector. Locked-tower help is rendered only in the original 23-pixel bottom message panel; the population threshold comes from the BuildCity core rather than being duplicated in renderer constants.

## Fix 14: exact valid-lot pulse and top HUD compositor

A second pass over `m.a(Graphics, boolean)` closes three earlier presentation assumptions.

The placement timer `x` advances modulo 800 ms. For selected family `t=1..4`, the source uses RGB endpoint pairs from `m.f`:

- Residential: `#0054A0 -> #3CC8FF`
- Commercial: `#A00200 -> #FF6946`
- Office: `#009800 -> #37FF37`
- Luxury: `#935A00 -> #F0FF00`

The interpolation phase is triangular: `0 -> 400 -> 1` over timer values `0..799`. Each channel uses Java integer arithmetic `start + (end-start)*phase/400`. On every valid placement sector, both the 14x14 outer lot and its 12x12 inner region receive that color, while the 10x10 center keeps the normal lot color. The GBA renderer therefore uses a transparent-center 14x14 two-pixel ring with one shared mutable sprite palette. RGB888 is reduced to native GBA 5-bit channels only at the final palette write; the recovered timing/math remains exact.

Resources 20-22 also have stricter call-site meanings than the earlier Fix-11 approximation:

- resource 20 top edge clips occupy y `0..22`, so their centers are `(1,11)` and `(238,11)`; the lower clips remain centered at y=148;
- resource 21 source x `9..15` is the orange/gold active-placement glyph, while source x `16..22` is the neutral gray browse/transition glyph;
- resource 22 is the six-cell population backing at target x `19 + 8*n`, y=0. In the static state used by the current GBA compositor, cells 0-4 use source state 0 and cell 5 uses state 3. It is not the top-right comparison panel.

The two top-right comparison boxes are Java2D rectangles. Their outer bounds are `(189,1,24,9)` and `(213,1,24,9)`. Active outer/inner colors are `#E2E2E2` and `#0D0C0C`; the city background already contains the inactive `#C7BFB2/#AD9C83` boxes. Active placement also draws a 4x7 family badge whose first six rows use `#4371D7/#E11A08/#11AF0C/#DA9F00` and whose last row uses `#0B2D8E/#841111/#045C00/#503800`. White comparison digits use five-pixel glyphs at four-pixel spacing with right anchors x=211 and x=235.

Two visual details are deliberately left as follow-up rather than guessed here. The JAR has a `q/r/s` population digit-roll state which selects resource-22 states 1/2 during counter animation; Fix 14 uses the proven static state 0 + state-3 cap. The static initializer also exposes four city lot palette themes (`m.d/m.e`) selected as city progress advances; the current 240x160 background still bakes the level-zero lot palette. Those require their own state/timing pass.
