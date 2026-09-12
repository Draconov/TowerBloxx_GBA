from __future__ import annotations

import struct
import zipfile

TABLE_ENTRY_COUNT = 47
RESOURCE_COUNT = 46
TABLE_BYTES = TABLE_ENTRY_COUNT * 4


def parse_r0_table(raw: bytes) -> tuple[int, ...]:
    if len(raw) < TABLE_BYTES:
        raise ValueError("r0 is too short for its 47-entry resource table")
    table = struct.unpack(">47i", raw[:TABLE_BYTES])
    if table[0] != TABLE_BYTES:
        raise ValueError(f"unexpected first r0 payload offset: {table[0]}")
    if table[46] > len(raw):
        raise ValueError("r0 end marker exceeds packed file length")
    return table


def resource_size(table: tuple[int, ...], resource_id: int) -> int:
    if not 0 <= resource_id < RESOURCE_COUNT:
        raise IndexError(f"resource id out of range: {resource_id}")
    start = table[resource_id]
    if start < 0:
        return -start
    for next_index in range(resource_id + 1, TABLE_ENTRY_COUNT):
        end = table[next_index]
        if end >= 0 and end > start:
            return end - start
    raise ValueError(f"no end offset found for resource {resource_id}")


def read_resource(jar: zipfile.ZipFile, resource_id: int) -> bytes:
    raw = jar.read("r0")
    table = parse_r0_table(raw)
    start = table[resource_id]
    expected_size = resource_size(table, resource_id)
    if start < 0:
        payload = jar.read(str(resource_id))
    else:
        payload = raw[start:start + expected_size]
    if len(payload) != expected_size:
        raise ValueError(
            f"resource {resource_id} size mismatch: expected {expected_size}, got {len(payload)}"
        )
    return payload
