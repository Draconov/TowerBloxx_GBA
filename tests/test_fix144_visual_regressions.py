from __future__ import annotations

import json
from pathlib import Path


def _root() -> Path:
    return Path(__file__).resolve().parents[1]


def _bmp_palette_entry(path: Path, index: int) -> tuple[int, int, int, int]:
    data = path.read_bytes()
    offset = 14 + 40 + index * 4
    blue, green, red, reserved = data[offset:offset + 4]
    return red, green, blue, reserved


def test_build_city_valid_lot_pulse_is_visible_in_browser_and_placement() -> None:
    scene = (_root() / "gba/src/build_city_scene.cpp").read_text()
    assert "pulse_building_type = snapshot.selected_building_type" in scene
    assert "pulse_building_type = snapshot.pending_building_type" in scene
    assert "build_city_valid_lot_rgb(pulse_building_type, _placement_flash_ms)" in scene
    assert "! snapshot.placement_committing" in scene


def test_build_city_selected_browser_slot_has_exact_orange_pulse_under_preview() -> None:
    root = _root()
    manifest = json.loads((root / "gba/reference/ui_assets_manifest.json").read_text())
    selector = manifest["city_assets"]["supplemental"]["selector_active_slot"]
    assert selector["name"] == "city_selector_active_slot"
    assert (selector["width"], selector["height"]) == (15, 15)
    part = selector["parts"][0]
    # #FEA100 quantized to the GBA's 5-bit channels by the exporter. Palette
    # sharing may move it away from index 1, so assert on rendered opaque pixels.
    from PIL import Image
    image = Image.open(root / "gba/graphics/ui" / f"{part['asset']}.bmp").convert("RGB")
    colors = {color: count for count, color in (image.getcolors(maxcolors=65536) or [])}
    assert colors.get((248, 160, 0)) == 225

    scene = (root / "gba/src/build_city_scene.cpp").read_text()
    assert "build_city_selector_slot_active(_selector_flash_ms)" in scene
    assert "generated::city_selector_active_slot" in scene
    assert "selector_screen_left + 2" in scene
    assert "selector_screen_top + 2 + (selected - 1) * 16" in scene


def test_placement_commit_uses_source_effect_windows_not_looping_rubble() -> None:
    scene = (_root() / "gba/src/build_city_scene.cpp").read_text()
    assert "build_city_placement_effect_frame(replacing, snapshot.placement_timer_ms)" in scene
    assert "(elapsed / 100) % 6" not in scene
    assert "snapshot.pending_building_type >= 1 && snapshot.pending_building_type <= 4 &&" in scene
    assert "! snapshot.placement_committing" in scene
    assert "snapshot.placement_transition_ms == 0 && ! snapshot.placement_committing" in scene


def test_placement_commit_status_does_not_claim_valid_lot_is_invalid() -> None:
    scene = (_root() / "gba/src/build_city_scene.cpp").read_text()
    assert "snapshot.placement_committing || snapshot.placement_valid" in scene


def test_name_entry_highlights_the_entire_selected_row_and_centers_name_columns() -> None:
    shell = (_root() / "gba/src/ui_shell.cpp").read_text()
    assert "const int selected_row = controller.name_cursor() / columns;" in shell
    assert "_show_composite(generated::menu_highlight, 0, selected_row_y, 100);" in shell
    assert "_show_composite(generated::menu_highlight, x, y, 100);" not in shell
    assert "constexpr int name_column_offset" in shell
    assert "-name_column_offset, name_line_y" in shell
    assert "name_column_offset, name_line_y" in shell


def test_browser_pulses_keep_obj_palette_headroom() -> None:
    from PIL import Image

    root = _root()
    graphics = root / "gba/graphics/ui"
    live_assets = (
        "city_valid_lot_ring_p0", "city_selector_active_slot_p0", "city_lot_f0_p0",
        "city_building_1_f3_p0", "city_edge_top_left_p0", "city_edge_top_right_p0",
        "city_edge_bottom_left_p0", "city_edge_bottom_right_p0", "city_population_icon_p0",
        "city_status_panel_f0_p0", "city_status_panel_f3_p0", "hud_brown_digit_f0_p0",
        "city_status_browse_p0", "city_progress_segment_p0", "tower_font",
    )

    def palette_signature(asset: str, entries: int) -> tuple[int, ...]:
        image = Image.open(graphics / f"{asset}.bmp")
        palette = image.getpalette()
        assert palette is not None
        return tuple(palette[: entries * 3])

    bpp4_signatures: set[tuple[int, ...]] = set()
    bpp8_slots = 0
    for asset in live_assets:
        metadata = json.loads((graphics / f"{asset}.json").read_text())
        if metadata["bpp_mode"] == "bpp_4":
            bpp4_signatures.add(palette_signature(asset, 16))
        else:
            bpp8_slots = max(bpp8_slots, int(metadata["colors_count"]) // 16)

    # The new orange slot shares the lot/badge palette rather than consuming
    # another OBJ bank; browser worst-case retains the same three-bank margin.
    assert palette_signature("city_selector_active_slot_p0", 16) == palette_signature("city_lot_f0_p0", 16)
    total_slots = bpp8_slots + len(bpp4_signatures)
    assert total_slots <= 13, f"Build City browser uses {total_slots}/16 OBJ palette slots"
