from __future__ import annotations

import math
import zipfile

from tower_bloxx_extract.m3g import Camera, Mesh, World, parse_m3g
from tower_bloxx_extract.resources import read_resource


def _scene(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        return parse_m3g(read_resource(jar, 45))


def test_canonical_object_inventory(tower_bloxx_jar):
    scene = _scene(tower_bloxx_jar)
    assert scene.type_counts() == {
        0: 1,
        1: 1,
        3: 44,
        5: 1,
        6: 3,
        8: 2,
        10: 17,
        11: 44,
        13: 44,
        14: 19,
        17: 17,
        20: 57,
        21: 19,
        22: 1,
    }
    assert isinstance(scene.world, World)
    assert scene.world.user_id == 269
    assert scene.world.children == tuple(range(251, 270))
    assert scene.world.active_camera == 206
    assert scene.world.background == 0


def test_mesh_user_ids_and_object_indices(tower_bloxx_jar):
    scene = _scene(tower_bloxx_jar)
    meshes = [obj for obj in scene.objects[1:] if isinstance(obj, Mesh)]
    assert [mesh.object_index for mesh in meshes] == list(range(251, 270))
    assert [mesh.user_id for mesh in meshes] == [
        31, 10, 30, 32, 33, 40, 41, 42, 43, 8,
        7, 20, 11, 12, 13, 21, 22, 23, 9,
    ]
    assert set(scene.meshes_by_user_id()) == {
        7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43
    }


def test_stored_camera_decodes_exactly(tower_bloxx_jar):
    scene = _scene(tower_bloxx_jar)
    camera = scene.objects[206]
    assert isinstance(camera, Camera)
    assert camera.user_id == 224
    assert camera.projection_type == 50
    assert math.isclose(camera.fovy, 45.0)
    assert math.isclose(camera.aspect_ratio, 1.0)
    assert math.isclose(camera.near, 20.625749588012695)
    assert math.isclose(camera.far, 20625.74609375)
    assert camera.translation == (4712.09814453125, 364.46380615234375, 3850.1484375)
