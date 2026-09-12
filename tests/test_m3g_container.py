from __future__ import annotations

import zipfile

import pytest

from tower_bloxx_extract.m3g import parse_m3g
from tower_bloxx_extract.resources import read_resource


def _canonical_m3g(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        return read_resource(jar, 45)


def test_m3g_container_shape(tower_bloxx_jar):
    scene = parse_m3g(_canonical_m3g(tower_bloxx_jar))
    assert scene.header.total_file_size == 121614
    assert len(scene.sections) == 4
    assert [section.compression_scheme for section in scene.sections] == [0, 0, 0, 0]
    assert [len(section.object_bytes) for section in scene.sections] == [47, 0, 119651, 1852]
    assert len(scene.chunks) == 270


def test_m3g_rejects_bad_checksum(tower_bloxx_jar):
    raw = bytearray(_canonical_m3g(tower_bloxx_jar))
    raw[100] ^= 1
    with pytest.raises(ValueError, match="checksum"):
        parse_m3g(bytes(raw))


def test_m3g_rejects_truncated_section(tower_bloxx_jar):
    raw = _canonical_m3g(tower_bloxx_jar)[:-1]
    with pytest.raises(ValueError, match="section"):
        parse_m3g(raw)
