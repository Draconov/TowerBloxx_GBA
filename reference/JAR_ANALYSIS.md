# Tower Bloxx 1.5.22 JAR reference analysis

Canonical input: `Tower-Bloxx_v1522.jar`

SHA-256: `7aebc77593a68e8bf58ce6e633c2e3bc4fae0d873eb909a64628bb293f920cf0`

## Package profile

- MIDlet: `Tower Bloxx(TM)`
- Vendor: Digital Chocolate, Inc.
- Version: 1.5.22
- Java class version: 46.0 (Java 1.2 era)
- Java ME profile: MIDP 1.0 / CLDC 1.0
- Main class: `House`
- 17 compiled classes: `GameMIDlet`, `House`, and obfuscated classes `a` through `o` except no `p`.
- Uses Nokia `FullCanvas` / `DeviceControl`, JSR-184 M3G, RMS storage, and Java ME media/MIDI APIs.

## Resource layout

The JAR uses a custom resource pack rather than exposing most assets as normal files.

- `r0`: 36,645-byte packed resource container with a 47-entry signed offset/length table.
- Resource IDs 0-36: 37 PNG images.
- Resource IDs 37-42: 6 Standard MIDI files, format 0, PPQN 480.
- Resource IDs 43-44: binary game/font/layout data.
- Resource ID 45: external `/45` file, 121,614 bytes, JSR-184 M3G scene.
- Resource-table entry 45 is `-121614`, matching the exact M3G file length; entry 46 is the `r0` end offset (36,645).
- `/45` SHA-256: `41f755aeeeeb42a7cd1c9acaf642a4609e4d7993d910cf5c5fe6b217cba79ae1`
- `r0` SHA-256: `5fd54f70405123670eb5de304ecd22abbe7d6917ab31b2757f0c948ff15cde49`

`resource_inventory.csv` records every resource ID, type, byte size, dimensions where applicable, and SHA-256.

## Graphics findings

The 37 packed PNGs are primarily interface art, logos, number/indicator strips, city-grid pieces, trees, construction decorations, icons, and bitmap-font material. The tower bodies themselves are not present as ordinary PNG sprites.

The game loads `/45` with `javax.microedition.m3g.Loader`. At startup it asks the M3G `World` for mesh user IDs:

`7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43`

This establishes 19 directly referenced M3G meshes. The file identifies itself as JSR-184 and contains the exporter string `HI 3D Text plug-in for 3ds max`.

The M3G camera uses a 60-degree field of view and computes its projection from the actual canvas dimensions. The original code also scales city-layout X coordinates from a 176-pixel reference width to the current canvas width, which confirms that the original game was designed to adapt to more than one Java ME display size rather than assuming one fixed framebuffer.

## Localization findings

Five complete localization resources are included, each with 134 strings:

- `en-EN` — English
- `fr-FR` — Français
- `it-IT` — Italiano
- `de-DE` — Deutsch
- `es-ES` — Español

The English data confirms the two principal game modes and their original terminology:

- `Quick Game`
- `Build City`

It also confirms the four building classes: Residential, Commercial, Office, and Luxury Towers; the city progression from Tiny Town through Megalopolis; three construction chances in Build City; combo/perfect-drop mechanics; population scoring; placement-neighbor rules; demolition; trophy roofs; and a 20-milestone city progression.

## Runtime findings relevant to the GBA port

- The main loop is time-delta driven rather than locked to a Java frame rate. It uses an eight-sample moving average of millisecond deltas and clamps a single raw delta to 500 ms.
- Input is translated through Java ME game actions and numeric keypad semantics (2/4/5/6/8 in the instructional text).
- Persistent data uses Java ME RMS. A record named `towermode` stores six integers; settings and other records are also present.
- Audio is Java ME MIDI through `javax.microedition.media.Player` and `VolumeControl`.
- M3G is used for tower/crane-related 3D presentation; ordinary UI and city presentation use 2D Java ME drawing and packed PNGs.

## Porting consequence

The GBA port should not attempt to embed a Java ME VM or JSR-184 renderer. The reference assets/data should be decoded offline. M3G meshes should be converted/pre-rendered into GBA-friendly 2D frames while gameplay, scoring, city rules, timing, and state transitions are reimplemented natively from the recovered Java behavior.

## Phase 1 reproducible extraction formats

The exploratory resource findings above are now implemented as deterministic host-side parsers under `tools/tower_bloxx_extract/` and guarded by JAR-backed tests.

### Binary resource 43

`House.a()` reads resource ID 43 through `DataInputStream` in this exact order:

```text
u8 entry_count
u8 int_count
int_count * i32 values
entry_count * {
    u8 x_ref
    u8 width_ref
    u16 y_start
    u16 height
    u8 kind
}
```

For the canonical 1.5.22 JAR this resolves to 88 entries and 17 32-bit values. The structure consumes all 686 bytes exactly. The runtime subsequently scales the 176-reference-width X fields; the extractor intentionally preserves the original source values.

### Binary resource 44

Constructor `b(Image, DataInputStream, Font)` reads resource ID 44 in this exact order:

```text
u8 width_count
width_count * u8 widths
8 * u8 special_map
u8 glyph_count
glyph_count * {
    i8 source_x
    u8 source_width
    i16 source_y
    u8 source_height
    u8 advance_or_class
}
```

For the canonical JAR both `width_count` and `glyph_count` are 191. The eight-byte special map is `[33, 126, 32, 161, 255, 66, 1, 11]`. The decoder consumes all 1,347 bytes exactly.

### Determinism gate

Two clean extraction runs are required to compare byte-for-byte equal. The Phase 1 extractor emits 37 PNGs, 6 MIDIs, resources 43/44 as exact binary copies plus decoded JSON, external resource 45 as an exact M3G copy, five 134-string locale JSON files, and stable JSON/CSV manifests. No timestamps or absolute host paths are written to generated output.

## Phase 4 font / menu shell findings

The Java ME bitmap font is now reconstructed byte-for-byte from PNG resource 36 and binary resource 44. The atlas is 87x65, the font has 191 mapping/glyph records, uses the special-map bytes `[33, 126, 32, 161, 255, 66, 1, 11]`, and has a one-pixel inter-character spacing with an 11-pixel source line height. The complete five-locale character union outside ASCII is exactly:

`¡ ° ¿ È à á â ä ç è é ê ì í ñ ò ó ô ö ù ú û ü`

For GBA output these source glyphs are placed losslessly after BGR555 conversion into 8x16 4bpp cells. Resource 7 is the 109x26 Tower Bloxx logo and resource 10 is the 41x10 Sumea logo; both fit losslessly into 4bpp OBJ composites.

The main shell uses the original localized string indices proven by the JAR: 17/18/19/21 (`New game`, `Settings`, `Instructions`, `About` in English), 91/92 (`Quick Game`, `Build City`), 24 (`Sound:`), 27 (`Language`), and 13/14 (`On`/`Off`). The five original locale tables remain unmodified. Separate display-only instruction copies adapt keypad controls to GBA (`5` to A and `4/6/2/8` to D-pad) and are pre-wrapped using the recovered bitmap-font widths.

Phone-only vibration/backlight controls and network/SMS/licensing/game-lobby flows are deliberately omitted from the native GBA menu. They are not gameplay and have no meaningful GBA equivalent.

## Phase 5 Quick Game recovery

The normal stacking loop has now been separated from the obfuscated `House` class and implemented as a deterministic native C++ core. The detailed method-by-method evidence is in `reference/QUICK_GAME_ANALYSIS.md`.

Key recovered contracts now guarded by host tests include:

- fresh primary input (`5` / Java action 8, mapped to GBA A) starts a drop only from the attached state and only when the camera has reached its target;
- raw simulation delta is clamped to 150 ms and the gameplay physics group runs once accumulated time reaches 25 ms;
- crane swing uses the game's generated 360-entry integer sine table and its unusual `c(angle) = b(angle - 90)` sign convention;
- initial camera/current target is 512, initial world anchor is 2432, initial rope extension is 0, and normal maximum rope extension is 1664;
- released blocks inherit the crane velocity and use the normal falling equation `startY + vy*t/256 - t*t/200`;
- top-floor landing succeeds for absolute horizontal offsets through 127 fixed units and slips/fails beyond that;
- successful offsets retain the original `<25`, `<50`, `<80`, `<=127` accuracy bands for later scoring work;
- Quick Game starts with three construction chances and delays the next block by 400 ms after a resolved placement/miss;
- stable floors are spaced by 256 fixed units and preserve their signed placement offsets;
- floor progression applies the exact Quick Game period/amplitude/bias formulas using target heights 10/20/30/40 and the recovered tuning arrays;
- camera target movement is 256 units per floor after the opening floor and interpolates over 500 ms.

The Phase-5 core intentionally does not claim the bytecode branches for combo/population scoring, final result tally, random camera shake, dynamic recent-floor sway, or Build City. Those remain explicit later milestones rather than approximations hidden inside the renderer.
