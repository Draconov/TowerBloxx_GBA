from __future__ import annotations

import hashlib
import zipfile

import pytest

from tower_bloxx_extract.m3g import parse_m3g
from tower_bloxx_extract.m3g_geometry import (
    ResolvedMesh,
    ResolvedSubmesh,
    decode_image_rgba,
    resolve_mesh,
)
from tower_bloxx_extract.m3g_render import (
    TextureRGBA,
    class_n_camera_transform,
    identity_matrix,
    post_rotate,
    post_translate,
    project_camera_point,
    render_mesh_reference,
    runtime_camera,
    sample_texture_nearest,
    transform_point,
)
from tower_bloxx_extract.resources import read_resource


def _submesh(triangles, *, image_index=None):
    return ResolvedSubmesh(
        index_buffer_index=1,
        appearance_index=1,
        triangles=tuple(triangles),
        texture_index=image_index,
        image_index=image_index,
        compositing_mode_index=0,
        polygon_mode_index=0,
    )


def _mesh(*, positions, texcoords=None, color=(255, 255, 255, 255), submeshes):
    return ResolvedMesh(
        user_id=999,
        object_index=999,
        vertex_buffer_index=999,
        positions=tuple(positions),
        texcoords=None if texcoords is None else tuple(texcoords),
        default_color=color,
        submeshes=tuple(submeshes),
        translation=(0.0, 0.0, 0.0),
        scale=(1.0, 1.0, 1.0),
        orientation_angle=0.0,
        orientation_axis=(0.0, 0.0, 1.0),
        matrix=None,
    )


def test_java_post_transform_order_is_translate_then_local_rotate():
    matrix = identity_matrix()
    matrix = post_translate(matrix, 10.0, 20.0, 30.0)
    matrix = post_rotate(matrix, 90.0, 0.0, 0.0, 1.0)

    x, y, z = transform_point(matrix, (2.0, 0.0, 0.0))
    assert x == pytest.approx(10.0, abs=1e-6)
    assert y == pytest.approx(22.0, abs=1e-6)
    assert z == pytest.approx(30.0, abs=1e-6)


def test_class_n_camera_transform_matches_identity_facing_minus_z():
    camera_transform = class_n_camera_transform(
        position=(0.0, 0.0, 0.0),
        direction=(0.0, 0.0, -1.0),
        up=(0.0, 1.0, 0.0),
    )
    assert camera_transform == pytest.approx(identity_matrix(), abs=1e-7)


def test_runtime_camera_uses_bytecode_proven_gba_projection_adaptation():
    camera = runtime_camera(240, 160)
    assert camera.base_fov == 60.0
    assert camera.fov_y == pytest.approx(46.0)
    assert camera.aspect_ratio == pytest.approx(1.35)
    assert camera.near == 10.0
    assert camera.far == 10000.0

    x, y, depth = project_camera_point(camera, (0.0, 0.0, -20.0))
    assert x == pytest.approx(120.0)
    assert y == pytest.approx(80.0)
    assert depth == pytest.approx(20.0)


def test_nearest_texture_sampling_clamps_and_preserves_alpha():
    texture = TextureRGBA(
        width=2,
        height=1,
        rgba=bytes((255, 0, 0, 255, 0, 0, 255, 0)),
    )
    assert sample_texture_nearest(texture, -2.0, 0.5) == (255, 0, 0, 255)
    assert sample_texture_nearest(texture, 0.24, 0.5) == (255, 0, 0, 255)
    assert sample_texture_nearest(texture, 0.76, 0.5) == (0, 0, 255, 0)
    assert sample_texture_nearest(texture, 4.0, 0.5) == (0, 0, 255, 0)


def test_rasterizer_depth_tests_and_skips_transparent_texels():
    positions = (
        (-8.0, -8.0, -20.0), (8.0, -8.0, -20.0), (0.0, 8.0, -20.0),
        (-8.0, -8.0, -30.0), (8.0, -8.0, -30.0), (0.0, 8.0, -30.0),
    )
    texcoords = ((0.0, 0.0), (0.0, 0.0), (0.0, 0.0)) * 2
    mesh = _mesh(
        positions=positions,
        texcoords=texcoords,
        submeshes=(
            _submesh(((0, 1, 2),), image_index=1),
            _submesh(((3, 4, 5),), image_index=2),
        ),
    )
    textures = {
        1: TextureRGBA(1, 1, bytes((255, 0, 0, 255))),
        2: TextureRGBA(1, 1, bytes((0, 0, 255, 255))),
    }
    camera = runtime_camera(64, 64)
    frame = render_mesh_reference(mesh, textures=textures, camera=camera)
    center = (32 * 64 + 32) * 4
    assert frame.rgba[center : center + 4] == bytes((255, 0, 0, 255))
    assert frame.depth[32 * 64 + 32] == pytest.approx(20.0, rel=1e-3)

    transparent_near = dict(textures)
    transparent_near[1] = TextureRGBA(1, 1, bytes((255, 0, 0, 0)))
    frame = render_mesh_reference(mesh, textures=transparent_near, camera=camera)
    assert frame.rgba[center : center + 4] == bytes((0, 0, 255, 255))
    assert frame.depth[32 * 64 + 32] == pytest.approx(30.0, rel=1e-3)


def test_untextured_mesh_uses_vertex_buffer_default_color():
    mesh = _mesh(
        positions=((-8.0, -8.0, -20.0), (8.0, -8.0, -20.0), (0.0, 8.0, -20.0)),
        color=(86, 86, 86, 255),
        submeshes=(_submesh(((0, 1, 2),)),),
    )
    frame = render_mesh_reference(mesh, camera=runtime_camera(64, 64))
    center = (32 * 64 + 32) * 4
    assert frame.rgba[center : center + 4] == bytes((86, 86, 86, 255))


def test_representative_canonical_mesh_renders_are_deterministic(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        scene = parse_m3g(read_resource(jar, 45))

    camera = runtime_camera(96, 96, adapt_projection=False)
    for user_id in (7, 9, 10, 20, 30, 40):
        mesh = resolve_mesh(scene, user_id)
        textures = {}
        for submesh in mesh.submeshes:
            if submesh.image_index is not None and submesh.image_index not in textures:
                width, height, rgba = decode_image_rgba(scene, submesh.image_index)
                textures[submesh.image_index] = TextureRGBA(width, height, rgba)

        minimum, maximum = mesh.aabb
        center = tuple((lo + hi) * 0.5 for lo, hi in zip(minimum, maximum))
        extent = max(maximum[i] - minimum[i] for i in range(3))
        distance = max(30.0, extent * 2.2)
        draw = identity_matrix()
        draw = post_translate(draw, -center[0], -center[1], -center[2] - distance)
        draw = post_rotate(draw, 25.0, 1.0, 0.0, 0.0)
        draw = post_rotate(draw, -35.0, 0.0, 1.0, 0.0)

        first = render_mesh_reference(mesh, textures=textures, camera=camera, draw_transform=draw)
        second = render_mesh_reference(mesh, textures=textures, camera=camera, draw_transform=draw)
        assert hashlib.sha256(first.rgba).digest() == hashlib.sha256(second.rgba).digest()
        assert any(first.rgba[offset + 3] for offset in range(0, len(first.rgba), 4)), user_id
