# GitHub ROM builds and releases

The repository includes `.github/workflows/gba-release.yml`, which makes GitHub Actions the authoritative GBA build environment for this port.

## What happens on every push and pull request

1. `host-tests` runs the permanent host regression suite with Python 3.11.
2. `gba-build` runs in the official `devkitpro/devkitarm` container.
3. The build job clones **Butano 21.7.1** into `.ci/butano` and builds the existing `gba/Makefile` with an explicit `LIBBUTANO` path.
4. `gba/TowerBloxxGBA.gba` is copied to `dist/TowerBloxx.gba`.
5. A SHA-256 file is generated as `dist/TowerBloxx.gba.sha256` and verified before upload.
6. GitHub Actions uploads both files as a workflow artifact named `TowerBloxx-GBA-<commit SHA>`.

All graphics and audio needed by the runtime are committed as final GBA-ready assets; no external game data is required to build the ROM.

## Manual build or release from GitHub Actions

Open the repository on GitHub, choose **Actions** → **GBA ROM CI and Release** → **Run workflow**.

The `version` field is optional:

- Leave it blank to run the full tests/build and create only the normal downloadable Actions artifact.
- Enter `0.1.0` or `v0.1.0` to build and publish a GitHub Release. A missing `v` is added automatically.

For a versioned manual run, the workflow accepts semantic versions in `vX.Y.Z` form, points that version tag at the commit being built, then creates or updates the matching GitHub Release. Running `0.1.0` again therefore refreshes `v0.1.0` with the ROM from the new successful run rather than creating a duplicate release.

The release contains:

- `TowerBloxx.gba`
- `TowerBloxx.gba.sha256`

If those assets already exist on the release, they are replaced with `--clobber`.

## Downloading a ROM from an ordinary build

Open the repository on GitHub, choose **Actions**, open a successful **GBA ROM CI and Release** run, then download the `TowerBloxx-GBA-<commit SHA>` artifact. GitHub wraps workflow artifacts in a ZIP; inside are:

- `TowerBloxx.gba`
- `TowerBloxx.gba.sha256`

These ordinary build artifacts are retained for 30 days.

## Creating a release by pushing a tag

The original tag-driven route remains supported. Push a version tag whose name begins with `v`, for example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The same host tests and ARM build must pass first. The `release` job then creates or updates the GitHub Release named from that tag and attaches the same ROM and checksum assets.

## Build environment

The workflow intentionally pins **Butano 21.7.1**, matching `gba/Makefile`. devkitARM and the GBA tools come from the official `devkitpro/devkitarm` container, whose `gba-dev` installation includes the ARM toolchain and GBA asset tools such as grit.

The Makefile's normal local `LIBBUTANO` default is left unchanged. CI overrides it with:

```text
$GITHUB_WORKSPACE/.ci/butano/butano
```

so the checked-out project never needs to vendor Butano.

## Release permissions

The workflow has read-only repository contents permission by default. Only the release job requests `contents: write`, which is required to create/update tags and GitHub Releases.
