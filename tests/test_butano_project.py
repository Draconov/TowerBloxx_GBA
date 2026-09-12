from __future__ import annotations

import json
from pathlib import Path


def _root() -> Path:
    return Path(__file__).resolve().parents[1]


def test_butano_makefile_is_pinned_and_has_gba_metadata() -> None:
    makefile = (_root() / "gba" / "Makefile").read_text()
    assert "TARGET" in makefile and "TowerBloxxGBA" in makefile
    assert "LIBBUTANO" in makefile
    assert "LIBBUTANO       ?= ../../butano/butano" in makefile
    assert "ROMTITLE" in makefile and "TOWER BLOXX" in makefile
    assert "ROMCODE" in makefile and "TBGA" in makefile
    assert "GRAPHICS" in makefile and "graphics/generated" in makefile
    assert "graphics/ui" in makefile
    assert "include $(LIBBUTANOABS)/butano.mak" in makefile


def test_runtime_boots_butano_and_uses_fresh_input_pipeline() -> None:
    main = (_root() / "gba" / "src" / "main.cpp").read_text()
    assert "bn::core::init()" in main
    assert "bn::core::update()" in main
    assert "bn::keypad::left_held()" in main
    assert "app.update_input" in main
    assert "UiShell" in main
    assert "ui.update" in main
    assert "store_save(save)" in main

    gallery = (_root() / "gba" / "src" / "tower_gallery.cpp").read_text()
    assert 'generated/tower_mesh_assets.h' in gallery
    assert "generated::meshes" in gallery
    assert "input.pressed(Key::Left)" in gallery
    assert "input.pressed(Key::Right)" in gallery


def test_phase4_ui_shell_uses_original_assets_and_localization() -> None:
    root = _root()
    shell_h = (root / "gba" / "include" / "tb" / "ui_shell.h").read_text()
    shell = (root / "gba" / "src" / "ui_shell.cpp").read_text()
    main = (root / "gba" / "src" / "main.cpp").read_text()

    assert "bn::sprite_text_generator" in shell_h or "bn::sprite_text_generator" in shell
    assert 'generated/tower_font.h' in shell
    assert 'generated/tower_localization.h' in shell
    assert 'generated/tower_ui_assets.h' in shell
    assert "tower_bloxx_logo" in shell
    assert "sumea_logo" in shell
    assert "localized_strings" in shell
    assert "quick_game_instruction_lines" in shell
    assert "build_city_instruction_lines" in shell
    assert "UiScene::Settings" in shell
    assert "UiScene::InstructionsPage" in shell
    assert "UiScene::About" in shell
    assert "save_dirty" in main
    assert "store_save(save)" in main

    forbidden = ("Vibration", "Backlight", "Get More Games", "SMS", "license", "Game Lobby")
    for token in forbidden:
        assert token not in shell


def test_sram_access_is_centralized() -> None:
    src_dir = _root() / "gba" / "src"
    save_store = (src_dir / "save_store.cpp").read_text()
    assert "bn::sram::read" in save_store
    assert "bn::sram::write" in save_store
    for path in src_dir.glob("*.cpp"):
        if path.name != "save_store.cpp":
            assert "bn::sram::" not in path.read_text()


def test_generated_mesh_assets_are_checked_in_and_traceable() -> None:
    root = _root()
    manifest_path = root / "gba" / "reference" / "generated_assets_manifest.json"
    manifest = json.loads(manifest_path.read_text())
    assert manifest["butano_version"] == "21.7.1"
    assert manifest["mesh_count"] == 19
    assert len(manifest["files"]) > 19
    for record in manifest["files"]:
        assert (root / record["path"]).is_file()


def test_gba_readme_documents_exact_external_build_requirements() -> None:
    readme = (_root() / "gba" / "README.md").read_text()
    assert "Butano 21.7.1" in readme
    assert "devkitARM" in readme
    assert "LIBBUTANO" in readme
    assert "make -j" in readme


def test_phase5_quick_game_scene_consumes_platform_neutral_simulation() -> None:
    root = _root()
    scene_h_path = root / "gba" / "include" / "tb" / "quick_game_scene.h"
    scene_cpp_path = root / "gba" / "src" / "quick_game_scene.cpp"
    assert scene_h_path.is_file()
    assert scene_cpp_path.is_file()

    scene_h = scene_h_path.read_text()
    scene = scene_cpp_path.read_text()
    main = (root / "gba" / "src" / "main.cpp").read_text()

    assert 'tb/quick_game.h' in scene_h or 'tb/quick_game.h' in scene
    assert "QuickGame _game" in scene_h
    assert "_game.update" in scene
    assert "snapshot()" in scene
    assert 'generated/tower_mesh_assets.h' in scene
    assert 'generated/tower_font.h' in scene_h or 'generated/tower_font.h' in scene
    assert 'generated/tower_localization.h' in scene
    assert "floors" in scene.lower()
    assert "chances" in scene.lower()

    assert "QuickGameScene" in main
    assert "GameRequest::QuickGame" in main
    assert "pending_game_request()" in main
    assert "clear_game_request()" in main
    assert "quick_game.update" in main

    # Gameplay physics remains in quick_game.cpp, not duplicated in the Butano renderer.
    forbidden_renderer_physics = ("elapsed_ms * elapsed_ms", "landing_max_abs_offset", "period_table")
    for token in forbidden_renderer_physics:
        assert token not in scene


def test_phase5_quick_game_scene_uses_fresh_a_and_b_exit_contract() -> None:
    scene = (_root() / "gba" / "src" / "quick_game_scene.cpp").read_text()
    assert "input.pressed(Key::B)" in scene
    assert "InputFrame" in scene
    assert "16" in scene and "17" in scene  # deterministic 60 Hz millisecond cadence


def test_phase6_quick_game_hud_results_and_record_persistence_are_wired() -> None:
    root = _root()
    scene_h = (root / "gba" / "include" / "tb" / "quick_game_scene.h").read_text()
    scene = (root / "gba" / "src" / "quick_game_scene.cpp").read_text()
    main = (root / "gba" / "src" / "main.cpp").read_text()

    assert "population" in scene.lower()
    assert "combo" in scene.lower()
    assert "QuickGameStatus::Results" in scene
    assert "apply_quick_result" in scene
    assert "QuickRecordFlags" in scene_h or "QuickRecordFlags" in scene
    assert "save_dirty" in scene_h and "save_dirty" in main
    assert "localized_strings[_language][93]" in scene
    assert "localized_strings[_language][94]" in scene
    assert "localized_strings[_language][95]" in scene
    assert "localized_strings[_language][96]" in scene
    assert "snapshot.target_floors" not in scene
    assert "floors_text.append('/')" not in scene
    assert "store_save(save)" in main
