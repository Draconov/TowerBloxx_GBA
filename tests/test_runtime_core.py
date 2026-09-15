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
        str(root / "gba" / "src" / "hall_of_fame.cpp"),
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


def test_menu_worker_field_compiles_and_matches_reference_motion(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "menu_workers_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "menu_workers.cpp"),
        str(root / "tests" / "cpp" / "menu_workers_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "menu workers ok\n"


def test_gameplay_worker_field_compiles_and_matches_house_reference(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "gameplay_workers_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "gameplay_workers.cpp"),
        str(root / "tests" / "cpp" / "gameplay_workers_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "gameplay workers ok\n"


def test_hall_of_fame_core_compiles_and_runs(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "hall_of_fame_test"
    command = ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "gba" / "include"), str(root / "gba" / "src" / "hall_of_fame.cpp"), str(root / "tests" / "cpp" / "hall_of_fame_test.cpp"), "-o", str(output)]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True); assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True); assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "hall of fame ok\n"


def test_tower_session_coordinator_compiles_and_runs(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "tower_session_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "tower_session.cpp"),
        str(root / "tests" / "cpp" / "tower_session_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "tower session ok\n"


def test_fix10_flow_compiles_and_runs(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "fix10_flow_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "app_state.cpp"),
        str(root / "gba" / "src" / "hall_of_fame.cpp"),
        str(root / "gba" / "src" / "save_data.cpp"),
        str(root / "gba" / "src" / "ui_controller.cpp"),
        str(root / "gba" / "src" / "tower_session.cpp"),
        str(root / "tests" / "cpp" / "fix10_flow_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "fix10 flow ok\n"


def test_build_city_event_controller_compiles_and_runs(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "build_city_events_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "gba" / "src" / "hall_of_fame.cpp"),
        str(root / "gba" / "src" / "quick_game.cpp"),
        str(root / "gba" / "src" / "save_data.cpp"),
        str(root / "gba" / "src" / "build_city_events.cpp"),
        str(root / "tests" / "cpp" / "build_city_events_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "build city events ok\n"


def test_life_indicator_animation_matches_house_timing(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "life_indicator_animation_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba" / "include"),
        str(root / "tests" / "cpp" / "life_indicator_animation_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "life indicator animation ok\n"
