from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets


ROOT = Path(__file__).resolve().parents[1]


def test_root_menu_renders_left_icon_for_all_six_rows() -> None:
    source = (ROOT / "gba/src/ui_shell.cpp").read_text(encoding="utf-8")
    assert "generated::menu_continue_icon" in source
    assert "generated::menu_build_city_icon" in source
    assert "generated::menu_quick_game_icon" in source
    assert "generated::menu_high_scores_icon" in source
    assert "generated::menu_instructions_icon" in source
    assert "generated::menu_settings_icon" in source


def test_menu_confirmation_scenes_use_shared_dialog_window() -> None:
    header = (ROOT / "gba/include/tb/ui_shell.h").read_text(encoding="utf-8")
    source = (ROOT / "gba/src/ui_shell.cpp").read_text(encoding="utf-8")

    assert "void _show_dialog_backdrop();" in header
    assert "void UiShell::_show_dialog_backdrop()" in source

    for function in (
        "_show_overwrite_confirm",
        "_show_reset_city_confirm",
        "_show_clear_high_scores_confirm",
    ):
        start = source.index(f"void UiShell::{function}")
        next_function = source.find("\nvoid UiShell::", start + 1)
        body = source[start : next_function if next_function >= 0 else len(source)]
        assert "_show_dialog_backdrop();" in body


def test_custom_high_scores_and_instructions_icons_are_generated(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "project"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)
    icon_names = {record["name"] for record in manifest["menu_assets"]["icons"]}
    assert "menu_high_scores_icon" in icon_names
    assert "menu_instructions_icon" in icon_names

    for name in ("menu_high_scores_icon_p0", "menu_instructions_icon_p0"):
        bmp = project / "gba/graphics/ui" / f"{name}.bmp"
        metadata = project / "gba/graphics/ui" / f"{name}.json"
        assert bmp.exists()
        assert metadata.exists()
        image = Image.open(bmp)
        assert image.size == (16, 16)
        assert image.mode == "P"
        used = set(image.get_flattened_data())
        assert 0 in used
        assert len(used - {0}) >= 2


def test_shared_dialog_window_asset_is_generated_and_used_in_all_modal_flows(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "project_dialog"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)
    procedural = set(manifest["procedural_assets"])
    assert "dialog_window" in procedural

    header = (project / "gba/include/generated/tower_ui_assets.h").read_text(encoding="utf-8")
    assert "dialog_window_parts" in header

    for relative in (
        "gba/src/ui_shell.cpp",
        "gba/src/build_city_scene.cpp",
        "gba/src/tower_construction_scene.cpp",
    ):
        source = (ROOT / relative).read_text(encoding="utf-8")
        assert "generated::dialog_window" in source


def _palette16(path: Path) -> tuple[int, ...]:
    image = Image.open(path)
    palette = image.getpalette()
    assert palette is not None
    return tuple(palette[: 16 * 3])


def test_root_icons_and_dialog_reuse_existing_shared_palettes(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    project = tmp_path / "project_palettes"
    export_gba_ui_assets(tower_bloxx_jar, project)
    graphics = project / "gba/graphics/ui"

    icon_assets = (
        "menu_continue_icon_p0.bmp",
        "menu_build_city_icon_p0.bmp",
        "menu_quick_game_icon_p0.bmp",
        "menu_settings_icon_p0.bmp",
        "menu_exit_icon_p0.bmp",
        "menu_high_scores_icon_p0.bmp",
        "menu_instructions_icon_p0.bmp",
    )
    assert len({_palette16(graphics / name) for name in icon_assets}) == 1
    assert _palette16(graphics / "dialog_window_p0.bmp") == _palette16(
        graphics / "city_comparison_panel_active_p0.bmp"
    )


def test_overwrite_warning_uses_generated_wrapped_lines() -> None:
    source = (ROOT / "gba/src/ui_shell.cpp").read_text(encoding="utf-8")
    assert "overwrite_game_confirmation_lines_line_counts" in source
    assert "overwrite_game_confirmation_lines[language][index]" in source
