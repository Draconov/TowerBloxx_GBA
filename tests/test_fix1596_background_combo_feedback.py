from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BACKGROUNDS = ROOT / 'gba/graphics/backgrounds'


def _construction_background_names() -> list[str]:
    return [f'construction_sky_{index:02d}' for index in range(17)] + [
        f'construction_scenery_{index}' for index in range(3)
    ]


def test_all_construction_layers_share_one_palette_and_sky_never_uses_transparent_index() -> None:
    images = [Image.open(BACKGROUNDS / f'{name}.bmp') for name in _construction_background_names()]
    palettes = [tuple(image.getpalette() or ()) for image in images]
    assert all(palette == palettes[0] for palette in palettes[1:])

    # Butano regular backgrounds treat palette index 0 as transparent.  The
    # shared construction palette reserves it for the alpha holes in scenery;
    # the opaque sky must therefore never reference index 0.
    for image in images[:17]:
        assert 0 not in set(image.get_flattened_data())


def test_resource47_combo_sparkles_follow_visible_landed_floors_not_the_crane_block() -> None:
    for source_name, floor_accessor in (
        ('quick_game_scene.cpp', '_game.floor'),
        ('tower_construction_scene.cpp', '_construction.floor'),
    ):
        source = (ROOT / 'gba/src' / source_name).read_text(encoding='utf-8')
        body = source[source.index('::_update_block_sparkle'): source.index('::_rebuild_hud')]
        assert 'snapshot.combo_count' in body
        assert '_visible_floor_start' in body
        assert floor_accessor in body
        assert 'legacy_block_sparkle_frames[frame]' in body
        assert '_background_clock_ms / 100' in body
        assert 'snapshot.current_x' not in body
        assert 'snapshot.current_y' not in body
        assert 'BlockState::Attached' not in body
        assert 'BlockState::Falling' not in body


def test_combo_population_bonus_matches_house_two_second_100ms_blink_with_plus_sign() -> None:
    for source_name in ('quick_game_scene.cpp', 'tower_construction_scene.cpp'):
        source = (ROOT / 'gba/src' / source_name).read_text(encoding='utf-8')
        assert 'combo_bonus_pending' in source
        assert 'combo_meter_ms > -2000' in source
        assert '(-snapshot.combo_meter_ms / 100) % 2 == 0' in source
        # House.a(..., 3, true) renders exactly three source-15 digits and then
        # source-15 cell 10, the plus marker.
        assert 'draw_source_number(snapshot.combo_bonus_pending, 3, 130, 10' in source
        assert 'hud_brown_digit_frames[10]' in source
