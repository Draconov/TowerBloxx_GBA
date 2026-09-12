from __future__ import annotations

import hashlib
import io
import zipfile

from PIL import Image

from tower_bloxx_extract.binary44 import decode_resource_44
from tower_bloxx_extract.font44 import Font44
from tower_bloxx_extract.resources import read_resource


def _font(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        atlas = Image.open(io.BytesIO(read_resource(jar, 36))).convert("RGBA")
        decoded = decode_resource_44(read_resource(jar, 44))
    return Font44(atlas, decoded)


def test_font44_reconstructs_canonical_mapping_and_metrics(tower_bloxx_jar):
    font = _font(tower_bloxx_jar)

    assert font.atlas_size == (87, 65)
    assert font.line_height == 11
    assert font.space_between_characters == 1
    assert font.glyph_index("A") == 79
    assert font.glyph_index("a") == 152
    assert font.glyph_index("0") == 132
    assert font.glyph_index("ä") == 95
    assert font.glyph_index("™") == 167
    assert font.glyph_index("\u2603") == 169  # Unsupported chars use mapping entry 0.


def test_font44_renders_bytecode_exact_reference_text(tower_bloxx_jar):
    font = _font(tower_bloxx_jar)

    assert font.text_width("Tower Bloxx(TM)") == 73
    rendered = font.render_text("Tower Bloxx(TM)")
    assert rendered.size == (73, 11)
    assert rendered.getbbox() == (0, 2, 72, 11)
    assert hashlib.sha256(rendered.tobytes()).hexdigest() == (
        "06ea7af8e10608ef7cec587cd54f62e1752d6b881a3d4d1cbcdf15c4c9a8e396"
    )


def test_font44_sheet_uses_original_pixels_in_8x16_cells(tower_bloxx_jar):
    font = _font(tower_bloxx_jar)
    characters = ["A", "B", "é", "¿"]

    sheet = font.render_font_sheet(characters)
    assert sheet.size == (8 * len(characters), 16)
    assert font.character_width("A") == 5
    assert font.character_width("B") == 4
    assert font.character_width("é") == 4
    assert font.character_width("¿") <= 8
    assert sheet.getbbox() is not None
