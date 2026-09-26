from __future__ import annotations

from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
GBA = ROOT / "gba"
WORKFLOW = ROOT / ".github" / "workflows" / "gba-release.yml"
PACKAGER = ROOT / "scripts" / "ci" / "package_rom.sh"

CPP_SOURCES = [
    "gba/src/app_state.cpp",
    "gba/src/build_city.cpp",
    "gba/src/build_city_events.cpp",
    "gba/src/hall_of_fame.cpp",
    "gba/src/menu_clouds.cpp",
    "gba/src/quick_game.cpp",
    "gba/src/save_data.cpp",
    "gba/src/tower_construction.cpp",
    "gba/src/tower_session.cpp",
    "gba/src/ui_controller.cpp",
]


def test_gameplay_cpp_compiles_and_runs(tmp_path: Path) -> None:
    output = tmp_path / "towerbloxx_gameplay_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-DTB_HOST_TEST", "-I", str(GBA / "include"),
        *[str(ROOT / source) for source in CPP_SOURCES],
        str(ROOT / "tests" / "test_gameplay.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=ROOT, capture_output=True, text=True, check=False)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "towerbloxx permanent gameplay regressions ok\n"


def test_release_repo_has_no_archaeology() -> None:
    forbidden = [ROOT / "reference", ROOT / "tools", GBA / "reference", GBA / "graphics" / "generated"]
    assert all(not path.exists() for path in forbidden)
    files = [path for path in ROOT.rglob("*") if path.is_file()]
    lower_names = [path.name.lower() for path in files]
    assert not any(name.endswith(".jar") for name in lower_names)
    assert not any("parity" in name or "extract" in name or "m3g_analysis" in name for name in lower_names)


def test_only_two_permanent_test_sources_remain() -> None:
    files = sorted(
        path.relative_to(ROOT).as_posix()
        for path in (ROOT / "tests").rglob("*")
        if path.is_file() and path.suffix in {".py", ".cpp"}
    )
    assert files == ["tests/test_gameplay.cpp", "tests/test_repo.py"]


def test_tower_gallery_is_removed() -> None:
    assert not (GBA / "src" / "tower_gallery.cpp").exists()
    assert not (GBA / "include" / "tb" / "tower_gallery.h").exists()
    source_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in list((GBA / "src").rglob("*.cpp")) + list((GBA / "include").rglob("*.h"))
    )
    assert "TowerGallery" not in source_text
    assert "tower_gallery" not in source_text


def test_yellow_crane_boom_is_live_and_split_for_gba_obj_limits() -> None:
    parts = [
        GBA / "graphics" / "gameplay" / "crane_special_boom_p0.bmp",
        GBA / "graphics" / "gameplay" / "crane_special_boom_p1.bmp",
        GBA / "graphics" / "gameplay" / "crane_special_boom_p2.bmp",
    ]
    assert all(path.is_file() for path in parts)
    sizes = []
    for path in parts:
        with Image.open(path) as image:
            sizes.append(image.size)
            assert image.mode == "P"
    assert sizes == [(64, 32), (64, 32), (32, 32)]

    # Reconstruct the unpadded 151x31 source image and lock its visible pixels
    # without keeping the original JAR or extraction artifact in the repository.
    rebuilt = Image.new("RGBA", (151, 31), (255, 255, 255, 0))
    x = 0
    for path, width in zip(parts, (64, 64, 23)):
        with Image.open(path) as image:
            palette = image.getpalette() or []
            rgba = Image.new("RGBA", image.size, (255, 255, 255, 0))
            pixels = []
            for value in image.tobytes():
                if value == 0:
                    pixels.append((255, 255, 255, 0))
                else:
                    base = value * 3
                    pixels.append(tuple(palette[base:base + 3]) + (255,))
            rgba.putdata(pixels)
            rebuilt.alpha_composite(rgba.crop((0, 0, width, 31)), (x, 0))
        x += width
    assert hashlib.sha256(rebuilt.tobytes()).hexdigest() == "14d9413229e787245e6c97d7d1bdab6b45b33b06772eb15ccd8620a8504ae73e"

    scene = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    header = (GBA / "include" / "tb" / "tower_construction_scene.h").read_text(encoding="utf-8")
    for index in range(3):
        assert f"bn_sprite_items_crane_special_boom_p{index}.h" in scene
    assert "CranePresentationMode::Special" in scene
    assert "_special_boom_sprites" in scene
    assert "_special_boom_sprites" in header


def test_first_block_intro_contract_is_permanent() -> None:
    test = (ROOT / "tests" / "test_gameplay.cpp").read_text(encoding="utf-8")
    assert "TowerConstructionBlockState::Attached" in test
    assert "snapshot.rope_length == 1664" in test
    assert "snapshot.camera_y == 2432" in test
    assert "snapshot.camera_target_y == 512" in test
    assert "elapsed_ms >= 3400" in test
    assert "TowerConstructionBlockState::Falling" in test


def test_construction_backgrounds_share_palette() -> None:
    background_root = GBA / "graphics" / "backgrounds"
    names = [f"construction_sky_{index:02d}" for index in range(17)] + [
        f"construction_scenery_{index}" for index in range(3)
    ]
    images = [Image.open(background_root / f"{name}.bmp") for name in names]
    palettes = [tuple(image.getpalette() or ()) for image in images]
    assert all(palette == palettes[0] for palette in palettes[1:])
    for image in images[:17]:
        assert 0 not in image.tobytes()


def test_combo_feedback_contract_is_retained() -> None:
    for source_name in ("quick_game_scene.cpp", "tower_construction_scene.cpp"):
        source = (GBA / "src" / source_name).read_text(encoding="utf-8")
        assert "snapshot.combo_count" in source
        assert "combo_bonus_pending" in source
        assert "combo_meter_ms > -2000" in source
        assert "(-snapshot.combo_meter_ms / 100) % 2 == 0" in source
        assert "hud_brown_digit_frames[10]" in source
        # House.i(Graphics) draws the combo meter after the mode-specific HUD
        # branch, so the meter is common to Quick Game and Build City.
        assert "quick_combo_meter_frame" in source
        assert "_update_combo_meter(snapshot)" in source

    construction_header = (GBA / "include" / "tb" / "tower_construction_scene.h").read_text(encoding="utf-8")
    assert "_combo_meter_fill_sprite" in construction_header
    assert "_combo_meter_flash_sprite" in construction_header
    assert "_combo_star_sprites" in construction_header


def test_build_city_top_hud_uses_valid_assets_and_wiring() -> None:
    source = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    generated = (GBA / "include" / "generated" / "tower_ui_assets.h").read_text(encoding="utf-8")

    # Keep this test structural while the Build City HUD is still being tuned.
    # Do not freeze exact X/Y positions or individual background pixels here.
    assert "_show_progress_line(snapshot)" in source
    assert "generated::city_population_icon" in source
    assert "generated::city_status_browse" in source
    assert "generated::city_status_placement" in source
    assert "generated::city_comparison_panel_active" in source
    assert "generated::city_milestone_badge_empty" in source
    assert "generated::city_milestone_badge" in source
    # JAR parity: unlocked tower selector switches to trophy-roof information
    # once that tower type's special roof is available.
    assert "snapshot.selected_building_type <= snapshot.max_trophy_building_type" in source
    assert "generated::localized_strings[_language][62]" in source
    assert "trophy_population_thresholds" in source

    # JAR parity: during the three-second placement commit, show the exact
    # population increase/no-change/decrease result instead of the placement hint.
    assert "snapshot.placement_committing && snapshot.cursor_column >= 0" in source
    assert "generated::localized_strings[_language][67]" in source
    assert "generated::localized_strings[_language][68]" in source
    assert "generated::localized_strings[_language][69]" in source
    # On an occupied lot the pulsing floor marker must remain behind its
    # already placed tower, including when that cell can be replaced.
    assert "constexpr int valid_lot_ring_z_order = 1" in source
    assert "sprite->set_z_order(valid_lot_ring_z_order)" in source
    assert "constexpr int badge_row_y = status_row_y - 1" in source
    # The English neighbor requirements fit on one centered line; longer
    # localized strings retain the original two-line fallback.
    assert "instruction[index + 1] == 'n'" in source
    assert "show_instruction(generated::localized_strings" in source
    assert "show_instruction(instruction)" in source
    assert "on_second_line && _language == 0" in source
    assert "first_line.append(second_line);" in source
    assert "_text_generator.generate_optional(0, 68, first_line" in source
    assert "_text_generator.generate_optional(0, 63, first_line" in source
    assert "_text_generator.generate_optional(0, 73, second_line" in source
    assert "constexpr int comparison_panel_row_y = comparison_row_y - 1" in source
    assert "snapshot.pending_population, 204, comparison_panel_row_y" in source
    assert "snapshot.replacement_population, 231, comparison_panel_row_y" in source
    # The comparison frames were intentionally extended DOWN one pixel.
    # Their existing top edges stay put, while the lower border now fills
    # sprite row 8 (formerly transparent) and the backdrop reaches row 12.
    with Image.open(GBA / "graphics" / "ui" / "city_comparison_panel_active_p0.bmp") as active:
        assert active.mode == "P" and active.size == (32, 16)
        assert all(active.getpixel((x, 7)) != 0 and active.getpixel((x, 8)) != 0
                   and active.getpixel((x, 9)) == 0 for x in range(24))
    for theme in range(4):
        with Image.open(GBA / "graphics" / "backgrounds" / f"city_bg_theme_{theme}.bmp") as bg:
            assert bg.mode == "P" and bg.size == (256, 256)
            for x in (182, 209):
                # The indexed city background is centered at (8, 48).
                # The placeholder interior extends through row 12, with its
                # existing border/outer fill still beginning at row 13.
                assert bg.getpixel((x + 8, 4 + 48)) == bg.getpixel((x + 8, 12 + 48))
                assert bg.getpixel((x + 8, 12 + 48)) != bg.getpixel((x + 8, 13 + 48))

    assert '#include "bn_sprite_items_city_milestone_badge_empty_p0.h"' in generated
    assert '#include "bn_sprite_items_city_milestone_badge_empty_p1.h"' in generated
    assert "city_milestone_badge_empty_parts" in generated

    for name in (
        "city_milestone_badge_p0",
        "city_milestone_badge_p1",
        "city_milestone_badge_empty_p0",
        "city_milestone_badge_empty_p1",
    ):
        with Image.open(GBA / "graphics" / "ui" / f"{name}.bmp") as image:
            assert image.mode == "P"
            assert image.size in ((32, 16), (16, 16))

    for name, x in (("city_edge_top_left_p0", 0), ("city_edge_top_right_p0", 2)):
        with Image.open(GBA / "graphics" / "ui" / f"{name}.bmp") as image:
            assert image.mode == "P"
            assert image.size == (8, 32)
            palette = image.getpalette() or []
            assert tuple(palette[3:6]) == (0, 0, 0)
            assert all(image.getpixel((x, y)) == 1 for y in range(12, 32))


def test_perfect_landing_feedback_and_continue_prompts_are_wired() -> None:
    ui = GBA / "graphics" / "ui"
    assert (ui / "combo_seam_flash_white_p0.bmp").is_file()
    assert (ui / "combo_seam_flash_white_p0.json").is_file()
    for trail in ("accuracy_trail_white_p0", "accuracy_trail_yellow_p0", "accuracy_trail_red_p0"):
        assert (ui / f"{trail}.bmp").is_file()
        assert (ui / f"{trail}.json").is_file()
        with Image.open(ui / f"{trail}.bmp") as image:
            assert image.mode == "P"
            assert image.size == (8, 8)

    generated = (GBA / "include" / "generated" / "tower_ui_assets.h").read_text(encoding="utf-8")
    assert "accuracy_trail_white" in generated
    assert "accuracy_trail_yellow" in generated
    assert "accuracy_trail_red" in generated
    with Image.open(ui / "combo_seam_flash_p0.bmp") as yellow_seam, \
         Image.open(ui / "combo_seam_flash_white_p0.bmp") as white_seam:
        # White reuses the existing HUD palette bank: the seam pixels switch
        # from palette index 13 (yellow) to index 14 (white), not to a new palette.
        assert yellow_seam.getpalette() == white_seam.getpalette()
        assert set(white_seam.get_flattened_data()) <= {0, 14}

    quick = (GBA / "src" / "quick_game_scene.cpp").read_text(encoding="utf-8")
    construction = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    build_city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    for source in (quick, construction):
        assert "_update_perfect_landing_effect" in source
        assert "accuracy_star_f0" in source
        assert "accuracy_star_f1" in source
        assert "accuracy_star_f2" in source
        assert "perfect_landing_star_trail_elapsed" in source
        assert "perfect_landing_sprite_capacity" in source or "_perfect_star_sprites" in source
        assert "_perfect_landing_seed" in source
        assert "snapshot.current_x" in source
        assert "legacy_block_sparkle_frames" in source
        assert "combo_seam_flash_white" in source

    assert "support_nav_f2" in quick
    assert "support_nav_f2" in construction
    assert "support_nav_f2" in build_city
    assert "_show_composite(generated::support_nav_f2, 0, centered_y(134), -100);" in build_city
    assert "current_roof_blink_bucket" in construction
    assert "_last_hud_roof_blink_bucket" in construction
    assert "construction_target_badge_f4" in construction
    assert "generated::city_population_icon" in build_city
    assert "generated::hud_population_icon" not in build_city
    assert "generated::city_status_browse" in build_city
    assert "generated::city_status_placement" in build_city
    assert "generated::city_status_icon_f3" not in build_city
    assert "generated::city_status_icon_f4" not in build_city

    for header_name in ("tower_construction_scene.h", "quick_game_scene.h"):
        header = (GBA / "include" / "tb" / header_name).read_text(encoding="utf-8")
        assert "bn::vector<bn::sprite_ptr, perfect_landing_sprite_capacity> _perfect_star_sprites;" in header


def test_build_city_backgrounds_remain_butano_safe() -> None:
    backgrounds = GBA / "graphics" / "backgrounds"
    for theme in range(4):
        with Image.open(backgrounds / f"city_bg_theme_{theme}.bmp") as image:
            # Butano bpp_8 backgrounds must stay indexed; exact artwork/placement
            # is intentionally not frozen while the HUD is still being adjusted.
            assert image.mode == "P"
            assert image.size == (256, 256)
            # A single uninterrupted black outer frame, including the lower
            # edge after the two-tone progress line at screen rows 17 and 18.
            assert tuple((image.getpalette() or [])[255 * 3:256 * 3]) == (0, 0, 0)
            assert all(image.getpixel((x, 48)) == 255 for x in range(8, 248))
            assert all(image.getpixel((x, 67)) == 255 for x in range(8, 248))
            assert image.getpixel((32, 65)) != 255





def test_quick_hud_assets_share_one_lossless_bpp4_palette() -> None:
    ui = GBA / "graphics" / "ui"
    names = [
        *[f"hud_white_digit_f{i}_p0" for i in range(14)],
        *[f"hud_brown_digit_f{i}_p0" for i in range(12)],
        "quick_counter_frame_p0",
        "hud_population_icon_p0",
        "quick_combo_meter_frame_p0",
        "quick_combo_meter_frame_p1",
        "quick_combo_meter_fill_p0",
        "quick_combo_meter_flash_p0",
    ]
    palettes = []
    for name in names:
        with Image.open(ui / f"{name}.bmp") as image:
            palettes.append(tuple((image.getpalette() or [])[: 16 * 3]))
    assert all(palette == palettes[0] for palette in palettes[1:])

def test_all_building_color_palette_budget_contracts() -> None:
    def palette(path: Path) -> tuple[int, ...]:
        image = Image.open(path)
        values = tuple(image.getpalette() or ())
        image.close()
        return values

    gameplay = GBA / "graphics" / "gameplay"
    # Every building color uses one BPP8 palette across its base/floor/roof
    # family, so changing phases never allocates a second 8-bit palette.
    for color in range(4):
        family = (10 + color, 20 + color, 30 + color, 40 + color)
        signatures: set[tuple[int, ...]] = set()
        for mesh_id in family:
            json_path = gameplay / f"tb_mesh_{mesh_id:03d}_p0.json"
            assert '"bpp_mode": "bpp_8"' in json_path.read_text(encoding="utf-8")
            for bmp in gameplay.glob(f"tb_mesh_{mesh_id:03d}_p*.bmp"):
                signatures.add(palette(bmp))
        assert len(signatures) == 1, f"building color {color + 1} uses multiple BPP8 palettes"

    # The special rig/cable/platform retain their established shared BPP4 bank.
    shared = palette(gameplay / "tb_mesh_007_p0.bmp")[: 16 * 3]
    for name in (
        "crane_special_cable_segment.bmp",
        "crane_hook_pose_00_p0.bmp",
        "crane_hook_pose_00_p1.bmp",
        "tb_mesh_009_p0.bmp",
        "tb_mesh_009_p1.bmp",
        "tb_mesh_009_p2.bmp",
        "tb_mesh_009_p3.bmp",
    ):
        assert palette(gameplay / name)[: 16 * 3] == shared

def test_special_crane_palette_is_released_before_falling_sprite_rebuild() -> None:
    for source_name in ("quick_game_scene.cpp", "tower_construction_scene.cpp"):
        source = (GBA / "src" / source_name).read_text(encoding="utf-8")
        update_start = source.index("::update(")
        class_name = "QuickGameScene" if source_name == "quick_game_scene.cpp" else "TowerConstructionScene"
        update_end = source.index(f"bool {class_name}::active() const", update_start)
        body = source[update_start:update_end]
        release = body.index("_special_boom_sprites.clear()")
        current = body.index("_rebuild_current_sprites(snapshot)")
        assert release < current



def test_menu_ambience_assets_and_wiring() -> None:
    ui = GBA / "graphics" / "ui"
    bg = GBA / "graphics" / "backgrounds" / "menu_bg.bmp"
    for name in (
        "menu_cloud_large_p0", "menu_cloud_large_p1", "menu_cloud_small_p0",
    ):
        assert (ui / f"{name}.bmp").is_file()
        assert (ui / f"{name}.json").is_file()

    with Image.open(bg) as image:
        assert image.size == (256, 256)
        assert len(image.getcolors(maxcolors=256) or []) <= 32

    shell = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    assert "MenuCloudField" in shell
    assert "_show_menu_clouds" in shell
    assert "_menu_sky_offset" in shell
    assert "menu_cloud_large" in shell
    assert "menu_cloud_small" in shell

def test_makefile_builds_only_live_asset_roots() -> None:
    makefile = (GBA / "Makefile").read_text(encoding="utf-8")
    assert "GRAPHICS        := graphics/gameplay graphics/ui graphics/backgrounds" in makefile
    assert "graphics/generated" not in makefile
    assert "AUDIO           := audio" in makefile


def test_manual_release_contract_and_packager(tmp_path: Path) -> None:
    workflow = WORKFLOW.read_text(encoding="utf-8")
    assert "push:" in workflow
    assert "pull_request:" in workflow
    assert "workflow_dispatch:" in workflow
    assert "version:" in workflow
    assert "required: false" in workflow
    assert 'release_tag="v${release_tag}"' in workflow
    assert '^v[0-9]+\\.[0-9]+\\.[0-9]+$' in workflow
    assert 'git tag -f "${RELEASE_TAG}" "${GITHUB_SHA}"' in workflow
    assert 'gh release upload "${RELEASE_TAG}"' in workflow
    assert "--clobber" in workflow
    assert "TowerBloxx.gba.sha256" in workflow
    assert "21.7.1" in workflow
    assert "devkitpro/devkitarm" in workflow

    gba_dir = tmp_path / "gba"
    gba_dir.mkdir()
    source_rom = gba_dir / "TowerBloxxGBA.gba"
    source_rom.write_bytes(b"towerbloxx-cleanup-test\n")
    result = subprocess.run(
        ["bash", str(PACKAGER), str(tmp_path)], cwd=ROOT, capture_output=True, text=True, check=False
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert (tmp_path / "dist" / "TowerBloxx.gba").read_bytes() == source_rom.read_bytes()
    assert (tmp_path / "dist" / "TowerBloxx.gba.sha256").is_file()


def _live_asset_sources() -> dict[str, Path]:
    result: dict[str, Path] = {}
    for root in (GBA / "graphics" / "gameplay", GBA / "graphics" / "ui", GBA / "graphics" / "backgrounds"):
        for path in root.glob("*.bmp"):
            result[path.stem] = path
    for path in (GBA / "audio").glob("*"):
        if path.is_file():
            result[path.stem] = path
    return result


def test_direct_butano_asset_includes_have_source_inputs() -> None:
    sources = _live_asset_sources()
    allow_generated = {
        "tower_mesh_assets", "tower_font", "tower_localization", "tower_ui_assets",
    }
    pattern = re.compile(r'#include\s+"bn_(?:sprite|regular_bg|music|sound)_items_([A-Za-z0-9_]+)\.h"')
    missing: set[str] = set()
    for path in list((GBA / "src").rglob("*.cpp")) + list((GBA / "include").rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for name in pattern.findall(text):
            if name not in sources and name not in allow_generated:
                missing.add(name)
    assert not missing, f"missing live asset inputs: {sorted(missing)}"


def test_no_exact_duplicate_live_bmps_unless_intentionally_retained() -> None:
    groups: dict[str, list[Path]] = defaultdict(list)
    for root in (GBA / "graphics" / "gameplay", GBA / "graphics" / "ui", GBA / "graphics" / "backgrounds"):
        for path in root.glob("*.bmp"):
            groups[hashlib.sha256(path.read_bytes()).hexdigest()].append(path)

    # Tiny intentional frame aliases are allowed when identity is part of an animation/composite contract.
    allow_pairs = {
        frozenset({"menu_highlight_left.bmp", "menu_highlight_right.bmp"}),
    }
    unexpected: list[list[str]] = []
    for paths in groups.values():
        if len(paths) < 2:
            continue
        names = frozenset(path.name for path in paths)
        if names in allow_pairs:
            continue
        unexpected.append(sorted(path.relative_to(ROOT).as_posix() for path in paths))
    assert not unexpected, f"unexpected exact duplicate BMP groups: {unexpected}"


def test_all_building_families_fit_two_obj_palette_banks() -> None:
    gameplay = GBA / "graphics" / "gameplay"

    for building_type in range(1, 5):
        mesh_ids = (9 + building_type, 19 + building_type, 29 + building_type, 39 + building_type)
        family_jsons: list[Path] = []
        for mesh_id in mesh_ids:
            family_jsons.extend(gameplay.glob(f"tb_mesh_{mesh_id:03d}_p*.json"))
        for mesh_id in (9 + building_type, 19 + building_type):
            family_jsons.extend(gameplay.glob(f"tumble_m{mesh_id:03d}_*.json"))

        assert family_jsons, building_type
        palette_signatures: set[tuple[int, ...]] = set()
        for json_path in family_jsons:
            text = json_path.read_text(encoding="utf-8")
            assert '"bpp_mode": "bpp_8"' in text
            match = re.search(r'"colors_count"\s*:\s*(\d+)', text)
            assert match is not None
            assert int(match.group(1)) <= 32, json_path.name

            bmp_path = json_path.with_suffix(".bmp")
            with Image.open(bmp_path) as image:
                assert max(image.tobytes()) < 32, bmp_path.name
                palette_signatures.add(tuple((image.getpalette() or [])[: 32 * 3]))

        assert len(palette_signatures) == 1, f"building type {building_type} has multiple tower palettes"



def test_ci_workflow_uses_node24_ready_actions_and_pinned_ubuntu() -> None:
    workflow = WORKFLOW.read_text(encoding="utf-8")
    assert workflow.count("runs-on: ubuntu-24.04") >= 3
    assert "actions/checkout@v5" in workflow
    assert "actions/setup-python@v6" in workflow
    assert "actions/upload-artifact@v6" in workflow
    assert "actions/download-artifact@v5" in workflow


def test_build_city_sandbox_is_volatile_and_construction_only() -> None:
    main = (GBA / "src" / "main.cpp").read_text(encoding="utf-8")
    city = (GBA / "src" / "build_city.cpp").read_text(encoding="utf-8")
    scene = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    construction = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    assert "sandbox_save = save;" in main
    assert "result.save_dirty && ! build_city.sandbox_active()" in main
    assert "build_city.sandbox_active() ? sandbox_save : save" in main
    assert "build_city.resume_presentation(build_city.sandbox_active() ? sandbox_save : save)" in main
    assert "pending_presentation = PendingPresentation::CityResume;" in main
    assert "_secret_step" in city
    assert "_request.stationary_crane = _sandbox_active" in city
    assert "result.placement_committed = ! _city.sandbox_active()" in scene
    assert "request.stationary_crane" in construction
    tower = (GBA / "src" / "tower_construction.cpp").read_text(encoding="utf-8")
    assert "SELECT + Up, Up, Down, Down" in tower
    assert "_stationary_crane = ! _stationary_crane" in tower


def test_every_scene_transition_waits_for_sprite_vram_reclamation() -> None:
    main = (GBA / "src" / "main.cpp").read_text(encoding="utf-8")
    # The outgoing scene releases its sprite/background references, then a full
    # bn::core::update() occurs before the pending scene allocates new graphics.
    pending = main[main.index("if(pending_presentation != PendingPresentation::None)"):]
    assert "pending_presentation = PendingPresentation::None;" in pending
    assert pending.index("bn::core::update();") < pending.index("const tb::InputFrame input")
    for transition in (
        "QuickStart", "CityStart", "ConstructionStart",
        "QuickResume", "ConstructionResume", "CityResume",
    ):
        assert f"case PendingPresentation::{transition}:" in pending
    assert "pending_construction_request = request;" in main
    assert "build_city.clear_construction_request();" in main
    assert "pending_presentation = PendingPresentation::ConstructionStart;" in main
    assert "pending_presentation = PendingPresentation::QuickResume;" in main
    assert "pending_presentation = PendingPresentation::ConstructionResume;" in main
    assert "pending_presentation = PendingPresentation::CityResume;" in main
    assert "build_city.resume_presentation(construction_save)" not in main


def test_construction_cosmetics_never_preempt_required_sprite_allocations() -> None:
    # GBA OAM has only 128 sprite slots. A third perfect landing used to
    # allocate the next block/floor/HUD while retaining the previous burst of
    # 32 star/trail sprites, triggering bn_sprites_manager::create.
    source = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    assert '#include "bn_sprites.h"' in source
    assert "bn::sprites::available_items_count()" in source
    assert "_perfect_star_sprites.clear();\n    _block_sparkle_sprites.clear();\n    if(snapshot.floor_count" in source

    # In normal startup, live updates, and resume, construct mandatory HUD
    # before recreating star effects; stars then use only remaining slots.
    start = source.index("void TowerConstructionScene::start(")
    update = source.index("TowerConstructionSceneUpdateResult TowerConstructionScene::update(")
    resume = source.index("void TowerConstructionScene::resume_presentation()")
    discard = source.index("void TowerConstructionScene::discard()")
    for section in (source[start:update], source[update:resume], source[resume:discard]):
        assert section.index("_rebuild_hud(snapshot);") < section.index("_update_perfect_landing_effect(snapshot);")
        assert section.index("_update_combo_meter(snapshot);") < section.index("_update_perfect_landing_effect(snapshot);")

    effect = source[source.index("void TowerConstructionScene::_update_perfect_landing_effect("):]
    assert "head_asset.part_count + seam_reserve" in effect
    assert "trail_asset.part_count + seam_reserve" in effect


def test_resource_pressure_never_allocates_unchecked_cosmetics() -> None:
    for name in ("quick_game_scene", "tower_construction_scene"):
        source = (GBA / "src" / f"{name}.cpp").read_text(encoding="utf-8")
        assert "part.item->create_sprite_optional" in source
        assert "output.max_size() - output.size() < asset.part_count" in source
        assert "while(output.size() > first) { output.pop_back(); }" in source
        assert "_floor_sprites.clear();\n                _floor_affine_mats.clear();" in source
        assert "_rendered_floor_count = -1;" in source
        assert "_rendered_current_mesh_id = -1; // Retry the pose next frame." in source
        assert "_text_generator.generate_optional(" in source
        # Every sprite allocation in a construction scene must now be fallible.
        assert "->create_sprite(" not in source
        assert ".create_sprite(" not in source

    quick = (GBA / "src" / "quick_game_scene.cpp").read_text(encoding="utf-8")
    assert "const int seam_reserve" in quick
    assert "_perfect_star_sprites.max_size() - _perfect_star_sprites.size()" in quick
    assert "bn::sprites::available_items_count()" in quick

    city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    assert "part.item->create_sprite_optional" in city
    assert "_text_generator.generate_optional(" in city
    assert "->create_sprite(" not in city
    assert city.index("_show_status(save, snapshot);", city.index("void BuildCityScene::_rebuild(")) < city.index(
        "_show_city_tiles(save, snapshot);", city.index("void BuildCityScene::_rebuild("))
    assert city.index("// Saved towers:") < city.index("if(pulse_building_type != 0)")
    menu = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    assert ".generate_optional(" in menu
    assert "->create_sprite(" not in menu
    sky = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    assert "bn::sprites::available_items_count() < asset.part_count + 12" in sky
    assert "slot.sprites.size() != event_asset.frames[frame]->part_count" in sky
    assert "->create_sprite(" not in sky


def test_all_sprite_palette_indices_fit_declared_bpp() -> None:
    # Catch the exact milestone-badge crash class across every tracked BMP,
    # not merely the four images that triggered it previously.
    for manifest in (GBA / "graphics").rglob("*.json"):
        data = json.loads(manifest.read_text(encoding="utf-8"))
        bmp = manifest.with_suffix(".bmp")
        if not bmp.exists() or data.get("bpp_mode") != "bpp_4":
            continue
        with Image.open(bmp) as image:
            assert image.mode == "P", manifest
            assert max(image.get_flattened_data()) < 16, f"{bmp}: BPP4 sprite references palette index >= 16"


def test_one_time_planets_and_direct_special_roof_transition() -> None:
    backdrop = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    quick_scene = (GBA / "src" / "quick_game_scene.cpp").read_text(encoding="utf-8")
    city_scene = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    tower = (GBA / "src" / "tower_construction.cpp").read_text(encoding="utf-8")
    assert "_spawned_celestial_events |= celestial_event_flag(type)" in backdrop
    assert "! (_spawned_celestial_events & celestial_event_flag(candidate))" in backdrop
    assert "if(new_run)" in backdrop
    assert "_backdrop.start(snapshot.presentation_camera_y, _background_clock_ms, false)" in quick_scene
    assert "_backdrop.start(snapshot.presentation_camera_y, _background_clock_ms, false)" in city_scene
    assert "centered_roof_lowering" in tower
    assert "_roof_phase && _block_state == TowerConstructionBlockState::Raising" in tower
    assert "_camera_y == _camera_target_y) ||" in tower
    assert "unique_celestial_event(candidate)" in backdrop
    assert "const int min_x = half_width + margin" in backdrop
    assert "const int source_screen_x = slot.x_eighths >> 3" in backdrop
    assert "camera_three_quarters + 96 + _legacy_random(352)" in backdrop
