from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess


def _build_and_run(tmp_path: Path) -> bytes:
    tmp_path.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    source = root / "tests" / "cpp" / "tower_construction_replay.cpp"
    assert source.is_file()

    output = tmp_path / "tower_construction_replay"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "tower_construction.cpp"),
        str(source),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True)
    assert ran.returncode == 0, ran.stdout.decode(errors="replace") + ran.stderr.decode(errors="replace")
    return ran.stdout


def test_tower_construction_replay_is_deterministic_and_pinned(tmp_path: Path) -> None:
    first = _build_and_run(tmp_path / "a")
    second = _build_and_run(tmp_path / "b")
    assert first == second
    assert first.rstrip().endswith(b"RESULT 1 182 2 10 3")
    assert b"ROOF_RETRY " in first
    assert hashlib.sha256(first).hexdigest() == "ef2404e1ca0adee0067ccde3a4e11ea49d1b5803f7c87402471c96177da5842c"
