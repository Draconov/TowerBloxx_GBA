# Tower Bloxx GBA runtime

This directory is the native GBA half of the faithful Tower Bloxx port. The runtime targets **Butano 21.7.1** on **devkitARM**.

## External toolchain

1. Install devkitPro's GBA Development package / `gba-dev` group so `DEVKITARM` and the ARM compiler are available.
2. Download Butano 21.7.1.
3. Point `LIBBUTANO` at the Butano library directory (the directory that contains `butano.mak`). The Makefile default is `../../butano/butano`, suitable when the Butano repository is cloned beside the project root expected by this checkout.
4. From this `gba` directory, run `make -j4` (or another `make -jN` value matching your CPU).

The expected output is `TowerBloxxGBA.gba`.

## Phase-4 controls

The runtime now boots into the faithful title/menu shell instead of the Phase-3 asset gallery.

- A / Start on Title: open the main menu.
- D-pad Up / Down: move through menu entries with wraparound.
- A: activate the selected entry.
- B: go back one menu level.
- Settings: Sound and Language only; the Nokia-only vibration/backlight options are intentionally omitted.
- Instructions / About: Left / Right changes text pages.
- Quick Game now launches the Phase-5 native stacking scene. Build City remains a distinct pending request for its later implementation phase.

The shell uses the original Tower Bloxx logo (resource 7), original Sumea logo (resource 10), reconstructed bitmap font (resource 36 + resource 44), and all five original locale packs. Phone keypad wording is adapted only for playable controls (`5` -> `A`, `4/6/2/8` -> D-pad). Network, SMS, licensing, phone backlight, and vibration flows are not ported.

## Generated graphics

`graphics/generated/`, `include/generated/tower_mesh_assets.h`, and `reference/generated_assets_manifest.json` are generated deterministically from the canonical JAR by `tools/tower_bloxx_extract/gba_project_export.py`. Phase-4 UI assets under `graphics/ui/`, `include/generated/tower_font.h`, `tower_localization.h`, `tower_ui_assets.h`, and `reference/ui_assets_manifest.json` come from `tools/tower_bloxx_extract/gba_ui_export.py`. The original JAR is not committed.

## Phase-4 verification snapshot

The Phase-3 mesh asset set remains unchanged: 19 reconstructed meshes split into 71 legal OBJ sprite parts and 143 generated asset files, with deterministic tree hash:

`5ac0e22a6229ff047340e7fdca85b7a55f262fb0515b3fa3c20100bd2261dd7c`

The Phase-4 UI exporter adds a lossless 4bpp 8x16 font sheet, two Tower Bloxx logo OBJ parts, one Sumea logo OBJ part, generated font/localization/UI headers, and the UI manifest. Its current deterministic UI tree hash is:

`d91ee68a90c455678209dad2eda28fdeff413e344994bd9e5df0542d2e59bbf6`

The localization header contains all 5 x 134 original strings plus pre-wrapped instruction/about lines for the 240x160 shell. Host/reference tests compile the navigation/save core with `g++ -std=c++20 -Wall -Wextra -Werror -pedantic`.

The packaging environment used for this checkpoint does **not** contain `arm-none-eabi-g++`, devkitARM, grit, or a local Butano checkout, so an ARM `.gba` build cannot be honestly verified here. The Makefile and generated asset inputs are present for a normal Butano 21.7.1 + devkitARM build as soon as that external toolchain is available.

## Phase-6 Quick Game controls and scope

The `Quick Game` menu entry runs the native endless stacking mode recovered from the Nokia bytecode.

- A: drop the current block when attached and the camera is ready.
- B: leave an active run; during the third-miss delay input is locked, then A/B leaves the result screen.
- Quick Game uses original mode `L=4` and starts at a 1550 ms swing period.
- It does **not** finish at 10 or 40 floors. The run ends only after all three construction chances are consumed.
- The HUD shows tower height, remaining attempts, population, and the active combo meter/count.

Population uses the recovered OK/Good/Great/Perfect values 1/2/3/4 plus the original height-decade bonus. Perfect placements start/refill the 6000 ms combo timer; active-combo placements accumulate the exact deferred Nokia bonus, with the original faster drain for longer combos. A miss settles an active combo before consuming a construction chance.

After the third miss the runtime waits the recovered 2000 ms, then shows the original localized result fields: **Population**, **Tower height**, and **Longest combo**. The three records persist independently in SRAM using strict-greater comparisons and localized `New record!` markers. Save format v2 migrates the Phase-5 v1 language/sound settings and old Quick high score into best population. Obsolete Mobile League/network submission is omitted.

The Butano scene uses the recovered M3G-derived GBA OBJ composites for the crane and floor. Phase 7 restores the original **five-floor** tower presentation window, Java-integer sway/rocking, the 0..800 ms top-floor settle wobble, the recovered ±45-degree Z tumble for slipping blocks, and the 800 ms miss camera-impact shake. Presentation transforms are kept separate from collision coordinates, so visual lean never changes a placement result.

Each four-OBJ floor composite shares one Butano affine matrix and rotates its part centers around the recovered mesh origin. The original M3G slip path also used a secondary random ±60-degree Y-axis rotation; that perspective-changing axis cannot be represented faithfully by the current single-view 2D floor assets, so it remains explicitly deferred until alternate M3G viewpoints are generated rather than approximated with fake 2D squash.

The GBA's fixed 60 Hz loop feeds the Java-style millisecond simulation with deterministic `16,17,17` ms cadence. The simulation retains the original 25 ms accumulator gate and 150 ms frame-delta clamp.

### Phase-7 deterministic replay

A host replay using public gameplay input stacks ten floors, then naturally misses three drops and reaches the delayed Results state. Its gameplay result remains population 118, height 10, longest combo 8. The serialized trace now also includes presentation camera, sway, floor-pose and tumble fields; its SHA-256 is:

`7ba74aa0820973f49e12eebb87308d976f4b529790b7d38913a78d0ccb6f795b`

The checkpoint environment still lacks `arm-none-eabi-g++`, `grit`, and a local Butano checkout, so this phase remains source/host verified rather than claiming an unbuilt `.gba`.

## Phase-8 Build City management and persistence

The `Build City` menu entry now opens the native 5x5 city-management scene backed by the bytecode-recovered `BuildCity` core. Save format v3 preserves all 25 city tile records plus the original 46 tutorial flags while retaining Phase-6 Quick Game records and migrating valid v2/v1 saves.

The core uses the exact 21 population milestones, the original building unlocks (Residential 0, Commercial 250, Office 800, Luxury 2200), the nine named city levels through Megalopolis at 19000, and the four-cardinal-neighbor placement rules. Construction requests expose the recovered 10/20/30/40 floor targets and trophy eligibility thresholds; completed construction results enter the original placement/replacement flow through the public `accept_constructed_tower` handoff.

The city renderer uses the original resource 24–27 four-frame building strips, with the saved roof value selecting the frame exactly as `m.class` does. Resource 28 is used only as the placement/validity outline; empty sectors are not filled with it. Population, city level, building names and placement messages use the original localized Nokia strings and reconstructed bitmap font.

This checkpoint intentionally stops at the **construction handoff boundary**. Selecting an unlocked building produces the exact construction request, but the 10/20/30/40-floor Build City construction run and trophy-roof scoring are not fabricated from Quick Game rules; that is the next recovery slice.

### Phase-8 deterministic city replay

The host replay starts from an empty city, places and replaces towers using the public placement API, crosses the 75/250/400/800/2200 progression points, unlocks 10/20/30/40-floor construction requests, and exercises the original column `-1` discard selector without mutating a saved city tile. Trace SHA-256:

`691a92771e2c6a4c0e08c477dfa7b0dc93555328f8e31e445ccec0b961b2b055`
