from pathlib import Path

import pytest

from tower_bloxx_extract.io import CANONICAL_SHA256, sha256_file, validate_canonical_jar


def test_canonical_jar_hash(tower_bloxx_jar: Path) -> None:
    assert sha256_file(tower_bloxx_jar) == CANONICAL_SHA256


def test_rejects_wrong_jar(tmp_path: Path) -> None:
    path = tmp_path / "wrong.jar"
    path.write_bytes(b"not the game")
    with pytest.raises(ValueError, match="SHA-256"):
        validate_canonical_jar(path)
