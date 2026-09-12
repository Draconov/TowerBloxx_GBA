from tower_bloxx_extract.jar import inspect_jar


def test_manifest_identity(tower_bloxx_jar):
    info = inspect_jar(tower_bloxx_jar)
    assert info.manifest["MIDlet-Name"] == "Tower Bloxx(TM)"
    assert info.manifest["MIDlet-Vendor"] == "Digital Chocolate, Inc."
    assert info.manifest["MIDlet-Version"] == "1.5.22"
    assert info.manifest["MicroEdition-Configuration"] == "CLDC-1.0"
    assert info.manifest["MicroEdition-Profile"] == "MIDP-1.0"


def test_class_inventory(tower_bloxx_jar):
    info = inspect_jar(tower_bloxx_jar)
    assert info.class_names == (
        "GameMIDlet", "House", "a", "b", "c", "d", "e", "f", "g",
        "h", "i", "j", "k", "l", "m", "n", "o",
    )
