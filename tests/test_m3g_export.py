from __future__ import annotations

import json
from pathlib import Path

from tower_bloxx_extract.cli import extract_reference
from tower_bloxx_extract.manifest import tree_hash
from tower_bloxx_extract.m3g_export import export_m3g_reference


def test_m3g_export_is_byte_deterministic_and_complete(tower_bloxx_jar, tmp_path: Path):
    out_a = tmp_path / "a"
    out_b = tmp_path / "b"
    export_m3g_reference(tower_bloxx_jar, out_a)
    export_m3g_reference(tower_bloxx_jar, out_b)

    assert tree_hash(out_a) == tree_hash(out_b)
    assert {
        str(path.relative_to(out_a))
        for path in out_a.rglob("*")
        if path.is_file()
    } == {
        str(path.relative_to(out_b))
        for path in out_b.rglob("*")
        if path.is_file()
    }

    catalog = json.loads((out_a / "mesh_catalog.json").read_text(encoding="utf-8"))
    manifest = json.loads((out_a / "manifest.json").read_text(encoding="utf-8"))
    assert len(catalog) == 19
    assert len(list((out_a / "textures").glob("image_*.png"))) == 17
    assert len(list((out_a / "renders").glob("mesh_*.png"))) == 19
    assert manifest["target"] == {"height": 160, "width": 240}
    assert manifest["mesh_count"] == 19
    assert manifest["texture_count"] == 17
    assert len(manifest["renders"]) == 19

    for render in manifest["renders"]:
        assert render["visible_bounds"] is not None
        raw = out_a / render["bgr555_path"]
        assert raw.stat().st_size == 240 * 160 * 2
        assert isinstance(render["unique_bgr555_colors"], int)
        assert render["four_bpp_lossless"] <= render["eight_bpp_lossless"]
        if render["indexed_path"] is not None:
            assert (out_a / render["indexed_path"]).is_file()
            assert (out_a / render["palette_path"]).is_file()


def test_phase1_cli_can_include_m3g_derivatives(tower_bloxx_jar, tmp_path: Path):
    out = tmp_path / "extract"
    extract_reference(tower_bloxx_jar, out, include_m3g=True)
    assert (out / "resources/045.m3g").is_file()
    assert (out / "m3g/scene.json").is_file()
    assert (out / "m3g/manifest.json").is_file()
    assert len(list((out / "m3g/renders").glob("mesh_*.png"))) == 19


def test_cli_m3g_flag_enables_derivative_export(tower_bloxx_jar, tmp_path: Path):
    from tower_bloxx_extract.cli import main

    out = tmp_path / "cli"
    assert main([str(tower_bloxx_jar), str(out), "--m3g"]) == 0
    assert (out / "m3g/manifest.json").is_file()
