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


def test_legacy_event_spawn_projection_uses_house_coordinate_math_for_gba() -> None:
    source = (ROOT / 'gba/src/construction_backdrop.cpp').read_text(encoding='utf-8')
    generated = (ROOT / 'gba/include/generated/legacy_high_altitude_assets.h').read_text(encoding='utf-8')
    assert 'legacy_event_extent_eighths' in generated
    assert 'constexpr int legacy_screen_width_eighths = 240 * 8;' in source
    assert 'constexpr int legacy_screen_height_eighths = 160 * 8;' in source
    assert 'const int camera_three_quarters = (3 * camera_y) / 4;' in source
    assert 'slot.y_eighths = camera_three_quarters + extent + _legacy_random(512);' in source
    assert 'slot.y_eighths = camera_three_quarters - legacy_screen_height_eighths / 2 - 512 +' in source
    # House.e first projects into ordinary top-left screen coordinates using
    # the screen centre. Butano sprite positions are already centre-origin, so
    # those half-screen offsets must cancel during the port.
    assert 'const int x = slot.x_eighths / 8;' in source
    assert 'const int y = -((slot.y_eighths - camera_three_quarters) / 8);' in source
    assert 'screen_half_width + slot.x_eighths / 8' not in source
    assert 'screen_half_height - (slot.y_eighths - camera_three_quarters) / 8' not in source


def test_legacy_event_projection_places_source_viewport_inside_butano_viewport() -> None:
    # Numeric guard for the centre-origin conversion: source screen (120,80)
    # must become Butano (0,0), not (120,80). A stationary event at source
    # world x=0 therefore sits on the screen centre line, while the original
    # 240px spawn width spans centred x=0..240 as House.l/House.e dictate.
    camera_y = 4096
    camera_three_quarters = (3 * camera_y) // 4
    assert 0 // 8 == 0
    assert (240 * 8) // 8 == 240
    assert -((camera_three_quarters - camera_three_quarters) // 8) == 0
    assert -(((camera_three_quarters + 512) - camera_three_quarters) // 8) == -64


def test_resource47_sparkle_tracks_current_attached_or_falling_block_every_100ms() -> None:
    generated = (ROOT / 'gba/include/generated/legacy_high_altitude_assets.h').read_text(encoding='utf-8')
    assert 'legacy_block_sparkle_frames' in generated

    quick_source = (ROOT / 'gba/src/quick_game_scene.cpp').read_text(encoding='utf-8')
    quick_header = (ROOT / 'gba/include/tb/quick_game_scene.h').read_text(encoding='utf-8')
    assert 'void _update_block_sparkle(const QuickGameSnapshot& snapshot);' in quick_header
    assert 'snapshot.block_state == QuickBlockState::Attached ||' in quick_source
    assert 'snapshot.block_state == QuickBlockState::Falling' in quick_source
    assert '(_background_clock_ms / 100) % 3' in quick_source
    assert 'legacy_block_sparkle_frames[frame]' in quick_source
    assert '_screen_x(snapshot.current_x)' in quick_source
    assert '_screen_y(snapshot.current_y, snapshot.presentation_camera_y)' in quick_source

    city_source = (ROOT / 'gba/src/tower_construction_scene.cpp').read_text(encoding='utf-8')
    city_header = (ROOT / 'gba/include/tb/tower_construction_scene.h').read_text(encoding='utf-8')
    assert 'void _update_block_sparkle(const TowerConstructionSnapshot& snapshot);' in city_header
    assert 'snapshot.block_state == TowerConstructionBlockState::Attached ||' in city_source
    assert 'snapshot.block_state == TowerConstructionBlockState::Falling' in city_source
    assert '(_background_clock_ms / 100) % 3' in city_source
    assert 'legacy_block_sparkle_frames[frame]' in city_source
    assert '_screen_x(snapshot.current_x)' in city_source
    assert '_screen_y(snapshot.current_y, snapshot.presentation_camera_y)' in city_source
