import zipfile

from tower_bloxx_extract.binary43 import decode_resource_43
from tower_bloxx_extract.resources import read_resource


def test_resource_43_consumes_exact_payload(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        raw = read_resource(jar, 43)
    decoded = decode_resource_43(raw)
    assert decoded.bytes_consumed == len(raw)
    assert len(decoded.entries) == decoded.entry_count
    assert len(decoded.int_values) == decoded.int_count
    assert decoded.entry_count == 88
    assert decoded.int_count == 17
    assert decoded.int_values[0] == 9144970
    assert decoded.int_values[-1] == 8226695
    assert decoded.entries[0].x_ref == 88
    assert decoded.entries[0].width_ref == 40
    assert decoded.entries[-1].y_start == 434
    assert decoded.entries[-1].kind == 14
