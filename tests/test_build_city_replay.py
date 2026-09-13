from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess


def test_build_city_replay_is_deterministic_and_pinned(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    source = root / "tests" / "cpp" / "build_city_replay.cpp"
    assert source.is_file()

    output = tmp_path / "build_city_replay"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "app_state.cpp"),
        str(root / "gba" / "src" / "build_city.cpp"),
        str(root / "gba" / "src" / "hall_of_fame.cpp"),
        str(root / "gba" / "src" / "save_data.cpp"),
        str(source),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr

    run_a = subprocess.run([str(output)], cwd=root, capture_output=True, text=True, check=True).stdout
    run_b = subprocess.run([str(output)], cwd=root, capture_output=True, text=True, check=True).stdout
    assert run_a == run_b
    assert run_a.startswith("START 0 0 0 1 10\n")
    assert "UNLOCK 2 20" in run_a
    assert "UNLOCK 3 30" in run_a
    assert "UNLOCK 4 40" in run_a
    assert "FINAL 2200 10 4 4 4" in run_a
    assert "DISCARD 2200 0" in run_a

    digest = hashlib.sha256(run_a.encode()).hexdigest()
    assert digest == "691a92771e2c6a4c0e08c477dfa7b0dc93555328f8e31e445ccec0b961b2b055"


def test_build_city_hud_state_contract(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "build_city_hud_state_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "app_state.cpp"),
        str(root / "gba" / "src" / "build_city.cpp"),
        str(root / "gba" / "src" / "hall_of_fame.cpp"),
        str(root / "gba" / "src" / "quick_game.cpp"),
        str(root / "gba" / "src" / "save_data.cpp"),
        str(root / "tests" / "cpp" / "build_city_hud_state_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "build city hud state ok\n"
