from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "gba-release.yml"
PACKAGER = ROOT / "scripts" / "ci" / "package_rom.sh"


def _workflow_text() -> str:
    assert WORKFLOW.exists(), "GitHub Actions ROM workflow is missing"
    return WORKFLOW.read_text(encoding="utf-8")


def test_workflow_runs_host_tests_and_arm_builds_on_push_pr_and_manual_dispatch():
    text = _workflow_text()
    assert "push:" in text
    assert "pull_request:" in text
    assert "workflow_dispatch:" in text
    assert "host-tests:" in text
    assert "python -m pytest -q" in text
    assert "gba-build:" in text
    assert "container:" in text
    assert "devkitpro/devkitarm" in text



def test_workflow_adds_devkitarm_bin_to_github_path_before_tool_verification():
    text = _workflow_text()
    path_line = 'echo "$DEVKITARM/bin" >> "$GITHUB_PATH"'
    assert path_line in text
    assert 'test -d "$DEVKITARM/bin"' in text
    assert text.index(path_line) < text.index('command -v arm-none-eabi-g++')

def test_workflow_pins_butano_and_builds_existing_gba_makefile():
    text = _workflow_text()
    assert "21.7.1" in text
    assert "GValiente/butano" in text
    assert 'make -C gba' in text
    assert 'LIBBUTANO="$GITHUB_WORKSPACE/.ci/butano/butano"' in text
    assert "TowerBloxxGBA.gba" in text


def test_workflow_uploads_renamed_rom_and_checksum_on_every_successful_build():
    text = _workflow_text()
    assert "actions/upload-artifact@v4" in text
    assert "TowerBloxx.gba" in text
    assert "TowerBloxx.gba.sha256" in text
    assert 'bash scripts/ci/package_rom.sh "$GITHUB_WORKSPACE"' in text
    assert PACKAGER.exists(), "ROM packaging script is missing"


def test_release_is_tag_gated_and_uses_github_token():
    text = _workflow_text()
    assert "tags:" in text
    assert "'v*'" in text or '"v*"' in text
    assert "release:" in text
    assert "startsWith(github.ref, 'refs/tags/v')" in text
    assert "needs: [host-tests, gba-build]" in text
    assert "actions/download-artifact@v4" in text
    assert "gh release create" in text
    assert "GH_TOKEN: ${{ github.token }}" in text


def test_package_rom_script_verifies_checksum_from_dist_directory(tmp_path):
    import subprocess

    gba_dir = tmp_path / "gba"
    gba_dir.mkdir()
    source_rom = gba_dir / "TowerBloxxGBA.gba"
    source_rom.write_bytes(b"tower-bloxx-ci-fixture\n")

    result = subprocess.run(
        ["bash", str(PACKAGER), str(tmp_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr + result.stdout
    release_rom = tmp_path / "dist" / "TowerBloxx.gba"
    checksum = tmp_path / "dist" / "TowerBloxx.gba.sha256"
    assert release_rom.read_bytes() == source_rom.read_bytes()
    assert checksum.read_text(encoding="utf-8").endswith("  TowerBloxx.gba\n")
