"""Read-only Christmas transparency and crane geometry regressions.

Asset cleanup is committed directly into BMPs; CI must never repair source
assets at runtime or depend on a missing external Python utility.
"""
from pathlib import Path
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def rogue_magenta_pixels(image: Image.Image) -> int:
    """Count opaque hot-pink palette aliases when index 0 is magenta-keyed."""
    if image.mode != "P":
        return 0
    palette = image.getpalette() or []
    if tuple(palette[:3]) != (255, 0, 255):
        return 0
    aliases = [
        index for index in range(1, len(palette) // 3)
        if palette[index * 3] >= 240
        and palette[index * 3 + 1] <= 40
        and palette[index * 3 + 2] >= 210
    ]
    histogram = image.histogram()
    return sum(histogram[index] for index in aliases)


def test_identifies_opaque_magenta_alias_without_modifying_the_sprite(tmp_path):
    sprite = tmp_path / "test.bmp"
    palette = [0] * 768
    palette[:3] = [255, 0, 255]
    palette[3:6] = [255, 180, 0]
    palette[10 * 3:10 * 3 + 3] = [252, 5, 232]
    image = Image.new("P", (8, 8), 1)
    image.putpalette(palette)
    image.putpixel((2, 3), 10)
    image.save(sprite)
    original = sprite.read_bytes()
    with Image.open(sprite) as saved:
        assert rogue_magenta_pixels(saved) == 1
    assert sprite.read_bytes() == original


def test_legitimate_pink_with_black_transparency_is_not_flagged():
    image = Image.new("P", (8, 8), 1)
    palette = [0] * 768
    palette[3:6] = [255, 10, 220]
    image.putpalette(palette)
    assert rogue_magenta_pixels(image) == 0


def test_christmas_assets_have_no_visible_magenta_key_aliases():
    christmas = ROOT / "gba" / "graphics" / "christmas"
    assert christmas.is_dir(), christmas
    leaks = []
    for path in sorted(christmas.rglob("*.bmp")):
        with Image.open(path) as image:
            count = rogue_magenta_pixels(image)
        if count:
            leaks.append(f"{path.relative_to(ROOT)}: {count} pixel(s)")
    assert not leaks, "Opaque magenta pixels remain: " + "; ".join(leaks)


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
