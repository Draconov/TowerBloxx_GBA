from __future__ import annotations

import zipfile

from tower_bloxx_extract.m3g import Image2D, parse_m3g
from tower_bloxx_extract.m3g_geometry import decode_image_rgba, resolve_mesh
from tower_bloxx_extract.resources import read_resource

GAME_MESH_IDS = (7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43)


def _scene(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        return parse_m3g(read_resource(jar, 45))


def test_all_game_meshes_resolve_to_valid_triangles(tower_bloxx_jar):
    scene = _scene(tower_bloxx_jar)
    resolved = {user_id: resolve_mesh(scene, user_id) for user_id in GAME_MESH_IDS}
    assert resolved[7].vertex_count == 11
    assert resolved[7].triangle_count == 6
    assert resolved[9].vertex_count == 132
    assert resolved[9].triangle_count == 44
    assert resolved[31].vertex_count == 224
    assert resolved[31].triangle_count == 143
    assert resolved[41].vertex_count == 613
    assert resolved[41].triangle_count == 300
    for mesh in resolved.values():
        assert len(mesh.positions) == mesh.vertex_count
        for submesh in mesh.submeshes:
            for triangle in submesh.triangles:
                assert min(triangle) >= 0
                assert max(triangle) < mesh.vertex_count


def test_all_embedded_images_decode_to_rgba(tower_bloxx_jar):
    scene = _scene(tower_bloxx_jar)
    images = [obj for obj in scene.objects[1:] if isinstance(obj, Image2D)]
    assert len(images) == 17
    for image in images:
        width, height, rgba = decode_image_rgba(scene, image.object_index)
        assert (width, height) == (image.width, image.height)
        assert len(rgba) == width * height * 4
    assert (images[0].format, images[0].width, images[0].height) == (99, 64, 64)
    assert len(images[0].palette) == 54
    assert (images[10].format, images[10].width, images[10].height) == (100, 16, 16)
    assert len(images[10].palette) == 0


def test_meshes_use_at_most_one_texture_unit(tower_bloxx_jar):
    scene = _scene(tower_bloxx_jar)
    for user_id in GAME_MESH_IDS:
        mesh = resolve_mesh(scene, user_id)
        for submesh in mesh.submeshes:
            assert submesh.texture_index is None or submesh.image_index is not None


def test_mesh_catalog_is_stable(tower_bloxx_jar):
    from tower_bloxx_extract.m3g_geometry import build_mesh_catalog
    import hashlib
    import json

    scene = _scene(tower_bloxx_jar)
    catalog = build_mesh_catalog(scene)
    assert [entry["user_id"] for entry in catalog] == list(GAME_MESH_IDS)
    payload = (json.dumps(catalog, sort_keys=True, separators=(",", ":")) + "\n").encode()
    assert hashlib.sha256(payload).hexdigest() == "97519ab3314949a19e4e4ebb8b24d8db2a26d795395df7744027105a85a5b40e"
