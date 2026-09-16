from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_fix159_reference_test_does_not_hardcode_chatgpt_sandbox_path() -> None:
    source = (ROOT / 'tests/test_fix159_legacy_high_altitude_parity.py').read_text(encoding='utf-8')
    assert '/mnt/data/' not in source
    assert 'TOWER_BLOXX_LEGACY_JAR' in source
