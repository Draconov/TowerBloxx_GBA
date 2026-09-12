import zipfile

from tower_bloxx_extract.resources import parse_r0_table, read_resource, resource_size


def test_r0_table_shape(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        table = parse_r0_table(jar.read("r0"))
    assert len(table) == 47
    assert table[0] == 188
    assert table[45] == -121614
    assert table[46] == 36645


def test_resource_boundaries_match_canonical_jar(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        assert read_resource(jar, 0).startswith(b"\x89PNG\r\n\x1a\n")
        assert read_resource(jar, 37).startswith(b"MThd")
        assert len(read_resource(jar, 45)) == 121614
        assert read_resource(jar, 45) == jar.read("45")


def test_all_resources_are_nonempty_and_sized(tower_bloxx_jar):
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        table = parse_r0_table(jar.read("r0"))
        for resource_id in range(46):
            payload = read_resource(jar, resource_id)
            assert payload
            assert len(payload) == resource_size(table, resource_id)
