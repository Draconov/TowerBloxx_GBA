from __future__ import annotations

from pathlib import Path
import subprocess

from tower_bloxx_extract.gba_ui_export import export_gba_ui_assets

ROOT = Path(__file__).resolve().parents[1]


def _compile_and_run(tmp_path: Path, name: str, sources: list[Path]) -> str:
    output = tmp_path / name
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(ROOT / "gba/include"),
        *map(str, sources),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=ROOT, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    return ran.stdout


def test_crane_and_first_floor_presentation_contract_is_host_tested(tmp_path: Path) -> None:
    stdout = _compile_and_run(
        tmp_path,
        "crane_presentation_test",
        [ROOT / "tests/cpp/crane_presentation_test.cpp"],
    )
    assert stdout == "crane presentation ok\n"


def test_build_city_selection_uses_recovered_500ms_confirmation_pulse(tmp_path: Path) -> None:
    stdout = _compile_and_run(
        tmp_path,
        "build_city_selection_pulse_test",
        [
            ROOT / "gba/src/app_state.cpp",
            ROOT / "gba/src/build_city.cpp",
            ROOT / "gba/src/hall_of_fame.cpp",
            ROOT / "gba/src/save_data.cpp",
            ROOT / "tests/cpp/build_city_selection_pulse_test.cpp",
        ],
    )
    assert stdout == "build city selection pulse ok\n"


def test_scenes_use_initial_base_mesh_special_rig_and_source_selected_offset() -> None:
    quick = (ROOT / "gba/src/quick_game_scene.cpp").read_text()
    construction = (ROOT / "gba/src/tower_construction_scene.cpp").read_text()
    city = (ROOT / "gba/src/build_city_scene.cpp").read_text()

    for source in (quick, construction):
        assert "crane_presentation_mode(" in source
        assert "CranePresentationMode::Special" in source
        assert "CranePresentationMode::Hidden" in source
        assert "initial_base_mesh_id(" in source

    assert "floor_index == 0 ? initial_base_mesh_id" in quick
    assert "floor_index == 0 ? initial_base_mesh_id" in construction
    assert "build_city_preview_raised(selected, snapshot.selected_building_type," in city
    assert "build_city_preview_raised(type, snapshot.selected_building_type," in city
    assert "preview_left += 2;" in city
    assert "preview_baseline -= 2;" in city
    # Orange browser cursor exists even when the highlighted type is locked.
    assert "if(snapshot.selected_building_type >= 1 && snapshot.selected_building_type <= 4)" in city


def test_ui_export_emits_recovered_construction_meter_parts(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    project = tmp_path / "project"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)
    hud = manifest["hud_assets"]

    assert [record["name"] for record in hud["construction_meter_fills"]] == [
        "construction_meter_fill_1", "construction_meter_fill_2",
        "construction_meter_fill_3", "construction_meter_fill_4",
    ]
    assert hud["construction_meter_base"]["name"] == "construction_meter_base"
    assert hud["construction_meter_empty"]["name"] == "construction_meter_empty"
    assert [record["name"] for record in hud["construction_meter_rails"]] == [
        "construction_meter_rails_10", "construction_meter_rails_20",
        "construction_meter_rails_30", "construction_meter_rails_40",
    ]

    header = (project / "gba/include/generated/tower_ui_assets.h").read_text()
    for name in (
        "construction_meter_fill_1", "construction_meter_fill_4",
        "construction_meter_base", "construction_meter_empty",
        "construction_meter_rails_10", "construction_meter_rails_40",
    ):
        assert f"{name}_parts" in header


def test_construction_scene_draws_meter_at_recovered_240x160_geometry() -> None:
    source = (ROOT / "gba/src/tower_construction_scene.cpp").read_text()
    assert "const int meter_left = 11;" in source
    assert "const int meter_top = 150 - 2 * snapshot.target_height;" in source
    assert "const int row_top = 146 - 2 * slot;" in source
    assert "construction_meter_fill_frames[building_index]" in source
    assert "generated::construction_meter_empty" in source
    assert "generated::construction_meter_base" in source
    assert "construction_meter_rails_frames[building_index]" in source


def test_initial_base_and_later_tower_family_share_one_bpp8_palette(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    from PIL import Image
    from tower_bloxx_extract.gba_project_export import export_gba_project_assets

    project = tmp_path / "mesh_project"
    manifest = export_gba_project_assets(tower_bloxx_jar, project)
    by_id = {record["mesh_id"]: record for record in manifest["meshes"]}
    graphics = project / "gba/graphics/gameplay"

    for family in ((10, 20, 30, 40), (11, 21, 31, 41), (12, 22, 32, 42), (13, 23, 33, 43)):
        records = [by_id[mesh_id] for mesh_id in family]
        assert {record["bpp"] for record in records} == {8}
        assert len({record["palette_entries"] for record in records}) == 1

        palette_signatures = set()
        for record in records:
            for part in record["parts"]:
                image = Image.open(graphics / f"{part['asset']}.bmp")
                palette_signatures.add(tuple(image.getpalette() or ()))
                image.close()
        assert len(palette_signatures) == 1


def test_construction_common_hud_reuses_one_bpp4_palette(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    from PIL import Image

    project = tmp_path / "ui_palette_project"
    manifest = export_gba_ui_assets(tower_bloxx_jar, project)
    hud = manifest["hud_assets"]
    graphics = project / "gba/graphics/ui"

    records = [
        hud["population_icon"],
        hud["white_digits"][0],
        hud["construction_meter_base"],
        hud["construction_meter_empty"],
        *hud["construction_meter_rails"],
    ]
    signatures = set()
    for record in records:
        for part in record["parts"]:
            image = Image.open(graphics / f"{part['asset']}.bmp")
            signatures.add(tuple(image.getpalette() or ()))
            image.close()
    assert len(signatures) == 1


def test_special_crane_and_separate_cable_share_one_bpp4_palette(tower_bloxx_jar: Path, tmp_path: Path) -> None:
    from PIL import Image
    from tower_bloxx_extract.gba_project_export import export_gba_project_assets

    project = tmp_path / "special_crane_palette_project"
    export_gba_project_assets(tower_bloxx_jar, project)
    graphics = project / "gba/graphics/gameplay"

    signatures = set()
    for name in ("tb_mesh_007_p0", "crane_special_cable_segment"):
        image = Image.open(graphics / f"{name}.bmp")
        signatures.add(tuple(image.getpalette() or ()))
        image.close()
    assert len(signatures) == 1
