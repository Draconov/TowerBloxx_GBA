from __future__ import annotations

import hashlib
from pathlib import Path

CANONICAL_SHA256 = "7aebc77593a68e8bf58ce6e633c2e3bc4fae0d873eb909a64628bb293f920cf0"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_canonical_jar(path: Path) -> None:
    actual = sha256_file(path)
    if actual != CANONICAL_SHA256:
        raise ValueError(f"unexpected Tower Bloxx JAR SHA-256: {actual}")
