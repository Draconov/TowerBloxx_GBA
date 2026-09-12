#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-$(pwd)}"
source_rom="${2:-$repo_root/gba/TowerBloxxGBA.gba}"
dist_dir="${3:-$repo_root/dist}"
release_rom="$dist_dir/TowerBloxx.gba"
checksum_file="$dist_dir/TowerBloxx.gba.sha256"

if [[ ! -f "$source_rom" ]]; then
    echo "error: built ROM not found: $source_rom" >&2
    exit 1
fi

mkdir -p "$dist_dir"
cp "$source_rom" "$release_rom"
(
    cd "$dist_dir"
    sha256sum TowerBloxx.gba > TowerBloxx.gba.sha256
)

test -s "$release_rom"
(
    cd "$dist_dir"
    sha256sum --check TowerBloxx.gba.sha256
)

printf 'Packaged %s\n' "$release_rom"
printf 'Checksum: '
cat "$checksum_file"
