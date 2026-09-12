# GitHub ROM builds and releases

The repository includes `.github/workflows/gba-release.yml`, which makes GitHub Actions the authoritative GBA build environment for this port.

## What happens on every push and pull request

1. `host-tests` runs the complete Python/host parity suite with Python 3.11.
2. `gba-build` runs in the official `devkitpro/devkitarm` container.
3. The build job clones **Butano 21.7.1** into `.ci/butano` and builds the existing `gba/Makefile` with an explicit `LIBBUTANO` path.
4. `gba/TowerBloxxGBA.gba` is copied to `dist/TowerBloxx.gba`.
5. A SHA-256 file is generated as `dist/TowerBloxx.gba.sha256` and verified before upload.
6. GitHub Actions uploads both files as a workflow artifact named `TowerBloxx-GBA-<commit SHA>`.

The workflow does not require the original Nokia JAR. All graphics needed by the current runtime are already present as generated project assets.

## Downloading a ROM from an ordinary build

Open the repository on GitHub, choose **Actions**, open a successful **GBA ROM CI and Release** run, then download the `TowerBloxx-GBA-<commit SHA>` artifact. GitHub wraps workflow artifacts in a ZIP; inside are:

- `TowerBloxx.gba`
- `TowerBloxx.gba.sha256`

These ordinary build artifacts are retained for 30 days.

## Creating a GitHub Release

Push a version tag whose name begins with `v`, for example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The same host tests and ARM build must pass first. The `release` job then creates a GitHub Release named from the tag and attaches:

- `TowerBloxx.gba`
- `TowerBloxx.gba.sha256`

If the release job is rerun for a tag that already has a release, the two assets are uploaded again with `--clobber` instead of failing.

## Build environment

The workflow intentionally pins **Butano 21.7.1**, matching `gba/Makefile`. devkitARM and the GBA tools come from the official `devkitpro/devkitarm` container, whose `gba-dev` installation includes the ARM toolchain and GBA asset tools such as grit.

The Makefile's normal local `LIBBUTANO` default is left unchanged. CI overrides it with:

```text
$GITHUB_WORKSPACE/.ci/butano/butano
```

so the checked-out project never needs to vendor Butano.

## Release permissions

The workflow has read-only repository contents permission by default. Only the tag-gated `release` job requests `contents: write`, which is required to create or update GitHub Releases.
