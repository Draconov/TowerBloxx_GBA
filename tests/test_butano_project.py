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
    assert "GRAPHICS" in makefile and "graphics/gameplay" in makefile
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


def test_main_menu_uses_reference_jar_compositor_instead_of_placeholder_shell() -> None:
    root = _root()
    shell_h = (root / "gba/include/tb/ui_shell.h").read_text()
    shell = (root / "gba/src/ui_shell.cpp").read_text()

    # The captured v1.5.22 JAR menu is a composed sky scene: logo, yellow
    # selection band/red text, source icons and the blue worker animation.
    assert "_selected_text_generator" in shell_h
    assert "bn::optional<bn::regular_bg_ptr> _background" in shell_h
    assert "bn_regular_bg_items_menu_bg.h" in shell
    assert "bn::regular_bg_items::menu_bg.create_bg" in shell
    assert "generated::tower_bloxx_logo" in shell
    assert "generated::menu_highlight" in shell
    assert "generated::menu_build_city_icon" in shell
    assert "generated::menu_quick_game_icon" in shell
    assert "generated::menu_settings_icon" in shell
    assert "generated::menu_worker_blue_f0" in shell
    assert "generated::menu_worker_red_f0" in shell
    assert "menu_worker_blue_frames" in shell
    assert "MenuWorkerField _menu_workers" in shell_h
    assert 'generate(-96, y, ">"' not in shell


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


def test_phase7_quick_game_scene_applies_group_affine_presentation_pose() -> None:
    root = _root()
    scene_h = (root / "gba" / "include" / "tb" / "quick_game_scene.h").read_text()
    scene = (root / "gba" / "src" / "quick_game_scene.cpp").read_text()

    assert 'bn_sprite_affine_mat_ptr.h' in scene_h
    assert "bn::sprite_affine_mat_ptr" in scene_h
    assert "_floor_affine_mats" in scene_h
    assert "_current_affine_mat" in scene_h
    assert "max_visible_floors = 5" in scene
    assert "floor_render_pose" in scene
    assert "presentation_camera_y" in scene
    assert "current_z_angle_degrees" in scene
    assert "set_affine_mat" in scene
    assert "set_rotation_angle" in scene
    assert "degrees_lut_sin_and_cos_safe" in scene

    # Recovered presentation fields are consumed only by rendering; physics constants stay in the core.
    for forbidden in ("29491200", "58982400", "_tower_instability", "camera_impact_ms"):
        assert forbidden not in scene


def test_phase8_build_city_scene_uses_original_city_art_and_core() -> None:
    root = _root()
    scene_h = root / "gba" / "include" / "tb" / "build_city_scene.h"
    scene_cpp = root / "gba" / "src" / "build_city_scene.cpp"
    assert scene_h.is_file()
    assert scene_cpp.is_file()

    header = scene_h.read_text()
    scene = scene_cpp.read_text()
    main = (root / "gba" / "src" / "main.cpp").read_text()

    assert 'generated/tower_ui_assets.h' in header
    assert "BuildCity _city" in header
    assert "BuildCitySnapshot" in scene
    assert "generated::city_building_1_f0" in scene
    assert "generated::city_building_4_f3" in scene
    assert "generated::city_lot_f" in scene
    assert "generated::city_status_icon_f0" in scene
    assert "generated::hud_brown_digit_f0" in scene
    assert "generated::city_panel_f0" in scene
    assert "generated::city_effect_f0" in scene
    assert "placement_valid" in scene
    assert "_show_composite(*lot_assets[0], x, y)" not in scene
    assert "construction_request" in scene
    assert "BuildCityScene" in main
    assert "GameRequest::BuildCity" in main
    assert "build_city.update" in main

    # City rules remain in build_city.cpp, not duplicated in the Butano renderer.
    for forbidden in ("19000", "2200", "TARGET_HEIGHTS", "placement_capability"):
        assert forbidden not in scene


def test_phase9_build_city_uses_dedicated_tower_construction_scene_and_roof_assets() -> None:
    root = _root()
    header_path = root / "gba" / "include" / "tb" / "tower_construction_scene.h"
    source_path = root / "gba" / "src" / "tower_construction_scene.cpp"
    assert header_path.is_file()
    assert source_path.is_file()

    header = header_path.read_text()
    source = source_path.read_text()
    main = (root / "gba" / "src" / "main.cpp").read_text()

    assert "TowerConstruction _construction" in header
    assert "BuildCityConstructionRequest" in header
    assert "TowerConstructionSnapshot" in source
    assert "generated::meshes" in source
    assert "normal_floor_mesh_id" in source
    assert "30 +" in source or "normal_roof_mesh_id" in source
    assert "40 +" in source or "trophy_roof_mesh_id" in source
    assert "snapshot.roof_phase" in source
    assert "result.roof" in source or "roof_result" in source
    assert "QuickGame _game" not in header
    assert "QuickGame _game" not in source
    assert "mesh_id = 20" not in source

    assert "TowerConstructionScene" in main
    assert "construction.active()" in main
    assert "build_city.construction_request()" in main
    assert "build_city.clear_construction_request()" in main
    assert "build_city.accept_constructed_tower" in main


def test_phase9_construction_handoff_does_not_persist_until_city_placement() -> None:
    root = _root()
    main = (root / "gba" / "src" / "main.cpp").read_text()
    scene = (root / "gba" / "src" / "tower_construction_scene.cpp").read_text()

    assert "TowerConstructionScene construction" in main
    assert "if(construction.active())" in main
    assert "TowerConstructionSceneUpdateResult result = construction.update(input)" in main
    assert "build_city.accept_constructed_tower(result.building_type, result.population, result.roof)" in main
    assert "build_city.clear_construction_request()" in main
    assert "construction.start(request, controller.language())" in main
    assert "store_save" not in scene

    construction_branch = main.split("if(construction.active())", 1)[1].split("else if(quick_game.active())", 1)[0]
    assert "store_save(save)" not in construction_branch


def test_playability_backdrops_are_scene_owned_and_never_boot_black() -> None:
    root = _root()
    ui = (root / "gba/src/ui_shell.cpp").read_text()
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    city = (root / "gba/src/build_city_scene.cpp").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()
    backdrop = root / "gba/include/tb/scene_backdrop.h"

    assert backdrop.is_file()
    backdrop_text = backdrop.read_text()
    assert "bn::color(19, 25, 29)" in backdrop_text
    assert "bn::color(22, 26, 30)" in backdrop_text

    for scene in (ui, quick, city, construction):
        assert "set_transparent_color(bn::color(0, 0, 0))" not in scene

    assert "set_ui_backdrop();" in ui
    assert "set_gameplay_backdrop();" in quick
    assert "set_city_backdrop();" in city
    assert "set_gameplay_backdrop();" in construction


def test_playability_gameplay_uses_runtime_mesh_ids_and_camera_center_projection() -> None:
    root = _root()
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()

    # Quick Game bytecode forces L=4, so its ordinary block mesh is user ID 13.
    assert "constexpr int floor_mesh_id = 13;" in quick

    # Mesh 9 is the world/base platform, not a crane-top sprite. Mesh 8 is the
    # normal translated crane line/hook object.
    for source in (quick, construction):
        assert "constexpr int platform_mesh_id = 9;" in source
        assert "constexpr int crane_hook_mesh_id = 8;" in source
        assert "crane_top_mesh_id" not in source
        assert "world_screen_baseline_y = 0" in source
        assert "_platform_sprites" in source

    for header_name in ("quick_game_scene.h", "tower_construction_scene.h"):
        header = (root / "gba/include/tb" / header_name).read_text()
        assert "bn::vector<bn::sprite_ptr, 4> _platform_sprites;" in header


def test_playability_makefile_compiles_gameplay_assets_not_inspection_renders() -> None:
    makefile = (_root() / "gba/Makefile").read_text()
    assert "GRAPHICS        := graphics/gameplay graphics/ui" in makefile
    assert "graphics/generated graphics/ui" not in makefile


def test_playability_fix3_uses_original_scene_background_items() -> None:
    root = _root()
    makefile = (root / "gba/Makefile").read_text()
    quick_h = (root / "gba/include/tb/quick_game_scene.h").read_text()
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    city_h = (root / "gba/include/tb/build_city_scene.h").read_text()
    city = (root / "gba/src/build_city_scene.cpp").read_text()
    construction_h = (root / "gba/include/tb/tower_construction_scene.h").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()

    assert "graphics/backgrounds" in makefile
    assert "bn::optional<bn::regular_bg_ptr> _background" in quick_h
    assert "bn_regular_bg_items_construction_bg.h" in quick
    assert "bn::regular_bg_items::construction_bg.create_bg" in quick
    assert "bn::optional<bn::regular_bg_ptr> _background" in construction_h
    assert "bn_regular_bg_items_construction_bg.h" in construction
    assert "bn::regular_bg_items::construction_bg.create_bg" in construction
    assert "bn::optional<bn::regular_bg_ptr> _background" in city_h
    assert "bn_regular_bg_items_city_bg.h" in city
    assert "bn::regular_bg_items::city_bg.create_bg" in city


def test_playability_fix3_crane_uses_recovered_affine_orientation() -> None:
    root = _root()
    for header_name, source_name in (
        ("quick_game_scene.h", "quick_game_scene.cpp"),
        ("tower_construction_scene.h", "tower_construction_scene.cpp"),
    ):
        header = (root / "gba/include/tb" / header_name).read_text()
        source = (root / "gba/src" / source_name).read_text()
        assert "bn::sprite_affine_mat_ptr _crane_affine_mat" in header
        assert "snapshot.crane_angle_degrees" in source
        # Java M3G uses world +Z rotation with a Y-up projection. GBA sprite
        # coordinates are Y-down, so the visible affine angle is negated.
        assert "-snapshot.crane_angle_degrees" in source
        assert "_crane_affine_mat" in source


def test_playability_fix3_maxmod_audio_routing_is_wired() -> None:
    root = _root()
    makefile = (root / "gba/Makefile").read_text()
    main = (root / "gba/src/main.cpp").read_text()
    audio_h = root / "gba/include/tb/game_audio.h"
    audio_cpp = root / "gba/src/game_audio.cpp"

    assert "AUDIO           := audio" in makefile
    assert "AUDIOBACKEND    := maxmod" in makefile
    assert audio_h.is_file() and audio_cpp.is_file()
    source = audio_cpp.read_text()
    for name in ("menu_theme", "tower_theme", "city_theme", "construction_fail", "normal_roof", "trophy_roof"):
        assert f"bn::music_items::{name}" in source
    assert "GameAudio audio" in main
    assert "audio.update" in main
    assert "audio.play_construction_result" in main


def test_fix6_menu_worker_uses_descending_reference_field_not_fixed_reel() -> None:
    root = _root()
    shell_h = (root / "gba/include/tb/ui_shell.h").read_text()
    shell = (root / "gba/src/ui_shell.cpp").read_text()
    assert 'tb/menu_workers.h' in shell_h
    assert "MenuWorkerField _menu_workers" in shell_h
    assert "_menu_workers.update" in shell
    assert "generated::menu_worker_blue_f0" in shell
    assert "generated::menu_worker_red_f0" in shell
    assert "int _menu_worker_frame =" not in shell_h
    assert "int _menu_worker_tick =" not in shell_h
    assert "(_menu_worker_frame + 1) % 10" not in shell
    assert "*menu_worker_frames" not in shell
    assert "worker.y_fixed >= _menu_workers.height_fixed()" in shell


def test_fix6_quick_and_construction_huds_use_reference_texture_assets() -> None:
    root = _root()
    quick = (root / "gba/src/quick_game_scene.cpp").read_text()
    construction = (root / "gba/src/tower_construction_scene.cpp").read_text()

    assert "generated::quick_counter_frame" in quick
    assert "generated::hud_white_digit_f0" in quick
    assert "generated::hud_brown_digit_f0" in quick
    assert "generated::hud_population_icon" in quick
    assert "generated::hud_state_indicator_f6" in quick
    assert "floors_text" not in quick
    assert "chances_text" not in quick

    assert "generated::construction_target_badge_f0" in construction
    assert "generated::hud_state_indicator_f0" in construction
    assert "value_line(generated::localized_strings[_language][80]" not in construction
    assert "bn::string<8> chances" not in construction


def test_fix7_crane_and_build_city_use_separate_reference_state_and_compositor_assets() -> None:
    root = _root()
    quick_h = (root / "gba/include/tb/quick_game.h").read_text()
    quick_scene = (root / "gba/src/quick_game_scene.cpp").read_text()
    construction_scene = (root / "gba/src/tower_construction_scene.cpp").read_text()
    city_scene = (root / "gba/src/build_city_scene.cpp").read_text()

    # Crane state survives release independently from the ballistic block.
    assert "int crane_x" in quick_h
    assert "int crane_y" in quick_h
    assert "snapshot.crane_x" in quick_scene
    assert "snapshot.crane_y" in quick_scene
    assert "snapshot.crane_x" in construction_scene
    assert "snapshot.crane_y" in construction_scene

    # Build City uses the recovered JAR compositor instead of the old text/placeholder shell.
    for asset in (
        "city_status_icon_f0",
        "hud_brown_digit_f0",
        "city_panel_f0",
        "city_building_1_f3",
        "city_lot_f3",
        "city_effect_f0",
    ):
        assert f"generated::{asset}" in city_scene
    assert "grid_screen_left = 89" in city_scene
    assert "grid_screen_top = 32" in city_scene
    assert "grid_spacing = 17" in city_scene
    assert "localized_strings[_language][92]" not in city_scene  # no fake Build City title over the board
    assert "value_line(generated::localized_strings[_language][82]" not in city_scene
