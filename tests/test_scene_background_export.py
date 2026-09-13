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

    # The original menu also has pale cloud banks behind the tower/logo scene.
    assert image.getpixel((20, 40))[:3] == (170, 204, 230)
    assert image.getpixel((180, 42))[:3] == (170, 204, 230)


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


def test_city_background_matches_recovered_build_city_compositor() -> None:
    image = render_city_background()
    assert image.size == (240, 160)
    assert image.mode == "RGBA"

    # m.a(Graphics,boolean): the playfield is the exact #AFE5FF -> #588CFF
    # vertical gradient between y=13 and y=137 on a 240x160 viewport.
    # y=13/14 are overwritten by the recovered top-band border.
    assert image.getpixel((120, 15))[:3] == (174, 228, 255)
    assert image.getpixel((120, 136))[:3] == (90, 142, 255)

    # Source top status bar and bottom white instruction panel are structural UI,
    # not the tan placeholder field from Fix 6.
    assert image.getpixel((20, 5))[:3] == (173, 156, 131)
    assert image.getpixel((20, 150))[:3] == (255, 255, 255)

    # Recovered 88x88 city board at x=89,y=32 with 17px cell pitch.
    assert image.getpixel((89, 32))[:3] == (255, 255, 255)
    assert image.getpixel((90, 33))[:3] == (64, 64, 64)
    assert image.getpixel((92, 35))[:3] == (120, 188, 40)
    assert image.getpixel((93, 36))[:3] == (67, 120, 23)

    # Browse-mode top-right status boxes start at width-51 after the 9px icon slot.
    assert image.getpixel((189, 1))[:3] == (199, 191, 178)
    assert image.getpixel((190, 2))[:3] == (173, 156, 131)
    assert image.getpixel((213, 1))[:3] == (199, 191, 178)


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
