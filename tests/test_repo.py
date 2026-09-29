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



def test_all_graphics_json_manifests_declare_type() -> None:
    missing = []
    invalid = []
    for manifest in sorted((GBA / "graphics").rglob("*.json")):
        data = json.loads(manifest.read_text(encoding="utf-8"))
        asset_type = data.get("type")
        if asset_type is None:
            missing.append(manifest.relative_to(ROOT).as_posix())
        elif asset_type not in {"sprite", "regular_bg", "affine_bg", "palette"}:
            invalid.append((manifest.relative_to(ROOT).as_posix(), asset_type))
    assert not missing, f"graphics JSON manifests missing required type field: {missing}"
    assert not invalid, f"graphics JSON manifests have unsupported type values: {invalid}"

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
        assert "brown_digit_frames[10]" in source
        assert "generated::christmas_hud_brown_digit_frames" in source
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
    for root in (GBA / "graphics" / "gameplay", GBA / "graphics" / "ui", GBA / "graphics" / "backgrounds", GBA / "graphics" / "christmas"):
        for path in root.rglob("*.bmp"):
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
    resume = source.index("void TowerConstructionScene::resume_presentation(const SaveData& save)")
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


def test_quick_game_suspend_destroys_transient_landing_effects() -> None:
    source = (GBA / "src" / "quick_game_scene.cpp").read_text(encoding="utf-8")
    start = source.index("void QuickGameScene::suspend_presentation()")
    end = source.index("void QuickGameScene::resume_presentation", start)
    body = source[start:end]
    # Suspended gameplay reuses the same process while the root menu is shown.
    # Every transient star/sparkle sprite must therefore be destroyed here.
    for cleanup in (
        "_perfect_star_sprites.clear();",
        "_combo_star_sprites.clear();",
        "_block_sparkle_sprites.clear();",
    ):
        assert cleanup in body


def test_christmas_visuals_keep_classic_geometry_envelopes() -> None:
    classic_ui = GBA / "graphics" / "ui"
    christmas_ui = GBA / "graphics" / "christmas" / "ui"
    classic_gameplay = GBA / "graphics" / "gameplay"
    christmas_gameplay = GBA / "graphics" / "christmas" / "gameplay"

    # These are the high-risk assets visible in the real-ROM screenshots.
    # Christmas art may have different pixels, but it must occupy the same GBA
    # object canvas as the Classic layout/physics counterpart.
    ui_pairs = []
    for building in range(1, 5):
        for frame in range(4):
            ui_pairs.append((
                classic_ui / f"city_building_{building}_f{frame}_p0.bmp",
                christmas_ui / f"christmas_city_building_{building}_f{frame}_p0.bmp",
            ))
    for frame in range(5):
        ui_pairs.append((
            classic_ui / f"construction_target_badge_f{frame}_p0.bmp",
            christmas_ui / f"christmas_construction_target_badge_f{frame}_p0.bmp",
        ))
    for frame in range(3):
        ui_pairs.append((
            classic_ui / f"accuracy_star_f{frame}_p0.bmp",
            christmas_ui / f"christmas_accuracy_star_f{frame}_p0.bmp",
        ))

    for classic, christmas in ui_pairs:
        assert classic.is_file(), classic
        assert christmas.is_file(), christmas
        with Image.open(classic) as classic_image, Image.open(christmas) as christmas_image:
            assert christmas_image.size == classic_image.size, (classic.name, christmas.name)

    mesh_pairs = [
        ("tb_mesh_007_p0.bmp", "christmas_tb_mesh_007_p0.bmp"),
        ("tb_mesh_008_p0.bmp", "christmas_tb_mesh_008_p0.bmp"),
        ("tb_mesh_008_p1.bmp", "christmas_tb_mesh_008_p1.bmp"),
        ("tb_mesh_010_p0.bmp", "christmas_tb_mesh_010_p0.bmp"),
        ("tb_mesh_020_p0.bmp", "christmas_tb_mesh_020_p0.bmp"),
        ("tb_mesh_030_p0.bmp", "christmas_tb_mesh_030_p0.bmp"),
        ("tb_mesh_040_p0.bmp", "christmas_tb_mesh_040_p0.bmp"),
        ("crane_hook_pose_00_p0.bmp", "christmas_crane_hook_pose_00_p0.bmp"),
        ("tb_mesh_008_p0.bmp", "christmas_crane_hook_pose_24_p0.bmp"),
        ("tb_mesh_008_p1.bmp", "christmas_crane_hook_pose_24_p1.bmp"),
        ("crane_hook_pose_48_p0.bmp", "christmas_crane_hook_pose_48_p0.bmp"),
        ("tumble_m010_c0_s01_p0.bmp", "christmas_tumble_m010_c0_s01_p0.bmp"),
        ("tumble_m023_c3_s12_p0.bmp", "christmas_tumble_m023_c3_s12_p0.bmp"),
    ]
    for classic_name, christmas_name in mesh_pairs:
        classic = classic_gameplay / classic_name
        christmas = christmas_gameplay / christmas_name
        assert classic.is_file(), classic
        assert christmas.is_file(), christmas
        with Image.open(classic) as classic_image, Image.open(christmas) as christmas_image:
            assert christmas_image.size == classic_image.size, (classic_name, christmas_name)

    # Composite offsets are just as important as pixel dimensions: a correct
    # sprite with the old Santa-JAR anchor still appears shifted on hardware.
    classic_header = (GBA / "include" / "generated" / "tower_mesh_assets.h").read_text(encoding="utf-8")
    christmas_header = (GBA / "include" / "generated" / "christmas_tower_mesh_assets.h").read_text(encoding="utf-8")

    def offsets(text: str, symbol: str) -> list[tuple[int, int]]:
        match = re.search(
            rf"inline const MeshPartAsset {re.escape(symbol)}\[\] = \{{(.*?)\n\}};",
            text,
            re.S,
        )
        assert match, symbol
        return [(int(x), int(y)) for x, y in re.findall(r",\s*(-?\d+),\s*(-?\d+)\s*\}", match.group(1))]

    for symbol in (
        "mesh_007_parts",
        "mesh_008_parts",
        "mesh_010_parts",
        "mesh_020_parts",
        "mesh_030_parts",
        "mesh_040_parts",
        "crane_hook_pose_00_parts",
        "crane_hook_pose_24_parts",
        "crane_hook_pose_48_parts",
        "tumble_m010_c0_s01_parts",
        "tumble_m023_c3_s12_parts",
    ):
        assert offsets(christmas_header, symbol) == offsets(classic_header, symbol), symbol

    shell = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    assert "constexpr int christmas_worker_width = 19;" in shell
    assert "constexpr int christmas_worker_height = 23;" in shell
    assert "_show_composite(generated::christmas_tower_logo, 0, -62);" in shell
    city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    assert "constexpr int christmas_building_widths[4] = {15, 16, 17, 19};" in city
    assert "constexpr int christmas_building_heights[4] = {14, 16, 17, 19};" in city


def test_one_time_planets_and_direct_special_roof_transition() -> None:
    backdrop = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    quick_scene = (GBA / "src" / "quick_game_scene.cpp").read_text(encoding="utf-8")
    city_scene = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    tower = (GBA / "src" / "tower_construction.cpp").read_text(encoding="utf-8")
    assert "_spawned_celestial_events |= celestial_event_flag(type)" in backdrop
    assert "! (_spawned_celestial_events & celestial_event_flag(candidate))" in backdrop
    assert "if(new_run)" in backdrop
    assert "_backdrop.start(snapshot.presentation_camera_y, _background_clock_ms, false, _visual_theme)" in quick_scene
    assert "_backdrop.start(snapshot.presentation_camera_y, _background_clock_ms, false, _visual_theme)" in city_scene
    assert "const bool centered_roof_lowering" in tower
    assert "_roof_phase && _block_state == TowerConstructionBlockState::Raising" in tower
    assert "if(_stationary_crane || centered_roof_lowering)" in tower
    assert "_camera_y == _camera_target_y) ||" in tower


def test_christmas_theme_assets_are_isolated_and_wired() -> None:
    christmas = GBA / "graphics" / "christmas" / "ui"
    assert christmas.is_dir()
    assert (christmas / "christmas_tower_logo_p0.bmp").is_file()
    assert (christmas / "christmas_worker_blue_f0_p0.bmp").is_file()
    assert (christmas / "christmas_city_building_1_f0_p0.bmp").is_file()
    assert (christmas / "christmas_city_level_icon_f8_p0.bmp").is_file()
    assert (christmas / "christmas_sky_type_9_f0_p0.bmp").is_file()

    makefile = (GBA / "Makefile").read_text(encoding="utf-8")
    assert "graphics/christmas/ui" in makefile
    controller = (GBA / "src" / "ui_controller.cpp").read_text(encoding="utf-8")
    shell = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    backdrop = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    assert "VisualTheme::Christmas" in controller
    assert "generated::christmas_tower_logo" in shell
    assert "generated::christmas_worker_blue_frames" in shell
    assert "generated::christmas_city_building_1_frames" in city
    assert "generated::christmas_city_level_icon_frames" in city
    assert "generated::christmas_sky_event_assets" in backdrop


def test_christmas_construction_meshes_are_generated_with_hybrid_crane() -> None:
    gameplay = GBA / "graphics" / "christmas" / "gameplay"
    assert gameplay.is_dir()
    for name in (
        "christmas_tb_mesh_007_p0.bmp",
        "christmas_tb_mesh_008_p0.bmp",
        "christmas_tb_mesh_008_p1.bmp",
        "christmas_tb_mesh_010_p0.bmp",
        "christmas_tb_mesh_020_p0.bmp",
        "christmas_tb_mesh_030_p0.bmp",
        "christmas_tb_mesh_040_p0.bmp",
        "christmas_crane_hook_pose_24_p0.bmp",
        "christmas_crane_hook_pose_24_p1.bmp",
        "christmas_tumble_m010_c0_s01_p0.bmp",
        "christmas_tumble_m023_c3_s12_p0.bmp",
    ):
        assert (gameplay / name).is_file(), name

    generated = (GBA / "include" / "generated" / "christmas_tower_mesh_assets.h").read_text(encoding="utf-8")
    assert "namespace tb::generated::christmas" in generated
    assert "inline constexpr int mesh_count = 18;" in generated
    assert "inline constexpr int crane_hook_frame_count = 49;" in generated
    assert "inline constexpr int tumble_pose_count = 384;" in generated
    # Santa pack's platform model is intentionally not emitted: construction
    # falls back to the established classic flat platform asset for mesh 9.
    assert "{ 9, mesh_009_parts" not in generated

    makefile = (GBA / "Makefile").read_text(encoding="utf-8")
    assert "graphics/christmas/gameplay" in makefile
    for source_name in ("quick_game_scene.cpp", "tower_construction_scene.cpp"):
        source = (GBA / "src" / source_name).read_text(encoding="utf-8")
        assert '#include "generated/christmas_tower_mesh_assets.h"' in source
        assert "generated::christmas::mesh_by_id(mesh_id)" in source
        assert "generated::christmas::tumble_pose_for" in source
        # Christmas keeps its blocks/tumbles and may theme the one-off
        # special/intro crane mesh, but the normal swing rope/hook remains the
        # proven Classic GBA geometry.
        assert "generated::christmas::crane_hook_frame_for_step" not in source
        assert "return generated::crane_hook_frame_for_step(step);" in source
        assert "mesh_by_id(_visual_theme, special_crane_mesh_id)" in source
        assert "VisualTheme::Christmas" in source


def test_christmas_construction_palette_budgets_are_stable() -> None:
    gameplay = GBA / "graphics" / "christmas" / "gameplay"

    # Each building colour owns exactly one BPP8 palette across the static
    # base/floor/roof/trophy and every pre-rendered tumble pose.
    for building_type in range(1, 5):
        mesh_ids = (9 + building_type, 19 + building_type, 29 + building_type, 39 + building_type)
        manifests: list[Path] = []
        for mesh_id in mesh_ids:
            manifests.extend(gameplay.glob(f"christmas_tb_mesh_{mesh_id:03d}_p*.json"))
        for mesh_id in (9 + building_type, 19 + building_type):
            manifests.extend(gameplay.glob(f"christmas_tumble_m{mesh_id:03d}_*.json"))
        assert manifests, building_type

        palettes: set[tuple[int, ...]] = set()
        for manifest in manifests:
            data = json.loads(manifest.read_text(encoding="utf-8"))
            assert data["bpp_mode"] == "bpp_8"
            assert data["colors_count"] <= 32
            with Image.open(manifest.with_suffix(".bmp")) as image:
                assert image.mode == "P"
                assert max(image.tobytes()) < 32
                palettes.add(tuple((image.getpalette() or [])[: 32 * 3]))
        assert len(palettes) == 1, f"Christmas building type {building_type} uses multiple palettes"

    # The special Santa crane and all 49 swing frames use one BPP4 bank.
    crane_manifests = list(gameplay.glob("christmas_tb_mesh_00[78]_p*.json"))
    crane_manifests += list(gameplay.glob("christmas_crane_hook_pose_*_p*.json"))
    assert crane_manifests
    crane_palettes: set[tuple[int, ...]] = set()
    for manifest in crane_manifests:
        data = json.loads(manifest.read_text(encoding="utf-8"))
        assert data["bpp_mode"] == "bpp_4"
        with Image.open(manifest.with_suffix(".bmp")) as image:
            assert image.mode == "P"
            assert max(image.tobytes()) < 16
            crane_palettes.add(tuple((image.getpalette() or [])[: 16 * 3]))
    assert len(crane_palettes) == 1


def test_christmas_phase7_hud_effects_and_menu_icons_are_wired() -> None:
    christmas = GBA / "graphics" / "christmas" / "ui"
    for name in (
        "christmas_menu_continue_icon_p0.bmp",
        "christmas_menu_build_city_icon_p0.bmp",
        "christmas_menu_quick_game_icon_p0.bmp",
        "christmas_menu_settings_icon_p0.bmp",
        "christmas_construction_target_badge_f0_p0.bmp",
        "christmas_construction_target_badge_f4_p0.bmp",
        "christmas_hud_white_digit_f0_p0.bmp",
        "christmas_hud_white_digit_f13_p0.bmp",
        "christmas_hud_brown_digit_f11_p0.bmp",
        "christmas_hud_population_icon_p0.bmp",
        "christmas_hud_state_indicator_f9_p0.bmp",
        "christmas_accuracy_star_f2_p0.bmp",
        "christmas_combo_star_f3_p0.bmp",
        "christmas_block_sparkle_f2_p0.bmp",
    ):
        assert (christmas / name).is_file(), name

    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")
    for symbol in (
        "christmas_construction_target_badge_frames",
        "christmas_hud_white_digit_frames",
        "christmas_hud_brown_digit_frames",
        "christmas_hud_state_indicator_frames",
        "christmas_accuracy_star_frames",
        "christmas_combo_star_frames",
        "christmas_block_sparkle_frames",
    ):
        assert symbol in generated

    shell = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    assert "generated::christmas_menu_continue_icon" in shell
    assert "generated::christmas_menu_build_city_icon" in shell
    assert "generated::christmas_menu_quick_game_icon" in shell
    assert "generated::christmas_menu_settings_icon" in shell
    # Santa's resource set has no honest equivalents for these root entries.
    # They intentionally retain the classic source artwork as a fallback.
    assert "generated::menu_high_scores_icon" in shell
    assert "generated::menu_instructions_icon" in shell

    for source_name in ("quick_game_scene.cpp", "tower_construction_scene.cpp"):
        source = (GBA / "src" / source_name).read_text(encoding="utf-8")
        assert '#include "generated/christmas_assets.h"' in source
        assert "gameplay_worker_asset(worker, _visual_theme)" in source
        assert "generated::christmas_worker_blue_frames" in source
        assert "generated::christmas_worker_red_frames" in source
        assert "generated::christmas_hud_white_digit_frames" in source
        assert "generated::christmas_hud_brown_digit_frames" in source
        assert "generated::christmas_hud_state_indicator_frames" in source
        assert "generated::christmas_hud_population_icon" in source
        assert "generated::christmas_combo_star_frames" in source
        assert "generated::christmas_block_sparkle_frames" in source
        assert "generated::christmas_accuracy_star_frames" in source

    construction = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    assert "generated::christmas_construction_target_badge_frames" in construction

    build_city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    assert "generated::christmas_hud_white_digit_frames" in build_city
    assert "generated::christmas_hud_brown_digit_frames" in build_city
    assert "generated::christmas_hud_red_digit_frames" in build_city
    assert "generated::christmas_hud_white_digit_f12" in build_city


def test_christmas_phase7_ui_palette_budgets_are_stable() -> None:
    christmas = GBA / "graphics" / "christmas" / "ui"

    def palette16(path: Path) -> tuple[int, ...]:
        with Image.open(path) as image:
            assert image.mode == "P", path
            assert max(image.tobytes()) < 16, path
            return tuple((image.getpalette() or [])[: 16 * 3])

    hud_patterns = (
        "christmas_construction_target_badge_f*_p*.bmp",
        "christmas_hud_white_digit_f*_p*.bmp",
        "christmas_hud_brown_digit_f*_p*.bmp",
        "christmas_hud_red_digit_f*_p*.bmp",
        "christmas_hud_population_icon_p*.bmp",
        "christmas_hud_state_indicator_f*_p*.bmp",
    )
    hud_files = [path for pattern in hud_patterns for path in christmas.glob(pattern)]
    assert len(hud_files) == 53
    assert len({palette16(path) for path in hud_files}) == 1

    effect_patterns = (
        "christmas_accuracy_star_f*_p*.bmp",
        "christmas_combo_star_f*_p*.bmp",
        "christmas_block_sparkle_f*_p*.bmp",
    )
    effect_files = [path for pattern in effect_patterns for path in christmas.glob(pattern)]
    assert len(effect_files) == 10
    assert len({palette16(path) for path in effect_files}) == 1

    # Root-menu Santa icons reuse the already-established Christmas worker
    # palette bank instead of introducing another OBJ palette at menu time.
    worker_palette = palette16(christmas / "christmas_worker_blue_f0_p0.bmp")
    menu_files = list(christmas.glob("christmas_menu_*_icon_p*.bmp"))
    assert len(menu_files) == 5
    assert all(palette16(path) == worker_palette for path in menu_files)



def test_christmas_phase8_city_chrome_and_hybrid_crane_are_wired() -> None:
    gameplay = GBA / "graphics" / "christmas" / "gameplay"
    ui = GBA / "graphics" / "christmas" / "ui"
    for name in (
        "christmas_crane_special_boom_p0.bmp",
        "christmas_crane_special_boom_p1.bmp",
        "christmas_crane_special_boom_p2.bmp",
    ):
        assert (gameplay / name).is_file(), name
    assert (ui / "christmas_city_population_icon_p0.bmp").is_file()
    assert (ui / "christmas_city_action_icon_p0.bmp").is_file()

    for source_name in ("quick_game_scene.cpp", "tower_construction_scene.cpp"):
        source = (GBA / "src" / source_name).read_text(encoding="utf-8")
        # Hybrid crane contract: the special/intro crane uses the red Santa
        # mesh and boom; normal swing rope/hook/cable stay on Classic's proven
        # GBA geometry.
        assert "bn::sprite_items::christmas_crane_special_boom_p0" in source
        assert "bn::sprite_items::christmas_crane_special_boom_p1" in source
        assert "bn::sprite_items::christmas_crane_special_boom_p2" in source
        assert "bn::sprite_items::crane_special_boom_p0" in source
        assert "mesh_by_id(_visual_theme, special_crane_mesh_id)" in source
        assert "return generated::crane_hook_frame_for_step(step);" in source
        assert "bn::sprite_items::crane_special_cable_segment" in source

    build_city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    assert "generated::christmas_city_population_icon" in build_city
    assert "generated::christmas_city_action_icon" in build_city

    legal_sizes = {(8, 8), (16, 8), (8, 16), (16, 16), (32, 8), (8, 32),
                   (32, 16), (16, 32), (32, 32), (64, 32), (32, 64), (64, 64)}
    bmps = list(gameplay.glob("christmas_crane_special_boom_p*.bmp"))
    bmps += [ui / "christmas_city_population_icon_p0.bmp", ui / "christmas_city_action_icon_p0.bmp"]
    assert len(bmps) == 5
    for bmp in bmps:
        with Image.open(bmp) as image:
            assert image.size in legal_sizes, (bmp, image.size)
            assert image.mode == "P"
            assert max(image.tobytes()) < 16


def test_christmas_phase9_build_city_panels_status_and_quick_counter_are_wired() -> None:
    ui = GBA / "graphics" / "christmas" / "ui"
    expected = [
        *(f"christmas_city_status_panel_f{index}_p0.bmp" for index in range(4)),
        "christmas_city_status_placement_p0.bmp",
        "christmas_city_status_browse_p0.bmp",
        "christmas_city_status_aux_p0.bmp",
        "christmas_quick_counter_frame_p0.bmp",
    ]
    for name in expected:
        assert (ui / name).is_file(), name

    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")
    assert "christmas_city_status_panel_frames" in generated
    assert "christmas_city_status_placement" in generated
    assert "christmas_city_status_browse" in generated
    assert "christmas_quick_counter_frame" in generated

    build_city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    assert "generated::christmas_city_status_panel_frames" in build_city
    assert "generated::christmas_city_status_placement" in build_city
    assert "generated::christmas_city_status_browse" in build_city

    quick_game = (GBA / "src" / "quick_game_scene.cpp").read_text(encoding="utf-8")
    assert "generated::christmas_quick_counter_frame" in quick_game

    # Phase 9 intentionally shares one BPP4 OBJ palette bank across all of
    # these small Christmas HUD additions.
    palettes = []
    for name in expected:
        with Image.open(ui / name) as image:
            assert image.mode == "P", name
            assert max(image.tobytes()) < 16, name
            palettes.append(tuple((image.getpalette() or [])[: 16 * 3]))
    assert len(set(palettes)) == 1


def test_christmas_theme_switches_the_three_distinct_santa_music_tracks() -> None:
    audio_dir = GBA / "audio"
    for name in ("christmas_menu_theme.mod", "christmas_tower_theme.mod", "christmas_city_theme.mod"):
        path = audio_dir / name
        assert path.is_file()
        assert path.stat().st_size > 4096

    audio_cpp = (GBA / "src" / "game_audio.cpp").read_text(encoding="utf-8")
    audio_h = (GBA / "include" / "tb" / "game_audio.h").read_text(encoding="utf-8")
    main_cpp = (GBA / "src" / "main.cpp").read_text(encoding="utf-8")
    assert "VisualTheme theme" in audio_h
    assert "theme_changed" in audio_cpp
    assert "bn::music_items::christmas_menu_theme" in audio_cpp
    assert "bn::music_items::christmas_tower_theme" in audio_cpp
    assert "bn::music_items::christmas_city_theme" in audio_cpp
    assert "audio.update(save.sound_enabled != 0, audio_scene, tb::visual_theme(save));" in main_cpp

    # Santa JAR result jingles 82/83/84 are byte-identical to Classic 92/93/94,
    # so Phase 10 deliberately reuses the existing roof/fail modules instead of
    # wasting ROM on duplicate music assets.
    assert not (audio_dir / "christmas_normal_roof.mod").exists()
    assert not (audio_dir / "christmas_trophy_roof.mod").exists()
    assert not (audio_dir / "christmas_construction_fail.mod").exists()


def test_christmas_build_city_uses_full_theme_background_and_chrome() -> None:
    source = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    makefile = (GBA / "Makefile").read_text(encoding="utf-8")
    christmas_bg = GBA / "graphics" / "christmas" / "backgrounds"
    christmas_ui = GBA / "graphics" / "christmas" / "ui"

    assert "graphics/christmas/backgrounds" in makefile
    for index in range(4):
        assert (christmas_bg / f"christmas_city_bg_theme_{index}.bmp").is_file()
        assert (christmas_bg / f"christmas_city_bg_theme_{index}.json").is_file()
        assert f"bn::regular_bg_items::christmas_city_bg_theme_{index}" in source

    for name in (
        "christmas_city_milestone_badge_left_p0.bmp",
        "christmas_city_milestone_badge_right_p0.bmp",
        "christmas_city_milestone_badge_empty_left_p0.bmp",
        "christmas_city_milestone_badge_empty_right_p0.bmp",
        "christmas_city_comparison_panel_active_p0.bmp",
        "christmas_city_progress_f0_p0.bmp",
        "christmas_city_progress_f7_p0.bmp",
        "christmas_city_type_badge_1_p0.bmp",
        "christmas_city_type_badge_4_p0.bmp",
    ):
        assert (christmas_ui / name).is_file(), name

    assert "generated::christmas_city_milestone_badge" in source
    assert "generated::christmas_city_milestone_badge_empty" in source
    assert "generated::christmas_city_comparison_panel_active" in source
    assert "generated::christmas_city_progress_segment" in source
    assert "generated::christmas_city_progress_tails" in source
    assert "generated::christmas_city_type_badges" in source




def test_christmas_construction_backdrop_uses_classic_pipeline_with_santa_assets() -> None:
    source = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    header = (GBA / "include" / "tb" / "construction_backdrop.h").read_text(encoding="utf-8")
    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")
    classic_bg = GBA / "graphics" / "backgrounds"
    backgrounds = GBA / "graphics" / "christmas" / "backgrounds"
    gameplay = GBA / "graphics" / "christmas" / "gameplay"

    # Phase 20 mirrors the proven Classic architecture: the 17 moving colour
    # bands are one regular BG, while city/ground scenery is a second regular
    # BG using the same 3-chunk selection and scroll formula. The old Phase-18
    # approach baked canyon geometry into the sky and caused the visible bars,
    # seams and repeating street reported on real ROM builds.
    assert "create_christmas_sky_background(index) : create_sky_background(index)" in source
    assert "create_christmas_scenery_background(chunk) : create_scenery_background(chunk)" in source
    assert "const int scroll = ((camera_y - 512) * 22) / 256;" in source
    assert "generated::construction_scenery_chunk_centers" in source
    assert "generated::construction_scenery_max_scroll" in source
    assert "_scenery_background->set_y(scroll - chunk_center);" in source

    # Exact Santa House.t palette, quantized to GBA RGB555. The geometry of
    # every sky band must remain identical to Classic: only colours differ.
    santa_rgb888 = [
        0x000000, 0x000000, 0x123D88, 0x152860, 0x162F72, 0x162965,
        0x122458, 0x102050, 0x131D48, 0x34204C, 0x372C51, 0x2D4B4B,
        0x4A6742, 0x674723, 0x532733, 0x802A2B, 0x511A2F,
    ]
    def gba_rgb(value: int) -> tuple[int, int, int]:
        return tuple(((value >> shift) & 0xFF) & ~7 for shift in (16, 8, 0))

    expected = [gba_rgb(value) for value in santa_rgb888]
    palettes = []
    for index in range(17):
        stem = f"christmas_construction_sky_{index:02d}"
        bmp = backgrounds / f"{stem}.bmp"
        manifest = backgrounds / f"{stem}.json"
        assert bmp.is_file() and manifest.is_file(), stem
        assert f"bn::regular_bg_items::{stem}" in source
        with Image.open(bmp) as image:
            assert image.mode == "P" and image.size == (256, 512)
            palettes.append(tuple(image.getpalette() or ()))
            actual_counts = sorted(count for count, _ in (image.convert("RGB").getcolors(1_000_000) or []))
        with Image.open(classic_bg / f"construction_sky_{index:02d}.bmp") as classic:
            classic_counts = sorted(count for count, _ in (classic.convert("RGB").getcolors(1_000_000) or []))
        # Bands 0/9 can collapse two equal/near colours, so compare total
        # geometry where possible and always require the authored endpoint.
        if len(actual_counts) == len(classic_counts):
            assert actual_counts == classic_counts
        with Image.open(bmp) as image:
            used = {color for _, color in (image.convert("RGB").getcolors(1_000_000) or [])}
        assert expected[index] in used
    assert all(palette == palettes[0] for palette in palettes[1:])

    # Christmas gets dedicated scenery chunks; the runtime no longer has a
    # separate repeating ground BG or the obsolete 'mountain/cloud' scenery
    # sprite hack.
    scenery_palettes = []
    for chunk in range(3):
        stem = f"christmas_construction_scenery_{chunk}"
        bmp = backgrounds / f"{stem}.bmp"
        manifest = backgrounds / f"{stem}.json"
        assert bmp.is_file() and manifest.is_file(), stem
        with Image.open(bmp) as image:
            assert image.mode == "P" and image.size == (256, 512)
            scenery_palettes.append(tuple(image.getpalette() or ()))
        assert f"bn::regular_bg_items::{stem}" in source
    assert all(palette == scenery_palettes[0] for palette in scenery_palettes[1:])
    # GBA 8bpp regular backgrounds share the hardware BG palette; sky and
    # scenery must therefore use one identical 256-colour palette, just like
    # the proven Classic assets do. A mismatch here causes the kind of wild
    # cyan/black colour corruption seen in earlier ROM screenshots.
    assert scenery_palettes[0] == palettes[0]
    assert "christmas_construction_ground" not in source
    assert "christmas_construction_cloud_large" not in source
    assert "christmas_construction_cloud_small" not in source
    assert "christmas_mountain" not in source
    assert "_christmas_scenery_sprites" not in source
    assert "_christmas_scenery_sprites" not in header

    # Exact Santa resource_048 tree frames are a base-only world decoration,
    # not part of the repeating sky. They are explicitly culled with altitude.
    for frame in range(2):
        bmp = gameplay / f"christmas_construction_tree_f{frame}.bmp"
        manifest = gameplay / f"christmas_construction_tree_f{frame}.json"
        assert bmp.is_file() and manifest.is_file()
        with Image.open(bmp) as image:
            assert image.mode == "P" and image.size == (64, 64)
            assert max(image.tobytes()) < 16
    assert "christmas_construction_tree_f0.create_sprite_optional" in source
    assert "christmas_construction_tree_f1.create_sprite_optional" in source
    assert "scroll < -80 || scroll > 72 || _scenery_chunk != 0" in source
    assert "const int tree_y = 88 + scroll;" in source
    assert "_christmas_tree_sprites.clear();" in source
    assert "_christmas_tree_sprites" in header

    # Clouds/flyers/planets stay in the original sky-event system. All Santa
    # counterparts are selected from the theme-aware event table; type 13 is
    # intentionally shared because Santa has no separate counterpart there.
    for event_type in range(1, 29):
        if event_type == 13:
            continue
        assert f"christmas_sky_type_{event_type}_frames" in generated
    assert "{ legacy_sky_type_13_frames, 1, 8, 8 }" in generated
    assert "generated::christmas_sky_event_assets[type]" in source

    # Normal swing hook/rope/cable remain Classic by design.
    for scene_name in ("quick_game_scene.cpp", "tower_construction_scene.cpp"):
        scene = (GBA / "src" / scene_name).read_text(encoding="utf-8")
        assert "return generated::crane_hook_frame_for_step(step);" in scene
        assert "bn::sprite_items::crane_special_cable_segment" in scene



def test_christmas_phase16_cityscape_asset_is_not_forced_into_gameplay() -> None:
    source = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    scenery = GBA / "graphics" / "christmas" / "scenery"

    # The obsolete MBAC model-46 pre-render was never part of the live Santa
    # canyon path. Phase 23 removes the dead source assets entirely now that
    # Phase 20's dedicated regular-BG scenery pipeline is stable.
    for part in range(4):
        stem = f"christmas_construction_cityscape_p{part}"
        assert not (scenery / f"{stem}.bmp").exists()
        assert not (scenery / f"{stem}.json").exists()
        assert f"bn::sprite_items::{stem}" not in source
    assert "_christmas_cityscape_sprites" not in source



def test_christmas_low_altitude_foreground_and_snow_do_not_repeat() -> None:
    source = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")
    backgrounds = GBA / "graphics" / "christmas" / "backgrounds"
    gameplay = GBA / "graphics" / "christmas" / "gameplay"

    # Santa resources 45-47 are baked only into scenery chunk 0, which shares
    # Classic's world scroll. There is no independent ground BG to wrap up the
    # screen at higher altitude.
    low = Image.open(backgrounds / "christmas_construction_scenery_0.bmp")
    mid = Image.open(backgrounds / "christmas_construction_scenery_1.bmp")
    high = Image.open(backgrounds / "christmas_construction_scenery_2.bmp")
    assert low.mode == mid.mode == high.mode == "P"
    assert low.size == mid.size == high.size == (256, 512)
    # The authored snowy street gives chunk 0 substantially more palette/content
    # complexity than the pure city-canyon continuation chunks.
    assert len(low.getcolors(maxcolors=1_000_000) or []) > len(mid.getcolors(maxcolors=1_000_000) or [])
    low.close(); mid.close(); high.close()

    snow = gameplay / "christmas_snowflake_p0.bmp"
    snow_json = gameplay / "christmas_snowflake_p0.json"
    assert snow.is_file() and snow_json.is_file()
    with Image.open(snow) as image:
        assert image.mode == "P" and image.size == (8, 8)
        assert max(image.tobytes()) < 16

    assert "bn::sprite_items::christmas_snowflake_p0" in source
    assert "continuous screen-space snowfall" in source
    assert "Deterministic falling snow in screen space" in source
    assert "christmas_construction_ground" not in source
    assert "set_y(scroll + 44)" not in source


def test_christmas_phase17_ports_santa_missed_block_snow_burst() -> None:
    scene = (GBA / "src" / "tower_construction_scene.cpp").read_text(encoding="utf-8")
    header = (GBA / "include" / "tb" / "tower_construction_scene.h").read_text(encoding="utf-8")
    gameplay = GBA / "graphics" / "christmas" / "gameplay"

    # Santa image 39 is a 69x33 sheet of three 23px-wide snow/ice burst
    # frames. House draws the selected frame in a 23x34 bottom-edge clip when
    # a missed block leaves the viewport; it is not a static floor icicle.
    palettes = []
    for frame in range(3):
        stem = f"christmas_miss_snow_f{frame}"
        bmp = gameplay / f"{stem}.bmp"
        manifest = gameplay / f"{stem}.json"
        assert bmp.is_file() and manifest.is_file(), stem
        with Image.open(bmp) as image:
            assert image.mode == "P"
            assert image.size == (32, 64)
            assert max(image.tobytes()) < 16
            palettes.append(tuple((image.getpalette() or [])[: 16 * 3]))
            # Content is bottom-aligned so y=48 places its source bottom at
            # Butano screen y=+80, matching the Java bottom-edge clip.
            bbox = image.getbbox()
            assert bbox is not None
            assert bbox[1] >= 31
            assert bbox[3] <= 64
    assert all(palette == palettes[0] for palette in palettes[1:])

    assert "before.block_state != TowerConstructionBlockState::Missed" in scene
    assert "snapshot.block_state == TowerConstructionBlockState::Missed" in scene
    assert "christmas_miss_snow_frame_ms = 100" in scene
    assert "christmas_miss_snow_f0.create_sprite_optional" in scene
    assert "christmas_miss_snow_f1.create_sprite_optional" in scene
    assert "christmas_miss_snow_f2.create_sprite_optional" in scene
    assert "_christmas_miss_snow_sprite->set_position(_christmas_miss_snow_x, 48)" in scene
    assert "_update_christmas_miss_snow_effect();" in scene
    assert "_christmas_miss_snow_sprite" in header

    # Presentation teardown must remove the burst as well, otherwise a miss on
    # the last construction frame could leak a Christmas sprite into menus.
    suspend = scene[scene.index("void TowerConstructionScene::suspend_presentation"):
                    scene.index("void TowerConstructionScene::resume_presentation")]
    assert "_christmas_miss_snow_sprite.reset();" in suspend
    assert "_christmas_miss_snow_elapsed_ms = -1;" in suspend


def test_christmas_phase19_combo_meter_style_is_preserved():
    root = Path(__file__).resolve().parents[1]
    ui = root / "gba" / "graphics" / "christmas" / "ui"

    # Christmas uses the same proven geometry as the Classic combo frame,
    # but the OG Santa border is light/white rather than Classic yellow.
    for part in ("p0", "p1"):
        image = Image.open(ui / f"christmas_quick_combo_meter_frame_{part}.bmp")
        assert image.size == (64, 32)
        palette = image.getpalette()
        assert tuple(palette[13 * 3:13 * 3 + 3]) == (248, 248, 248)

    qg = (root / "gba" / "src" / "quick_game_scene.cpp").read_text()
    tc = (root / "gba" / "src" / "tower_construction_scene.cpp").read_text()
    assert "generated::christmas_quick_combo_meter_frame" in qg
    assert "generated::christmas_quick_combo_meter_frame" in tc



def test_christmas_phase22_menu_clouds_and_core_menu_icons_use_raw_santa_geometry() -> None:
    source = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")
    originals = GBA / "themes" / "christmas" / "originals"
    ui = GBA / "graphics" / "christmas" / "ui"

    # Santa resources 81/82 are the dedicated 105x27 and 54x14 menu clouds,
    # matching the Classic menu-cloud envelopes exactly. They must be used
    # directly instead of borrowing construction cloud events.
    assert "menu_cloud_asset(VisualTheme theme, bool large)" in source
    assert "generated::christmas_menu_cloud_large" in source
    assert "generated::christmas_menu_cloud_small" in source
    assert "_show_menu_clouds(controller.visual_theme())" in source
    for stem in (
        "christmas_menu_cloud_large_p0",
        "christmas_menu_cloud_large_p1",
        "christmas_menu_cloud_small_p0",
    ):
        assert (ui / f"{stem}.bmp").is_file()
        assert (ui / f"{stem}.json").is_file()
    assert "christmas_menu_cloud_large_parts" in generated
    assert "christmas_menu_cloud_small_parts" in generated

    # Core Santa root-menu icons are resources 6..10. Keep their original
    # 20x12 pixel geometry; only transparent GBA-safe padding is allowed.
    icon_sources = {
        "christmas_menu_continue_icon_p0": 6,
        "christmas_menu_build_city_icon_p0": 7,
        "christmas_menu_quick_game_icon_p0": 8,
        "christmas_menu_settings_icon_p0": 9,
        "christmas_menu_exit_icon_p0": 10,
    }
    for stem, resource_id in icon_sources.items():
        with Image.open(originals / f"resource_{resource_id:03d}.png").convert("RGBA") as src, \
             Image.open(ui / f"{stem}.bmp") as packed:
            assert packed.mode == "P"
            assert packed.size == (32, 16)
            packed_mask = [value != 0 for value in packed.tobytes()]
            src_mask = [px[3] != 0 for px in src.get_flattened_data()]
            # Packed source pixels are written at the top-left without scaling.
            for y in range(src.height):
                for x in range(src.width):
                    assert packed_mask[y * packed.width + x] == src_mask[y * src.width + x]


def test_christmas_phase22_all_sky_events_match_raw_santa_pixel_geometry() -> None:
    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")
    originals = GBA / "themes" / "christmas" / "originals"
    ui = GBA / "graphics" / "christmas" / "ui"

    mapping = {event: event + 53 for event in range(1, 13)}
    mapping.update({event: event + 52 for event in range(14, 29)})
    multi = {6: ("h", 2), 12: ("h", 2), 28: ("v", 2)}

    def packed_part(stem: str) -> Image.Image:
        image = Image.open(ui / f"{stem}.bmp")
        assert image.mode == "P"
        assert max(image.tobytes()) < 16
        return image

    for event, resource_id in mapping.items():
        source_sheet = Image.open(originals / f"resource_{resource_id:03d}.png").convert("RGBA")
        direction, frame_count = multi.get(event, ("single", 1))
        if frame_count == 1:
            frames = [source_sheet]
        elif direction == "h":
            width = source_sheet.width // frame_count
            frames = [source_sheet.crop((index * width, 0, (index + 1) * width, source_sheet.height))
                      for index in range(frame_count)]
        else:
            height = source_sheet.height // frame_count
            frames = [source_sheet.crop((0, index * height, source_sheet.width, (index + 1) * height))
                      for index in range(frame_count)]

        for frame_index, source in enumerate(frames):
            part_name = f"christmas_sky_type_{event}_f{frame_index}_parts"
            match = re.search(
                rf"inline const UiSpritePartAsset {part_name}\[\] = \{{(.*?)\}};",
                generated,
                re.S,
            )
            assert match, part_name
            parts = re.findall(
                r"&bn::sprite_items::([A-Za-z0-9_]+),\s*(-?\d+),\s*(-?\d+)",
                match.group(1),
            )
            assert 1 <= len(parts) <= 4
            rendered = [[False] * source.width for _ in range(source.height)]
            for stem, x_text, y_text in parts:
                packed = packed_part(stem)
                x = int(x_text)
                y = int(y_text)
                left = source.width // 2 + x - packed.width // 2
                top = source.height // 2 + y - packed.height // 2
                values = list(packed.tobytes())
                for py in range(packed.height):
                    for px in range(packed.width):
                        if values[py * packed.width + px] == 0:
                            continue
                        sx = left + px
                        sy = top + py
                        if 0 <= sx < source.width and 0 <= sy < source.height:
                            rendered[sy][sx] = True
            source_mask = [[False] * source.width for _ in range(source.height)]
            for y in range(source.height):
                for x in range(source.width):
                    source_mask[y][x] = source.getpixel((x, y))[3] != 0
            assert rendered == source_mask, f"Christmas sky event {event} frame {frame_index} geometry drifted"

        width, height = frames[0].size
        frame_symbol = f"christmas_sky_type_{event}_frames"
        expected_count = len(frames)
        assert f"{{ {frame_symbol}, {expected_count}, {width}, {height} }}," in generated

    # Type 13 has no separate Santa image and intentionally keeps the tiny
    # shared Classic star-dot.
    assert "{ legacy_sky_type_13_frames, 1, 8, 8 }," in generated


def test_christmas_phase22_key_hud_strips_keep_santa_source_geometry() -> None:
    originals = GBA / "themes" / "christmas" / "originals"
    ui = GBA / "graphics" / "christmas" / "ui"

    # Target badges are five 13x14 frames from resource 26.
    target = Image.open(originals / "resource_026.png").convert("RGBA")
    for frame in range(5):
        src = target.crop((frame * 13, 0, (frame + 1) * 13, 14))
        packed = Image.open(ui / f"christmas_construction_target_badge_f{frame}_p0.bmp")
        assert packed.mode == "P" and packed.size == (16, 16)
        for y in range(14):
            for x in range(13):
                assert (packed.getpixel((x, y)) != 0) == (src.getpixel((x, y))[3] != 0)

    # Brown runtime frames are digits 0-9, plus, x from raw resource 28.
    brown = Image.open(originals / "resource_028.png").convert("RGBA")
    source_indices = list(range(11)) + [13]
    for frame, source_index in enumerate(source_indices):
        src = brown.crop((source_index * 6, 0, (source_index + 1) * 6, 9))
        packed = Image.open(ui / f"christmas_hud_brown_digit_f{frame}_p0.bmp")
        assert packed.mode == "P" and packed.size == (8, 16)
        for y in range(9):
            for x in range(6):
                assert (packed.getpixel((x, y)) != 0) == (src.getpixel((x, y))[3] != 0)

    # Red runtime frames are the exact 0-9/minus strip from resource 29.
    red = Image.open(originals / "resource_029.png").convert("RGBA")
    for frame in range(11):
        src = red.crop((frame * 7, 0, (frame + 1) * 7, 9))
        packed = Image.open(ui / f"christmas_hud_red_digit_f{frame}_p0.bmp")
        assert packed.mode == "P" and packed.size == (8, 16)
        for y in range(9):
            for x in range(7):
                assert (packed.getpixel((x, y)) != 0) == (src.getpixel((x, y))[3] != 0)


def test_construction_backdrop_closes_helper_namespace_before_member_definitions() -> None:
    source = (GBA / "src" / "construction_backdrop.cpp").read_text(encoding="utf-8")

    # Helper functions live in tb::{anonymous}, but ConstructionBackdrop member
    # definitions must live directly in namespace tb. A missing closing brace
    # compiles nowhere in devkitARM and previously escaped host-only tests.
    marker = "\n}\n\nvoid ConstructionBackdrop::start("
    assert marker in source
    assert source.index(marker) < source.index("void ConstructionBackdrop::update(")


def test_christmas_phase23_results_navigation_and_city_continue_use_santa_assets() -> None:
    ui_shell = (GBA / "src" / "ui_shell.cpp").read_text(encoding="utf-8")
    city = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")
    ui = GBA / "graphics" / "christmas" / "ui"

    # Santa resource 5 supplies the red up/down navigation arrows used for
    # paged support/instruction screens; resource 12 supplies the small
    # continue arrow used by modal Build City presentation.
    for stem in (
        "christmas_support_nav_up_p0",
        "christmas_support_nav_down_p0",
        "christmas_city_continue_arrow_p0",
    ):
        bmp = ui / f"{stem}.bmp"
        manifest = ui / f"{stem}.json"
        assert bmp.is_file() and manifest.is_file()
        with Image.open(bmp) as image:
            assert image.mode == "P" and image.size == (8, 8)
            assert max(image.tobytes()) < 16

    assert "generated::christmas_support_nav_up" in ui_shell
    assert "generated::christmas_support_nav_down" in ui_shell
    assert "controller.visual_theme()" in ui_shell
    assert "generated::christmas_city_continue_arrow" in city
    assert "christmas_support_nav_up_parts" in generated
    assert "christmas_support_nav_down_parts" in generated
    assert "christmas_city_continue_arrow_parts" in generated


def test_christmas_phase23_build_city_final_visual_palette_budget() -> None:
    ui = GBA / "graphics" / "christmas" / "ui"
    files = list(ui.glob("christmas_city_*.bmp")) + list(ui.glob("christmas_hud_*.bmp"))
    assert files

    # Audit every live Build City Christmas sprite against its manifest. The
    # detailed city/building art intentionally uses bpp8 (up to 96 colours);
    # small HUD chrome remains bpp4. This catches accidental palette growth
    # without incorrectly forcing the bpp8 city art into a 16-colour bank.
    for path in files:
        manifest = path.with_suffix(".json")
        assert manifest.is_file(), path.name
        data = json.loads(manifest.read_text(encoding="utf-8"))
        with Image.open(path) as image:
            assert image.mode == "P", path.name
            colors = len(image.getcolors(maxcolors=1_000_000) or [])
            if data.get("bpp_mode") == "bpp_4":
                assert colors <= 16, (path.name, colors)
            else:
                assert data.get("bpp_mode") == "bpp_8", path.name
                assert colors <= int(data.get("colors_count", 256)), (path.name, colors)

    source = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    for symbol in (
        "christmas_city_progress_segment",
        "christmas_city_milestone_badge",
        "christmas_city_milestone_badge_empty",
        "christmas_city_population_icon",
        "christmas_city_status_panel_frames",
        "christmas_city_status_placement",
        "christmas_city_status_browse",
        "christmas_city_comparison_panel_active",
        "christmas_city_type_badges",
        "christmas_city_action_icon",
        "christmas_city_continue_arrow",
    ):
        assert symbol in source


def test_christmas_phase23_audio_result_fallback_is_explicit_and_no_fake_sfx_are_added() -> None:
    audio_cpp = (GBA / "src" / "game_audio.cpp").read_text(encoding="utf-8")
    audio_h = (GBA / "include" / "tb" / "game_audio.h").read_text(encoding="utf-8")
    main = (GBA / "src" / "main.cpp").read_text(encoding="utf-8")
    originals = GBA / "themes" / "christmas" / "originals"

    # The Santa pack has six MIDI resources: three looping scene tracks and
    # three finite result jingles. There are no WAV/AMR/sample resources in the
    # extracted pack, so do not invent block/menu SFX that the source does not
    # provide evidence for.
    assert len(list(originals.glob("music_*.mid"))) == 6
    assert not list(originals.glob("*.wav"))
    assert not list(originals.glob("*.amr"))
    assert not list(originals.glob("*.mp3"))

    assert "play_construction_result(uint8_t roof, VisualTheme theme)" in audio_h
    assert "Santa JAR resources 82-84 are byte-identical" in audio_cpp
    assert "(void) theme;" in audio_cpp
    assert "audio.play_construction_result(result.roof, tb::visual_theme(save));" in main


def test_christmas_phase23_removes_obsolete_backdrop_assets_and_keeps_bg_budget_safe() -> None:
    christmas = GBA / "graphics" / "christmas"
    generated = (GBA / "include" / "generated" / "christmas_assets.h").read_text(encoding="utf-8")

    obsolete = [
        christmas / "backgrounds" / "christmas_construction_ground.bmp",
        christmas / "backgrounds" / "christmas_construction_ground.json",
        *(christmas / "gameplay" / f"christmas_mountain_{size}_p{part}.{ext}"
          for size, parts in (("large", range(2)), ("small", range(2)))
          for part in parts for ext in ("bmp", "json")),
        *(christmas / "scenery" / f"christmas_construction_cityscape_p{part}.{ext}"
          for part in range(4) for ext in ("bmp", "json")),
    ]
    assert all(not path.exists() for path in obsolete)
    assert "christmas_mountain" not in generated
    assert "christmas_construction_cloud_large" not in generated
    assert "christmas_construction_cloud_small" not in generated

    # Regular BG tile counts remain far below one 8bpp charblock's 256-tile
    # budget for the active Christmas construction chunks.
    bg_root = christmas / "backgrounds"
    for path in sorted(bg_root.glob("christmas_construction_*.bmp")):
        with Image.open(path) as image:
            assert image.mode == "P" and image.size == (256, 512)
            raw = image.tobytes()
            width, height = image.size
            tiles = set()
            for y in range(0, height, 8):
                for x in range(0, width, 8):
                    tile = b"".join(raw[(y + row) * width + x:(y + row) * width + x + 8]
                                    for row in range(8))
                    tiles.add(tile)
            assert len(tiles) <= 224, (path.name, len(tiles))
