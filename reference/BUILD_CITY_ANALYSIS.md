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
