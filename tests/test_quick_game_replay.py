from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess


def _build_and_run(tmp_path: Path) -> bytes:
    tmp_path.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "quick_game_replay"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "app_state.cpp"),
        str(root / "gba" / "src" / "quick_game.cpp"),
        str(root / "tests" / "cpp" / "quick_game_replay.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True)
    assert ran.returncode == 0, ran.stdout.decode(errors="replace") + ran.stderr.decode(errors="replace")
    return ran.stdout


def test_quick_game_replay_is_deterministic_and_reaches_results(tmp_path: Path) -> None:
    first = _build_and_run(tmp_path / "a")
    second = _build_and_run(tmp_path / "b")
    assert first == second
    assert first.rstrip().endswith(b"RESULTS 10 0 2 118 8")
    assert hashlib.sha256(first).hexdigest() == "2278f4bea6a837f885d4440df17120f08bba3886557caf4dd54a43b994ba832a"
