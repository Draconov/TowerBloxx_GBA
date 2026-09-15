from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.gba_project_export import export_gba_project_assets
from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets

ROOT = Path(__file__).resolve().parents[1]


def _palette16(path: Path) -> tuple[int, ...]:
    image = Image.open(path)
    palette = image.getpalette()
    assert palette is not None
    return tuple(palette[: 16 * 3])


def test_quick_game_live_hud_states_share_palette_headroom(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "ui"
    export_gba_ui_assets(tower_bloxx_jar, project)
    graphics = project / "gba/graphics/ui"

    # With the 128-color yellow-tower BPP8 palette live, Quick Game only has
    # eight OBJ BPP4 banks left.  A miss shows both frame 6 (remaining chance)
    # and frame 8 (spent chance), while a combo also needs resource 15.  Keep
    # these exact-color assets on one shared bank instead of allocating three.
    shared = _palette16(graphics / "quick_counter_frame_p0.bmp")
    for name in (
        "hud_brown_digit_f0_p0.bmp",
        "hud_brown_digit_f11_p0.bmp",
        "hud_state_indicator_f6_p0.bmp",
        "hud_state_indicator_f8_p0.bmp",
    ):
        assert _palette16(graphics / name) == shared


def test_quick_game_crane_platform_family_uses_one_bpp4_palette(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "gameplay"
    export_gba_project_assets(tower_bloxx_jar, project)
    graphics = project / "gba/graphics/gameplay"

    shared = _palette16(graphics / "tb_mesh_007_p0.bmp")
    for name in (
        "crane_special_cable_segment.bmp",
        "crane_hook_pose_00_p0.bmp",
        "crane_hook_pose_00_p1.bmp",
        "tb_mesh_009_p0.bmp",
        "tb_mesh_009_p1.bmp",
        "tb_mesh_009_p2.bmp",
        "tb_mesh_009_p3.bmp",
    ):
        assert _palette16(graphics / name) == shared


def test_special_cable_rotates_toward_sling_and_uses_double_size_canvas() -> None:
    for relative in ("gba/src/quick_game_scene.cpp", "gba/src/tower_construction_scene.cpp"):
        source = (ROOT / relative).read_text(encoding="utf-8")
        start = source.index("::_rebuild_special_cable(")
        end = source.index("::_update_world_positions", start)
        body = source[start:end]

        # The old -dx mirrors the cable around screen centre: a left-side sling
        # gets a right-leaning cable, exactly like the emulator screenshot.
        assert "bn::degrees_atan2(dx, dy)" in body
        assert "bn::degrees_atan2(-dx, dy)" not in body
        assert "bn::sprite_double_size_mode::ENABLED" in body
