from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image

from tower_bloxx_extract.scene_background_export import (
    SKY_COLORS,
    export_scene_backgrounds,
    render_city_background,
    render_construction_background,
    render_menu_background,
)



def test_menu_background_matches_reference_jar_sky_band() -> None:
    # House.<clinit> t[] recovered from the canonical v1.5.22 bytecode.
    assert SKY_COLORS == (
        0xB2D6F2, 0x9AC8EA, 0x80BBE7, 0x66AFE4, 0x518EE4, 0x407ABE,
        0x1C5B96, 0x0C3F7C, 0x13306A, 0x34204C, 0x372C51, 0x2D4B4B,
        0x4A6742, 0x674723, 0x532733, 0x802A2B, 0x511A2F,
    )

    image = render_menu_background()
    assert image.size == (240, 160)
    assert image.mode == "RGBA"
    # The captured JAR menu is sky band 1 with an 11px band-2 cap.
    assert image.getpixel((0, 0))[:3] == (0x80, 0xBB, 0xE7)
    assert image.getpixel((0, 20))[:3] == (0x9A, 0xC8, 0xEA)


def test_construction_background_uses_recovered_nokia_layers(tower_bloxx_jar: Path) -> None:
    image = render_construction_background(tower_bloxx_jar)
    assert image.size == (240, 160)
    assert image.mode == "RGBA"

    # Pin the complete canonical opening composite. Resource 43 intentionally
    # covers much of the raw sky at the top of the frame, so sampling (0, 0)
    # as a "sky pixel" is incorrect; the full RGBA hash proves sky + skyline
    # + resources 31-34 + ground are composed in the recovered order.
    assert hashlib.sha256(image.tobytes()).hexdigest() == (
        "82d1efb49022f76b998a8d7de6010f5b0c17909e0054881daf6c3ee5aba686d9"
    )

    # House.h draws the construction foreground at the recovered projected
    # ground line around y=105 and then its brown ground fill below it.
    assert image.getpixel((10, 150))[:3] == (30, 25, 15)
    # Resource 31 fence/site strip contributes a non-sky pixel here.
    assert image.getpixel((82, 109))[:3] != (178, 214, 242)


def test_city_background_contains_visible_grid() -> None:
    image = render_city_background()
    assert image.size == (240, 160)
    assert image.mode == "RGBA"
    # Empty Build City must no longer be a single flat backdrop.
    colors = set(image.get_flattened_data())
    assert len(colors) >= 5
    assert image.getpixel((120, 80)) != image.getpixel((0, 0))


def test_export_scene_backgrounds_writes_butano_regular_bg_assets(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    manifest = export_scene_backgrounds(tower_bloxx_jar, tmp_path)
    assert manifest["visible_size"] == [240, 160]
    assert manifest["asset_size"] == [256, 256]
    assert manifest["assets"] == ["construction_bg", "city_bg", "menu_bg"]

    graphics = tmp_path / "gba" / "graphics" / "backgrounds"
    for name in manifest["assets"]:
        bmp = Image.open(graphics / f"{name}.bmp")
        assert bmp.size == (256, 256)
        metadata = json.loads((graphics / f"{name}.json").read_text())
        assert metadata["type"] == "regular_bg"
