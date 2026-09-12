from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct
import zipfile

from .resources import read_resource


@dataclass(frozen=True)
class AudioRoute:
    resource_id: int
    name: str
    loop: bool
    channels: int


# Two spare cells are reserved above the observed peak note polyphony so row
# effects (tracker speed and tempo) never have to steal a musical voice.  The
# largest module is still below Butano's default 16 Direct Sound music channels.
AUDIO_ROUTES = (
    AudioRoute(37, "menu_theme", True, 8),
    AudioRoute(38, "tower_theme", True, 8),
    AudioRoute(39, "city_theme", True, 12),
    AudioRoute(40, "construction_fail", False, 7),
    AudioRoute(41, "normal_roof", False, 7),
    AudioRoute(42, "trophy_roof", False, 7),
)


@dataclass(frozen=True)
class MidiAnalysis:
    division: int
    tempo_bpm: int
    last_tick: int
    note_count: int


@dataclass(frozen=True)
class _MidiEvent:
    tick: int
    kind: str
    channel: int = 0
    a: int = 0
    b: int = 0


def _read_vlq(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if offset >= len(data):
            raise ValueError("truncated MIDI VLQ")
        byte = data[offset]
        offset += 1
        value = (value << 7) | (byte & 0x7F)
        if not (byte & 0x80):
            return value, offset
    raise ValueError("invalid MIDI VLQ")


def _parse_midi(data: bytes) -> tuple[int, list[_MidiEvent]]:
    if len(data) < 22 or data[:4] != b"MThd":
        raise ValueError("invalid MIDI")
    header_length = struct.unpack(">I", data[4:8])[0]
    fmt, tracks, division = struct.unpack(">HHH", data[8:14])
    if header_length != 6 or fmt != 0 or tracks != 1 or division & 0x8000:
        raise ValueError("only format-0 PPQN MIDI is supported")
    offset = 8 + header_length
    if data[offset:offset + 4] != b"MTrk":
        raise ValueError("missing MIDI track")
    length = struct.unpack(">I", data[offset + 4:offset + 8])[0]
    track = data[offset + 8:offset + 8 + length]
    if len(track) != length:
        raise ValueError("truncated MIDI track")

    events: list[_MidiEvent] = []
    pos = 0
    tick = 0
    running_status: int | None = None
    while pos < len(track):
        delta, pos = _read_vlq(track, pos)
        tick += delta
        if pos >= len(track):
            raise ValueError("truncated MIDI event")
        byte = track[pos]
        if byte & 0x80:
            status = byte
            pos += 1
            if status < 0xF0:
                running_status = status
        else:
            if running_status is None:
                raise ValueError("MIDI running status without status")
            status = running_status

        if status == 0xFF:
            running_status = None
            if pos >= len(track):
                raise ValueError("truncated MIDI meta event")
            meta_type = track[pos]
            pos += 1
            meta_length, pos = _read_vlq(track, pos)
            payload = track[pos:pos + meta_length]
            if len(payload) != meta_length:
                raise ValueError("truncated MIDI meta payload")
            pos += meta_length
            if meta_type == 0x51 and meta_length == 3:
                micros = int.from_bytes(payload, "big")
                events.append(_MidiEvent(tick, "tempo", a=micros))
            if meta_type == 0x2F:
                break
            continue
        if status in (0xF0, 0xF7):
            running_status = None
            sysex_length, pos = _read_vlq(track, pos)
            pos += sysex_length
            if pos > len(track):
                raise ValueError("truncated MIDI sysex")
            continue
        if status >= 0xF0:
            running_status = None
            # System-common events are not expected in the canonical files.
            lengths = {0xF1: 1, 0xF2: 2, 0xF3: 1, 0xF6: 0, 0xF8: 0, 0xFA: 0, 0xFB: 0, 0xFC: 0, 0xFE: 0}
            count = lengths.get(status)
            if count is None:
                raise ValueError(f"unsupported MIDI status 0x{status:02X}")
            pos += count
            continue

        message = status >> 4
        channel = status & 0x0F
        if message in (0xC, 0xD):
            if pos >= len(track):
                raise ValueError("truncated MIDI channel event")
            first = track[pos]
            pos += 1
            second = 0
        else:
            if pos + 2 > len(track):
                raise ValueError("truncated MIDI channel event")
            first, second = track[pos], track[pos + 1]
            pos += 2

        if message == 0x8:
            events.append(_MidiEvent(tick, "off", channel, first, second))
        elif message == 0x9:
            events.append(_MidiEvent(tick, "on" if second else "off", channel, first, second))
        elif message == 0xC:
            events.append(_MidiEvent(tick, "program", channel, first, 0))
        elif message == 0xB and first == 7:
            events.append(_MidiEvent(tick, "volume", channel, second, 0))
    return division, events


def analyze_midi(data: bytes) -> MidiAnalysis:
    division, events = _parse_midi(data)
    tempos = [event.a for event in events if event.kind == "tempo" and event.a]
    micros = tempos[0] if tempos else 500_000
    bpm = int(round(60_000_000 / micros))
    return MidiAnalysis(
        division=division,
        tempo_bpm=bpm,
        last_tick=max((event.tick for event in events), default=0),
        note_count=sum(1 for event in events if event.kind == "on"),
    )


def _sample_wave(kind: str, length: int = 64) -> bytes:
    values: list[int] = []
    for index in range(length):
        phase = index / length
        if kind == "sine":
            value = math.sin(phase * math.tau)
        elif kind == "triangle":
            value = 1.0 - 4.0 * abs(phase - 0.5)
        elif kind == "square":
            value = 1.0 if phase < 0.5 else -1.0
        elif kind == "pulse":
            value = 1.0 if phase < 0.25 else -0.65
        elif kind == "saw":
            value = 2.0 * phase - 1.0
        elif kind == "steel":
            value = 0.72 * math.sin(phase * math.tau) + 0.28 * math.sin(phase * math.tau * 3)
        else:
            value = math.sin(phase * math.tau)
        signed = max(-127, min(127, int(round(value * 92))))
        values.append(signed & 0xFF)
    return bytes(values)


def _drum_wave(kind: str, length: int = 384) -> bytes:
    # Deterministic pseudo-noise + simple pitched components. These samples are
    # intentionally compact; MIDI timing/pitch/routing is the canonical part,
    # while the original Nokia synth timbre varied by handset.
    state = 0x12345
    out = bytearray()
    for index in range(length):
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        noise = ((state >> 16) & 0xFF) - 128
        decay = (length - index) / length
        if kind == "kick":
            tone = math.sin(index * (0.19 - 0.12 * index / length)) * 105
            value = tone * decay
        elif kind == "hat":
            value = noise * decay * 0.7
        elif kind == "tom":
            value = math.sin(index * 0.31) * 95 * decay
        else:  # snare / cymbal
            tone = math.sin(index * 0.47) * 30
            value = (noise * 0.72 + tone) * decay
        signed = max(-127, min(127, int(round(value))))
        out.append(signed & 0xFF)
    if len(out) & 1:
        out.append(0)
    return bytes(out)


# 1-based MOD sample IDs.
_TONAL_SAMPLES = {
    4: 1,    # electric piano
    28: 2,   # muted guitar
    35: 3,   # bass
    36: 4,   # slap bass
    45: 5,   # pizzicato strings
    49: 6,   # strings
    76: 7,   # flute family
    114: 8,  # steel drums / mallet family
}


def _program_sample(program: int) -> int:
    if program in _TONAL_SAMPLES:
        return _TONAL_SAMPLES[program]
    # Deterministic coarse GM family fallback.
    if program < 16:
        return 1
    if program < 32:
        return 2
    if program < 40:
        return 3
    if program < 56:
        return 6
    if program < 80:
        return 7
    return 8


def _drum_sample(note: int) -> int:
    if note in (35, 36):
        return 9
    if note in (38, 40):
        return 10
    if note in (42, 44, 46):
        return 11
    if 41 <= note <= 50:
        return 12
    return 13


def _mod_period(note: int) -> int:
    # ProTracker C-3 period 428; keep the 12-bit period rather than octave-
    # folding so high/low MIDI notes retain their pitch ratio in Maxmod.
    period = int(round(428.0 * (2.0 ** ((48 - note) / 12.0))))
    return max(1, min(4095, period))


def _cell(sample: int = 0, period: int = 0, effect: int = 0, parameter: int = 0) -> bytes:
    return bytes((
        (sample & 0xF0) | ((period >> 8) & 0x0F),
        period & 0xFF,
        ((sample & 0x0F) << 4) | (effect & 0x0F),
        parameter & 0xFF,
    ))


def _sample_bank() -> list[tuple[str, bytes, bool]]:
    return [
        ("E.PIANO", _sample_wave("sine"), True),
        ("M.GUITAR", _sample_wave("saw"), True),
        ("BASS", _sample_wave("square"), True),
        ("SLAPBASS", _sample_wave("pulse"), True),
        ("PIZZ", _sample_wave("triangle"), True),
        ("STRINGS", _sample_wave("saw"), True),
        ("FLUTE", _sample_wave("sine"), True),
        ("STEEL", _sample_wave("steel"), True),
        ("KICK", _drum_wave("kick"), False),
        ("SNARE", _drum_wave("snare"), False),
        ("HAT", _drum_wave("hat"), False),
        ("TOM", _drum_wave("tom"), False),
        ("CYMBAL", _drum_wave("cymbal", 512), False),
    ]


def _mod_signature(channels: int) -> bytes:
    if channels == 4:
        return b"M.K."
    if 5 <= channels <= 9:
        return f"{channels}CHN".encode("ascii")
    if 10 <= channels <= 16:
        return f"{channels}CH".encode("ascii")
    raise ValueError("MOD channel count must be between 4 and 16")


def midi_to_mod(data: bytes, title: str, channels: int = 8) -> bytes:
    _mod_signature(channels)  # validate early
    division, events = _parse_midi(data)

    # The canonical files use PPQN 480 and many 40/80-tick note boundaries.
    # A 40-tick row is 1/12 of a quarter note. MOD speed 2 gives a row time
    # of 5/BPM seconds, exactly matching 40 PPQN ticks at the same MIDI BPM.
    ticks_per_row = max(1, division // 12)
    programs = [0] * 16
    channel_volumes = [127] * 16
    active: dict[tuple[int, int], tuple[int, int]] = {}
    voice_started = [-(10**9)] * channels
    voice_key: list[tuple[int, int] | None] = [None] * channels

    max_tick = max((event.tick for event in events), default=0)
    row_count = max(1, (max_tick + ticks_per_row - 1) // ticks_per_row + 2)
    pattern_count = (row_count + 63) // 64
    if pattern_count > 128:
        raise ValueError("MIDI is too long for MOD order table")
    cells = [[bytearray(_cell()) for _ in range(channels)] for _ in range(pattern_count * 64)]
    global_effects: dict[int, list[int]] = {0: [2]}  # F02 tracker speed

    def choose_voice(row: int) -> int:
        for voice, key in enumerate(voice_key):
            # Percussion is not held, but its cell must remain reserved for the
            # current row so simultaneous drum hits cannot overwrite each other.
            if key is None and voice_started[voice] != row:
                return voice
        return min(range(channels), key=lambda voice: voice_started[voice])

    for event in events:
        row = min(len(cells) - 1, (event.tick + ticks_per_row // 2) // ticks_per_row)
        if event.kind == "program":
            programs[event.channel] = event.a
            continue
        if event.kind == "volume":
            channel_volumes[event.channel] = event.a
            continue
        if event.kind == "tempo":
            bpm = max(32, min(255, int(round(60_000_000 / event.a))))
            global_effects.setdefault(row, []).append(bpm)
            continue
        if event.kind == "off":
            key = (event.channel, event.a)
            found = active.pop(key, None)
            if found is not None:
                voice, _started = found
                if voice_key[voice] == key:
                    cells[row][voice] = bytearray(_cell(effect=0x0C, parameter=0))
                    voice_key[voice] = None
            continue
        if event.kind != "on":
            continue

        key = (event.channel, event.a)
        # Retrigger of same key releases its previous voice first.
        old = active.pop(key, None)
        if old is not None and voice_key[old[0]] == key:
            voice_key[old[0]] = None
        voice = choose_voice(row)
        stolen = voice_key[voice]
        if stolen is not None:
            active.pop(stolen, None)
        voice_key[voice] = key if event.channel != 9 else None
        voice_started[voice] = row
        if event.channel != 9:
            active[key] = (voice, row)
            sample = _program_sample(programs[event.channel])
            note = event.a
        else:
            sample = _drum_sample(event.a)
            # Percussion samples are authored around C-3.
            note = 48
        velocity = event.b * channel_volumes[event.channel] // 127
        volume = max(1, min(64, (velocity * 64 + 63) // 127))
        cells[row][voice] = bytearray(_cell(sample, _mod_period(note), 0x0C, volume))

    def place_global_effect(row: int, parameter: int) -> None:
        row_cells = cells[row]
        # Prefer unused cells. The route channel budget intentionally leaves two
        # spare voices at peak polyphony, so canonical rows should always land here.
        for cell_data in row_cells:
            if cell_data == bytearray(_cell()):
                cell_data[2] = (cell_data[2] & 0xF0) | 0x0F
                cell_data[3] = parameter
                return
        # A full-volume C40 effect is redundant because sample default volume is
        # already 64; it is safe to reuse that effect column without changing pitch.
        for cell_data in row_cells:
            if (cell_data[2] & 0x0F) == 0x0C and cell_data[3] == 64:
                cell_data[2] = (cell_data[2] & 0xF0) | 0x0F
                cell_data[3] = parameter
                return
        raise ValueError(f"no MOD effect slot available at row {row}")

    for row, parameters in global_effects.items():
        for parameter in parameters:
            place_global_effect(row, parameter)

    samples = _sample_bank()
    header = bytearray()
    header.extend(title.encode("ascii", "replace")[:20].ljust(20, b"\0"))
    for index in range(31):
        if index < len(samples):
            name, payload, looped = samples[index]
            header.extend(name.encode("ascii")[:22].ljust(22, b"\0"))
            header.extend(struct.pack(">H", len(payload) // 2))
            header.append(0)  # finetune
            header.append(64)
            if looped:
                header.extend(struct.pack(">HH", 0, len(payload) // 2))
            else:
                header.extend(struct.pack(">HH", 0, 1))
        else:
            header.extend(b"\0" * 30)
    header.append(pattern_count)
    header.append(0x7F)
    header.extend(bytes(range(pattern_count)).ljust(128, b"\0"))
    header.extend(_mod_signature(channels))
    if len(header) != 1084:
        raise AssertionError(len(header))

    pattern_bytes = bytearray()
    for cell_row in cells:
        for cell_data in cell_row:
            pattern_bytes.extend(cell_data)
    sample_bytes = b"".join(payload for _name, payload, _looped in samples)
    return bytes(header + pattern_bytes + sample_bytes)


def export_gba_audio_assets(jar_path: Path, project_dir: Path) -> dict[str, object]:
    jar_path = Path(jar_path)
    project_dir = Path(project_dir)
    audio_dir = project_dir / "gba" / "audio"
    reference_dir = project_dir / "gba" / "reference"
    audio_dir.mkdir(parents=True, exist_ok=True)
    reference_dir.mkdir(parents=True, exist_ok=True)

    routes: list[dict[str, object]] = []
    with zipfile.ZipFile(jar_path) as jar:
        for route in AUDIO_ROUTES:
            midi = read_resource(jar, route.resource_id)
            analysis = analyze_midi(midi)
            channels = route.channels
            module = midi_to_mod(midi, f"TB {route.resource_id} {route.name}", channels)
            output = audio_dir / f"{route.name}.mod"
            output.write_bytes(module)
            routes.append({
                "resource_id": route.resource_id,
                "name": route.name,
                "loop": route.loop,
                "channels": channels,
                "tempo_bpm": analysis.tempo_bpm,
                "division": analysis.division,
                "note_count": analysis.note_count,
                "sha256": hashlib.sha256(module).hexdigest(),
                "size": len(module),
            })

    manifest: dict[str, object] = {
        "module_count": len(routes),
        "format": "route-sized multichannel MOD modules",
        "timing": "40-tick MIDI grid step per MOD row at speed 2",
        "ticks_per_row": 40,
        "mod_speed": 2,
        "routes": routes,
    }
    (reference_dir / "audio_manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8"
    )
    return manifest
