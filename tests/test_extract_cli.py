import json
from pathlib import Path

from tower_bloxx_extract.cli import extract_reference
from tower_bloxx_extract.io import CANONICAL_SHA256
from tower_bloxx_extract.manifest import tree_hash


def test_extraction_is_deterministic(tower_bloxx_jar, tmp_path: Path):
    out_a = tmp_path / "a"
    out_b = tmp_path / "b"
    extract_reference(tower_bloxx_jar, out_a)
    extract_reference(tower_bloxx_jar, out_b)

    assert tree_hash(out_a) == tree_hash(out_b)
    manifest = json.loads((out_a / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["jar_sha256"] == CANONICAL_SHA256
    assert manifest["resource_counts"] == {
        "binary": 2,
        "locale": 5,
        "m3g": 1,
        "midi": 6,
        "png": 37,
    }
    assert manifest["m3g_sha256"] == "41f755aeeeeb42a7cd1c9acaf642a4609e4d7993d910cf5c5fe6b217cba79ae1"


def test_extraction_output_shape(tower_bloxx_jar, tmp_path: Path):
    out = tmp_path / "extract"
    extract_reference(tower_bloxx_jar, out)

    expected = {
        *(f"resources/{rid:03d}.png" for rid in range(37)),
        *(f"resources/{rid:03d}.mid" for rid in range(37, 43)),
        "resources/043.bin",
        "resources/044.bin",
        "resources/045.m3g",
        "locales/en-EN.json",
        "locales/fr-FR.json",
        "locales/it-IT.json",
        "locales/de-DE.json",
        "locales/es-ES.json",
        "decoded/resource_43.json",
        "decoded/resource_44.json",
        "manifest.json",
        "resource_inventory.csv",
    }
    actual = {
        str(path.relative_to(out))
        for path in out.rglob("*")
        if path.is_file()
    }
    assert actual == expected


def test_decoded_binary_shapes_are_pinned(tower_bloxx_jar, tmp_path: Path):
    out = tmp_path / "extract"
    extract_reference(tower_bloxx_jar, out)
    r43 = json.loads((out / "decoded/resource_43.json").read_text(encoding="utf-8"))
    r44 = json.loads((out / "decoded/resource_44.json").read_text(encoding="utf-8"))
    assert r43["entry_count"] == 88
    assert r43["int_count"] == 17
    assert r44["width_count"] == 191
    assert r44["glyph_count"] == 191
