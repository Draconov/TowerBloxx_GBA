from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import zipfile

from .io import validate_canonical_jar


@dataclass(frozen=True)
class JarInfo:
    manifest: dict[str, str]
    class_names: tuple[str, ...]
    entries: tuple[str, ...]


def parse_manifest(raw: bytes) -> dict[str, str]:
    text = raw.decode("utf-8")
    logical_lines: list[str] = []
    for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        if line.startswith(" ") and logical_lines:
            logical_lines[-1] += line[1:]
        elif line:
            logical_lines.append(line)
    result: dict[str, str] = {}
    for line in logical_lines:
        if ": " not in line:
            continue
        key, value = line.split(": ", 1)
        result[key] = value
    return result


def inspect_jar(path: Path) -> JarInfo:
    validate_canonical_jar(path)
    with zipfile.ZipFile(path) as jar:
        entries = tuple(sorted(jar.namelist()))
        manifest = parse_manifest(jar.read("META-INF/MANIFEST.MF"))
        class_names = tuple(sorted(
            name[:-6]
            for name in entries
            if name.endswith(".class") and "/" not in name
        ))
    return JarInfo(manifest=manifest, class_names=class_names, entries=entries)
