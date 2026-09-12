from __future__ import annotations

from dataclasses import dataclass

from PIL import Image

from .binary44 import Resource44, Resource44Glyph


@dataclass(frozen=True)
class Font44:
    atlas: Image.Image
    data: Resource44

    def __post_init__(self) -> None:
        rgba = self.atlas.convert("RGBA")
        object.__setattr__(self, "atlas", rgba)
        if len(self.data.special_map) != 8:
            raise ValueError("resource 44 special map must contain 8 values")

    @property
    def atlas_size(self) -> tuple[int, int]:
        return self.atlas.size

    @property
    def space_between_characters(self) -> int:
        return self.data.special_map[6]

    @property
    def line_height(self) -> int:
        return self.data.special_map[7]

    def glyph_index(self, character: str) -> int:
        if len(character) != 1:
            raise ValueError("glyph lookup requires one character")
        value = ord(character)
        special = self.data.special_map
        if special[0] <= value <= special[1]:
            map_index = value - special[2]
        elif special[3] <= value <= special[4]:
            map_index = value - special[5]
        elif value == 8482:  # TRADE MARK SIGN, special-cased by the Java renderer.
            map_index = len(self.data.widths) - 1
        else:
            map_index = 0
        if not 0 <= map_index < len(self.data.widths):
            map_index = 0
        return self.data.widths[map_index]

    def glyph(self, character: str) -> Resource44Glyph:
        return self.data.glyphs[self.glyph_index(character)]

    def character_width(self, character: str) -> int:
        if character == " ":
            glyph = self.data.glyphs[self.data.widths[0]]
        else:
            glyph = self.glyph(character)
        return glyph.glyph_width

    def text_width(self, text: str) -> int:
        return sum(self.character_width(character) + self.space_between_characters for character in text)

    def _blit_glyph(self, target: Image.Image, character: str, x: int, y: int) -> None:
        glyph = self.glyph(character)
        if glyph.glyph_width <= 0 or glyph.glyph_height <= 0:
            return
        crop = self.atlas.crop(
            (
                glyph.atlas_x,
                glyph.atlas_y,
                glyph.atlas_x + glyph.glyph_width,
                glyph.atlas_y + glyph.glyph_height,
            )
        )
        target.alpha_composite(crop, (x, y + glyph.y_offset))

    def render_text(self, text: str) -> Image.Image:
        width = self.text_width(text)
        result = Image.new("RGBA", (max(width, 1), self.line_height), (255, 255, 255, 0))
        x = 0
        for character in text:
            if character != " ":
                self._blit_glyph(result, character, x, 0)
            x += self.character_width(character) + self.space_between_characters
        return result

    def render_font_sheet(self, characters: list[str] | tuple[str, ...]) -> Image.Image:
        if not characters:
            raise ValueError("font sheet requires at least one character")
        result = Image.new("RGBA", (8 * len(characters), 16), (255, 255, 255, 0))
        for index, character in enumerate(characters):
            if self.character_width(character) > 8:
                raise ValueError(f"character does not fit 8x16 cell: {character!r}")
            if character != " ":
                self._blit_glyph(result, character, index * 8, 0)
        return result
