from __future__ import annotations

from pathlib import Path

from tower_bloxx_extract.gba_audio_export import (
    AUDIO_ROUTES,
    analyze_midi,
    export_gba_audio_assets,
)
from tower_bloxx_extract.resources import read_resource
import zipfile


def test_original_midi_contract_is_recovered(tower_bloxx_jar: Path) -> None:
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        infos = [analyze_midi(read_resource(jar, rid)) for rid in range(37, 43)]
    assert [info.division for info in infos] == [480] * 6
    assert [info.tempo_bpm for info in infos] == [160, 160, 140, 160, 160, 160]
    assert [route.resource_id for route in AUDIO_ROUTES] == [37, 38, 39, 40, 41, 42]
    assert [route.name for route in AUDIO_ROUTES] == [
        "menu_theme", "tower_theme", "city_theme",
        "construction_fail", "normal_roof", "trophy_roof",
    ]
    assert [route.loop for route in AUDIO_ROUTES] == [True, True, True, False, False, False]


def test_export_gba_audio_assets_writes_deterministic_8chn_modules(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    first = tmp_path / "a"
    second = tmp_path / "b"
    manifest_a = export_gba_audio_assets(tower_bloxx_jar, first)
    manifest_b = export_gba_audio_assets(tower_bloxx_jar, second)

    assert manifest_a == manifest_b
    assert manifest_a["module_count"] == 6
    assert manifest_a["routes"][0]["name"] == "menu_theme"
    for route in manifest_a["routes"]:
        left = first / "gba" / "audio" / f"{route['name']}.mod"
        right = second / "gba" / "audio" / f"{route['name']}.mod"
        assert left.read_bytes() == right.read_bytes()
        payload = left.read_bytes()
        expected_signature = b"8CHN" if route["loop"] else b"M.K."
        assert payload[1080:1084] == expected_signature
        assert route["channels"] == (8 if route["loop"] else 4)
        assert len(payload) > 1084
