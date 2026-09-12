import zipfile

from tower_bloxx_extract.media import midi_info, png_info
from tower_bloxx_extract.resources import read_resource


def test_media_resource_ranges(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        pngs = [png_info(read_resource(jar, rid)) for rid in range(37)]
        midis = [midi_info(read_resource(jar, rid)) for rid in range(37, 43)]
    assert len(pngs) == 37
    assert all(p.width > 0 and p.height > 0 for p in pngs)
    assert all(m.format == 0 and m.division == 480 for m in midis)


def test_png_ihdr_is_required():
    bad = b"\x89PNG\r\n\x1a\n" + b"\x00" * 25
    try:
        png_info(bad)
    except ValueError as exc:
        assert "IHDR" in str(exc)
    else:
        raise AssertionError("invalid PNG was accepted")
