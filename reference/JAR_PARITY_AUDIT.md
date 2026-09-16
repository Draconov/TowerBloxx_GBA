# Tower Bloxx v1.5.22 JAR parity audit

This audit compares the clean-room GBA port with the supplied canonical `Tower-Bloxx_v1522.jar`. It tracks player-visible game features, presentation systems, persistence and original resources. Phone/network/licensing infrastructure is listed separately because it should not be recreated on GBA.

Status meanings: **complete** = the GBA port has the recovered behavior; **partial** = the core exists but original state/UI/presentation is incomplete; **missing** = original player-visible behavior is not yet present; **omit** = deliberately excluded phone/service functionality.

## Current priority queue

1. **Fix 15.2 live-ROM validation** — implementation-complete, hardware validation pending. Verify the four Build City themes/population roll, 5-degree true-M3G tumble stepping, publisher splash/support arrows and mutually-exclusive result music on a compiled GBA build.
2. **Final screenshot/audio polish** — compare the remaining high-risk construction states (slip extremes, roof/fail transitions, crane rigs) and long localized support/dialogue pages against the JAR on real hardware/emulator captures.
3. **Release/resource headroom** — keep the established OBJ palette/VRAM stress checks in the release gate as later visual additions land.

## Implemented or substantially recovered

- Quick Game drop/landing simulation, misses, population/combo core and personal-best persistence.
- Finite Build City tower construction targets 10/20/30/40, misses, roof/trophy result and population handoff.
- Build City 25-tile save data, cardinal-neighbor placement prerequisites, population milestones/unlocks and 46 original tutorial/event flag slots.
- Source building/lot/effect/HUD/crane resource extraction and Butano asset generation for resources 13-35 used by the current runtime.
- Original menu logo/icons, yellow selection treatment and three moving blue/red worker actors.
- Construction foreground resources 31-34 and recovered procedural skyline/background composition.
- **Fix 9:** Settings now exposes localized string 81 `Reset City`; the localized string-98 confirmation dialog defaults to `No`. Confirming clears all 25 city tile records and all 46 city tutorial/event flags while preserving language, sound and Quick Game records.
- **Fix 10:** The temporary New Game submenu is replaced by the original-style root flow: conditional `Continue game`, direct `Build City`/`Quick Game`, `High Scores`, `Settings`, and `Instructions`; Java ME `Exit` is intentionally omitted on GBA. Quick Game and Build City construction can be suspended in RAM and resumed without resetting their simulation state, and starting another tower mode uses the original overwrite warning with `No` as the safe default.
- **Fix 10 Hall of Fame:** `j.class`'s two persistent population tables are ported with exactly three named entries each, `SUMEA / 0` defaults, strict third-place qualification/equal-score stability, last-name persistence, GBA name entry, and localized table/clear/qualification flows. Quick Game submits final population; Build City submits total city population only after a successful committed placement/replacement.
- **Fix 10 persistence:** SRAM save version 4 adds Hall data and losslessly migrates valid v3 saves while preserving language, sound, all three Quick Game personal-best scalars, 25 city tiles and 46 tutorial/event flags. v1/v2 migration remains supported.
- **Fix 11 Build City progression:** all 46 code-proven `citymode` event flags are now dispatched through a dedicated event controller with recovered ID/localization mapping, milestone/city-level/unlock/trophy/parade ordering, one-shot persistence, modal input blocking and interrupted-chain recovery. Existing progressed Fix-10 cities with all-zero event flags are not flooded with obsolete onboarding; Reset City restores the complete fresh-city sequence.
- **Fix 11 construction messages:** House string 55 is persisted independently in existing v4 reserved storage, matching the separate `towermode` record; strings 56/57 are repeatable failure/trophy result modals and block construction handoff until acknowledged. Reset City deliberately preserves the construction-instruction bit.
- **Fix 11 Build City HUD:** the source milestone progress line, pending-vs-replaced population values, 750 ms placement-entry lock/slide, resource-20 3px clips, resource-21 9+7+7+7 clips, resource-22 8px state clips and resource-8 continue arrow are wired into runtime presentation. Valid sectors follow the recovered 800 ms cadence; exact source color interpolation remains a visual-tuning item.
- **Fix 12 gameplay workers:** Quick Game and Tower Construction now reproduce `House`'s separate 8-slot gameplay-worker layer (not the menu's three-worker field), including resource-11/12 blue/red selection, source-frame reversal rather than generic sprite flipping, 4/3/2/1 landing-quality spawn bands, curved arrival/walk/scatter/fall states, the 25 ms update gate, roof-phase spawn suppression, slot-variant preservation on scatter reuse, and suspend/resume continuity. The existing resource-31..34 construction scenery compositor remains unchanged; exact J2ME random-seed identity is intentionally replaced by deterministic cosmetic RNG.
- **Fix 13 crane / M3G presentation:** the normal crane keeps recovered mesh 8 with House's `r / 540 * 360` Z rotation; Build City roof phase switches to unrotated mesh 7 and a separately rendered two-pixel black cable, matching the original split mesh/Java2D branch. Quick Game and Tower Construction now also recover the independent random +/-60-degree bad-placement Y target and its 500 ms interpolation. The GBA sprite renderer represents that Y rotation with affine horizontal foreshortening rather than claiming bit-identical M3G rasterization; final cable/foreshortening screenshot anchor tuning remains visual-only.
- **Fix 14 Build City compositor correction:** valid placement sectors now use `m.class`'s continuous 800 ms triangular RGB pulse with the four exact family endpoint pairs and a shared GBA palette; resource-20 top clips are restored to y=0..22; resource-21 active-placement and neutral-browse source slices are no longer reversed; resource-22 is restored behind the five population digits with its terminal cap; and the top-right pending/replacement comparison boxes are rebuilt from their exact Java2D rectangle, badge and white-digit geometry. GBA RGB555 quantization is the only color-space adaptation.
- **Fix 14.7 crane shared-origin rasterization:** normal mesh 8 is pre-rendered for every reachable `r = craneX >> 4` step (-24..24) using House's exact `r*2/3` M3G Z rotation, eliminating per-OBJ affine pivot tearing. The special mesh-7 cable endpoint now uses the recovered `(p, q + 528)` projection rather than a sprite-center offset guess.
- **Fix 14.9 M3G texture/cable parity:** removed the erroneous extra vertical texture-coordinate flip, regenerating all textured M3G-derived gameplay meshes and all 49 crane poses in the source row orientation. The special mesh-7 Java2D cable now uses one continuous affine two-pixel line from the recovered screen-centred `(0,-165)` source anchor to the exact projected `(p, q + 528)` endpoint, replacing the visibly segmented cable chain.
- **Fix 15.2 Build City presentation:** restores the source `q/r/s` population-cell roll, including resource-22 state 1/2 rolling cells, hidden changing suffix digits and the red decrease tail, plus the four recovered city palette themes selected at the original 0/250/800/2200 progression thresholds.
- **Fix 15.2 true M3G tumble:** replaces the affine `abs(cos(Y))` bad-placement approximation with shared-origin Z+Y M3G renders at 5-degree Y stages. Meshes 10..13 and 20..23 cover all four Z/Y sign combinations; stage zero reuses the ordinary mesh, so 384 stored poses cover the 416 logical states while keeping ROM growth bounded.
- **Fix 15.2 audio parity:** resources 37/38/39 remain infinite menu/tower/city loops; construction result resources 40/41/42 now replace the tower music as finite tracks instead of overlay jingles. The desired scene loop is remembered during a result track and resumes only after that one-shot ends; disabling sound cancels both states.
- **Fix 15.2 support graphics:** resource 1 exports its original up/down/phone-5 frames and supplies the GBA support-page arrows; resource 9 is restored as the centred Digital Chocolate publisher splash on white for about 2 seconds before Title, with A/Start skip. Resource 10 remains on the existing About presentation.
- **Fix 15.2 Build City torture matrix:** host regressions now exercise all 25 cells, all 16 family-to-family replacement pairs, cardinal capability combinations, every population/unlock/trophy/city-level boundary, full-grid/discard behavior, decreasing replacements, remembered Build City Hall identity/reset, and v4 save validity.

- **High-altitude construction backdrop parity:** construction sky and scenery are now independent runtime layers. The sky follows `House.a(Graphics,int,boolean)` across all 17 recovered colors and cycles 9..16 after band 16; the scenery layer preserves the complete 88-entry resource-43 vertical extent (621 scaled pixels), resources 31–34, ground, and the 12 `House.v()` procedural rooftop columns with source-style type-1 red blink timing. Three 256x512 scenery chunks prevent GBA regular-BG wrapping while both Quick Game and Build City construction share the same `ConstructionBackdrop` camera presentation. The previous `construction_bg_b1/b2/b3` assets were removed because their precomposed opaque skyline hid the sky changes and made the files byte-identical. The supplied Nokia 6230i **v1.3.37** JAR resolves the remaining easter-egg gap: its `r0` contains the exact high-altitude artwork in **resources 50-76** (cloud layers, plane, balloon, birds, Moon, satellite, Mars, asteroid belt, Jupiter, rocket, UFO, Saturn, Uranus, Neptune, icy moon and whale), while its `House.class` provides the 28 event types' band ranges, spawn chances, horizontal speeds and instance limits. Resource 46 is the source 4-frame/80 ms combo star and resource 47 is the source 3-frame/100 ms active-block sparkle shown while the block is Attached/Falling. These older-JAR source resources now replace the temporary hand-authored event art; event randomness remains deterministic cosmetic RNG on GBA so gameplay RNG/state is untouched.

## Intentionally omitted phone/service infrastructure

These are original J2ME product/platform features, not Tower Bloxx gameplay, and should remain absent unless a future GBA-native substitute is explicitly requested:

- Vibration and backlight settings.
- SMS sharing / tell-a-friend flow.
- Mobile League/network score submission and login.
- Game Lobby / Get More Games links.
- License purchase, trial/payment/operator flows.

## Evidence anchors

- `reference/localization_en-EN.txt`: original 134 strings, including Build City events 36-60, Reset City 81/98 and Hall-of-Fame strings 121-133.
- `reference/resource_inventory.csv`: original r0 resource dimensions/types, including resources 1, 8, 9 and gameplay/UI resources 13-35.
- `reference/BUILD_CITY_ANALYSIS.md`: recovered city save format, thresholds, placement rules, construction handoff and compositor geometry.
- `reference/BUILD_CITY_EVENTS.csv` / `reference/build_city_events.json`: exhaustive persistent event IDs 0-45, localization indices and recovered trigger mapping.
- `reference/MENU_COMPOSITOR_ANALYSIS.md`: original menu rendering branch and richer root-menu evidence.
- `reference/GAMEPLAY_WORKERS_ANALYSIS.md`: recovered 8-slot House worker storage, resources 11/12, spawn bands, state machine, transform, clipping and deterministic GBA integration.
- `reference/CRANE_M3G_ANALYSIS.md`: recovered mesh-7/8 branch, special cable formula, +/-60-degree secondary tumble, 500 ms interpolation and the documented GBA projection adaptation.
- `House.class`: `House.<clinit>` exposes two Hall-of-Fame table descriptors and the gameplay/menu scene state used by the port analysis.
- `j.class`: RMS `HoF` persistence and three named entries serialized for each score table.
