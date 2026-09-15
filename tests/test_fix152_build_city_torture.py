from pathlib import Path
import subprocess


def test_build_city_torture_matrix(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1]
    output = tmp_path / "build_city_torture_test"
    command = [
        "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-DTB_HOST_TEST",
        "-I", str(root / "gba/include"),
        str(root / "gba/src/app_state.cpp"),
        str(root / "gba/src/build_city.cpp"),
        str(root / "gba/src/hall_of_fame.cpp"),
        str(root / "gba/src/save_data.cpp"),
        str(root / "tests/cpp/build_city_torture_test.cpp"),
        "-o", str(output),
    ]
    built = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(output)], cwd=root, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    assert ran.stdout == "build city torture ok\n"
