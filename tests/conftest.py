from __future__ import annotations

import os
from pathlib import Path

import pytest


@pytest.fixture(scope="session")
def tower_bloxx_jar() -> Path:
    configured = os.environ.get("TOWER_BLOXX_JAR")
    if configured:
        path = Path(configured)
    else:
        path = Path("/mnt/data/Tower-Bloxx_v1522.jar")
    if not path.is_file():
        pytest.skip("canonical Tower Bloxx JAR is not available")
    return path
