from __future__ import annotations

from collections import defaultdict
import hashlib
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


def test_build_city_top_hud_uses_taller_bar_vertical_centering() -> None:
    source = (GBA / "src" / "build_city_scene.cpp").read_text(encoding="utf-8")
    generated = (GBA / "include" / "generated" / "tower_ui_assets.h").read_text(encoding="utf-8")
    assert "constexpr int status_row_y = 8;" in source
    assert "constexpr int population_icon_y = status_row_y - 3;" in source
    assert "constexpr int population_counter_y = status_row_y - 1;" in source
    assert "constexpr int comparison_row_y = status_row_y;" in source
    assert "const bool empty_milestone_badge = snapshot.total_population == 0;" in source
    assert "empty_milestone_badge ? generated::city_milestone_badge_empty : generated::city_milestone_badge" in source
    assert "centered_x(25)" in source
    assert "int width = (snapshot.total_population - current) * 234 / (next - current);" in source
    assert "if(width < 1) { width = 1; }" in source
    assert "if(width > 234) { width = 234; }" in source
    assert "int x = 3;" in source
    assert "centered_y(13)" in source

    assert '#include "bn_sprite_items_city_milestone_badge_empty_p0.h"' in generated
    assert '#include "bn_sprite_items_city_milestone_badge_empty_p1.h"' in generated
    assert "city_milestone_badge_empty_parts" in generated
    for name in ("city_milestone_badge_p0", "city_milestone_badge_p1", "city_milestone_badge_empty_p0", "city_milestone_badge_empty_p1"):
        with Image.open(GBA / "graphics" / "ui" / f"{name}.bmp") as image:
            assert image.size in ((32, 16), (16, 16))

    # JAR 240x160 shell: one-pixel side borders, two dark bottom rows,
    # a pale separator, then the final dark line. The right comparison shell
    # is x=184..231, y=2..11 in screen coordinates.
    for theme in range(4):
        with Image.open(GBA / "graphics" / "backgrounds" / f"city_bg_theme_{theme}.bmp") as image:
            rgb = image.convert("RGB")
            sx, sy = 8, 48
            assert rgb.getpixel((sx + 0, sy + 0)) == (239, 231, 231)
            assert rgb.getpixel((sx + 239, sy + 0)) == (239, 231, 231)
            assert rgb.getpixel((sx + 100, sy + 0)) == (173, 156, 132)
            assert rgb.getpixel((sx + 100, sy + 15)) == (33, 24, 16)
            assert rgb.getpixel((sx + 100, sy + 17)) == (239, 231, 231)
            assert rgb.getpixel((sx + 100, sy + 18)) == (16, 8, 8)
            assert rgb.getpixel((sx + 184, sy + 2)) == (198, 189, 181)
            assert rgb.getpixel((sx + 231, sy + 11)) == (198, 189, 181)

    for name in ("city_edge_top_left_p0", "city_edge_top_right_p0"):
        with Image.open(GBA / "graphics" / "ui" / f"{name}.bmp") as image:
            assert image.size == (8, 32)
            pixels = image.load()
            visible_columns = [
                x for x in range(image.width)
                if any(pixels[x, y] != 0 for y in range(image.height))
            ]
            assert len(visible_columns) == 1


def test_perfect_landing_feedback_and_continue_prompts_are_wired() -> None:
    ui = GBA / "graphics" / "ui"
    assert (ui / "combo_seam_flash_white_p0.bmp").is_file()
    assert (ui / "combo_seam_flash_white_p0.json").is_file()
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
        assert "perfect_landing_star_trail_elapsed" in source
        assert "_perfect_landing_seed" in source
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


def test_build_city_top_bar_is_only_three_pixels_taller() -> None:
    backgrounds = GBA / "graphics" / "backgrounds"
    for theme in range(4):
        with Image.open(backgrounds / f"city_bg_theme_{theme}.bmp") as image:
            rgb = image.convert("RGB")
            # Exact 240x160 JAR shell: two dark rows, pale separator, final dark line, then sky.
            assert rgb.getpixel((128, 48 + 15)) == (33, 24, 16)
            assert rgb.getpixel((128, 48 + 16)) == (33, 24, 16)
            assert rgb.getpixel((128, 48 + 17)) == (239, 231, 231)
            assert rgb.getpixel((128, 48 + 18)) == (16, 8, 8)
            assert rgb.getpixel((128, 48 + 19)) != (16, 8, 8)





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

