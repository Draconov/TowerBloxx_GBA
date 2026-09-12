from __future__ import annotations

from pathlib import Path
import subprocess


def test_runtime_core_compiles_and_runs(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "runtime_core_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "app_state.cpp"),
        str(root / "gba" / "src" / "build_city.cpp"),
        str(root / "gba" / "src" / "save_data.cpp"),
        str(root / "gba" / "src" / "ui_controller.cpp"),
        str(root / "gba" / "src" / "quick_game.cpp"),
        str(root / "tests" / "cpp" / "runtime_core_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "runtime core ok\n"


def test_tower_construction_core_compiles_and_runs(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "tower_construction_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "tower_construction.cpp"),
        str(root / "tests" / "cpp" / "tower_construction_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "tower construction ok\n"
