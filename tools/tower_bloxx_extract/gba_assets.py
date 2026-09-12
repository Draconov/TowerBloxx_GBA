from __future__ import annotations

from dataclasses import dataclass
from typing import Final

from PIL import Image


LEGAL_OBJ_SIZES: Final[tuple[tuple[int, int], ...]] = (
    (8, 8), (16, 16), (32, 32), (64, 64),
    (16, 8), (32, 8), (32, 16), (64, 32),
    (8, 16), (8, 32), (16, 32), (32, 64),
)


@dataclass(frozen=True)
class SpritePart:
    source_x: int
    source_y: int
    width: int
    height: int
    indices: bytes
    rgba: bytes


@dataclass(frozen=True)
class SpriteComposite:
    mesh_id: int
    canvas_width: int
    canvas_height: int
    bbox: tuple[int, int, int, int]
    bpp: int
    palette_bgr555: tuple[int, ...]
    parts: tuple[SpritePart, ...]


def choose_sprite_shape(required_width: int, required_height: int) -> tuple[int, int]:
    if required_width <= 0 or required_height <= 0:
        raise ValueError("sprite dimensions must be positive")
    candidates = [
        size for size in LEGAL_OBJ_SIZES
        if size[0] >= required_width and size[1] >= required_height
    ]
    if not candidates:
        raise ValueError(f"no legal GBA OBJ shape covers {required_width}x{required_height}")
    return min(candidates, key=lambda size: (size[0] * size[1], size[0] + size[1], size[0], size[1]))


def _bgr555(red: int, green: int, blue: int) -> int:
    return (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)


def _expand_bgr555(value: int) -> tuple[int, int, int, int]:
    return (
        (value & 0x1F) << 3,
        ((value >> 5) & 0x1F) << 3,
        ((value >> 10) & 0x1F) << 3,
        255,
    )


def _palette_for(image: Image.Image) -> tuple[int, tuple[int, ...], dict[int, int]]:
    opaque: list[int] = []
    seen: set[int] = set()
    for red, green, blue, alpha in image.get_flattened_data():
        if alpha == 0:
            continue
        if alpha != 255:
            raise ValueError("GBA sprite export requires binary alpha")
        color = _bgr555(red, green, blue)
        if color not in seen:
            seen.add(color)
            opaque.append(color)
    if len(opaque) > 255:
        raise ValueError("render needs more than 255 opaque GBA colors")
    bpp = 4 if len(opaque) <= 15 else 8
    palette = (0, *opaque)
    return bpp, palette, {color: index + 1 for index, color in enumerate(opaque)}


def slice_sprite(render: Image.Image, mesh_id: int) -> SpriteComposite:
    image = render.convert("RGBA")
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise ValueError("render has no visible pixels")

    bpp, palette, color_to_index = _palette_for(image)
    left, top, right, bottom = bbox
    parts: list[SpritePart] = []
    for source_y in range(top, bottom, 64):
        needed_height = min(64, bottom - source_y)
        for source_x in range(left, right, 64):
            needed_width = min(64, right - source_x)
            width, height = choose_sprite_shape(needed_width, needed_height)
            rgba = bytearray(width * height * 4)
            indices = bytearray(width * height)
            for local_y in range(height):
                canvas_y = source_y + local_y
                if canvas_y >= image.height:
                    continue
                for local_x in range(width):
                    canvas_x = source_x + local_x
                    if canvas_x >= image.width:
                        continue
                    red, green, blue, alpha = image.getpixel((canvas_x, canvas_y))
                    if alpha == 0:
                        continue
                    color = _bgr555(red, green, blue)
                    pixel_index = local_y * width + local_x
                    indices[pixel_index] = color_to_index[color]
                    output_offset = pixel_index * 4
                    rgba[output_offset : output_offset + 4] = bytes(_expand_bgr555(color))
            parts.append(SpritePart(source_x, source_y, width, height, bytes(indices), bytes(rgba)))

    return SpriteComposite(
        mesh_id=int(mesh_id),
        canvas_width=image.width,
        canvas_height=image.height,
        bbox=bbox,
        bpp=bpp,
        palette_bgr555=palette,
        parts=tuple(parts),
    )


def recompose_sprite(composite: SpriteComposite) -> Image.Image:
    output = Image.new("RGBA", (composite.canvas_width, composite.canvas_height), (0, 0, 0, 0))
    for part in composite.parts:
        part_image = Image.frombytes("RGBA", (part.width, part.height), part.rgba)
        output.alpha_composite(part_image, (part.source_x, part.source_y))
    return output
