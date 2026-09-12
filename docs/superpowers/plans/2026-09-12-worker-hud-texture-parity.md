# Worker + HUD Texture Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Replace the fake fixed-position menu worker reel and placeholder construction HUDs with ROM-derived behavior/assets proven by the Tower Bloxx v1.5.22 JAR.

**Architecture:** Keep gameplay rules untouched. Add a small host-testable `MenuWorkerField` state machine that mirrors `House.c(int,int)` and let `UiShell` render its workers using the two original strips. Expand the UI exporter to emit the JAR HUD families that `House` and `m` load, then consume the proven Quick Game and Tower Construction HUD pieces without guessing undocumented Build City layout.

**Tech Stack:** C++17 portable gameplay/runtime code, Butano GBA sprite/background APIs, Python 3.13 extraction tooling, Pillow, pytest.

**Spec:** `reference/HUD_TEXTURE_ANALYSIS.md` (created in Task 1 from recovered `House.javap` / `m.javap` evidence)

## Global Constraints

- Canonical source is `/mnt/data/Tower-Bloxx_v1522.jar` plus recovered bytecode and user captures.
- Do not change gameplay physics or scoring in this pass.
- Do not invent unknown resource meanings/positions; document uncertain assets and export them for the later compositor pass.
- Preserve deterministic host tests for cosmetic random motion.
- No version bump unless explicitly requested.
- Keep GitHub ROM workflow unchanged in this pass.
- Delivery must include a FULL source ZIP and an update-only ZIP over Fix 5.

---

### Task 1: Lock reference evidence and exporter expectations

**Files:**
- Create: `reference/HUD_TEXTURE_ANALYSIS.md`
- Modify: `tests/test_gba_ui_export.py`
- Modify: `tests/test_butano_project.py`

**Interfaces:**
- Consumes: JAR resources 11–35 and recovered `House`/`m` rendering behavior.
- Produces: test-enforced asset names used by later C++ tasks.

- [x] **Step 1: Write failing exporter assertions**

Require both worker strips and proven HUD families: resource 13 target badges, 14 white digits, 15 brown digits, 16 red digits, 17 status graphic, 18 state indicators, 19 Quick Game counter, 20–23 Build City UI pieces, 29 effects, 30 hook frames, 35 accuracy stars.

- [x] **Step 2: Run focused tests and verify RED**

Run: `PYTHONPATH=.:tools pytest -q tests/test_gba_ui_export.py tests/test_butano_project.py`
Expected: failures because Fix 5 exports only worker 11 and a subset of City assets.

- [x] **Step 3: Write the bytecode-backed reference note**

Record exact dimensions/frame counts and the worker update/render equations, including the 3-worker array, 25ms update gate, 150ms clamp, 300ms animation cadence, frame sequence, and respawn ranges.

- [x] **Step 4: Do not implement production code yet**

Keep the suite red until Task 2/3 production changes land.

### Task 2: Restore original descending menu-worker behavior

**Files:**
- Create: `gba/include/tb/menu_workers.h`
- Create: `gba/src/menu_workers.cpp`
- Create: `tests/cpp/menu_workers_test.cpp`
- Modify: `tests/test_runtime_core.py`
- Modify: `gba/include/tb/ui_shell.h`
- Modify: `gba/src/ui_shell.cpp`

**Interfaces:**
- Produces: `tb::MenuWorkerField`, `MenuWorkerField::update(int)`, `MenuWorkerField::worker(int)`, `MenuWorkerField::display_frame(int)`.
- `UiShell` consumes three worker records and renders source-strip blue/red frames at Java-equivalent screen coordinates.

- [x] **Step 1: Add a failing host C++ replay/behavior test**

Assert worker count 3, initial respawn after update, downward positive `vy`, frame map `{1,2,3,4,5,4,3,2}`, and eventual Y increase rather than fixed-position cycling.

- [x] **Step 2: Run the worker test and verify RED**

Expected: compile failure because `tb/menu_workers.h` does not exist.

- [x] **Step 3: Implement minimal `MenuWorkerField`**

Mirror the recovered Java fixed-point state and deterministic Java-style 48-bit visual RNG. Clamp update delta to 150ms, accumulate until >=25ms, advance animation every 300ms, update x/y with `velocity * dt >> 6`, and respawn at `y=-267-random(I)` with `vy=10+random(10)`.

- [x] **Step 4: Run host test and verify GREEN**

- [x] **Step 5: Replace `_menu_worker_frame/_tick` in `UiShell`**

Advance the field at 16/17/17ms per GBA frame. Render visible workers using resource-11 blue/resource-12 red frame composites and the recovered 1–5–2 sequence. Remove the `% 10` fixed-location reel.

- [x] **Step 6: Run focused runtime/project tests**

### Task 3: Export the missing source HUD texture families

**Files:**
- Modify: `tools/tower_bloxx_extract/gba_ui_export.py`
- Regenerate: `gba/graphics/ui/*`
- Regenerate: `gba/include/generated/tower_ui_assets.h`
- Regenerate: `gba/reference/ui_manifest.json`

**Interfaces:**
- Produces named composites:
  - `menu_worker_blue_f0..f9`, `menu_worker_red_f0..f9`
  - `construction_target_badge_f0..f4`
  - `hud_white_digit_f0..f13`
  - `hud_brown_digit_f0..f11`
  - `hud_red_digit_f0..f10`
  - `hud_status_graphic`
  - `hud_state_indicator_f0..f9`
  - `quick_counter_frame`
  - `city_hanging_ui`, `city_status_icon_f0..f4`, `city_panel_f0..f1`, `city_action_icon`
  - `city_effect_f0..f5`
  - `crane_hook_frame_f0..f4`
  - `accuracy_star_f0..f2`

- [x] **Step 1: Expand `SOURCE_RESOURCES` and load both worker strips/resources 13–23,29,30,35**

- [x] **Step 2: Export exact strip frame counts from source dimensions**

Use 5×11 for res13, 14×5 for res14, 12×5 for res15, 11×5 for res16, 10×6 for res18, 5×6 for res21, 2×13 for res22, 6×22 for res29, 5×18 for res30, 3×9 for res35.

- [x] **Step 3: Regenerate UI assets against the canonical JAR**

Run the existing UI export entry point used by tests/project generation.

- [x] **Step 4: Run exporter tests and verify GREEN**

### Task 4: Replace Quick Game placeholder HUD with JAR-derived sprites

**Files:**
- Modify: `gba/include/tb/quick_game_scene.h`
- Modify: `gba/src/quick_game_scene.cpp`
- Modify: `tests/test_butano_project.py`

**Interfaces:**
- Consumes: `quick_counter_frame`, `hud_white_digit_f*`, `hud_brown_digit_f*`.
- Keeps game state from `QuickGameSnapshot`; changes presentation only.

- [x] **Step 1: Add RED source/behavior assertions**

Require Quick Game to use `quick_counter_frame` and white digit composites, and prohibit the old localized `Floors:` placeholder line in normal play HUD.

- [x] **Step 2: Render resource-19 counter at the recovered lower-left placement**

Draw the floor count with three white source digits over its digit window.

- [x] **Step 3: Retain the proven Java combo bar geometry but replace combo text digits with resource 15 where applicable**

- [x] **Step 4: Remove placeholder title/population/chance text that does not exist in the captured gameplay HUD**

Only preserve terminal/result messages needed for navigation.

- [x] **Step 5: Run focused tests**

### Task 5: Replace Tower Construction placeholder HUD with JAR-derived sprites

**Files:**
- Modify: `gba/include/tb/tower_construction_scene.h`
- Modify: `gba/src/tower_construction_scene.cpp`
- Modify: `tests/test_butano_project.py`

**Interfaces:**
- Consumes: `construction_target_badge_f*` and `hud_state_indicator_f*`.
- Maps building type 1–4 to badge frames 0–3 and base state frame `2*(building_type-1)` as recovered from `House.i(Graphics)`.

- [x] **Step 1: Add RED assertions for source HUD use and removal of the old textual floor/target/chance display**

- [x] **Step 2: Draw the target badge and vertical state indicators at the lower-left HUD location**

Use remaining-chance state to choose active versus exhausted indicator frames while retaining terminal text only when the scene ends.

- [x] **Step 3: Run focused tests**

### Task 6: Full verification and packaging

**Files:**
- Update generated/reference files as needed.
- Create delivery ZIPs outside source tree.

**Interfaces:**
- Produces Fix 6 full/update packages.

- [x] **Step 1: Run `PYTHONPATH=.:tools pytest -q`**

Expected: all tests green.

- [x] **Step 2: Compile portable C++ host tests used by pytest**

Covered by runtime pytest gates; inspect output for warnings/errors.

- [x] **Step 3: Verify generated UI manifest/assets are reproducible from canonical JAR**

- [x] **Step 4: Build update-only file set by comparing Fix 5 versus Fix 6**

- [x] **Step 5: Apply update ZIP over an untouched Fix-5 source copy and byte-compare against Fix 6**

Expected: 0 missing, 0 extra, 0 byte mismatches.

- [x] **Step 6: Package FULL source ZIP and report SHA-256 hashes**

- [x] **Step 7: Do not claim an ARM `.gba` was built locally**

The source remains ready for the existing GitHub/devkitARM workflow.
