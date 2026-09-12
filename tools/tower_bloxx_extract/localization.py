from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO
import struct

from .javaio import read_java_utf

LOCALE_STRING_COUNT = 134


@dataclass(frozen=True)
class LocalePack:
    code: str
    display_name: str
    offsets: tuple[int, ...]
    strings: tuple[str, ...]


def decode_locale(raw: bytes) -> LocalePack:
    stream = BytesIO(raw)
    code = read_java_utf(stream)
    display_name = read_java_utf(stream)
    offsets_raw = stream.read(LOCALE_STRING_COUNT * 4)
    if len(offsets_raw) != LOCALE_STRING_COUNT * 4:
        raise ValueError("locale offset table is truncated")
    offsets = struct.unpack(f">{LOCALE_STRING_COUNT}i", offsets_raw)
    if any(offset < 0 or offset >= len(raw) for offset in offsets):
        raise ValueError("locale string offset is outside file")
    if tuple(sorted(offsets)) != offsets:
        raise ValueError("locale string offsets are not monotonic")

    strings: list[str] = []
    for offset in offsets:
        stream.seek(offset)
        strings.append(read_java_utf(stream))
    return LocalePack(
        code=code,
        display_name=display_name,
        offsets=tuple(offsets),
        strings=tuple(strings),
    )
