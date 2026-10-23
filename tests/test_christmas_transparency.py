"""Regression checks for Christmas magenta and hook atlas geometry.

CI sanitizes indexed BMPs before running this test and before Butano conversion.
"""
from pathlib import Path
import re
from PIL import Image
from scripts.repair_christmas_transparency import inspect_bmp, scan

ROOT = Path(__file__).resolve().parents[1]

def test_rogue_magenta_is_erased_without_palette_or_other_pixel_changes(tmp_path):
    sprite = tmp_path / "test.bmp"
    palette = [0] * 768
    palette[0:3] = [255, 0, 255]           # genuine GBA transparent key
    palette[3:6] = [255, 180, 0]           # yellow is opaque and must survive
    palette[10 * 3:10 * 3 + 3] = [252, 5, 232]  # stray hot pink
    im = Image.new("P", (8, 8), 1)
    im.putpalette(palette)
    im.putpixel((2, 3), 10)
    im.save(sprite)
    before = sprite.read_bytes()
    assert inspect_bmp(sprite, apply=False) == 1
    assert sprite.read_bytes() == before
    assert inspect_bmp(sprite, apply=True) == 1
    assert inspect_bmp(sprite, apply=True) == 0
    with Image.open(sprite) as fixed:
        assert fixed.getpixel((2, 3)) == 0
        assert fixed.getpixel((3, 3)) == 1
        assert fixed.getpalette() == im.getpalette()
    assert len(sprite.read_bytes()) == len(before)


def test_legitimate_colored_sprite_is_not_rewritten(tmp_path):
    sprite = tmp_path / "test.bmp"
    im = Image.new("P", (8, 8), 1)
    palette = [0] * 768
    palette[3:6] = [255, 10, 220]  # a legitimately opaque pink in an asset whose key is black
    im.putpalette(palette)
    im.save(sprite)
    orig = sprite.read_bytes()
    assert inspect_bmp(sprite, apply=True) == 0
    assert orig == sprite.read_bytes()


def test_christmas_assets_have_no_visible_magenta_key_aliases():
    files, pixels = scan(ROOT, apply=False)
    assert (files, pixels) == (0, 0), f"{pixels} visible magenta pixels in {files} sprites"


def hook_offsets(header: Path):
    contents = header.read_text(encoding="utf-8")
    parts = re.findall(r"inline const MeshPartAsset crane_hook_pose_(\d+)_parts\[\] = \{(.*?)\};", contents, re.S)
    return {int(frame): tuple((int(x), int(y)) for x, y in re.findall(
        r"\{\s*&bn::sprite_items::[^,]+,\s*(-?\d+),\s*(-?\d+)\s*\}", body))
        for frame, body in parts}


def test_christmas_hook_reuses_exact_classic_sprite_part_anchors():
    generated = ROOT / "gba" / "include" / "generated"
    classic = hook_offsets(generated / "tower_mesh_assets.h")
    christmas = hook_offsets(generated / "christmas_tower_mesh_assets.h")
    assert len(classic) == len(christmas) == 49
    assert classic == christmas
