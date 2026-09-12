from io import BytesIO
import zipfile

import pytest

from tower_bloxx_extract.javaio import read_java_utf
from tower_bloxx_extract.localization import decode_locale

EXPECTED = [
    ("l0", "en-EN", "English"),
    ("l1", "fr-FR", "Français"),
    ("l2", "it-IT", "Italiano"),
    ("l3", "de-DE", "Deutsch"),
    ("l4", "es-ES", "Español"),
]


@pytest.mark.parametrize("entry,code,name", EXPECTED)
def test_locale_pack(tower_bloxx_jar, entry, code, name):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        locale = decode_locale(jar.read(entry))
    assert locale.code == code
    assert locale.display_name == name
    assert len(locale.strings) == 134
    assert all(isinstance(value, str) for value in locale.strings)


def test_modified_utf_decodes_java_nul():
    stream = BytesIO(b"\x00\x04A\xc0\x80B")
    assert read_java_utf(stream) == "A\x00B"
