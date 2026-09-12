from __future__ import annotations

from pathlib import Path

from PIL import Image
import pytest

from tower_bloxx_extract.m3g_export import export_m3g_reference
from tower_bloxx_extract.gba_assets import (
    LEGAL_OBJ_SIZES,
    choose_sprite_shape,
    recompose_sprite,
    slice_sprite,
)


def test_choose_sprite_shape_uses_smallest_legal_cover() -> None:
    assert choose_sprite_shape(9, 7) == (16, 8)
    assert choose_sprite_shape(33, 17) == (64, 32)
    assert choose_sprite_shape(12, 40) == (32, 64)
    assert choose_sprite_shape(64, 64) == (64, 64)


def test_slice_sprite_rejects_fully_transparent_render() -> None:
    image = Image.new("RGBA", (240, 160), (0, 0, 0, 0))
    with pytest.raises(ValueError, match="no visible pixels"):
        slice_sprite(image, mesh_id=999)


def test_slice_sprite_round_trips_quantized_pixels() -> None:
    image = Image.new("RGBA", (240, 160), (0, 0, 0, 0))
    for y in range(27, 97):
        for x in range(51, 121):
            image.putpixel((x, y), (253, 129, 17, 255))

    composite = slice_sprite(image, mesh_id=123)
    assert composite.bbox == (51, 27, 121, 97)
    assert composite.bpp == 4
    assert len(composite.parts) == 4
    assert all((part.width, part.height) in LEGAL_OBJ_SIZES for part in composite.parts)

    recomposed = recompose_sprite(composite)
    for y in range(160):
        for x in range(240):
            expected_alpha = 255 if 51 <= x < 121 and 27 <= y < 97 else 0
            pixel = recomposed.getpixel((x, y))
            assert pixel[3] == expected_alpha
            if expected_alpha:
                # 253/129/17 quantized to GBA 5-bit channels and expanded to RGB888.
                assert pixel[:3] == (248, 128, 16)


def test_canonical_mesh_010_is_four_8bpp_legal_parts(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    output = tmp_path / "m3g"
    export_m3g_reference(tower_bloxx_jar, output)
    source = Image.open(output / "renders" / "mesh_010.png").convert("RGBA")

    composite = slice_sprite(source, mesh_id=10)
    assert composite.bbox == (58, 38, 177, 142)
    assert composite.bpp == 8
    assert len(composite.parts) == 4
    assert all((part.width, part.height) in LEGAL_OBJ_SIZES for part in composite.parts)
    assert len(composite.palette_bgr555) <= 256

    recomposed = recompose_sprite(composite)
    assert recomposed.getbbox() == source.getbbox()
    for expected, actual in zip(source.get_flattened_data(), recomposed.get_flattened_data()):
        if expected[3] == 0:
            assert actual[3] == 0
        else:
            assert actual == (
                (expected[0] >> 3) << 3,
                (expected[1] >> 3) << 3,
                (expected[2] >> 3) << 3,
                255,
            )
