from __future__ import annotations

from dataclasses import dataclass
import struct

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass(frozen=True)
class PngInfo:
    width: int
    height: int
    bit_depth: int
    color_type: int


@dataclass(frozen=True)
class MidiInfo:
    format: int
    tracks: int
    division: int


def png_info(data: bytes) -> PngInfo:
    if len(data) < 33 or not data.startswith(PNG_SIGNATURE):
        raise ValueError("invalid PNG signature or truncated header")
    length = struct.unpack(">I", data[8:12])[0]
    chunk_type = data[12:16]
    if chunk_type != b"IHDR" or length != 13:
        raise ValueError("PNG first chunk is not a 13-byte IHDR")
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0:
        raise ValueError("PNG dimensions must be positive")
    return PngInfo(
        width=width,
        height=height,
        bit_depth=data[24],
        color_type=data[25],
    )


def midi_info(data: bytes) -> MidiInfo:
    if len(data) < 14 or data[:4] != b"MThd":
        raise ValueError("invalid or truncated MIDI header")
    header_length = struct.unpack(">I", data[4:8])[0]
    if header_length != 6:
        raise ValueError(f"unexpected MIDI header length: {header_length}")
    fmt, tracks, division = struct.unpack(">HHH", data[8:14])
    return MidiInfo(format=fmt, tracks=tracks, division=division)
