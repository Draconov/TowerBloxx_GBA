from __future__ import annotations

from typing import BinaryIO


def _read_exact(stream: BinaryIO, size: int) -> bytes:
    data = stream.read(size)
    if len(data) != size:
        raise ValueError(f"truncated Java stream: expected {size} bytes, got {len(data)}")
    return data


def read_java_utf(stream: BinaryIO) -> str:
    """Decode Java DataInputStream.readUTF() modified UTF-8."""
    length_bytes = _read_exact(stream, 2)
    byte_length = int.from_bytes(length_bytes, "big", signed=False)
    data = _read_exact(stream, byte_length)

    code_units: list[int] = []
    index = 0
    while index < len(data):
        first = data[index]
        if first & 0x80 == 0:
            if first == 0:
                raise ValueError("modified UTF-8 contains forbidden literal NUL byte")
            code_units.append(first)
            index += 1
            continue
        if first & 0xE0 == 0xC0:
            if index + 1 >= len(data):
                raise ValueError("truncated two-byte modified UTF-8 sequence")
            second = data[index + 1]
            if second & 0xC0 != 0x80:
                raise ValueError("invalid modified UTF-8 continuation byte")
            value = ((first & 0x1F) << 6) | (second & 0x3F)
            if value < 0x80 and value != 0:
                raise ValueError("overlong modified UTF-8 sequence")
            code_units.append(value)
            index += 2
            continue
        if first & 0xF0 == 0xE0:
            if index + 2 >= len(data):
                raise ValueError("truncated three-byte modified UTF-8 sequence")
            second = data[index + 1]
            third = data[index + 2]
            if second & 0xC0 != 0x80 or third & 0xC0 != 0x80:
                raise ValueError("invalid modified UTF-8 continuation byte")
            value = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F)
            if value < 0x800:
                raise ValueError("overlong modified UTF-8 sequence")
            code_units.append(value)
            index += 3
            continue
        raise ValueError(f"unsupported modified UTF-8 lead byte: 0x{first:02x}")

    chars: list[str] = []
    index = 0
    while index < len(code_units):
        unit = code_units[index]
        if 0xD800 <= unit <= 0xDBFF:
            if index + 1 >= len(code_units):
                raise ValueError("unpaired high surrogate in modified UTF-8")
            low = code_units[index + 1]
            if not 0xDC00 <= low <= 0xDFFF:
                raise ValueError("high surrogate not followed by low surrogate")
            value = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00)
            chars.append(chr(value))
            index += 2
        elif 0xDC00 <= unit <= 0xDFFF:
            raise ValueError("unpaired low surrogate in modified UTF-8")
        else:
            chars.append(chr(unit))
            index += 1
    return "".join(chars)
