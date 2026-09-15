from __future__ import annotations

import json
from pathlib import Path

from PIL import Image


def _root() -> Path:
    return Path(__file__).resolve().parents[1]


def _palette_rgb(path: Path, entries: int = 256) -> tuple[int, ...]:
    image = Image.open(path)
    palette = image.getpalette()
    assert palette is not None
    return tuple(palette[: entries * 3])


def test_build_city_browser_uses_preview_frame_and_type_highlight() -> None:
    scene = (_root() / "gba/src/build_city_scene.cpp").read_text()
    assert "building_asset(type, 3)" in scene
    assert "*lot_assets[selected - 1]" in scene
    assert "type == snapshot.selected_building_type ? 1 : 3" not in scene


def test_selected_browser_tower_and_outline_render_in_front_of_neighbor_previews() -> None:
    scene = (_root() / "gba/src/build_city_scene.cpp").read_text()
    assert "constexpr int selected_preview_outline_z_order = -2;" in scene
    assert "constexpr int selected_preview_tower_z_order = -3;" in scene
    assert "if(type == selected)" in scene
    assert "selected_preview_outline_z_order" in scene
    assert "selected_preview_tower_z_order" in scene


def test_build_city_cursor_and_saved_buildings_match_recovered_grid_anchors() -> None:
    scene = (_root() / "gba/src/build_city_scene.cpp").read_text()
    # Stored buildings: recovered Java source-left/baseline contract.
    assert "grid_screen_left + 5 + column * grid_spacing" in scene
    assert "grid_screen_top + 15 + row * grid_spacing" in scene

    # Resource 28 frame 4 is a 23x23 logical canvas whose opaque 14x14
    # placement square begins at source (0, 9).  Its logical canvas therefore
    # needs to begin at (lot_left, lot_top - 9), not be centered on the lot.
    assert "cursor_canvas_left = cell_screen_left + snapshot.cursor_column * grid_spacing" in scene
    assert "cursor_canvas_top = cell_screen_top - 9 + snapshot.cursor_row * grid_spacing" in scene
    assert "*lot_assets[4]" in scene
    assert "snapshot.placement_valid ? 4 : 3" not in scene


def test_city_lot_resource_frame_geometry_matches_highlight_and_grid_contract() -> None:
    root = _root()
    manifest = json.loads((root / "gba/reference/ui_assets_manifest.json").read_text())
    buildings = {r["name"]: r for r in manifest["city_assets"]["buildings"]}
    lots = manifest["city_assets"]["lots"]

    # Resource 28 f0..f3 are larger silhouettes for the four selector previews.
    for tower_type in range(1, 5):
        building = buildings[f"city_building_{tower_type}_f3"]
        lot = lots[tower_type - 1]
        assert lot["width"] == 23 and lot["height"] == 23
        # The recovered selector composition places the building 2 px from the
        # highlight's logical left edge and with baseline at highlight_top+21.
        assert 23 - building["width"] >= 4
        assert 23 - building["height"] >= 4

    # f4 contains the 14x14 placement square at logical source (0, 9).
    frame4 = lots[4]
    assert frame4["width"] == 23 and frame4["height"] == 23
    assert frame4["parts"][0]["source_x"] == 0
    assert frame4["parts"][0]["source_y"] == 9

    image = Image.open(root / "gba/graphics/ui/city_lot_f4_p0.bmp")
    data = list(image.get_flattened_data())
    xs: list[int] = []
    ys: list[int] = []
    for y in range(image.height):
        for x in range(image.width):
            if data[y * image.width + x]:
                xs.append(x)
                ys.append(y)
    assert (min(xs), min(ys), max(xs) + 1, max(ys) + 1) == (0, 0, 14, 14)


def test_build_city_building_variants_share_one_bpp8_palette() -> None:
    root = _root()
    manifest = json.loads((root / "gba/reference/ui_assets_manifest.json").read_text())
    records = manifest["city_assets"]["buildings"]
    assert all(record["bpp"] == 8 for record in records)
    signatures = set()
    for record in records:
        for part in record["parts"]:
            signatures.add(_palette_rgb(root / "gba/graphics/ui" / f"{part['asset']}.bmp"))
    assert len(signatures) == 1


def test_each_tower_family_uses_one_shared_bpp8_palette() -> None:
    root = _root()
    manifest = json.loads((root / "gba/reference/generated_assets_manifest.json").read_text())
    by_id = {record["mesh_id"]: record for record in manifest["meshes"]}
    for family in ((10, 30, 40), (11, 31, 41), (12, 32, 42), (13, 33, 43)):
        signatures = set()
        for mesh_id in family:
            record = by_id[mesh_id]
            assert record["bpp"] == 8
            for part in record["parts"]:
                signatures.add(_palette_rgb(root / "gba/graphics/gameplay" / f"{part['asset']}.bmp"))
        assert len(signatures) == 1, family


def test_worker_frames_are_layered_4bpp_without_bpp8_obj_conflicts() -> None:
    root = _root()
    manifest = json.loads((root / "gba/reference/ui_assets_manifest.json").read_text())
    records = [
        *manifest["menu_assets"]["worker_blue_frames"],
        *manifest["menu_assets"]["worker_red_frames"],
    ]
    for record in records:
        assert record["bpp"] == 4
        assert len(record["parts"]) == 2
        for part in record["parts"]:
            metadata = json.loads((root / "gba/graphics/ui" / f"{part['asset']}.json").read_text())
            assert metadata["bpp_mode"] == "bpp_4"


def test_clean_mesh_export_regenerates_special_roof_cable() -> None:
    exporter = (_root() / "tools/tower_bloxx_extract/gba_project_export.py").read_text()
    assert "crane_special_cable_segment" in exporter


def test_build_city_first_placement_still_has_palette_slot_headroom_with_shared_bpp8() -> None:
    root = _root()
    graphics = root / "gba/graphics/ui"
    live_assets = (
        "city_valid_lot_ring_p0", "city_lot_f4_p0", "city_building_1_f0_p0",
        "city_edge_top_left_p0", "city_edge_top_right_p0", "city_edge_bottom_left_p0",
        "city_edge_bottom_right_p0", "city_population_icon_p0", "city_status_panel_f0_p0",
        "city_status_panel_f3_p0", "hud_brown_digit_f0_p0", "city_status_placement_p0",
        "city_comparison_panel_active_p0", "city_type_badge_1_p0", "hud_white_digit_f1_p0",
        "hud_white_digit_f0_p0", "tower_font",
    )
    bpp4_signatures: set[tuple[int, ...]] = set()
    bpp8_slots = 0
    for asset in live_assets:
        metadata = json.loads((graphics / f"{asset}.json").read_text())
        if metadata["bpp_mode"] == "bpp_4":
            bpp4_signatures.add(_palette_rgb(graphics / f"{asset}.bmp", 16))
        else:
            colors_count = int(metadata["colors_count"])
            bpp8_slots = max(bpp8_slots, colors_count // 16)
    total_slots = bpp8_slots + len(bpp4_signatures)
    assert total_slots <= 13, f"first placement uses {total_slots}/16 OBJ palette slots"
