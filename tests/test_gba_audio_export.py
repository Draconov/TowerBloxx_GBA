from __future__ import annotations

from pathlib import Path

from tower_bloxx_extract.gba_audio_export import (
    AUDIO_ROUTES,
    analyze_midi,
    export_gba_audio_assets,
    midi_to_mod,
)
from tower_bloxx_extract.resources import read_resource
import zipfile


def test_original_midi_contract_is_recovered(tower_bloxx_jar: Path) -> None:
    with zipfile.ZipFile(tower_bloxx_jar) as jar:
        infos = [analyze_midi(read_resource(jar, rid)) for rid in range(37, 43)]
    assert [info.division for info in infos] == [480] * 6
    assert [info.tempo_bpm for info in infos] == [160, 160, 140, 160, 160, 160]
    assert [route.resource_id for route in AUDIO_ROUTES] == [37, 38, 39, 40, 41, 42]
    assert [route.name for route in AUDIO_ROUTES] == [
        "menu_theme", "tower_theme", "city_theme",
        "construction_fail", "normal_roof", "trophy_roof",
    ]
    assert [route.loop for route in AUDIO_ROUTES] == [True, True, True, False, False, False]


def test_export_gba_audio_assets_writes_deterministic_route_sized_modules(
    tower_bloxx_jar: Path, tmp_path: Path
) -> None:
    first = tmp_path / "a"
    second = tmp_path / "b"
    manifest_a = export_gba_audio_assets(tower_bloxx_jar, first)
    manifest_b = export_gba_audio_assets(tower_bloxx_jar, second)

    assert manifest_a == manifest_b
    assert manifest_a["module_count"] == 6
    assert manifest_a["routes"][0]["name"] == "menu_theme"
    for route in manifest_a["routes"]:
        left = first / "gba" / "audio" / f"{route['name']}.mod"
        right = second / "gba" / "audio" / f"{route['name']}.mod"
        assert left.read_bytes() == right.read_bytes()
        payload = left.read_bytes()
        expected_channels = {
            "menu_theme": 8,
            "tower_theme": 8,
            "city_theme": 12,
            "construction_fail": 7,
            "normal_roof": 7,
            "trophy_roof": 7,
        }[route["name"]]
        expected_signature = (
            f"{expected_channels}CH".encode("ascii")
            if expected_channels >= 10
            else f"{expected_channels}CHN".encode("ascii")
        )
        assert payload[1080:1084] == expected_signature
        assert route["channels"] == expected_channels
        assert len(payload) > 1084

    assert manifest_a["ticks_per_row"] == 40
    assert manifest_a["mod_speed"] == 2
    assert "40-tick" in manifest_a["timing"]


def _vlq(value: int) -> bytes:
    encoded = [value & 0x7F]
    value >>= 7
    while value:
        encoded.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(encoded))


def _format0_midi(events: list[tuple[int, bytes]], division: int = 480) -> bytes:
    track = bytearray()
    for delta, payload in events:
        track.extend(_vlq(delta))
        track.extend(payload)
    track.extend(b"\x00\xFF\x2F\x00")
    return (
        b"MThd" + (6).to_bytes(4, "big") + (0).to_bytes(2, "big") +
        (1).to_bytes(2, "big") + division.to_bytes(2, "big") +
        b"MTrk" + len(track).to_bytes(4, "big") + bytes(track)
    )


def _mod_cell(module: bytes, channels: int, row: int, channel: int) -> tuple[int, int, int, int]:
    offset = 1084 + (row * channels + channel) * 4
    b0, b1, b2, parameter = module[offset:offset + 4]
    sample = (b0 & 0xF0) | (b2 >> 4)
    period = ((b0 & 0x0F) << 8) | b1
    return sample, period, b2 & 0x0F, parameter


def test_mod_tonal_samples_use_c3_calibrated_64_sample_cycles() -> None:
    midi = _format0_midi([
        (0, b"\xC0\x04"),
        (0, b"\x90\x30\x7F"),
    ])
    module = midi_to_mod(midi, "calibration", 4)

    # Sample 1 is a one-cycle tonal waveform. At ProTracker C-3 period 428,
    # a 64-sample cycle is the correct tracker octave; 512 samples is 3 octaves low.
    first_sample_length_words = int.from_bytes(module[42:44], "big")
    assert first_sample_length_words == 32


def test_mod_uses_40_tick_rows_speed_2_and_preserves_sub_16th_timing() -> None:
    midi = _format0_midi([
        (0, b"\xFF\x51\x03\x05\xB8\xD8"),  # 160 BPM
        (0, b"\xC0\x04"),
        (0, b"\x90\x30\x7F"),
        (40, b"\x90\x32\x7F"),
    ])
    module = midi_to_mod(midi, "timing", 4)

    row0 = [_mod_cell(module, 4, 0, channel) for channel in range(4)]
    row1 = [_mod_cell(module, 4, 1, channel) for channel in range(4)]
    assert any(effect == 0x0F and parameter == 0x02 for _, _, effect, parameter in row0)
    assert any(effect == 0x0F and parameter == 160 for _, _, effect, parameter in row0)
    assert any(sample != 0 for sample, _, _, _ in row1)


def test_mod_keeps_simultaneous_percussion_hits_on_distinct_voices() -> None:
    midi = _format0_midi([
        (0, b"\x99\x23\x7F"),  # kick, MIDI 35
        (0, b"\x99\x26\x7F"),  # snare, MIDI 38
    ])
    module = midi_to_mod(midi, "drums", 4)

    samples = {
        sample
        for channel in range(4)
        for sample, _, _, _ in [_mod_cell(module, 4, 0, channel)]
        if sample
    }
    assert {9, 10} <= samples
