from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import struct
from typing import Iterable

from .m3g import (
    Appearance,
    Image2D,
    M3GFile,
    Mesh,
    Texture2D,
    TriangleStripArray,
    VertexArray,
    VertexBuffer,
)

GAME_MESH_USER_IDS = (7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43)

_IMAGE_FORMAT_CHANNELS = {
    96: 1,  # ALPHA
    97: 1,  # LUMINANCE
    98: 2,  # LUMINANCE_ALPHA
    99: 3,  # RGB
    100: 4,  # RGBA
}


@dataclass(frozen=True)
class ResolvedSubmesh:
    index_buffer_index: int
    appearance_index: int
    triangles: tuple[tuple[int, int, int], ...]
    texture_index: int | None
    image_index: int | None
    compositing_mode_index: int
    polygon_mode_index: int


@dataclass(frozen=True)
class ResolvedMesh:
    user_id: int
    object_index: int
    vertex_buffer_index: int
    positions: tuple[tuple[float, float, float], ...]
    texcoords: tuple[tuple[float, float], ...] | None
    default_color: tuple[int, int, int, int]
    submeshes: tuple[ResolvedSubmesh, ...]
    translation: tuple[float, float, float]
    scale: tuple[float, float, float]
    orientation_angle: float
    orientation_axis: tuple[float, float, float]
    matrix: tuple[float, ...] | None

    @property
    def vertex_count(self) -> int:
        return len(self.positions)

    @property
    def triangle_count(self) -> int:
        return sum(len(submesh.triangles) for submesh in self.submeshes)

    @property
    def aabb(self) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
        if not self.positions:
            return ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
        xs = [value[0] for value in self.positions]
        ys = [value[1] for value in self.positions]
        zs = [value[2] for value in self.positions]
        return ((min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs)))


def _require(scene: M3GFile, index: int, expected_type):
    if index <= 0 or index >= len(scene.objects):
        raise ValueError(f"invalid M3G object reference {index}")
    value = scene.objects[index]
    if not isinstance(value, expected_type):
        raise ValueError(
            f"M3G object {index} is {type(value).__name__}, expected {expected_type.__name__}"
        )
    return value


def _signed_component(value: int, component_size: int) -> int:
    if component_size == 1:
        return value - 256 if value >= 128 else value
    return value


def _scaled_vectors(
    array: VertexArray,
    scale: float,
    bias: tuple[float, float, float],
    dimensions: int,
) -> tuple[tuple[float, ...], ...]:
    if array.component_count < dimensions:
        raise ValueError(
            f"VertexArray {array.object_index} has {array.component_count} components; need {dimensions}"
        )
    result: list[tuple[float, ...]] = []
    for raw in array.values:
        result.append(
            tuple(
                bias[component] + scale * _signed_component(raw[component], array.component_size)
                for component in range(dimensions)
            )
        )
    return tuple(result)


def _expand_triangle_strip(index_buffer: TriangleStripArray) -> tuple[tuple[int, int, int], ...]:
    total_index_count = sum(index_buffer.strip_lengths)
    if index_buffer.encoding < 128:
        if index_buffer.start_index is None:
            raise ValueError(f"TriangleStripArray {index_buffer.object_index} has no start index")
        indices = tuple(range(index_buffer.start_index, index_buffer.start_index + total_index_count))
    else:
        indices = index_buffer.indices
        if len(indices) != total_index_count:
            raise ValueError(
                f"TriangleStripArray {index_buffer.object_index} index count mismatch: "
                f"{len(indices)} != {total_index_count}"
            )

    triangles: list[tuple[int, int, int]] = []
    cursor = 0
    for strip_length in index_buffer.strip_lengths:
        if strip_length < 3:
            raise ValueError(
                f"TriangleStripArray {index_buffer.object_index} has strip shorter than 3"
            )
        strip = indices[cursor : cursor + strip_length]
        cursor += strip_length
        for i in range(strip_length - 2):
            a, b, c = strip[i], strip[i + 1], strip[i + 2]
            if i & 1:
                a, b = b, a
            if a != b and b != c and a != c:
                triangles.append((a, b, c))
    return tuple(triangles)


def resolve_mesh(scene: M3GFile, user_id: int) -> ResolvedMesh:
    meshes = scene.meshes_by_user_id()
    try:
        mesh = meshes[user_id]
    except KeyError as exc:
        raise KeyError(f"M3G mesh user ID {user_id} not found") from exc

    vertex_buffer = _require(scene, mesh.vertex_buffer, VertexBuffer)
    positions_array = _require(scene, vertex_buffer.positions, VertexArray)
    positions_raw = _scaled_vectors(
        positions_array,
        vertex_buffer.position_scale,
        vertex_buffer.position_bias,
        3,
    )
    positions = tuple((v[0], v[1], v[2]) for v in positions_raw)

    texcoords: tuple[tuple[float, float], ...] | None = None
    if vertex_buffer.texcoords:
        if len(vertex_buffer.texcoords) != 1:
            raise ValueError(
                f"VertexBuffer {vertex_buffer.object_index} uses {len(vertex_buffer.texcoords)} texture units"
            )
        texcoord_index, bias, scale = vertex_buffer.texcoords[0]
        texcoord_array = _require(scene, texcoord_index, VertexArray)
        texcoord_values = _scaled_vectors(texcoord_array, scale, bias, 2)
        texcoords = tuple((v[0], v[1]) for v in texcoord_values)
        if len(texcoords) != len(positions):
            raise ValueError(f"VertexBuffer {vertex_buffer.object_index} position/UV count mismatch")

    submeshes: list[ResolvedSubmesh] = []
    for index_buffer_index, appearance_index in mesh.submeshes:
        index_buffer = _require(scene, index_buffer_index, TriangleStripArray)
        appearance = _require(scene, appearance_index, Appearance)
        if len(appearance.textures) > 1:
            raise ValueError(f"Appearance {appearance.object_index} uses multiple texture units")
        texture_index: int | None = appearance.textures[0] if appearance.textures else None
        image_index: int | None = None
        if texture_index is not None:
            texture = _require(scene, texture_index, Texture2D)
            _require(scene, texture.image, Image2D)
            image_index = texture.image
        triangles = _expand_triangle_strip(index_buffer)
        for triangle in triangles:
            if min(triangle) < 0 or max(triangle) >= len(positions):
                raise ValueError(
                    f"Mesh {user_id} triangle {triangle} exceeds vertex count {len(positions)}"
                )
        submeshes.append(
            ResolvedSubmesh(
                index_buffer_index=index_buffer_index,
                appearance_index=appearance_index,
                triangles=triangles,
                texture_index=texture_index,
                image_index=image_index,
                compositing_mode_index=appearance.compositing_mode,
                polygon_mode_index=appearance.polygon_mode,
            )
        )

    return ResolvedMesh(
        user_id=user_id,
        object_index=mesh.object_index,
        vertex_buffer_index=mesh.vertex_buffer,
        positions=positions,
        texcoords=texcoords,
        default_color=vertex_buffer.default_color,
        submeshes=tuple(submeshes),
        translation=mesh.translation,
        scale=mesh.scale,
        orientation_angle=mesh.orientation_angle,
        orientation_axis=mesh.orientation_axis,
        matrix=mesh.matrix,
    )


def _pixel_to_rgba(image_format: int, pixel: bytes) -> tuple[int, int, int, int]:
    if image_format == 96:
        return (255, 255, 255, pixel[0])
    if image_format == 97:
        return (pixel[0], pixel[0], pixel[0], 255)
    if image_format == 98:
        return (pixel[0], pixel[0], pixel[0], pixel[1])
    if image_format == 99:
        return (pixel[0], pixel[1], pixel[2], 255)
    if image_format == 100:
        return (pixel[0], pixel[1], pixel[2], pixel[3])
    raise ValueError(f"unsupported Image2D format {image_format}")


def decode_image_rgba(scene: M3GFile, image_index: int) -> tuple[int, int, bytes]:
    image = _require(scene, image_index, Image2D)
    if image.mutable:
        raise ValueError(f"mutable Image2D {image_index} has no serialized pixel data")
    try:
        channels = _IMAGE_FORMAT_CHANNELS[image.format]
    except KeyError as exc:
        raise ValueError(f"unsupported Image2D format {image.format}") from exc

    pixel_count = image.width * image.height
    rgba = bytearray()
    if image.palette:
        if len(image.palette) % channels:
            raise ValueError(f"Image2D {image_index} palette has invalid length")
        palette_count = len(image.palette) // channels
        if not 1 <= palette_count <= 256:
            raise ValueError(f"Image2D {image_index} has invalid palette size")
        if len(image.pixels) != pixel_count:
            raise ValueError(f"Image2D {image_index} indexed pixel count mismatch")
        palette = [
            _pixel_to_rgba(image.format, image.palette[i : i + channels])
            for i in range(0, len(image.palette), channels)
        ]
        for palette_index in image.pixels:
            if palette_index >= len(palette):
                raise ValueError(f"Image2D {image_index} palette index out of range")
            rgba.extend(palette[palette_index])
    else:
        expected = pixel_count * channels
        if len(image.pixels) != expected:
            raise ValueError(
                f"Image2D {image_index} direct pixel length mismatch: {len(image.pixels)} != {expected}"
            )
        for offset in range(0, expected, channels):
            rgba.extend(_pixel_to_rgba(image.format, image.pixels[offset : offset + channels]))

    return image.width, image.height, bytes(rgba)


def _hash_floats(vectors: Iterable[Iterable[float]]) -> str:
    digest = hashlib.sha256()
    for vector in vectors:
        for value in vector:
            digest.update(struct.pack("<f", float(value)))
    return digest.hexdigest()


def _hash_triangles(triangles: Iterable[tuple[int, int, int]]) -> str:
    digest = hashlib.sha256()
    for triangle in triangles:
        digest.update(struct.pack("<III", *triangle))
    return digest.hexdigest()


def build_mesh_catalog(scene: M3GFile) -> list[dict[str, object]]:
    catalog: list[dict[str, object]] = []
    for user_id in GAME_MESH_USER_IDS:
        mesh = resolve_mesh(scene, user_id)
        minimum, maximum = mesh.aabb
        texture_records: list[dict[str, object]] = []
        triangle_stream: list[tuple[int, int, int]] = []
        for submesh in mesh.submeshes:
            triangle_stream.extend(submesh.triangles)
            texture_record: dict[str, object] = {
                "texture_index": submesh.texture_index,
                "image_index": submesh.image_index,
            }
            if submesh.image_index is not None:
                width, height, rgba = decode_image_rgba(scene, submesh.image_index)
                texture_record.update(
                    width=width,
                    height=height,
                    rgba_sha256=hashlib.sha256(rgba).hexdigest(),
                )
            texture_records.append(texture_record)
        catalog.append(
            {
                "user_id": mesh.user_id,
                "object_index": mesh.object_index,
                "vertex_buffer_index": mesh.vertex_buffer_index,
                "vertex_count": mesh.vertex_count,
                "triangle_count": mesh.triangle_count,
                "submesh_count": len(mesh.submeshes),
                "aabb_min": list(minimum),
                "aabb_max": list(maximum),
                "source_translation": list(mesh.translation),
                "source_scale": list(mesh.scale),
                "source_orientation_angle": mesh.orientation_angle,
                "source_orientation_axis": list(mesh.orientation_axis),
                "positions_sha256": _hash_floats(mesh.positions),
                "texcoords_sha256": _hash_floats(mesh.texcoords or ()),
                "indices_sha256": _hash_triangles(triangle_stream),
                "textures": texture_records,
            }
        )
    return catalog
