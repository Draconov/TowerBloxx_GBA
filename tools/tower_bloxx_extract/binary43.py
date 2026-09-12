from __future__ import annotations

from dataclasses import dataclass
import struct


@dataclass(frozen=True)
class Resource43Entry:
    x_ref: int
    width_ref: int
    y_start: int
    height: int
    kind: int


@dataclass(frozen=True)
class Resource43:
    entry_count: int
    int_count: int
    int_values: tuple[int, ...]
    entries: tuple[Resource43Entry, ...]
    bytes_consumed: int


def decode_resource_43(data: bytes) -> Resource43:
    if len(data) < 2:
        raise ValueError("resource 43 is truncated before counts")
    entry_count = data[0]
    int_count = data[1]
    offset = 2

    int_bytes = int_count * 4
    if offset + int_bytes > len(data):
        raise ValueError("resource 43 int table is truncated")
    int_values = struct.unpack(f">{int_count}i", data[offset:offset + int_bytes]) if int_count else ()
    offset += int_bytes

    entries: list[Resource43Entry] = []
    for _ in range(entry_count):
        if offset + 7 > len(data):
            raise ValueError("resource 43 entry table is truncated")
        x_ref = data[offset]
        width_ref = data[offset + 1]
        y_start = int.from_bytes(data[offset + 2:offset + 4], "big", signed=False)
        height = int.from_bytes(data[offset + 4:offset + 6], "big", signed=False)
        kind = data[offset + 6]
        entries.append(Resource43Entry(x_ref, width_ref, y_start, height, kind))
        offset += 7

    if offset != len(data):
        raise ValueError(f"resource 43 has {len(data) - offset} trailing bytes")
    return Resource43(
        entry_count=entry_count,
        int_count=int_count,
        int_values=tuple(int_values),
        entries=tuple(entries),
        bytes_consumed=offset,
    )
