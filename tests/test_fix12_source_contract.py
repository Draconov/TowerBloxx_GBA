from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_quick_game_scene_owns_and_renders_house_workers() -> None:
    header = (ROOT / "gba/include/tb/quick_game_scene.h").read_text()
    source = (ROOT / "gba/src/quick_game_scene.cpp").read_text()
    assert '#include "tb/gameplay_workers.h"' in header
    assert "GameplayWorkerField _gameplay_workers" in header
    assert "_gameplay_workers.reset()" in source
    assert "_gameplay_workers.begin_frame(delta_ms)" in source
    assert "spawn_for_landing" in source
    assert "scatter_floor" in source
    assert "GameplayWorkerField::source_frame" in source
    assert "_worker_sprites.clear()" in source
    assert "_rebuild_worker_sprites" in source


def test_tower_construction_scene_owns_and_renders_house_workers() -> None:
    header = (ROOT / "gba/include/tb/tower_construction_scene.h").read_text()
    source = (ROOT / "gba/src/tower_construction_scene.cpp").read_text()
    assert '#include "tb/gameplay_workers.h"' in header
    assert "GameplayWorkerField _gameplay_workers" in header
    assert "_gameplay_workers.reset()" in source
    assert "_gameplay_workers.begin_frame(delta_ms)" in source
    assert "spawn_for_landing" in source
    assert "scatter_floor" in source
    assert "GameplayWorkerField::source_frame" in source
    assert "_worker_sprites.clear()" in source
    assert "_rebuild_worker_sprites" in source


def test_worker_render_uses_original_resource_11_12_frames() -> None:
    quick = (ROOT / "gba/src/quick_game_scene.cpp").read_text()
    construction = (ROOT / "gba/src/tower_construction_scene.cpp").read_text()
    for source in (quick, construction):
        assert "menu_worker_blue_f0" in source
        assert "menu_worker_red_f0" in source
        assert "worker.variant == 1" in source
        assert "_worker_sprites, 10)" in source
