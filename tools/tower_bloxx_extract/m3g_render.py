from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Mapping, Sequence

from .m3g_geometry import ResolvedMesh

Matrix4 = tuple[float, ...]

_EPSILON = 1.0e-12
_SUBPIXEL_BITS = 8
_SUBPIXEL_SCALE = 1 << _SUBPIXEL_BITS
_SUBPIXEL_HALF = _SUBPIXEL_SCALE >> 1


@dataclass(frozen=True)
class TextureRGBA:
    width: int
    height: int
    rgba: bytes

    def __post_init__(self) -> None:
        if self.width <= 0 or self.height <= 0:
            raise ValueError("texture dimensions must be positive")
        expected = self.width * self.height * 4
        if len(self.rgba) != expected:
            raise ValueError(f"texture RGBA length mismatch: {len(self.rgba)} != {expected}")


@dataclass(frozen=True)
class RuntimeCamera:
    width: int
    height: int
    base_fov: float
    fov_y: float
    aspect_ratio: float
    near: float
    far: float
    transform: Matrix4


@dataclass(frozen=True)
class RenderFrame:
    width: int
    height: int
    rgba: bytes
    depth: tuple[float, ...]

    @property
    def alpha_bounds(self) -> tuple[int, int, int, int] | None:
        xs: list[int] = []
        ys: list[int] = []
        for pixel_index in range(self.width * self.height):
            if self.rgba[pixel_index * 4 + 3]:
                xs.append(pixel_index % self.width)
                ys.append(pixel_index // self.width)
        if not xs:
            return None
        return (min(xs), min(ys), max(xs) + 1, max(ys) + 1)


def identity_matrix() -> Matrix4:
    return (
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    )


def multiply_matrices(left: Sequence[float], right: Sequence[float]) -> Matrix4:
    if len(left) != 16 or len(right) != 16:
        raise ValueError("4x4 matrices must contain exactly 16 values")
    return tuple(
        sum(left[row * 4 + k] * right[k * 4 + column] for k in range(4))
        for row in range(4)
        for column in range(4)
    )


def translation_matrix(x: float, y: float, z: float) -> Matrix4:
    return (
        1.0, 0.0, 0.0, float(x),
        0.0, 1.0, 0.0, float(y),
        0.0, 0.0, 1.0, float(z),
        0.0, 0.0, 0.0, 1.0,
    )


def rotation_matrix(angle_degrees: float, x: float, y: float, z: float) -> Matrix4:
    length = math.sqrt(x * x + y * y + z * z)
    if length <= _EPSILON:
        raise ValueError("rotation axis must be non-zero")
    x /= length
    y /= length
    z /= length
    angle = math.radians(angle_degrees)
    c = math.cos(angle)
    s = math.sin(angle)
    t = 1.0 - c
    return (
        t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0.0,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0.0,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0.0,
        0.0, 0.0, 0.0, 1.0,
    )


def post_translate(matrix: Sequence[float], x: float, y: float, z: float) -> Matrix4:
    """Match M3G Transform.postTranslate: matrix <- matrix * T."""
    return multiply_matrices(matrix, translation_matrix(x, y, z))


def post_rotate(matrix: Sequence[float], angle_degrees: float, x: float, y: float, z: float) -> Matrix4:
    """Match M3G Transform.postRotate: matrix <- matrix * R."""
    return multiply_matrices(matrix, rotation_matrix(angle_degrees, x, y, z))


def transform_point(matrix: Sequence[float], point: tuple[float, float, float]) -> tuple[float, float, float]:
    if len(matrix) != 16:
        raise ValueError("4x4 matrix must contain exactly 16 values")
    x, y, z = point
    ox = matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3]
    oy = matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7]
    oz = matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11]
    ow = matrix[12] * x + matrix[13] * y + matrix[14] * z + matrix[15]
    if abs(ow) <= _EPSILON:
        raise ValueError("transform produced zero homogeneous W")
    if abs(ow - 1.0) > _EPSILON:
        return (ox / ow, oy / ow, oz / ow)
    return (ox, oy, oz)


def _inverse_affine(matrix: Sequence[float]) -> Matrix4:
    if len(matrix) != 16:
        raise ValueError("4x4 matrix must contain exactly 16 values")
    if any(abs(matrix[index] - expected) > 1.0e-7 for index, expected in zip((12, 13, 14, 15), (0, 0, 0, 1))):
        raise ValueError("camera transform must be affine")

    a, b, c = matrix[0], matrix[1], matrix[2]
    d, e, f = matrix[4], matrix[5], matrix[6]
    g, h, i = matrix[8], matrix[9], matrix[10]
    determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if abs(determinant) <= _EPSILON:
        raise ValueError("camera transform is singular")
    inv_det = 1.0 / determinant
    r00 = (e * i - f * h) * inv_det
    r01 = (c * h - b * i) * inv_det
    r02 = (b * f - c * e) * inv_det
    r10 = (f * g - d * i) * inv_det
    r11 = (a * i - c * g) * inv_det
    r12 = (c * d - a * f) * inv_det
    r20 = (d * h - e * g) * inv_det
    r21 = (b * g - a * h) * inv_det
    r22 = (a * e - b * d) * inv_det
    tx, ty, tz = matrix[3], matrix[7], matrix[11]
    return (
        r00, r01, r02, -(r00 * tx + r01 * ty + r02 * tz),
        r10, r11, r12, -(r10 * tx + r11 * ty + r12 * tz),
        r20, r21, r22, -(r20 * tx + r21 * ty + r22 * tz),
        0.0, 0.0, 0.0, 1.0,
    )


def class_n_camera_transform(
    *,
    position: tuple[float, float, float],
    direction: tuple[float, float, float],
    up: tuple[float, float, float],
) -> Matrix4:
    """Reproduce the camera-to-world matrix assembled by obfuscated class ``n``.

    The Java routine treats ``direction`` as the camera forward direction,
    derives a normalized right vector from ``direction x up``, derives the
    corrected up vector from ``right x direction``, and stores ``-direction``
    as the camera's local +Z axis.
    """
    dx, dy, dz = (0.0 if abs(v) < 1.0e-4 else float(v) for v in direction)
    ux, uy, uz = (float(v) for v in up)
    rx = dy * uz - dz * uy
    ry = dz * ux - dx * uz
    rz = dx * uy - dy * ux
    right_length = math.sqrt(rx * rx + ry * ry + rz * rz)
    if right_length <= _EPSILON:
        raise ValueError("camera direction and up vectors are parallel")
    rx /= right_length
    ry /= right_length
    rz /= right_length

    corrected_up_x = ry * dz - rz * dy
    corrected_up_y = rz * dx - rx * dz
    corrected_up_z = rx * dy - ry * dx
    px, py, pz = position
    return (
        rx, corrected_up_x, -dx, float(px),
        ry, corrected_up_y, -dy, float(py),
        rz, corrected_up_z, -dz, float(pz),
        0.0, 0.0, 0.0, 1.0,
    )


def runtime_camera(
    width: int,
    height: int,
    *,
    clip_top: int = 0,
    base_fov: float = 60.0,
    near: float = 10.0,
    far: float = 10000.0,
    transform: Matrix4 | None = None,
    adapt_projection: bool = True,
) -> RuntimeCamera:
    if width <= 0 or height <= 0:
        raise ValueError("camera dimensions must be positive")
    if not 0 <= clip_top < height:
        raise ValueError("clip_top must lie inside the target")
    if not 0.0 < base_fov < 180.0:
        raise ValueError("base FOV must be between 0 and 180 degrees")
    if near <= 0.0 or far <= near:
        raise ValueError("camera clipping planes are invalid")

    visible_height = height - clip_top
    if adapt_projection:
        # Exact class n::b(int,int,int) formulas.  The Java code receives
        # clip_top, width, height and then passes base_fov*c and b to
        # Camera.setPerspective().
        aspect = (width / visible_height) * 0.7 + 0.3
        fov_y = base_fov * ((visible_height / width) * 0.7 + 0.3)
    else:
        aspect = width / visible_height
        fov_y = base_fov
    return RuntimeCamera(
        width=width,
        height=height,
        base_fov=float(base_fov),
        fov_y=float(fov_y),
        aspect_ratio=float(aspect),
        near=float(near),
        far=float(far),
        transform=identity_matrix() if transform is None else tuple(transform),
    )


def project_camera_point(camera: RuntimeCamera, point: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = point
    depth = -z
    if depth <= 0.0:
        raise ValueError("point is behind the camera")
    tangent = math.tan(math.radians(camera.fov_y) * 0.5)
    ndc_x = x / (depth * tangent * camera.aspect_ratio)
    ndc_y = y / (depth * tangent)
    screen_x = 0.5 * camera.width * ndc_x + (camera.width >> 1)
    screen_y = -0.5 * camera.height * ndc_y + (camera.height >> 1)
    return (screen_x, screen_y, depth)


def sample_texture_nearest(texture: TextureRGBA, u: float, v: float) -> tuple[int, int, int, int]:
    u = min(1.0, max(0.0, u))
    v = min(1.0, max(0.0, v))
    x = min(texture.width - 1, int(math.floor(u * (texture.width - 1) + 0.5)))
    # M3G texture coordinates use the conventional lower-left texture origin;
    # serialized Image2D scanlines are consumed top-to-bottom by the exporter.
    y = min(texture.height - 1, int(math.floor((1.0 - v) * (texture.height - 1) + 0.5)))
    offset = (y * texture.width + x) * 4
    return tuple(texture.rgba[offset : offset + 4])  # type: ignore[return-value]


def _edge(ax: int, ay: int, bx: int, by: int, px: int, py: int) -> int:
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax)


def _source_over(dst: tuple[int, int, int, int], src: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    sr, sg, sb, sa = src
    if sa <= 0:
        return dst
    if sa >= 255:
        return src
    dr, dg, db, da = dst
    # Integer source-over with a transparent RGBA atlas as destination.
    out_a_numerator = sa * 255 + da * (255 - sa)
    if out_a_numerator == 0:
        return (0, 0, 0, 0)
    out_a = (out_a_numerator + 127) // 255
    denominator = out_a_numerator
    r = (sr * sa * 255 + dr * da * (255 - sa) + denominator // 2) // denominator
    g = (sg * sa * 255 + dg * da * (255 - sa) + denominator // 2) // denominator
    b = (sb * sa * 255 + db * da * (255 - sa) + denominator // 2) // denominator
    return (min(255, r), min(255, g), min(255, b), min(255, out_a))


def render_mesh_reference(
    mesh: ResolvedMesh,
    *,
    camera: RuntimeCamera,
    textures: Mapping[int, TextureRGBA] | None = None,
    draw_transform: Matrix4 | None = None,
) -> RenderFrame:
    textures = {} if textures is None else textures
    draw_transform = identity_matrix() if draw_transform is None else draw_transform
    view = _inverse_affine(camera.transform)
    model_view = multiply_matrices(view, draw_transform)

    rgba = bytearray(camera.width * camera.height * 4)
    depth_buffer = [math.inf] * (camera.width * camera.height)

    transformed = [transform_point(model_view, point) for point in mesh.positions]
    projected: list[tuple[float, float, float] | None] = []
    for point in transformed:
        depth = -point[2]
        if depth <= 0.0:
            projected.append(None)
        else:
            projected.append(project_camera_point(camera, point))

    for submesh in mesh.submeshes:
        texture: TextureRGBA | None = None
        if submesh.image_index is not None:
            try:
                texture = textures[submesh.image_index]
            except KeyError as exc:
                raise ValueError(f"missing RGBA texture for Image2D {submesh.image_index}") from exc
            if mesh.texcoords is None:
                raise ValueError(f"mesh {mesh.user_id} is textured but has no texture coordinates")

        for i0, i1, i2 in submesh.triangles:
            p0 = projected[i0]
            p1 = projected[i1]
            p2 = projected[i2]
            if p0 is None or p1 is None or p2 is None:
                continue
            if any(p[2] < camera.near or p[2] > camera.far for p in (p0, p1, p2)):
                # The canonical export poses do not cross the clip planes.  A
                # strict all-or-nothing clip keeps this small reference renderer
                # deterministic without pretending to be a generic M3G engine.
                continue

            fx0, fy0 = round(p0[0] * _SUBPIXEL_SCALE), round(p0[1] * _SUBPIXEL_SCALE)
            fx1, fy1 = round(p1[0] * _SUBPIXEL_SCALE), round(p1[1] * _SUBPIXEL_SCALE)
            fx2, fy2 = round(p2[0] * _SUBPIXEL_SCALE), round(p2[1] * _SUBPIXEL_SCALE)
            area = _edge(fx0, fy0, fx1, fy1, fx2, fy2)
            if area == 0:
                continue

            min_x = max(0, int(math.floor(min(p0[0], p1[0], p2[0]) - 0.5)))
            max_x = min(camera.width - 1, int(math.ceil(max(p0[0], p1[0], p2[0]) - 0.5)))
            min_y = max(0, int(math.floor(min(p0[1], p1[1], p2[1]) - 0.5)))
            max_y = min(camera.height - 1, int(math.ceil(max(p0[1], p1[1], p2[1]) - 0.5)))
            if min_x > max_x or min_y > max_y:
                continue

            inv_depth0 = 1.0 / p0[2]
            inv_depth1 = 1.0 / p1[2]
            inv_depth2 = 1.0 / p2[2]
            if texture is not None:
                assert mesh.texcoords is not None
                u0, v0 = mesh.texcoords[i0]
                u1, v1 = mesh.texcoords[i1]
                u2, v2 = mesh.texcoords[i2]

            for y in range(min_y, max_y + 1):
                py = y * _SUBPIXEL_SCALE + _SUBPIXEL_HALF
                for x in range(min_x, max_x + 1):
                    px = x * _SUBPIXEL_SCALE + _SUBPIXEL_HALF
                    e0 = _edge(fx1, fy1, fx2, fy2, px, py)
                    e1 = _edge(fx2, fy2, fx0, fy0, px, py)
                    e2 = _edge(fx0, fy0, fx1, fy1, px, py)
                    if area > 0:
                        if e0 < 0 or e1 < 0 or e2 < 0:
                            continue
                    elif e0 > 0 or e1 > 0 or e2 > 0:
                        continue

                    b0 = e0 / area
                    b1 = e1 / area
                    b2 = e2 / area
                    inv_depth = b0 * inv_depth0 + b1 * inv_depth1 + b2 * inv_depth2
                    if inv_depth <= 0.0:
                        continue
                    depth = 1.0 / inv_depth
                    pixel_index = y * camera.width + x
                    if depth >= depth_buffer[pixel_index]:
                        continue

                    if texture is None:
                        source = mesh.default_color
                    else:
                        u = (
                            b0 * u0 * inv_depth0
                            + b1 * u1 * inv_depth1
                            + b2 * u2 * inv_depth2
                        ) / inv_depth
                        v = (
                            b0 * v0 * inv_depth0
                            + b1 * v1 * inv_depth1
                            + b2 * v2 * inv_depth2
                        ) / inv_depth
                        source = sample_texture_nearest(texture, u, v)

                    if source[3] == 0:
                        continue
                    offset = pixel_index * 4
                    destination = tuple(rgba[offset : offset + 4])
                    output = _source_over(destination, source)
                    rgba[offset : offset + 4] = bytes(output)
                    depth_buffer[pixel_index] = depth

    return RenderFrame(
        width=camera.width,
        height=camera.height,
        rgba=bytes(rgba),
        depth=tuple(depth_buffer),
    )
