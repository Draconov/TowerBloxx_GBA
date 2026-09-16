# Tower Bloxx GBA

Native Game Boy Advance port of Tower Bloxx built with **Butano 21.7.1** and **devkitARM**.

## Build locally

1. Install devkitPro's GBA development tools so `DEVKITARM` is available.
2. Download Butano 21.7.1.
3. Point `LIBBUTANO` at the directory containing `butano.mak`.
4. From the repository root, run:

```bash
make -C gba -j4 LIBBUTANO=/path/to/butano/butano
```

The ROM is written as `gba/TowerBloxxGBA.gba`.

## Controls

- **D-pad**: menu navigation / Build City navigation.
- **A**: confirm, drop an attached block, advance dialogs.
- **B**: back or leave/suspend an active construction session where supported.
- **Start**: confirm/advance on supported UI screens.
- **Select**: space during high-score name entry.

## Game modes

- **Quick Game**: endless tower stacking with three construction chances, combos, population scoring, records and the original first-block lowering presentation.
- **Build City**: construct finite towers, place or replace them in the city grid, unlock higher building types, and progress through city levels.

The repository contains only the final GBA source, runtime assets, permanent regression tests, and release tooling needed to build and maintain the port.

## Tests

From the repository root:

```bash
python -m pip install pytest Pillow
python -m pytest -q
```

The permanent suite is intentionally small: `tests/test_gameplay.cpp` protects deterministic gameplay/state rules, while `tests/test_repo.py` builds/runs that host test and checks assets, backgrounds, cleanup invariants, packaging, and the GitHub release workflow.

## GitHub ROM builds and releases

`.github/workflows/gba-release.yml` runs the permanent host suite and builds the ROM with devkitARM + Butano 21.7.1.

From **Actions → GBA ROM CI and Release → Run workflow**:

- leave `version` blank for a build-only artifact;
- enter `1.0.1` or `v1.0.1` to create or refresh that GitHub Release.

Tag-triggered `v*` releases remain supported. See `docs/GITHUB_RELEASES.md` for details.
