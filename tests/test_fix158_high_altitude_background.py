from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_shared_construction_backdrop_owns_layered_sky_and_scenery() -> None:
    header = (ROOT / "gba/include/tb/construction_backdrop.h").read_text(encoding="utf-8")
    source = (ROOT / "gba/src/construction_backdrop.cpp").read_text(encoding="utf-8")

    assert "class ConstructionBackdrop" in header
    assert "void start(int camera_y, int clock_ms);" in header
    assert "void update(int camera_y, int clock_ms);" in header
    assert "void reset();" in header
    assert "bn::optional<bn::regular_bg_ptr> _sky_background;" in header
    assert "bn::optional<bn::regular_bg_ptr> _scenery_background;" in header

    for index in range(17):
        assert f"bn_regular_bg_items_construction_sky_{index:02d}.h" in source
    for index in range(3):
        assert f"bn_regular_bg_items_construction_scenery_{index}.h" in source
    assert "bn_sprite_items_construction_high_blink_p0.h" in source
    assert "generated/construction_background_data.h" in source
    assert "generated/legacy_high_altitude_assets.h" in source


def test_runtime_sky_math_matches_house_and_cycles_9_through_16() -> None:
    source = (ROOT / "gba/src/construction_backdrop.cpp").read_text(encoding="utf-8")
    assert "const int scaled = (2 * camera_y) / 3;" in source
    assert "const int band = scaled / 2048;" in source
    assert "const int horizon = (22 * (scaled % 2048)) >> 8;" in source
    assert "return band <= 16 ? band : 9 + ((band - 9) % 8);" in source
    assert "_sky_background->set_y(horizon - 80);" in source


def test_runtime_scenery_uses_camera_scroll_chunks_and_hides_without_wrap() -> None:
    source = (ROOT / "gba/src/construction_backdrop.cpp").read_text(encoding="utf-8")
    assert "const int scroll = ((camera_y - 512) * 22) / 256;" in source
    assert "construction_scenery_chunk_centers" in source
    assert "construction_scenery_max_scroll" in source
    assert "_scenery_background.reset();" in source
    assert "_scenery_background->set_y(scroll - chunk_center);" in source


def test_high_altitude_type1_blink_uses_source_phase_and_background_priority() -> None:
    source = (ROOT / "gba/src/construction_backdrop.cpp").read_text(encoding="utf-8")
    assert "((clock_ms + decoration.world_y) / 200) % 2 == 0" in source
    assert "decoration.kind == 1" in source
    assert "blink.set_bg_priority(2);" in source
    assert "blink.set_position" in source
    assert "_update_legacy_events(camera_y, clock_ms);" in source
    assert "legacy_event_min_band" in source
    assert "HighAltitudeEventKind" not in source


def test_both_tower_scenes_delegate_to_shared_construction_backdrop() -> None:
    for header_name, source_name in (
        ("quick_game_scene.h", "quick_game_scene.cpp"),
        ("tower_construction_scene.h", "tower_construction_scene.cpp"),
    ):
        header = (ROOT / "gba/include/tb" / header_name).read_text(encoding="utf-8")
        source = (ROOT / "gba/src" / source_name).read_text(encoding="utf-8")
        assert '#include "tb/construction_backdrop.h"' in header
        assert "ConstructionBackdrop _backdrop;" in header
        assert "int _background_clock_ms = 0;" in header
        assert "_backdrop.start(snapshot.presentation_camera_y, _background_clock_ms);" in source
        assert "_backdrop.update(snapshot.presentation_camera_y, _background_clock_ms);" in source
        assert "_backdrop.reset();" in source
        assert "construction_bg_b1" not in source
        assert "_background_band" not in header


def test_bulldozer_offset_and_two_frame_visibility_are_preserved() -> None:
    source = (ROOT / "gba/src/build_city_scene.cpp").read_text(encoding="utf-8")
    assert "discard_building_center_x = discard_cell_center_x + 2" in source
    assert "discard_building_center_y = discard_cell_center_y - 2" in source
    assert "effect_frame >= 4" in source


def test_high_altitude_audit_records_legacy_jar_event_reference() -> None:
    markdown = (ROOT / "reference/JAR_PARITY_AUDIT.md").read_text(encoding="utf-8")
    audit = (ROOT / "reference/jar_parity_audit.json").read_text(encoding="utf-8")
    assert "all 17 recovered colors" in markdown
    assert "88-entry resource-43" in markdown
    assert "12 `House.v()` procedural rooftop columns" in markdown
    assert "v1.3.37" in markdown
    assert "resources 50-76" in markdown
    assert '"sky_colors": 17' in audit
    assert '"resource43_entries": 88' in audit
    assert '"procedural_rooftop_decorations": 12' in audit
    assert '"legacy_1337_high_altitude_reference"' in audit
