from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCENES = (
    ROOT / "gba/src/quick_game_scene.cpp",
    ROOT / "gba/src/tower_construction_scene.cpp",
)


def _function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


def test_invisible_dropped_block_releases_sprite_palette_before_crane_reappears() -> None:
    """A missed/slipped block must not keep its BPP8 tumble palette alive invisibly."""
    for path in SCENES:
        source = path.read_text(encoding="utf-8")
        rebuild = _function_body(
            source,
            "::_rebuild_current_sprites(",
            "::_ensure_crane_sprites(",
        )

        assert "if(! current_visible)" in rebuild
        hidden = rebuild[rebuild.index("if(! current_visible)") : rebuild.index("const int mesh_id", rebuild.index("if(! current_visible)"))]
        assert "_current_sprites.clear();" in hidden
        assert "_rendered_current_mesh_id = -1;" in hidden
        assert "_rendered_tumble_stage = 0;" in hidden
        assert "return;" in hidden

        # The release has to happen before the missed-state special rig asks
        # Butano for another BPP4 palette bank.
        update_start = source.index("::update(const InputFrame& input")
        release_call = source.index("_rebuild_current_sprites(snapshot);", update_start)
        crane_call = source.index("_ensure_crane_sprites(snapshot);", update_start)
        assert release_call < crane_call


def test_special_cable_is_updated_in_place_with_endpoint_overlap() -> None:
    """Avoid per-frame affine sprite churn and cover both inclusive Java2D line endpoints."""
    for path in SCENES:
        source = path.read_text(encoding="utf-8")
        body = _function_body(
            source,
            "::_rebuild_special_cable(",
            "::_update_world_positions",
        )

        mode_branch = body.index("if(mode != CranePresentationMode::Special)")
        clear_index = body.index("_special_cable_sprites.clear();")
        assert clear_index > mode_branch
        assert "if(_special_cable_sprites.empty())" in body
        assert body.count("crane_special_cable_segment.create_sprite") == 1
        assert "bn::sprite_ptr& sprite = _special_cable_sprites[0];" in body

        assert "constexpr int endpoint_overlap = 2;" in body
        assert "const int cable_draw_length = cable_length + endpoint_overlap * 2;" in body
        assert "sprite.set_vertical_scale(bn::fixed(cable_draw_length) / 64);" in body
