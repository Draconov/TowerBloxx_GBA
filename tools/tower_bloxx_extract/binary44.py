from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Resource44Glyph:
    # Historical field names are kept for Phase-1 compatibility.  Class b's
    # renderer proves their actual roles; semantic aliases below expose them.
    source_x: int
    source_width: int
    source_y: int
    source_height: int
    advance_or_class: int

    @property
    def y_offset(self) -> int:
        return self.source_x

    @property
    def atlas_x(self) -> int:
        return self.source_width

    @property
    def atlas_y(self) -> int:
        return self.source_y

    @property
    def glyph_width(self) -> int:
        return self.source_height

    @property
    def glyph_height(self) -> int:
        return self.advance_or_class


@dataclass(frozen=True)
class Resource44:
    width_count: int
    widths: tuple[int, ...]
    special_map: tuple[int, ...]
    glyph_count: int
    glyphs: tuple[Resource44Glyph, ...]
    bytes_consumed: int


def _signed_byte(value: int) -> int:
    return value - 256 if value >= 128 else value


def decode_resource_44(data: bytes) -> Resource44:
    if not data:
        raise ValueError("resource 44 is empty")
    width_count = data[0]
    offset = 1

    if offset + width_count + 8 + 1 > len(data):
        raise ValueError("resource 44 header is truncated")
    widths = tuple(data[offset:offset + width_count])
    offset += width_count
    special_map = tuple(data[offset:offset + 8])
    offset += 8
    glyph_count = data[offset]
    offset += 1

    glyphs: list[Resource44Glyph] = []
    for _ in range(glyph_count):
        if offset + 6 > len(data):
            raise ValueError("resource 44 glyph table is truncated")
        source_x = _signed_byte(data[offset])
        source_width = data[offset + 1]
        source_y = int.from_bytes(data[offset + 2:offset + 4], "big", signed=True)
        source_height = data[offset + 4]
        advance_or_class = data[offset + 5]
        glyphs.append(
            Resource44Glyph(
                source_x=source_x,
                source_width=source_width,
                source_y=source_y,
                source_height=source_height,
                advance_or_class=advance_or_class,
            )
        )
        offset += 6

    if offset != len(data):
        raise ValueError(f"resource 44 has {len(data) - offset} trailing bytes")
    return Resource44(
        width_count=width_count,
        widths=widths,
        special_map=special_map,
        glyph_count=glyph_count,
        glyphs=tuple(glyphs),
        bytes_consumed=offset,
    )
