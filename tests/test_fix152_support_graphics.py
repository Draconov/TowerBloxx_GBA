from __future__ import annotations

from pathlib import Path

from PIL import Image

from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets

ROOT = Path(__file__).resolve().parents[1]


def test_resource1_navigation_and_resource9_publisher_logo_are_exported(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    export_gba_ui_assets(tower_bloxx_jar, tmp_path)
    ui = tmp_path / "gba/graphics/ui"
    for frame in range(3):
        image = Image.open(ui / f"support_nav_f{frame}_p0.bmp")
        assert image.size == (8, 8)
    assert (ui / "digital_chocolate_logo_p0.bmp").is_file()


def test_paged_support_screens_use_source_up_down_arrows_not_text_chevrons() -> None:
    source = (ROOT / "gba/src/ui_shell.cpp").read_text(encoding="utf-8")
    assert '"<  >"' not in source
    assert "generated::support_nav_f0" in source
    assert "generated::support_nav_f1" in source


def test_startup_opens_directly_on_title_and_title_accepts_any_button() -> None:
    header = (ROOT / "gba/include/tb/ui_controller.h").read_text(encoding="utf-8")
    controller = (ROOT / "gba/src/ui_controller.cpp").read_text(encoding="utf-8")
    shell = (ROOT / "gba/src/ui_shell.cpp").read_text(encoding="utf-8")
    assert "PublisherSplash" in header
    assert "UiScene _scene = UiScene::Title;" in header
    assert "bool any_pressed(const InputFrame& input)" in controller
    assert "case UiScene::PublisherSplash: _set_scene(UiScene::Title); break;" in controller
    assert "case UiScene::Title:" in controller
    assert "if(any_pressed(input))" in controller
    assert "Press any key to play" in shell
