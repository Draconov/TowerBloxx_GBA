import zipfile

from tower_bloxx_extract.binary44 import decode_resource_44
from tower_bloxx_extract.resources import read_resource


def test_resource_44_consumes_exact_payload(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        raw = read_resource(jar, 44)
    decoded = decode_resource_44(raw)
    assert decoded.bytes_consumed == len(raw)
    assert len(decoded.widths) == decoded.width_count
    assert len(decoded.special_map) == 8
    assert len(decoded.glyphs) == decoded.glyph_count
    assert decoded.width_count == 191
    assert decoded.glyph_count == 191
    assert decoded.special_map == (33, 126, 32, 161, 255, 66, 1, 11)
    assert decoded.glyphs[0].source_x == 2
    assert decoded.glyphs[0].source_height == 5
    assert decoded.glyphs[-1].source_y == 36
    assert decoded.glyphs[-1].advance_or_class == 1
