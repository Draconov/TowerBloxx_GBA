from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.gba_project_export import export_gba_project_assets
from tower_bloxx_extract.m3g_render import TextureRGBA, sample_texture_nearest


def test_m3g_texture_sampling_uses_serialized_image_row_order() -> None:
    # Image2D RGBA bytes are stored row-major top-to-bottom by decode_image_rgba.
    # The Tower Bloxx texture coordinates already match that row order; applying
    # an extra (1-v) here vertically flips the hook and every textured tower mesh.
    texture = TextureRGBA(
        width=2,
        height=2,
        rgba=bytes(
            (
                255, 0, 0, 255,      0, 255, 0, 255,      # top row
                0, 0, 255, 255,      255, 255, 0, 255,    # bottom row
            )
        ),
    )

    assert sample_texture_nearest(texture, 0.0, 0.0) == (255, 0, 0, 255)
    assert sample_texture_nearest(texture, 1.0, 0.0) == (0, 255, 0, 255)
    assert sample_texture_nearest(texture, 0.0, 1.0) == (0, 0, 255, 255)
    assert sample_texture_nearest(texture, 1.0, 1.0) == (255, 255, 0, 255)


def test_special_cable_export_is_one_continuous_affine_line_source(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "project"
    export_gba_project_assets(tower_bloxx_jar, project)

    bmp = project / "gba/graphics/gameplay/crane_special_cable_segment.bmp"
    image = Image.open(bmp)
    assert image.size == (32, 64)
    assert image.mode == "P"

    data = list(image.get_flattened_data())
    for y in range(64):
        row = data[y * 32 : (y + 1) * 32]
        assert [index for index, value in enumerate(row) if value] == [15, 16]



def test_recovered_special_cable_length_fits_one_doubled_64px_affine_canvas() -> None:
    # Special-rig crane Y follows camera+1920-rope+vertical_component. The cable
    # endpoint is q+528 projected at 22 px per 256 fixed units. Enumerate the
    # full recovered raising/attached envelope and both horizontal extremes.
    max_length_sq = 0
    for rope_length in range(0, 1665):
        for vertical_component in (-64, 64):
            end_y = -(((1920 - rope_length + vertical_component + 528) * 22) // 256)
            for end_x in (-11, 11):
                dx = end_x
                dy = end_y - (-165)
                max_length_sq = max(max_length_sq, dx * dx + dy * dy)

    # Butano/GBA affine sprites can use at most a doubled canvas. A 64px source
    # line therefore has a 128px maximum draw extent, comfortably above this.
    assert max_length_sq <= 128 * 128

def test_runtime_special_cable_uses_one_affine_sprite_and_exact_source_anchor() -> None:
    root = Path(__file__).resolve().parents[1]
    for relative in ("gba/src/quick_game_scene.cpp", "gba/src/tower_construction_scene.cpp"):
        source = (root / relative).read_text(encoding="utf-8")

        # Butano sprite coordinates are screen-centred, so source Java x=screen/2
        # is x=0 here. The source y anchor is 165 px above screen centre:
        # -(22 * (2432 - 512) >> 8) = -165.
        assert "constexpr int start_x = 0;" in source
        assert "constexpr int start_y = -165;" in source
        assert "const int end_x = _screen_x(snapshot.crane_x);" in source
        assert "const int end_y = _screen_y(snapshot.crane_y + 528, snapshot.presentation_camera_y);" in source

        # One affine 32x64 line replaces the segmented 8x16 chain. Java2D's
        # drawLine includes both endpoints, so the GBA affine replacement adds
        # a tiny overlap to survive fixed-point sampling at the V-sling join.
        assert "const int dx = end_x - start_x;" in source
        assert "const int dy = end_y - start_y;" in source
        assert "const int cable_length = bn::sqrt(dx * dx + dy * dy);" in source
        assert "constexpr int endpoint_overlap = 3;" in source
        assert "const int cable_draw_length = cable_length + endpoint_overlap * 2;" in source
        assert "sprite.set_vertical_scale(bn::fixed(cable_draw_length) / 64);" in source
        assert "sprite.set_rotation_angle_safe(bn::degrees_atan2(dx, dy));" in source
        assert "segment_count" not in source
