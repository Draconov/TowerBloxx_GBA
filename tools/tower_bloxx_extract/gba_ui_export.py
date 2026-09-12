from __future__ import annotations

from io import BytesIO
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

from PIL import Image

from .binary44 import decode_resource_44
from .font44 import Font44
from .gba_assets import SpriteComposite, slice_sprite
from .gba_project_export import _write_indexed_bmp
from .localization import LOCALE_STRING_COUNT, LocalePack, decode_locale
from .resources import read_resource


EXTENDED_CHARACTERS = (
    "¡", "°", "¿", "È", "à", "á", "â", "ä", "ç", "è", "é", "ê",
    "ì", "í", "ñ", "ò", "ó", "ô", "ö", "ù", "ú", "û", "ü",
)

ASCII_CHARACTERS = tuple(chr(code) for code in range(32, 127))
FONT_CHARACTERS = ASCII_CHARACTERS + EXTENDED_CHARACTERS
LOCALE_ENTRIES = ("l0", "l1", "l2", "l3", "l4")
SOURCE_RESOURCES = {"font_atlas": 36, "font_metrics": 44, "tower_logo": 7, "sumea_logo": 10}


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")


def _bgr555(red: int, green: int, blue: int) -> int:
    return (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)


def _indexed_rgba(image: Image.Image) -> tuple[bytes, tuple[int, ...]]:
    rgba = image.convert("RGBA")
    palette: list[int] = [0]
    color_to_index: dict[int, int] = {}
    indices = bytearray(rgba.width * rgba.height)
    for index, (red, green, blue, alpha) in enumerate(rgba.get_flattened_data()):
        if alpha == 0:
            continue
        if alpha != 255:
            raise ValueError("GBA UI export requires binary alpha")
        color = _bgr555(red, green, blue)
        palette_index = color_to_index.get(color)
        if palette_index is None:
            palette_index = len(palette)
            if palette_index >= 16:
                raise ValueError("4bpp UI asset exceeds 15 opaque colors")
            color_to_index[color] = palette_index
            palette.append(color)
        indices[index] = palette_index
    return bytes(indices), tuple(palette)


def _cpp_string(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "\\r")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def _utf8_literal(character: str) -> str:
    return _cpp_string(character)




def _normalize_display_text(value: str) -> str:
    return value.replace("\\n", "\n")


def _wrap_text(font: Font44, value: str, max_width: int = 220) -> tuple[str, ...]:
    lines: list[str] = []
    for paragraph in _normalize_display_text(value).split("\n"):
        if not paragraph:
            if lines and lines[-1] != "":
                lines.append("")
            continue
        words = paragraph.split(" ")
        current = ""
        for word in words:
            candidate = word if not current else f"{current} {word}"
            if font.text_width(candidate) <= max_width:
                current = candidate
                continue
            if current:
                lines.append(current)
                current = ""
            if font.text_width(word) <= max_width:
                current = word
                continue
            # Preserve content even for an unusually long token such as a URL.
            chunk = ""
            for character in word:
                candidate_chunk = chunk + character
                if chunk and font.text_width(candidate_chunk) > max_width:
                    lines.append(chunk)
                    chunk = character
                else:
                    chunk = candidate_chunk
            current = chunk
        if current:
            lines.append(current)
    while lines and lines[-1] == "":
        lines.pop()
    return tuple(lines or ("",))

def _font_header(font: Font44) -> str:
    widths = [font.character_width(character) for character in FONT_CHARACTERS]
    lines = [
        "#ifndef TB_GENERATED_TOWER_FONT_H",
        "#define TB_GENERATED_TOWER_FONT_H",
        "",
        "#include <cstdint>",
        '#include "bn_span.h"',
        '#include "bn_sprite_font.h"',
        '#include "bn_utf8_characters_map.h"',
        '#include "bn_sprite_items_tower_font.h"',
        "",
        "namespace tb::generated",
        "{",
        f"inline constexpr int space_between_characters = {font.space_between_characters};",
        f"inline constexpr int line_height = {font.line_height};",
        "",
        "inline constexpr bn::utf8_character tower_font_utf8_characters[] = {",
    ]
    lines.extend(f"    {_utf8_literal(character)}," for character in EXTENDED_CHARACTERS)
    lines.extend([
        "};",
        "",
        "inline constexpr int8_t tower_font_character_widths[] = {",
    ])
    lines.extend(f"    {width}," for width in widths)
    lines.extend([
        "};",
        "",
        "inline constexpr bn::span<const bn::utf8_character> tower_font_utf8_characters_span(tower_font_utf8_characters);",
        "inline constexpr auto tower_font_utf8_characters_map =",
        "        bn::utf8_characters_map<tower_font_utf8_characters_span>();",
        "",
        "inline constexpr bn::sprite_font tower_font(",
        "        bn::sprite_items::tower_font,",
        "        tower_font_utf8_characters_map.reference(),",
        "        tower_font_character_widths);",
        "}",
        "",
        "#endif",
        "",
    ])
    return "\n".join(lines)


def _localization_header(
    locales: tuple[LocalePack, ...],
    adapted: dict[str, dict[str, str]],
    wrapped: dict[str, dict[str, tuple[str, ...]]],
) -> str:
    lines = [
        "#ifndef TB_GENERATED_TOWER_LOCALIZATION_H",
        "#define TB_GENERATED_TOWER_LOCALIZATION_H",
        "",
        "namespace tb::generated",
        "{",
        f"inline constexpr int locale_count = {len(locales)};",
        f"inline constexpr int strings_per_locale = {LOCALE_STRING_COUNT};",
        "",
        "inline constexpr const char* locale_codes[locale_count] = {",
    ]
    lines.extend(f"    {_cpp_string(locale.code)}," for locale in locales)
    lines.extend([
        "};",
        "inline constexpr const char* locale_names[locale_count] = {",
    ])
    lines.extend(f"    {_cpp_string(locale.display_name)}," for locale in locales)
    lines.extend([
        "};",
        "",
        "inline constexpr const char* localized_strings[locale_count][strings_per_locale] = {",
    ])
    for locale in locales:
        lines.append("    {")
        lines.extend(f"        {_cpp_string(value)}," for value in locale.strings)
        lines.append("    },")
    lines.extend([
        "};",
        "",
        "inline constexpr const char* quick_game_instructions[locale_count] = {",
    ])
    lines.extend(f"    {_cpp_string(adapted[locale.code]['quick'])}," for locale in locales)
    lines.extend([
        "};",
        "inline constexpr const char* build_city_instructions[locale_count] = {",
    ])
    lines.extend(f"    {_cpp_string(adapted[locale.code]['city'])}," for locale in locales)
    lines.extend([
        "};",
        "",
        "inline constexpr int instruction_lines_per_page = 8;",
        "",
    ])

    for key, cpp_name in (("quick", "quick_game_instruction_lines"), ("city", "build_city_instruction_lines"), ("about", "about_lines")):
        max_lines = max(len(wrapped[locale.code][key]) for locale in locales)
        lines.append(f"inline constexpr int {cpp_name}_max_lines = {max_lines};")
        lines.append(f"inline constexpr int {cpp_name}_line_counts[locale_count] = {{")
        lines.extend(f"    {len(wrapped[locale.code][key])}," for locale in locales)
        lines.append("};")
        lines.append(f"inline constexpr const char* {cpp_name}[locale_count][{max_lines}] = {{")
        for locale in locales:
            values = wrapped[locale.code][key]
            lines.append("    {")
            lines.extend(f"        {_cpp_string(value)}," for value in values)
            lines.extend('        "",' for _ in range(max_lines - len(values)))
            lines.append("    },")
        lines.extend(["};", ""])

    lines.extend([
        "}",
        "",
        "#endif",
        "",
    ])
    return "\n".join(lines)


def _logo_header(records: list[dict[str, object]]) -> str:
    lines = [
        "#ifndef TB_GENERATED_TOWER_UI_ASSETS_H",
        "#define TB_GENERATED_TOWER_UI_ASSETS_H",
        "",
        "#include <cstdint>",
    ]
    for record in records:
        for part in record["parts"]:  # type: ignore[index]
            lines.append(f'#include "bn_sprite_items_{part["asset"]}.h"')  # type: ignore[index]
    lines.extend([
        "",
        "namespace tb::generated",
        "{",
        "struct UiSpritePartAsset",
        "{",
        "    const bn::sprite_item* item;",
        "    int16_t x;",
        "    int16_t y;",
        "};",
        "",
        "struct UiCompositeAsset",
        "{",
        "    const UiSpritePartAsset* parts;",
        "    int16_t part_count;",
        "};",
        "",
    ])
    for record in records:
        name = str(record["name"])
        lines.append(f"inline const UiSpritePartAsset {name}_parts[] = {{")
        for part in record["parts"]:  # type: ignore[index]
            lines.append(
                f"    {{ &bn::sprite_items::{part['asset']}, {part['screen_x']}, {part['screen_y']} }},"  # type: ignore[index]
            )
        lines.extend([
            "};",
            f"inline const UiCompositeAsset {name} = {{ {name}_parts, {len(record['parts'])} }};",  # type: ignore[arg-type]
            "",
        ])
    lines.extend(["}", "", "#endif", ""])
    return "\n".join(lines)


def _adapt_instructions(locale: LocalePack) -> dict[str, str]:
    quick = locale.strings[2]
    city = locale.strings[3]

    # The only content changes are control substitutions.  We replace the
    # literal Nokia keypad tokens, leaving all other localized prose intact.
    press5_phrases = {
        "en-EN": ("Press 5", "Press A"),
        "fr-FR": ("Appuie sur 5", "Appuie sur A"),
        "it-IT": ("Premi 5", "Premi A"),
        "de-DE": ("Drücke 5", "Drücke A"),
        "es-ES": ("Pulsa 5", "Pulsa A"),
    }
    if locale.code in press5_phrases:
        old, new = press5_phrases[locale.code]
        quick = quick.replace(old, new)

    city_place_phrases = {
        "en-EN": ("place the tower with 5", "place the tower with A"),
        "fr-FR": ("la placer avec 5", "la placer avec A"),
        "it-IT": ("collocare il palazzo premendo 5", "collocare il palazzo premendo A"),
        "de-DE": ("platzierst das Haus mit 5", "platzierst das Haus mit A"),
        "es-ES": ("coloca el rascacielos con 5", "coloca el rascacielos con A"),
    }
    if locale.code in city_place_phrases:
        old, new = city_place_phrases[locale.code]
        city = city.replace(old, new)

    movement_phrases = {
        "en-EN": ("Move the tower with 4, 6, 2 and 8.", "Move the tower with the D-pad."),
        "fr-FR": ("Déplace-la avec 4, 6, 2 et 8.", "Déplace-la avec le D-pad."),
        "it-IT": ("Sposta il palazzo usando 4, 6, 2, 8.", "Sposta il palazzo usando il D-pad."),
        "de-DE": ("Bewege das Haus mit 4, 6, 2, 8.", "Bewege das Haus mit dem D-pad."),
        "es-ES": ("Mueve el edificio con 4, 6, 2, 8.", "Mueve el edificio con el D-pad."),
    }
    if locale.code in movement_phrases:
        old, new = movement_phrases[locale.code]
        city = city.replace(old, new)

    # Preserve the proven 20-milestone rule in the playable City instructions.
    # The original long help text omits the count; its tutorial string (41)
    # explicitly states it.  Append that localized sentence rather than inventing prose.
    milestone = locale.strings[41]
    if milestone and milestone not in city:
        city = f"{city}\\n\\n{milestone}"

    return {"quick": quick, "city": city}


def _export_composite(
    image: Image.Image,
    name: str,
    graphics_dir: Path,
) -> tuple[SpriteComposite, dict[str, object]]:
    composite = slice_sprite(image, mesh_id=0)
    parts: list[dict[str, object]] = []
    for part_index, part in enumerate(composite.parts):
        asset = f"{name}_p{part_index}"
        bmp_path = graphics_dir / f"{asset}.bmp"
        json_path = graphics_dir / f"{asset}.json"
        _write_indexed_bmp(
            bmp_path,
            part.width,
            part.height,
            part.indices,
            composite.palette_bgr555,
            composite.bpp,
        )
        _write_json(json_path, {"bpp_mode": f"bpp_{composite.bpp}", "type": "sprite"})
        parts.append({
            "asset": asset,
            "source_x": part.source_x,
            "source_y": part.source_y,
            "width": part.width,
            "height": part.height,
            "screen_x": part.source_x + part.width // 2 - composite.canvas_width // 2,
            "screen_y": part.source_y + part.height // 2 - composite.canvas_height // 2,
        })
    return composite, {
        "name": name,
        "width": composite.canvas_width,
        "height": composite.canvas_height,
        "bpp": composite.bpp,
        "palette_entries": len(composite.palette_bgr555),
        "parts": parts,
    }


def export_gba_ui_assets(jar_path: Path, project_dir: Path) -> dict[str, object]:
    jar_path = Path(jar_path)
    project_dir = Path(project_dir)
    graphics_dir = project_dir / "gba" / "graphics" / "ui"
    include_dir = project_dir / "gba" / "include" / "generated"
    reference_dir = project_dir / "gba" / "reference"

    if graphics_dir.exists():
        shutil.rmtree(graphics_dir)
    graphics_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)
    reference_dir.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(jar_path) as jar:
        font_atlas = Image.open(BytesIO(read_resource(jar, 36))).convert("RGBA")
        font_metrics = decode_resource_44(read_resource(jar, 44))
        font = Font44(font_atlas, font_metrics)
        tower_logo = Image.open(BytesIO(read_resource(jar, 7))).convert("RGBA")
        sumea_logo = Image.open(BytesIO(read_resource(jar, 10))).convert("RGBA")
        locales = tuple(decode_locale(jar.read(entry)) for entry in LOCALE_ENTRIES)

    non_ascii = tuple(sorted({
        character
        for locale in locales
        for value in (locale.display_name, *locale.strings)
        for character in value
        if ord(character) >= 127
    }, key=ord))
    if non_ascii != EXTENDED_CHARACTERS:
        raise ValueError(f"unexpected localized character set: {non_ascii!r}")

    font_sheet = font.render_font_sheet(FONT_CHARACTERS)
    font_indices, font_palette = _indexed_rgba(font_sheet)
    _write_indexed_bmp(
        graphics_dir / "tower_font.bmp",
        font_sheet.width,
        font_sheet.height,
        font_indices,
        font_palette,
        4,
    )
    _write_json(
        graphics_dir / "tower_font.json",
        {"bpp_mode": "bpp_4", "height": 16, "type": "sprite", "width": 8},
    )

    logo_records: list[dict[str, object]] = []
    _tower_composite, tower_record = _export_composite(tower_logo, "tower_bloxx_logo", graphics_dir)
    _sumea_composite, sumea_record = _export_composite(sumea_logo, "sumea_logo", graphics_dir)
    logo_records.extend((tower_record, sumea_record))

    adapted = {locale.code: _adapt_instructions(locale) for locale in locales}
    wrapped: dict[str, dict[str, tuple[str, ...]]] = {}
    for locale in locales:
        about = _normalize_display_text(locale.strings[1])
        about = "\n".join(line for line in about.split("\n") if line.strip() != "img=10")
        wrapped[locale.code] = {
            "quick": _wrap_text(font, adapted[locale.code]["quick"]),
            "city": _wrap_text(font, adapted[locale.code]["city"]),
            "about": _wrap_text(font, about),
        }

    font_header = include_dir / "tower_font.h"
    localization_header = include_dir / "tower_localization.h"
    ui_assets_header = include_dir / "tower_ui_assets.h"
    font_header.write_text(_font_header(font), encoding="utf-8", newline="\n")
    localization_header.write_text(_localization_header(locales, adapted, wrapped), encoding="utf-8", newline="\n")
    ui_assets_header.write_text(_logo_header(logo_records), encoding="utf-8", newline="\n")

    tracked_files = sorted([
        *graphics_dir.glob("*.bmp"),
        *graphics_dir.glob("*.json"),
        font_header,
        localization_header,
        ui_assets_header,
    ])
    files = [
        {
            "path": path.relative_to(project_dir).as_posix(),
            "sha256": _sha256(path),
            "size": path.stat().st_size,
        }
        for path in tracked_files
    ]
    tree_digest = hashlib.sha256()
    for record in files:
        tree_digest.update(record["path"].encode())
        tree_digest.update(b"\0")
        tree_digest.update(record["sha256"].encode())
        tree_digest.update(b"\n")

    manifest: dict[str, object] = {
        "source_resources": SOURCE_RESOURCES,
        "locale_count": len(locales),
        "strings_per_locale": LOCALE_STRING_COUNT,
        "locale_codes": [locale.code for locale in locales],
        "locale_names": [locale.display_name for locale in locales],
        "utf8_characters": list(EXTENDED_CHARACTERS),
        "font_character_count": len(FONT_CHARACTERS),
        "font_space_between_characters": font.space_between_characters,
        "font_line_height": font.line_height,
        "adapted_instructions": adapted,
        "wrapped_line_counts": {
            locale.code: {key: len(value) for key, value in wrapped[locale.code].items()}
            for locale in locales
        },
        "logos": logo_records,
        "files": files,
        "tree_hash": tree_digest.hexdigest(),
    }
    _write_json(reference_dir / "ui_assets_manifest.json", manifest)
    return manifest
