# Tower Bloxx v1.5.22 M3G analysis

This document records the Phase-2 reverse engineering of resource `/45` from the canonical Tower Bloxx v1.5.22 JAR. The committed tooling never contains the original JAR or raw proprietary M3G bytes; it consumes the user-supplied canonical JAR and emits deterministic derivatives.

## Canonical identity

- JAR SHA-256: `7aebc77593a68e8bf58ce6e633c2e3bc4fae0d873eb909a64628bb293f920cf0`
- Resource `/45` size: 121,614 bytes
- Resource `/45` SHA-256: `41f755aeeeeb42a7cd1c9acaf642a4609e4d7993d910cf5c5fe6b217cba79ae1`
- M3G identifier: `AB 4A 53 52 31 38 34 BB 0D 0A 1A 0A`
- Authoring field: `HI 3D Text plug-in for 3ds max`

## Container

The file has four uncompressed sections. Every Adler-32 checksum is validated before object decoding.

| Section | Offset | Total length | Uncompressed object bytes |
| ---: | ---: | ---: | ---: |
| 0 | 12 | 60 | 47 |
| 1 | 72 | 13 | 0 |
| 2 | 85 | 119,664 | 119,651 |
| 3 | 119,749 | 1,865 | 1,852 |

The global stream contains exactly 270 serialized objects.

| Type | M3G class | Count |
| ---: | --- | ---: |
| 0 | Header | 1 |
| 1 | AnimationController | 1 |
| 3 | Appearance | 44 |
| 5 | Camera | 1 |
| 6 | CompositingMode | 3 |
| 8 | PolygonMode | 2 |
| 10 | Image2D | 17 |
| 11 | TriangleStripArray | 44 |
| 13 | Material | 44 |
| 14 | Mesh | 19 |
| 17 | Texture2D | 17 |
| 20 | VertexArray | 57 |
| 21 | VertexBuffer | 19 |
| 22 | World | 1 |

There are no serialized skeletal meshes, morph targets, lights, sprites, fog objects, animation tracks, keyframe sequences, or runtime features that would justify carrying a generic M3G engine onto the GBA.

## World and game-used meshes

World object 270 has user ID 269, children 251 through 269, and stored active-camera reference 206. The 19 children are exactly the 19 Mesh objects selected by the Java game.

The game-visible user IDs are:

`7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43`

Their serialized object indices and source authoring translations are:

| Mesh object | User ID | VertexBuffer | Source X translation |
| ---: | ---: | ---: | ---: |
| 251 | 31 | 187 | 1600 |
| 252 | 10 | 188 | -800 |
| 253 | 30 | 189 | 0 |
| 254 | 32 | 190 | 3200 |
| 255 | 33 | 191 | 4800 |
| 256 | 40 | 192 | 400 |
| 257 | 41 | 193 | 2000 |
| 258 | 42 | 194 | 3600 |
| 259 | 43 | 195 | 5200 |
| 260 | 8 | 196 | -4000 |
| 261 | 7 | 197 | -4400 |
| 262 | 20 | 198 | -400 |
| 263 | 11 | 199 | 800 |
| 264 | 12 | 200 | 2400 |
| 265 | 13 | 201 | 4000 |
| 266 | 21 | 202 | 1200 |
| 267 | 22 | 203 | 2800 |
| 268 | 23 | 204 | 4400 |
| 269 | 9 | 205 | -2503.186 |

Those source translations are authoring-layout metadata, not gameplay placement. The Java wrapper uses `Graphics3D.render(Node, Transform)`, whose immediate-mode transform replaces the rendered node's own transform. The Phase-2 renderer therefore records the source transforms for traceability but does not apply them to reference frames.

## Geometry

All 19 meshes resolve to explicit triangle lists. Representative recovered counts:

- user ID 7: 11 vertices, 6 triangles
- user ID 9: 132 vertices, 44 triangles
- user ID 31: 224 vertices, 143 triangles
- user ID 41: 613 vertices, 300 triangles

The file uses one texture coordinate set at most. Canonical VertexArrays are 1- or 2-byte components; the decoder also implements M3G delta reconstruction even though this file does not use delta encoding. Canonical TriangleStripArrays use encodings 129 and 130; the resolver supports implicit and explicit encodings 0/1/2/128/129/130.

## Images and textures

There are 17 Image2D objects and 17 Texture2D objects. Image formats are RGB (99) and RGBA (100), using both palette-indexed and direct pixel payloads. Dimensions range from 8x8 through 64x64. All texture transforms are identity.

The Java wrapper class `f` modifies each submesh before gameplay rendering:

- material is set to `null`, so material lighting is deliberately removed;
- texture blending is set to `REPLACE` (228);
- S and T wrapping are set to `CLAMP` (240/240).

The offline renderer reproduces those overrides, nearest-neighbor sampling, perspective-correct UV interpolation, and depth testing. It does not attempt to become a general M3G renderer.

## Camera contract recovered from class `n`

The M3G file contains an authoring camera (object 206, user ID 224, 45-degree FOV), but Tower Bloxx does not use that camera for immediate gameplay rendering. Class `n` creates a new Camera and supplies its own camera-to-world Transform.

Base settings are:

- base vertical FOV: 60 degrees;
- near plane: 10;
- far plane: 10000.

For a clip rectangle with top `a`, width `b`, and bottom/height coordinate `c`, the bytecode computes:

- perspective aspect argument: `b / (c - a) * 0.7 + 0.3`;
- FOV scale: `(c - a) / b * 0.7 + 0.3`;
- effective FOV: `baseFov * FOV scale`.

At the GBA target of 240x160 with clip top 0, this yields:

- effective vertical FOV: 46.0 degrees;
- perspective aspect argument: 1.35.

The camera transform helper in class `n` constructs a right vector from `direction x up`, normalizes it, recomputes an orthogonal up vector, stores `-direction` as local +Z, and writes camera position into the translation column. The Phase-2 implementation reproduces that matrix layout.

## Gameplay draw transform recovered from class `f`

Each draw starts by resetting an external M3G Transform to identity. Class `f` then uses `postTranslate`, followed by one or more `postRotate` calls. Its rotation helper accepts turns and multiplies by 360 before calling M3G `postRotate`.

The clean-room renderer uses row-major 4x4 matrices acting on column vectors and implements M3G post-operations as right multiplication (`M <- M * T`, `M <- M * R`). This preserves the observed Java call order.

## Phase-2 GBA derivatives

Reference meshes are rendered into a 240x160 transparent frame using the recovered GBA projection and a deterministic inspection pose. The inspection pose is not claimed to be a gameplay animation frame; it exists to validate geometry/textures and to generate hardware-oriented color statistics.

Every reference frame has binary alpha. After exact GBA BGR555 conversion without palette clustering:

- user IDs 7, 8, and 9 fit in 4bpp palettes;
- the remaining 16 user IDs fit in 8bpp palettes;
- all 19 therefore have a lossless-after-BGR555 indexed derivative;
- no lossy color clustering is performed in Phase 2.

Each frame also exports a full linear BGR555LE raster and one-bit alpha mask. Final Butano tile/sprite slicing is intentionally deferred until the exact gameplay pose atlas is known; Phase 2 does not prematurely lock asset partitioning.

## Boundary for the next phase

Phase 2 proves and exports the source 3D content. It does not yet decide which meshes become OBJ sprites versus background tiles, and it does not invent swing/drop animation poses. Phase 3 can now build the Butano shell against deterministic reference assets while the later gameplay-reconstruction work supplies exact per-state draw transforms from the Java bytecode.

## Phase-3 Butano OBJ slicing

Phase 3 turns the inspection/reference renders into actual Butano import assets without changing any visible post-BGR555 pixels. Each transparent 240x160 frame is cropped to its visible bounds and partitioned into legal GBA OBJ rectangles. Padding is transparent; each part stores its original screen-space center so composing all parts reproduces the reference render.

The generated set contains:

- 19 game-used meshes;
- 71 legal OBJ sprite parts;
- 71 paletted BMP files and 71 matching Butano sprite JSON files;
- one generated C++ descriptor header mapping mesh IDs to sprite items and exact screen offsets;
- 143 hash-tracked generated files total;
- deterministic generated tree hash `5ac0e22a6229ff047340e7fdca85b7a55f262fb0515b3fa3c20100bd2261dd7c`.

Meshes 7, 8, and 9 remain exact 4bpp-after-BGR555 assets; the other 16 remain 8bpp. The Phase-3 gallery only displays one recovered mesh at a time, keeping the temporary 8bpp OBJ VRAM footprint bounded. This partition is a verified runtime-bootstrap representation, not yet the final Quick Game pose/VRAM strategy; Phase 5 can specialize floor/crane assets once exact gameplay draw transforms are recovered.

The first native runtime uses these descriptors in a Butano asset gallery with Left/Right fresh presses. SRAM access is centralized behind a versioned 16-byte save record with magic/version/checksum. No tower-drop physics or menu behavior is invented in this phase.
