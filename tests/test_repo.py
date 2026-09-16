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
    assert "TowerConstructionBlockState::Raising" in test
    assert "snapshot.rope_length == 0" in test
    assert "snapshot.rope_length == 1664" in test
    assert "elapsed_ms >= 2500" in test
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
