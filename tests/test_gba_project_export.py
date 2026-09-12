from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct

from tower_bloxx_extract.gba_project_export import export_gba_project_assets

EXPECTED_MESH_IDS = [7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43]


def _tree_hash(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def _bmp_bits_per_pixel(path: Path) -> int:
    data = path.read_bytes()
    assert data[:2] == b"BM"
    return struct.unpack_from("<H", data, 28)[0]


def test_export_gba_project_assets_is_complete_and_deterministic(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    a = tmp_path / "a"
    b = tmp_path / "b"
    manifest_a = export_gba_project_assets(tower_bloxx_jar, a)
    manifest_b = export_gba_project_assets(tower_bloxx_jar, b)

    assert _tree_hash(a) == _tree_hash(b)
    assert manifest_a == manifest_b
    assert manifest_a["butano_version"] == "21.7.1"
    assert manifest_a["mesh_ids"] == EXPECTED_MESH_IDS
    assert manifest_a["mesh_count"] == 19

    graphics = a / "gba" / "graphics" / "generated"
    bmps = sorted(graphics.glob("*.bmp"))
    json_files = sorted(graphics.glob("*.json"))
    assert bmps
    assert len(bmps) == len(json_files)

    for bmp in bmps:
        assert _bmp_bits_per_pixel(bmp) in (4, 8)
        metadata = json.loads(bmp.with_suffix(".json").read_text())
        assert metadata["type"] == "sprite"
        assert metadata["bpp_mode"] in ("bpp_4", "bpp_8")

    header = (a / "gba" / "include" / "generated" / "tower_mesh_assets.h").read_text()
    assert "mesh_count = 19" in header
    for mesh_id in EXPECTED_MESH_IDS:
        assert f"mesh_{mesh_id:03d}_parts" in header
        assert f"tb_mesh_{mesh_id:03d}_p0" in header

    manifest_path = a / "gba" / "reference" / "generated_assets_manifest.json"
    on_disk = json.loads(manifest_path.read_text())
    assert on_disk == manifest_a
    assert len(on_disk["files"]) == len(bmps) * 2 + 1  # BMP+JSON plus generated header.
