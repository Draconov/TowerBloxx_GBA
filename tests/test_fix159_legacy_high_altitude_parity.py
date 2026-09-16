from __future__ import annotations

from io import BytesIO
from pathlib import Path
import os
import struct
import zipfile

import pytest

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
LEGACY_JAR_ENV = 'TOWER_BLOXX_LEGACY_JAR'


def _legacy_jar_path() -> Path:
    value = os.environ.get(LEGACY_JAR_ENV)
    if not value:
        pytest.skip(f'{LEGACY_JAR_ENV} is not set; CI validates the checked-in JAR-derived export instead')
    path = Path(value)
    if not path.is_file():
        pytest.skip(f'{LEGACY_JAR_ENV} does not point to an available file: {path}')
    return path


def _legacy_resources() -> dict[int, bytes]:
    with zipfile.ZipFile(_legacy_jar_path()) as jar:
        raw = jar.read('r0')
        table_bytes = struct.unpack('>i', raw[:4])[0]
        assert table_bytes == 392
        table = struct.unpack(f'>{table_bytes // 4}i', raw[:table_bytes])
        resources: dict[int, bytes] = {}
        for resource_id in range(len(table) - 1):
            start = table[resource_id]
            if start < 0:
                resources[resource_id] = jar.read(str(resource_id))
                continue
            end = len(raw)
            for value in table[resource_id + 1:]:
                if value >= 0 and value > start:
                    end = value
                    break
            resources[resource_id] = raw[start:end]
        return resources


def test_legacy_jar_contains_real_combo_and_high_altitude_art() -> None:
    resources = _legacy_resources()
    expected_sizes = {
        46: (88, 22), 47: (132, 44),
        50: (162, 20), 51: (171, 20), 52: (45, 14), 53: (23, 29),
        54: (59, 12), 55: (90, 16), 56: (24, 7), 57: (155, 7),
        58: (169, 10), 59: (102, 5), 60: (24, 7), 61: (34, 11),
        62: (44, 44), 63: (20, 16), 64: (3, 3), 65: (15, 15),
        66: (41, 41), 67: (10, 10), 68: (168, 32), 69: (73, 73),
        70: (103, 9), 71: (23, 18), 72: (115, 49), 73: (53, 98),
        74: (94, 43), 75: (29, 29), 76: (123, 54),
    }
    for resource_id, expected_size in expected_sizes.items():
        image = Image.open(BytesIO(resources[resource_id]))
        assert image.size == expected_size


def test_checked_in_legacy_high_altitude_export_exists() -> None:
    graphics = ROOT / 'gba/graphics/ui'
    # Real resource 46: four 22x22 combo-star frames.
    for frame in range(4):
        assert (graphics / f'legacy_combo_star_f{frame}_p0.bmp').is_file()
    # Real resource 47: three 44x44 active-block sparkle frames.
    for frame in range(3):
        assert (graphics / f'legacy_block_sparkle_f{frame}_p0.bmp').is_file()
    # Representative real sky/event resources from the 208x208 JAR.
    for name in ('plane', 'balloon', 'moon', 'mars', 'jupiter', 'saturn', 'uranus', 'neptune', 'whale'):
        assert any(graphics.glob(f'legacy_sky_{name}_f0_p*.bmp')), name
    assert (ROOT / 'gba/include/generated/legacy_high_altitude_assets.h').is_file()


def test_runtime_uses_legacy_event_band_rules_instead_of_fixed_made_up_positions() -> None:
    source = (ROOT / 'gba/src/construction_backdrop.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'gba/include/tb/construction_backdrop.h').read_text(encoding='utf-8')
    assert 'generated/legacy_high_altitude_assets.h' in source
    assert 'legacy_event_min_band' in source
    assert 'legacy_event_max_band' in source
    assert 'legacy_event_spawn_chance' in source
    assert 'legacy_event_x_speed' in source
    generated = (ROOT / 'gba/include/generated/legacy_high_altitude_assets.h').read_text(encoding='utf-8')
    assert 'legacy_event_resource_ids' in generated
    assert 'HighAltitudeEventKind' not in source
    assert 'void _update_legacy_events(int camera_y, int clock_ms);' in header
    # Source House.e has nine live event slots.
    assert 'bn::vector<LegacySkyEventSlot, 9>' in header


def test_combo_star_uses_resource46_four_frame_source_animation() -> None:
    source = (ROOT / 'gba/src/quick_game_scene.cpp').read_text(encoding='utf-8')
    assert 'legacy_combo_star_frames' in source
    assert '(_background_clock_ms / 80) % 4' in source
    assert '_combo_star_sprite' in source
    assert '_combo_star_affine_mat' not in source


def _java_shift_eighths(value: int) -> int:
    # House.e multiplies world deltas by 32 then arithmetic-shifts by 8,
    # which is exactly an arithmetic >> 3 in the stored 1/8-pixel space.
    return value >> 3


def test_legacy_event_spawn_projection_uses_house_coordinate_math_for_gba() -> None:
    source = (ROOT / 'gba/src/construction_backdrop.cpp').read_text(encoding='utf-8')
    generated = (ROOT / 'gba/include/generated/legacy_high_altitude_assets.h').read_text(encoding='utf-8')
    assert 'legacy_event_extent_eighths' in generated
    assert 'constexpr int legacy_screen_width_eighths = 240 * 8;' in source
    assert 'constexpr int legacy_screen_height_eighths = 160 * 8;' in source
    assert 'const int camera_three_quarters = (3 * camera_y) / 4;' in source
    assert 'slot.y_eighths = camera_three_quarters + extent + _legacy_random(512);' in source
    assert 'slot.y_eighths = camera_three_quarters - legacy_screen_height_eighths / 2 - 512 +' in source
    # House.e projects to ordinary top-left coordinates first.  The GBA port
    # then subtracts the 120x80 screen centre to obtain Butano coordinates.
    assert 'const int source_screen_x = screen_half_width + (slot.x_eighths >> 3);' in source
    assert 'const int source_screen_y = screen_half_height + ((camera_three_quarters - slot.y_eighths) >> 3);' in source
    assert 'const int x = source_screen_x - screen_half_width;' in source
    assert 'const int y = source_screen_y - screen_half_height;' in source


def test_legacy_event_projection_maps_java_viewport_to_butano_center_origin() -> None:
    # Exact House.e arithmetic-shift behavior, including negative coordinates.
    camera_y = 4096
    camera_three_quarters = (3 * camera_y) // 4

    def project(x_eighths: int, y_eighths: int) -> tuple[int, int]:
        source_x = 120 + _java_shift_eighths(x_eighths)
        source_y = 80 + _java_shift_eighths(camera_three_quarters - y_eighths)
        return source_x - 120, source_y - 80

    assert project(0, camera_three_quarters) == (0, 0)
    assert project(-960, camera_three_quarters) == (-120, 0)
    assert project(952, camera_three_quarters) == (119, 0)
    assert project(0, camera_three_quarters + 640) == (0, -80)
    assert project(0, camera_three_quarters - 632) == (0, 79)
    # Negative non-multiples prove Java's arithmetic shift is preserved rather
    # than C++ integer division truncating toward zero.
    assert project(-1, camera_three_quarters) == (-1, 0)
    assert project(0, camera_three_quarters + 1) == (0, -1)


def test_legacy_event_lifecycle_matches_house_k_bounds_and_respawn_delay() -> None:
    source = (ROOT / 'gba/src/construction_backdrop.cpp').read_text(encoding='utf-8')
    # House.k clears only after an event has passed the horizontal edges or
    # fallen below the screen.  Events above the viewport must survive while
    # the camera climbs toward them; event-band changes do not kill them.
    assert 'slot.x_eighths > legacy_screen_width_eighths + extent' in source
    assert 'slot.x_eighths < -extent' in source
    assert 'camera_three_quarters - slot.y_eighths > legacy_screen_height_eighths + extent' in source
    assert 'if(outside_source_bounds)' in source
    update_body = source[source.index('void ConstructionBackdrop::_update_legacy_events'): ]
    assert 'const bool in_band =' not in update_body
    assert 'onscreen_or_approaching' not in update_body
    # House.k schedules the cleared slot at as + random(2000), not +2000.
    assert 'slot.next_spawn_ms = clock_ms + _legacy_random(2000);' in source
    assert 'clock_ms + 2000 + _legacy_random(2000)' not in source


def test_event_far_above_viewport_is_retained_until_camera_reaches_it() -> None:
    # Numeric guard for the source House.k vertical condition.  A stationary
    # moon/planet can be hundreds of source pixels above the viewport and must
    # remain alive. It is removed only once the camera has passed far enough
    # that the event is below the bottom edge.
    height_eighths = 160 * 8
    extent = 352  # Moon resource width/extent in the recovered source table.
    event_y = 4096 + 1200
    camera_three_quarters = 4096
    assert not (camera_three_quarters - event_y > height_eighths + extent)
    camera_three_quarters = event_y + height_eighths + extent + 1
    assert camera_three_quarters - event_y > height_eighths + extent


def test_resource47_sparkle_tracks_visible_landed_combo_floors_every_100ms() -> None:
    generated = (ROOT / 'gba/include/generated/legacy_high_altitude_assets.h').read_text(encoding='utf-8')
    assert 'legacy_block_sparkle_frames' in generated

    for source_name, header_name, snapshot_type, floor_accessor in (
        ('quick_game_scene.cpp', 'quick_game_scene.h', 'QuickGameSnapshot', '_game.floor'),
        ('tower_construction_scene.cpp', 'tower_construction_scene.h', 'TowerConstructionSnapshot', '_construction.floor'),
    ):
        source = (ROOT / 'gba/src' / source_name).read_text(encoding='utf-8')
        header = (ROOT / 'gba/include/tb' / header_name).read_text(encoding='utf-8')
        assert f'void _update_block_sparkle(const {snapshot_type}& snapshot);' in header
        body = source[source.index('::_update_block_sparkle'): source.index('::_rebuild_hud')]
        assert 'snapshot.combo_count' in body
        assert '_visible_floor_start' in body
        assert floor_accessor in body
        assert '_background_clock_ms / 100' in body
        assert 'legacy_block_sparkle_frames[frame]' in body
        assert 'snapshot.current_x' not in body
        assert 'snapshot.current_y' not in body

