from __future__ import annotations

from collections import Counter
from dataclasses import dataclass
import struct
import zlib

M3G_IDENTIFIER = bytes.fromhex("ab4a5352313834bb0d0a1a0a")
SUPPORTED_OBJECT_TYPES = {0, 1, 3, 5, 6, 8, 10, 11, 13, 14, 17, 20, 21, 22}


@dataclass(frozen=True)
class M3GHeader:
    major: int
    minor: int
    has_external_references: bool
    total_file_size: int
    approximate_content_size: int
    authoring_field: str


@dataclass(frozen=True)
class M3GSection:
    offset: int
    compression_scheme: int
    total_length: int
    uncompressed_length: int
    object_bytes: bytes
    checksum: int


@dataclass(frozen=True)
class M3GChunk:
    index: int
    section_index: int
    object_type: int
    data: bytes


@dataclass(frozen=True)
class M3GObject:
    object_index: int
    object_type: int
    user_id: int
    animation_tracks: tuple[int, ...]
    user_parameters: tuple[tuple[int, bytes], ...]


@dataclass(frozen=True)
class Transformable(M3GObject):
    translation: tuple[float, float, float]
    scale: tuple[float, float, float]
    orientation_angle: float
    orientation_axis: tuple[float, float, float]
    matrix: tuple[float, ...] | None


@dataclass(frozen=True)
class Node(Transformable):
    enable_rendering: bool
    enable_picking: bool
    alpha_factor: int
    scope: int
    alignment: tuple[int, int, int, int] | None


@dataclass(frozen=True)
class AnimationController(M3GObject):
    speed: float
    weight: float
    active_interval_start: int
    active_interval_end: int
    reference_sequence_time: float
    reference_world_time: int


@dataclass(frozen=True)
class Appearance(M3GObject):
    layer: int
    compositing_mode: int
    fog: int
    polygon_mode: int
    material: int
    textures: tuple[int, ...]


@dataclass(frozen=True)
class Camera(Node):
    projection_type: int
    projection_matrix: tuple[float, ...] | None
    fovy: float | None
    aspect_ratio: float | None
    near: float | None
    far: float | None


@dataclass(frozen=True)
class CompositingMode(M3GObject):
    depth_test_enabled: bool
    depth_write_enabled: bool
    color_write_enabled: bool
    alpha_write_enabled: bool
    blending: int
    alpha_threshold: int
    depth_offset_factor: float
    depth_offset_units: float


@dataclass(frozen=True)
class PolygonMode(M3GObject):
    culling: int
    shading: int
    winding: int
    two_sided_lighting_enabled: bool
    local_camera_lighting_enabled: bool
    perspective_correction_enabled: bool


@dataclass(frozen=True)
class Image2D(M3GObject):
    format: int
    mutable: bool
    width: int
    height: int
    palette: bytes
    pixels: bytes


@dataclass(frozen=True)
class TriangleStripArray(M3GObject):
    encoding: int
    start_index: int | None
    indices: tuple[int, ...]
    strip_lengths: tuple[int, ...]


@dataclass(frozen=True)
class Material(M3GObject):
    ambient_color: tuple[int, int, int]
    diffuse_color: tuple[int, int, int, int]
    emissive_color: tuple[int, int, int]
    specular_color: tuple[int, int, int]
    shininess: float
    vertex_color_tracking_enabled: bool


@dataclass(frozen=True)
class Mesh(Node):
    vertex_buffer: int
    submeshes: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class Texture2D(Transformable):
    image: int
    blend_color: tuple[int, int, int]
    blending: int
    wrapping_s: int
    wrapping_t: int
    level_filter: int
    image_filter: int


@dataclass(frozen=True)
class VertexArray(M3GObject):
    component_size: int
    component_count: int
    encoding: int
    vertex_count: int
    values: tuple[tuple[int, ...], ...]


@dataclass(frozen=True)
class VertexBuffer(M3GObject):
    default_color: tuple[int, int, int, int]
    positions: int
    position_bias: tuple[float, float, float]
    position_scale: float
    normals: int
    colors: int
    texcoords: tuple[tuple[int, tuple[float, float, float], float], ...]


@dataclass(frozen=True)
class World(Node):
    children: tuple[int, ...]
    active_camera: int
    background: int


@dataclass(frozen=True)
class M3GFile:
    header: M3GHeader
    sections: tuple[M3GSection, ...]
    chunks: tuple[M3GChunk, ...]
    objects: tuple[M3GHeader | M3GObject | None, ...]

    def type_counts(self) -> dict[int, int]:
        return dict(sorted(Counter(chunk.object_type for chunk in self.chunks).items()))

    @property
    def world(self) -> World:
        worlds = [obj for obj in self.objects[1:] if isinstance(obj, World)]
        if len(worlds) != 1:
            raise ValueError(f"expected exactly one M3G World, got {len(worlds)}")
        return worlds[0]

    def meshes_by_user_id(self) -> dict[int, Mesh]:
        result: dict[int, Mesh] = {}
        for obj in self.objects[1:]:
            if isinstance(obj, Mesh):
                if obj.user_id in result:
                    raise ValueError(f"duplicate Mesh user ID: {obj.user_id}")
                result[obj.user_id] = obj
        return result


class _Reader:
    def __init__(self, raw: bytes, *, object_index: int):
        self.raw = raw
        self.pos = 0
        self.object_index = object_index

    def _take(self, size: int) -> bytes:
        if size < 0 or self.pos + size > len(self.raw):
            raise ValueError(f"M3G object {self.object_index} read past end")
        value = self.raw[self.pos : self.pos + size]
        self.pos += size
        return value

    def u8(self) -> int:
        return self._take(1)[0]

    def boolean(self) -> bool:
        value = self.u8()
        if value not in (0, 1):
            raise ValueError(f"M3G object {self.object_index} has invalid boolean {value}")
        return bool(value)

    def u16(self) -> int:
        return struct.unpack("<H", self._take(2))[0]

    def i16(self) -> int:
        return struct.unpack("<h", self._take(2))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self._take(4))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self._take(4))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self._take(4))[0]

    def byte_array(self) -> bytes:
        return self._take(self.u32())

    def u32_array(self) -> tuple[int, ...]:
        return tuple(self.u32() for _ in range(self.u32()))

    def ref(self) -> int:
        value = self.u32()
        if value != 0 and value >= self.object_index:
            raise ValueError(
                f"M3G object {self.object_index} has forward/invalid object reference {value}"
            )
        return value

    def ref_array(self) -> tuple[int, ...]:
        return tuple(self.ref() for _ in range(self.u32()))

    def finish(self) -> None:
        if self.pos != len(self.raw):
            raise ValueError(
                f"M3G object {self.object_index} has {len(self.raw) - self.pos} trailing bytes"
            )


def _u32le(raw: bytes, offset: int) -> int:
    return struct.unpack_from("<I", raw, offset)[0]


def _parse_header(data: bytes) -> M3GHeader:
    if len(data) < 11:
        raise ValueError("M3G header object is too short")
    major = data[0]
    minor = data[1]
    ext = data[2]
    if ext not in (0, 1):
        raise ValueError("invalid M3G header external-reference flag")
    total_file_size = _u32le(data, 3)
    approximate_content_size = _u32le(data, 7)
    string_raw = data[11:]
    nul = string_raw.find(b"\0")
    if nul < 0 or nul != len(string_raw) - 1:
        raise ValueError("invalid M3G header authoring field")
    try:
        author = string_raw[:nul].decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError("invalid M3G header authoring field") from exc
    return M3GHeader(
        major=major,
        minor=minor,
        has_external_references=bool(ext),
        total_file_size=total_file_size,
        approximate_content_size=approximate_content_size,
        authoring_field=author,
    )


def _object3d(r: _Reader) -> dict[str, object]:
    user_id = r.u32()
    animation_tracks = r.ref_array()
    parameters: list[tuple[int, bytes]] = []
    seen_parameter_ids: set[int] = set()
    for _ in range(r.u32()):
        parameter_id = r.u32()
        if parameter_id in seen_parameter_ids:
            raise ValueError(f"M3G object {r.object_index} has duplicate user parameter {parameter_id}")
        seen_parameter_ids.add(parameter_id)
        parameters.append((parameter_id, r.byte_array()))
    return {
        "object_index": r.object_index,
        "user_id": user_id,
        "animation_tracks": animation_tracks,
        "user_parameters": tuple(parameters),
    }


def _transformable(r: _Reader) -> dict[str, object]:
    result = _object3d(r)
    if r.boolean():
        translation = (r.f32(), r.f32(), r.f32())
        scale = (r.f32(), r.f32(), r.f32())
        orientation_angle = r.f32()
        orientation_axis = (r.f32(), r.f32(), r.f32())
    else:
        translation = (0.0, 0.0, 0.0)
        scale = (1.0, 1.0, 1.0)
        orientation_angle = 0.0
        orientation_axis = (0.0, 0.0, 1.0)
    matrix = tuple(r.f32() for _ in range(16)) if r.boolean() else None
    result.update(
        translation=translation,
        scale=scale,
        orientation_angle=orientation_angle,
        orientation_axis=orientation_axis,
        matrix=matrix,
    )
    return result


def _node(r: _Reader) -> dict[str, object]:
    result = _transformable(r)
    enable_rendering = r.boolean()
    enable_picking = r.boolean()
    alpha_factor = r.u8()
    scope = r.u32()
    alignment = None
    if r.boolean():
        alignment = (r.u8(), r.u8(), r.ref(), r.ref())
    result.update(
        enable_rendering=enable_rendering,
        enable_picking=enable_picking,
        alpha_factor=alpha_factor,
        scope=scope,
        alignment=alignment,
    )
    return result


def _parse_object(chunk: M3GChunk) -> M3GHeader | M3GObject:
    if chunk.object_type == 0:
        if chunk.index != 1:
            raise ValueError("M3G header object must be object 1")
        return _parse_header(chunk.data)
    if chunk.object_type not in SUPPORTED_OBJECT_TYPES:
        raise ValueError(f"unsupported M3G object type {chunk.object_type}")

    r = _Reader(chunk.data, object_index=chunk.index)
    t = chunk.object_type
    common: dict[str, object]
    if t == 1:
        common = _object3d(r)
        obj = AnimationController(
            object_type=t,
            **common,
            speed=r.f32(),
            weight=r.f32(),
            active_interval_start=r.i32(),
            active_interval_end=r.i32(),
            reference_sequence_time=r.f32(),
            reference_world_time=r.i32(),
        )
    elif t == 3:
        common = _object3d(r)
        obj = Appearance(
            object_type=t,
            **common,
            layer=r.u8(),
            compositing_mode=r.ref(),
            fog=r.ref(),
            polygon_mode=r.ref(),
            material=r.ref(),
            textures=r.ref_array(),
        )
    elif t == 5:
        common = _node(r)
        projection_type = r.u8()
        if projection_type == 48:
            projection_matrix = tuple(r.f32() for _ in range(16))
            fovy = aspect = near = far = None
        else:
            projection_matrix = None
            fovy, aspect, near, far = r.f32(), r.f32(), r.f32(), r.f32()
        obj = Camera(
            object_type=t,
            **common,
            projection_type=projection_type,
            projection_matrix=projection_matrix,
            fovy=fovy,
            aspect_ratio=aspect,
            near=near,
            far=far,
        )
    elif t == 6:
        common = _object3d(r)
        obj = CompositingMode(
            object_type=t,
            **common,
            depth_test_enabled=r.boolean(),
            depth_write_enabled=r.boolean(),
            color_write_enabled=r.boolean(),
            alpha_write_enabled=r.boolean(),
            blending=r.u8(),
            alpha_threshold=r.u8(),
            depth_offset_factor=r.f32(),
            depth_offset_units=r.f32(),
        )
    elif t == 8:
        common = _object3d(r)
        obj = PolygonMode(
            object_type=t,
            **common,
            culling=r.u8(),
            shading=r.u8(),
            winding=r.u8(),
            two_sided_lighting_enabled=r.boolean(),
            local_camera_lighting_enabled=r.boolean(),
            perspective_correction_enabled=r.boolean(),
        )
    elif t == 10:
        common = _object3d(r)
        image_format = r.u8()
        mutable = r.boolean()
        width = r.u32()
        height = r.u32()
        palette = b""
        pixels = b""
        if not mutable:
            palette = r.byte_array()
            pixels = r.byte_array()
        obj = Image2D(
            object_type=t,
            **common,
            format=image_format,
            mutable=mutable,
            width=width,
            height=height,
            palette=palette,
            pixels=pixels,
        )
    elif t == 11:
        common = _object3d(r)
        encoding = r.u8()
        start_index: int | None = None
        indices: tuple[int, ...] = ()
        if encoding == 0:
            start_index = r.u32()
        elif encoding == 1:
            start_index = r.u8()
        elif encoding == 2:
            start_index = r.u16()
        elif encoding == 128:
            indices = tuple(r.u32() for _ in range(r.u32()))
        elif encoding == 129:
            indices = tuple(r.u8() for _ in range(r.u32()))
        elif encoding == 130:
            indices = tuple(r.u16() for _ in range(r.u32()))
        else:
            raise ValueError(f"M3G object {chunk.index} has invalid index encoding {encoding}")
        strip_lengths = tuple(r.u32() for _ in range(r.u32()))
        obj = TriangleStripArray(
            object_type=t,
            **common,
            encoding=encoding,
            start_index=start_index,
            indices=indices,
            strip_lengths=strip_lengths,
        )
    elif t == 13:
        common = _object3d(r)
        obj = Material(
            object_type=t,
            **common,
            ambient_color=(r.u8(), r.u8(), r.u8()),
            diffuse_color=(r.u8(), r.u8(), r.u8(), r.u8()),
            emissive_color=(r.u8(), r.u8(), r.u8()),
            specular_color=(r.u8(), r.u8(), r.u8()),
            shininess=r.f32(),
            vertex_color_tracking_enabled=r.boolean(),
        )
    elif t == 14:
        common = _node(r)
        vertex_buffer = r.ref()
        submeshes = tuple((r.ref(), r.ref()) for _ in range(r.u32()))
        obj = Mesh(object_type=t, **common, vertex_buffer=vertex_buffer, submeshes=submeshes)
    elif t == 17:
        common = _transformable(r)
        obj = Texture2D(
            object_type=t,
            **common,
            image=r.ref(),
            blend_color=(r.u8(), r.u8(), r.u8()),
            blending=r.u8(),
            wrapping_s=r.u8(),
            wrapping_t=r.u8(),
            level_filter=r.u8(),
            image_filter=r.u8(),
        )
    elif t == 20:
        common = _object3d(r)
        component_size = r.u8()
        component_count = r.u8()
        encoding = r.u8()
        vertex_count = r.u16()
        if component_size not in (1, 2):
            raise ValueError(f"M3G object {chunk.index} has invalid component size {component_size}")
        if encoding not in (0, 1):
            raise ValueError(f"M3G object {chunk.index} has invalid vertex encoding {encoding}")
        accumulators = [0] * component_count
        values: list[tuple[int, ...]] = []
        for _ in range(vertex_count):
            row: list[int] = []
            for component in range(component_count):
                value = r.u8() if component_size == 1 else r.i16()
                if encoding == 1:
                    if component_size == 1:
                        accumulators[component] = (accumulators[component] + value) & 0xFF
                    else:
                        accumulators[component] = (
                            (accumulators[component] + value + 0x8000) & 0xFFFF
                        ) - 0x8000
                    value = accumulators[component]
                row.append(value)
            values.append(tuple(row))
        obj = VertexArray(
            object_type=t,
            **common,
            component_size=component_size,
            component_count=component_count,
            encoding=encoding,
            vertex_count=vertex_count,
            values=tuple(values),
        )
    elif t == 21:
        common = _object3d(r)
        default_color = (r.u8(), r.u8(), r.u8(), r.u8())
        positions = r.ref()
        position_bias = (r.f32(), r.f32(), r.f32())
        position_scale = r.f32()
        normals = r.ref()
        colors = r.ref()
        texcoords = tuple(
            (
                r.ref(),
                (r.f32(), r.f32(), r.f32()),
                r.f32(),
            )
            for _ in range(r.u32())
        )
        obj = VertexBuffer(
            object_type=t,
            **common,
            default_color=default_color,
            positions=positions,
            position_bias=position_bias,
            position_scale=position_scale,
            normals=normals,
            colors=colors,
            texcoords=texcoords,
        )
    elif t == 22:
        common = _node(r)
        children = r.ref_array()
        obj = World(
            object_type=t,
            **common,
            children=children,
            active_camera=r.ref(),
            background=r.ref(),
        )
    else:  # pragma: no cover - guarded by SUPPORTED_OBJECT_TYPES
        raise AssertionError(t)

    r.finish()
    return obj


def parse_m3g(raw: bytes) -> M3GFile:
    if not raw.startswith(M3G_IDENTIFIER):
        raise ValueError("invalid M3G file identifier")

    sections: list[M3GSection] = []
    chunks: list[M3GChunk] = []
    offset = len(M3G_IDENTIFIER)
    object_index = 1

    while offset < len(raw):
        if offset + 13 > len(raw):
            raise ValueError("truncated M3G section header")
        scheme = raw[offset]
        if scheme not in (0, 1):
            raise ValueError(f"unsupported M3G section compression scheme: {scheme}")
        total_length = _u32le(raw, offset + 1)
        uncompressed_length = _u32le(raw, offset + 5)
        if total_length < 13:
            raise ValueError("invalid M3G section length")
        end = offset + total_length
        if end > len(raw):
            raise ValueError("truncated M3G section")

        stored_object_length = total_length - 13
        stored_objects = raw[offset + 9 : offset + 9 + stored_object_length]
        checksum = _u32le(raw, end - 4)
        calculated = zlib.adler32(raw[offset : end - 4]) & 0xFFFFFFFF
        if checksum != calculated:
            raise ValueError(
                f"M3G section checksum mismatch at offset {offset}: "
                f"expected 0x{checksum:08x}, got 0x{calculated:08x}"
            )

        if scheme == 0:
            object_bytes = stored_objects
        else:
            try:
                object_bytes = zlib.decompress(stored_objects)
            except zlib.error as exc:
                raise ValueError("invalid compressed M3G section") from exc
        if len(object_bytes) != uncompressed_length:
            raise ValueError("M3G section uncompressed length mismatch")

        section_index = len(sections)
        sections.append(
            M3GSection(
                offset=offset,
                compression_scheme=scheme,
                total_length=total_length,
                uncompressed_length=uncompressed_length,
                object_bytes=object_bytes,
                checksum=checksum,
            )
        )

        pos = 0
        while pos < len(object_bytes):
            if pos + 5 > len(object_bytes):
                raise ValueError("truncated M3G object chunk")
            object_type = object_bytes[pos]
            length = _u32le(object_bytes, pos + 1)
            data_start = pos + 5
            data_end = data_start + length
            if data_end > len(object_bytes):
                raise ValueError("M3G object length exceeds section")
            chunks.append(
                M3GChunk(
                    index=object_index,
                    section_index=section_index,
                    object_type=object_type,
                    data=object_bytes[data_start:data_end],
                )
            )
            object_index += 1
            pos = data_end

        offset = end

    if offset != len(raw):
        raise ValueError("M3G file contains trailing bytes")
    if not sections:
        raise ValueError("M3G file contains no sections")
    if not chunks or chunks[0].object_type != 0:
        raise ValueError("M3G file has no header object")

    parsed_objects: list[M3GHeader | M3GObject | None] = [None]
    for chunk in chunks:
        parsed_objects.append(_parse_object(chunk))
    header = parsed_objects[1]
    if not isinstance(header, M3GHeader):
        raise ValueError("M3G object 1 is not a header")
    if header.total_file_size != len(raw):
        raise ValueError("M3G header total file size mismatch")

    return M3GFile(
        header=header,
        sections=tuple(sections),
        chunks=tuple(chunks),
        objects=tuple(parsed_objects),
    )
