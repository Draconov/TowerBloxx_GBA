from __future__ import annotations

import argparse
import csv
from dataclasses import asdict
from io import StringIO
import json
from pathlib import Path
import shutil
import zipfile

from .binary43 import decode_resource_43
from .binary44 import decode_resource_44
from .io import CANONICAL_SHA256, validate_canonical_jar
from .jar import inspect_jar
from .localization import decode_locale
from .manifest import sha256_bytes
from .media import midi_info, png_info
from .resources import parse_r0_table, read_resource

LOCALE_ENTRIES = ("l0", "l1", "l2", "l3", "l4")
M3G_SHA256 = "41f755aeeeeb42a7cd1c9acaf642a4609e4d7993d910cf5c5fe6b217cba79ae1"


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    path.write_text(text, encoding="utf-8", newline="\n")


def _resource_kind(resource_id: int) -> str:
    if 0 <= resource_id <= 36:
        return "png"
    if 37 <= resource_id <= 42:
        return "midi"
    if resource_id in (43, 44):
        return "binary"
    if resource_id == 45:
        return "m3g"
    raise ValueError(f"unknown resource id: {resource_id}")


def _resource_filename(resource_id: int) -> str:
    extension = {
        "png": "png",
        "midi": "mid",
        "binary": "bin",
        "m3g": "m3g",
    }[_resource_kind(resource_id)]
    return f"{resource_id:03d}.{extension}"


def extract_reference(jar_path: Path, output_dir: Path, *, include_m3g: bool = False) -> None:
    jar_path = Path(jar_path)
    output_dir = Path(output_dir)
    validate_canonical_jar(jar_path)
    info = inspect_jar(jar_path)

    if output_dir.exists():
        shutil.rmtree(output_dir)
    resources_dir = output_dir / "resources"
    locales_dir = output_dir / "locales"
    decoded_dir = output_dir / "decoded"
    resources_dir.mkdir(parents=True)
    locales_dir.mkdir(parents=True)
    decoded_dir.mkdir(parents=True)

    inventory: list[dict[str, object]] = []
    counts = {"png": 0, "midi": 0, "binary": 0, "m3g": 0, "locale": 0}

    with zipfile.ZipFile(jar_path) as jar:
        r0 = jar.read("r0")
        table = parse_r0_table(r0)
        for resource_id in range(46):
            payload = read_resource(jar, resource_id)
            kind = _resource_kind(resource_id)
            counts[kind] += 1
            destination = resources_dir / _resource_filename(resource_id)
            destination.write_bytes(payload)

            row: dict[str, object] = {
                "resource_id": resource_id,
                "container": "external" if table[resource_id] < 0 else "r0",
                "offset": "" if table[resource_id] < 0 else table[resource_id],
                "size": len(payload),
                "type": kind,
                "width": "",
                "height": "",
                "midi_format": "",
                "midi_tracks": "",
                "midi_division": "",
                "sha256": sha256_bytes(payload),
            }
            if kind == "png":
                meta = png_info(payload)
                row["width"] = meta.width
                row["height"] = meta.height
            elif kind == "midi":
                meta = midi_info(payload)
                row["midi_format"] = meta.format
                row["midi_tracks"] = meta.tracks
                row["midi_division"] = meta.division
            inventory.append(row)

        locale_manifest: list[dict[str, object]] = []
        for entry in LOCALE_ENTRIES:
            locale = decode_locale(jar.read(entry))
            counts["locale"] += 1
            locale_json = {
                "code": locale.code,
                "display_name": locale.display_name,
                "offsets": locale.offsets,
                "strings": locale.strings,
            }
            _write_json(locales_dir / f"{locale.code}.json", locale_json)
            locale_manifest.append({
                "entry": entry,
                "code": locale.code,
                "display_name": locale.display_name,
                "string_count": len(locale.strings),
                "sha256": sha256_bytes(jar.read(entry)),
            })

        decoded43 = decode_resource_43(read_resource(jar, 43))
        decoded44 = decode_resource_44(read_resource(jar, 44))
        _write_json(decoded_dir / "resource_43.json", asdict(decoded43))
        _write_json(decoded_dir / "resource_44.json", asdict(decoded44))

    fieldnames = [
        "resource_id", "container", "offset", "size", "type", "width", "height",
        "midi_format", "midi_tracks", "midi_division", "sha256",
    ]
    buffer = StringIO(newline="")
    writer = csv.DictWriter(buffer, fieldnames=fieldnames, lineterminator="\n")
    writer.writeheader()
    writer.writerows(inventory)
    (output_dir / "resource_inventory.csv").write_text(buffer.getvalue(), encoding="utf-8", newline="\n")

    manifest = {
        "jar_sha256": CANONICAL_SHA256,
        "midlet": info.manifest,
        "class_names": info.class_names,
        "resource_counts": counts,
        "resource_table": {
            "entry_count": 47,
            "first_payload_offset": inventory[0]["offset"],
            "external_45_length_marker": -121614,
            "r0_end_offset": 36645,
        },
        "locales": locale_manifest,
        "m3g_sha256": M3G_SHA256,
    }
    _write_json(output_dir / "manifest.json", manifest)

    if include_m3g:
        from .m3g_export import export_m3g_reference

        export_m3g_reference(jar_path, output_dir / "m3g")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Extract canonical Tower Bloxx 1.5.22 reference assets")
    parser.add_argument("jar", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--m3g", action="store_true", help="also generate decoded M3G reference assets")
    args = parser.parse_args(argv)
    extract_reference(args.jar, args.output, include_m3g=args.m3g)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
