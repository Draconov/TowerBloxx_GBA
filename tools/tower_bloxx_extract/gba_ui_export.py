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
# Butano stores space only in the variable-width table; tile set 0 is '!'.
FONT_GRAPHICS_CHARACTERS = ASCII_CHARACTERS[1:] + EXTENDED_CHARACTERS
LOCALE_ENTRIES = ("l0", "l1", "l2", "l3", "l4")
SOURCE_RESOURCES = {
    "font_atlas": 36,
    "font_metrics": 44,
    "tower_logo": 7,
    "sumea_logo": 10,
    "menu_icons": [2, 3, 4, 5, 6],
    "menu_workers": [11, 12],
    "construction_target_badges": 13,
    "hud_white_digits": 14,
    "hud_brown_digits": 15,
    "hud_red_digits": 16,
    "hud_status_graphic": 17,
    "hud_state_indicators": 18,
    "quick_counter_frame": 19,
    "city_continue_arrow": 8,
    "city_hanging_ui": 20,
    "city_status_icons": 21,
    "city_panels": 22,
    "city_action_icon": 23,
    "city_buildings": [24, 25, 26, 27],
    "city_lot": 28,
    "city_effects": 29,
    "crane_hook_frames": 30,
    "accuracy_stars": 35,
}


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
        # GBA OBJ palette index 0 is transparent. Preserve opaque near-black
        # source pixels by lifting an otherwise-zero BGR555 color to the
        # darkest representable non-zero neutral gray.
        if color == 0:
            color = 0x0421
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
        '#include "bn_sprite_items_tower_font_selected.h"',
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
        "        tower_font_character_widths,",
        "        space_between_characters);",
        "",
        "inline constexpr bn::sprite_font selected_tower_font(",
        "        bn::sprite_items::tower_font_selected,",
        "        tower_font_utf8_characters_map.reference(),",
        "        tower_font_character_widths,",
        "        space_between_characters);",
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
    city_modal_wrapped: dict[str, tuple[tuple[str, ...], ...]],
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

    for key, cpp_name in (
        ("quick", "quick_game_instruction_lines"),
        ("city", "build_city_instruction_lines"),
        ("about", "about_lines"),
        ("reset_city", "reset_city_confirmation_lines"),
    ):
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

    city_modal_max_lines = max(
        len(message_lines)
        for locale in locales
        for message_lines in city_modal_wrapped[locale.code]
    )
    lines.extend([
        "inline constexpr int city_modal_min_string_index = 36;",
        "inline constexpr int city_modal_max_string_index = 60;",
        "inline constexpr int city_modal_string_count = 25;",
        f"inline constexpr int city_modal_max_lines = {city_modal_max_lines};",
        "inline constexpr int city_modal_line_counts[locale_count][city_modal_string_count] = {",
    ])
    for locale in locales:
        lines.append("    {")
        lines.extend(f"        {len(message_lines)}," for message_lines in city_modal_wrapped[locale.code])
        lines.append("    },")
    lines.extend([
        "};",
        f"inline constexpr const char* city_modal_lines[locale_count][city_modal_string_count][{city_modal_max_lines}] = {{",
    ])
    for locale in locales:
        lines.append("    {")
        for message_lines in city_modal_wrapped[locale.code]:
            lines.append("        {")
            lines.extend(f"            {_cpp_string(value)}," for value in message_lines)
            lines.extend('            "",' for _ in range(city_modal_max_lines - len(message_lines)))
            lines.append("        },")
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




def _share_bpp4_palette(graphics_dir: Path, asset_names: tuple[str, ...]) -> int:
    """Reindex compatible sprite BMPs onto one exact 16-color OBJ palette.

    GBA OBJ 4bpp sprites select one of only 16 hardware palette banks.  Butano
    can reuse a bank only when the palette data is identical, so independently
    cropped UI pieces with overlapping colors can otherwise exhaust all banks.
    This helper keeps every pixel's BGR555 color unchanged while giving the
    selected assets one canonical palette.
    """
    if not asset_names:
        raise ValueError("shared palette group must not be empty")

    canonical_opaque: list[int] = []
    seen: set[int] = set()
    images: list[tuple[Path, Image.Image, tuple[int, ...]]] = []

    for asset_name in asset_names:
        bmp_path = graphics_dir / f"{asset_name}.bmp"
        json_path = graphics_dir / f"{asset_name}.json"
        metadata = json.loads(json_path.read_text(encoding="utf-8"))
        if metadata.get("bpp_mode") != "bpp_4":
            raise ValueError(f"{asset_name} is not a 4bpp sprite")

        image = Image.open(bmp_path)
        if image.mode != "P":
            raise ValueError(f"{asset_name} is not indexed")
        raw_palette = image.getpalette()
        if raw_palette is None:
            raise ValueError(f"{asset_name} has no palette")

        old_palette: list[int] = []
        for index in range(16):
            base = index * 3
            red, green, blue = raw_palette[base : base + 3]
            old_palette.append(_bgr555(red, green, blue))

        used_indices = set(image.get_flattened_data())
        for index in range(1, 16):
            if index not in used_indices:
                continue
            color = old_palette[index]
            if color not in seen:
                seen.add(color)
                canonical_opaque.append(color)

        images.append((bmp_path, image.copy(), tuple(old_palette)))
        image.close()

    if len(canonical_opaque) > 15:
        raise ValueError(
            f"shared 4bpp palette needs {len(canonical_opaque)} opaque colors: {asset_names!r}"
        )

    canonical_palette = (0, *canonical_opaque)
    color_to_index = {color: index + 1 for index, color in enumerate(canonical_opaque)}

    for bmp_path, image, old_palette in images:
        remapped = bytearray(image.width * image.height)
        for offset, old_index in enumerate(image.get_flattened_data()):
            if old_index:
                remapped[offset] = color_to_index[old_palette[old_index]]
        _write_indexed_bmp(
            bmp_path, image.width, image.height, bytes(remapped), canonical_palette, 4
        )

    return len(canonical_palette)



def _read_indexed_asset(graphics_dir: Path, asset_name: str) -> tuple[Image.Image, tuple[int, ...], int]:
    bmp_path = graphics_dir / f"{asset_name}.bmp"
    json_path = graphics_dir / f"{asset_name}.json"
    metadata = json.loads(json_path.read_text(encoding="utf-8"))
    mode = metadata.get("bpp_mode")
    if mode not in ("bpp_4", "bpp_8"):
        raise ValueError(f"{asset_name} has unsupported bpp mode: {mode!r}")
    bpp = 4 if mode == "bpp_4" else 8
    image = Image.open(bmp_path)
    if image.mode != "P":
        image.close()
        raise ValueError(f"{asset_name} is not indexed")
    raw_palette = image.getpalette()
    if raw_palette is None:
        image.close()
        raise ValueError(f"{asset_name} has no palette")
    palette_entries = 16 if bpp == 4 else 256
    colors: list[int] = []
    for index in range(palette_entries):
        base = index * 3
        red, green, blue = raw_palette[base : base + 3]
        colors.append(_bgr555(red, green, blue))
    return image, tuple(colors), bpp


def _share_bpp8_palette(
    graphics_dir: Path,
    records: tuple[dict[str, object], ...] | list[dict[str, object]],
) -> int:
    """Put a related sprite group on one bounded partial BPP8 OBJ palette."""
    asset_names = _record_asset_names(records)
    if not asset_names:
        raise ValueError("shared BPP8 palette group must not be empty")

    canonical_opaque: list[int] = []
    seen: set[int] = set()
    images: list[tuple[str, Image.Image, tuple[int, ...]]] = []
    for asset_name in asset_names:
        image, old_palette, _bpp = _read_indexed_asset(graphics_dir, asset_name)
        used = set(image.get_flattened_data())
        for index in sorted(used):
            if index == 0:
                continue
            color = old_palette[index]
            if color not in seen:
                seen.add(color)
                canonical_opaque.append(color)
        images.append((asset_name, image.copy(), old_palette))
        image.close()

    if len(canonical_opaque) > 255:
        raise ValueError(f"shared BPP8 palette needs {len(canonical_opaque)} opaque colors")
    exact_entries = 1 + len(canonical_opaque)
    colors_count = ((exact_entries + 15) // 16) * 16
    canonical_palette = (0, *canonical_opaque)
    color_to_index = {color: index + 1 for index, color in enumerate(canonical_opaque)}

    for asset_name, image, old_palette in images:
        remapped = bytearray(image.width * image.height)
        for offset, old_index in enumerate(image.get_flattened_data()):
            if old_index:
                remapped[offset] = color_to_index[old_palette[old_index]]
        _write_indexed_bmp(
            graphics_dir / f"{asset_name}.bmp",
            image.width,
            image.height,
            bytes(remapped),
            canonical_palette,
            8,
        )
        _write_json(
            graphics_dir / f"{asset_name}.json",
            {"bpp_mode": "bpp_8", "colors_count": colors_count, "type": "sprite"},
        )

    for record in records:
        record["bpp"] = 8
        record["palette_entries"] = colors_count
    return colors_count


def _split_worker_palettes(
    graphics_dir: Path,
    records: tuple[dict[str, object], ...] | list[dict[str, object]],
) -> tuple[int, int]:
    """Split all menu workers into two shared exact-color BPP4 layers.

    Workers use 29 opaque source colors in total, so a single 4bpp bank cannot
    represent them without recoloring.  Each source pixel is assigned to one
    of two shared palettes; the two sprites overlap at the same coordinates.
    """
    original_assets: list[tuple[dict[str, object], dict[str, object], Image.Image, tuple[int, ...]]] = []
    opaque_colors: list[int] = []
    seen: set[int] = set()
    for record in records:
        if len(record["parts"]) != 1:  # type: ignore[arg-type]
            raise ValueError(f"worker {record['name']} must have one source part before splitting")
        part = record["parts"][0]  # type: ignore[index]
        asset_name = str(part["asset"])
        image, old_palette, _bpp = _read_indexed_asset(graphics_dir, asset_name)
        used = set(image.get_flattened_data())
        for index in sorted(used):
            if index == 0:
                continue
            color = old_palette[index]
            if color not in seen:
                seen.add(color)
                opaque_colors.append(color)
        original_assets.append((record, part, image.copy(), old_palette))
        image.close()

    if len(opaque_colors) > 30:
        raise ValueError(f"workers need more than two 4bpp palettes: {len(opaque_colors)} opaque colors")
    group0 = opaque_colors[:15]
    group1 = opaque_colors[15:]
    palette0 = (0, *group0)
    palette1 = (0, *group1)
    map0 = {color: index + 1 for index, color in enumerate(group0)}
    map1 = {color: index + 1 for index, color in enumerate(group1)}

    for record, part, image, old_palette in original_assets:
        asset0 = str(part["asset"])
        if not asset0.endswith("_p0"):
            raise ValueError(f"unexpected worker asset name: {asset0}")
        asset1 = asset0[:-1] + "1"
        layer0 = bytearray(image.width * image.height)
        layer1 = bytearray(image.width * image.height)
        for offset, old_index in enumerate(image.get_flattened_data()):
            if not old_index:
                continue
            color = old_palette[old_index]
            if color in map0:
                layer0[offset] = map0[color]
            else:
                layer1[offset] = map1[color]

        _write_indexed_bmp(
            graphics_dir / f"{asset0}.bmp", image.width, image.height, bytes(layer0), palette0, 4
        )
        _write_indexed_bmp(
            graphics_dir / f"{asset1}.bmp", image.width, image.height, bytes(layer1), palette1, 4
        )
        metadata = {"bpp_mode": "bpp_4", "type": "sprite"}
        _write_json(graphics_dir / f"{asset0}.json", metadata)
        _write_json(graphics_dir / f"{asset1}.json", metadata)
        part1 = dict(part)
        part1["asset"] = asset1
        record["parts"] = [part, part1]
        record["bpp"] = 4
        record["palette_entries"] = 16

    return 1 + len(group0), 1 + len(group1)


def _record_asset_names(records: tuple[dict[str, object], ...] | list[dict[str, object]]) -> tuple[str, ...]:
    return tuple(
        str(part["asset"])
        for record in records
        for part in record["parts"]  # type: ignore[index]
    )


def _set_shared_palette_entries(records: tuple[dict[str, object], ...] | list[dict[str, object]], entries: int) -> None:
    for record in records:
        record["palette_entries"] = entries


def _export_strip_frames(
    image: Image.Image,
    frame_count: int,
    name_prefix: str,
    graphics_dir: Path,
) -> list[dict[str, object]]:
    if image.width % frame_count:
        raise ValueError(f"{name_prefix} width is not divisible by {frame_count}")
    frame_width = image.width // frame_count
    records: list[dict[str, object]] = []
    for frame in range(frame_count):
        left = frame * frame_width
        cropped = image.crop((left, 0, left + frame_width, image.height))
        _composite, record = _export_composite(cropped, f"{name_prefix}_f{frame}", graphics_dir)
        records.append(record)
    return records

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
        menu_icons = tuple(
            Image.open(BytesIO(read_resource(jar, resource_id))).convert("RGBA")
            for resource_id in SOURCE_RESOURCES["menu_icons"]
        )
        menu_worker_strips = tuple(
            Image.open(BytesIO(read_resource(jar, resource_id))).convert("RGBA")
            for resource_id in SOURCE_RESOURCES["menu_workers"]
        )
        construction_target_badges = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["construction_target_badges"])))).convert("RGBA")
        hud_white_digits = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["hud_white_digits"])))).convert("RGBA")
        hud_brown_digits = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["hud_brown_digits"])))).convert("RGBA")
        hud_red_digits = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["hud_red_digits"])))).convert("RGBA")
        hud_status_graphic = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["hud_status_graphic"])))).convert("RGBA")
        hud_state_indicators = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["hud_state_indicators"])))).convert("RGBA")
        quick_counter_frame = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["quick_counter_frame"])))).convert("RGBA")
        city_continue_arrow = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_continue_arrow"])))).convert("RGBA")
        city_hanging_ui = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_hanging_ui"])))).convert("RGBA")
        city_status_icons = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_status_icons"])))).convert("RGBA")
        city_panels = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_panels"])))).convert("RGBA")
        city_action_icon = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_action_icon"])))).convert("RGBA")
        city_building_strips = tuple(
            Image.open(BytesIO(read_resource(jar, resource_id))).convert("RGBA")
            for resource_id in SOURCE_RESOURCES["city_buildings"]
        )
        city_lot_strip = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_lot"])))).convert("RGBA")
        city_effects = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["city_effects"])))).convert("RGBA")
        crane_hook_frames = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["crane_hook_frames"])))).convert("RGBA")
        accuracy_stars = Image.open(BytesIO(read_resource(jar, int(SOURCE_RESOURCES["accuracy_stars"])))).convert("RGBA")
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

    font_sheet = font.render_font_sheet(FONT_GRAPHICS_CHARACTERS)
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

    # k.b(Graphics) uses palette entry b[7] (0xA50003) for selected text in
    # the default branch visible in the captured v1.5.22 JAR. Preserve the
    # exact source glyph alpha/mask and change only the opaque glyph color.
    selected_font_sheet = Image.new("RGBA", font_sheet.size, (165, 0, 3, 0))
    selected_font_sheet.putalpha(font_sheet.getchannel("A"))
    selected_font_indices, selected_font_palette = _indexed_rgba(selected_font_sheet)
    _write_indexed_bmp(
        graphics_dir / "tower_font_selected.bmp",
        selected_font_sheet.width,
        selected_font_sheet.height,
        selected_font_indices,
        selected_font_palette,
        4,
    )
    _write_json(
        graphics_dir / "tower_font_selected.json",
        {"bpp_mode": "bpp_4", "height": 16, "type": "sprite", "width": 8},
    )

    asset_records: list[dict[str, object]] = []
    _tower_composite, tower_record = _export_composite(tower_logo, "tower_bloxx_logo", graphics_dir)
    _sumea_composite, sumea_record = _export_composite(sumea_logo, "sumea_logo", graphics_dir)

    # k.b(Graphics) palette entry b[6] is 0xFFDD46 in the default branch shown
    # by the reference JAR capture. Keep the source five-pixel side margins
    # scaled to the 240px GBA viewport (230px-wide band).
    menu_highlight_image = Image.new("RGBA", (230, 16), (255, 221, 70, 255))
    _menu_highlight_composite, menu_highlight_record = _export_composite(
        menu_highlight_image, "menu_highlight", graphics_dir
    )
    # m.a(Graphics, boolean) draws the city-level progress line with Java color
    # -130816 == 0xFFFE0100 at y=12. Export an 8px segment plus exact 1..7px tails.
    city_progress_image = Image.new("RGBA", (8, 1), (254, 1, 0, 255))
    _city_progress_composite, city_progress_record = _export_composite(
        city_progress_image, "city_progress_segment", graphics_dir
    )
    city_progress_tail_records: list[dict[str, object]] = []
    for width in range(1, 8):
        tail = Image.new("RGBA", (width, 1), (254, 1, 0, 255))
        _tail_composite, tail_record = _export_composite(
            tail, f"city_progress_tail_f{width}", graphics_dir
        )
        city_progress_tail_records.append(tail_record)

    # m.a(Graphics, boolean) colors valid placement lots with a continuously
    # interpolated 2px ring while preserving the 10x10 source lot center.
    # The runtime recolors palette index 1 every frame, so export the mask as
    # white here and keep transparency in the center.
    city_valid_lot_ring_image = Image.new("RGBA", (14, 14), (255, 247, 255, 255))
    city_valid_lot_ring_image.paste((0, 0, 0, 0), (2, 2, 12, 12))
    _valid_lot_composite, city_valid_lot_ring_record = _export_composite(
        city_valid_lot_ring_image, "city_valid_lot_ring", graphics_dir
    )

    # m.a(Graphics, boolean) flashes the selected browser slot with Java color
    # -89856 == 0xFFFEA100 for 250ms of each 500ms selector cycle. The static
    # gray slot remains in city_bg during the other half of the cycle.
    city_selector_active_slot_image = Image.new("RGBA", (15, 15), (254, 161, 0, 255))
    _selector_slot_composite, city_selector_active_slot_record = _export_composite(
        city_selector_active_slot_image, "city_selector_active_slot", graphics_dir
    )

    # The comparison boxes at x=189/213 are Java2D rectangles, not resource
    # 22.  Export their active state as a tiny reusable overlay.
    city_comparison_panel_active_image = Image.new("RGBA", (24, 9), (226, 226, 226, 255))
    city_comparison_panel_active_image.paste((13, 12, 12, 255), (1, 1, 23, 8))
    _comparison_panel_composite, city_comparison_panel_active_record = _export_composite(
        city_comparison_panel_active_image, "city_comparison_panel_active", graphics_dir
    )

    city_type_badge_records: list[dict[str, object]] = []
    city_type_bright = ((67, 113, 215), (225, 26, 8), (17, 175, 12), (218, 159, 0))
    city_type_dark = ((11, 45, 142), (132, 17, 17), (4, 92, 0), (80, 56, 0))
    for building_type in range(1, 5):
        badge = Image.new("RGBA", (4, 7), (*city_type_dark[building_type - 1], 255))
        badge.paste((*city_type_bright[building_type - 1], 255), (0, 0, 4, 6))
        _badge_composite, badge_record = _export_composite(
            badge, f"city_type_badge_{building_type}", graphics_dir
        )
        city_type_badge_records.append(badge_record)

    asset_records.extend((
        tower_record, sumea_record, menu_highlight_record, city_progress_record,
        city_valid_lot_ring_record, city_selector_active_slot_record, city_comparison_panel_active_record,
    ))
    asset_records.extend(city_progress_tail_records)
    asset_records.extend(city_type_badge_records)

    menu_icon_names = (
        "menu_continue_icon",
        "menu_build_city_icon",
        "menu_quick_game_icon",
        "menu_settings_icon",
        "menu_exit_icon",
    )
    menu_icon_records: list[dict[str, object]] = []
    for image, name in zip(menu_icons, menu_icon_names, strict=True):
        _composite, record = _export_composite(image, name, graphics_dir)
        menu_icon_records.append(record)
        asset_records.append(record)
    menu_worker_blue_records = _export_strip_frames(menu_worker_strips[0], 10, "menu_worker_blue", graphics_dir)
    menu_worker_red_records = _export_strip_frames(menu_worker_strips[1], 10, "menu_worker_red", graphics_dir)
    asset_records.extend(menu_worker_blue_records)
    asset_records.extend(menu_worker_red_records)

    construction_target_badge_records = _export_strip_frames(
        construction_target_badges, 5, "construction_target_badge", graphics_dir
    )
    hud_white_digit_records = _export_strip_frames(hud_white_digits, 14, "hud_white_digit", graphics_dir)
    hud_brown_digit_records = _export_strip_frames(hud_brown_digits, 12, "hud_brown_digit", graphics_dir)
    hud_red_digit_records = _export_strip_frames(hud_red_digits, 11, "hud_red_digit", graphics_dir)
    _status_composite, hud_status_graphic_record = _export_composite(
        hud_status_graphic, "hud_status_graphic", graphics_dir
    )
    # House.i(Graphics) clips the top 6x9 cell of resource 17 for the
    # lower-right population marker in Quick Game. Export that proven clip as
    # a first-class composite instead of forcing the GBA renderer to emulate
    # MIDP drawImage+clip source offsets at runtime.
    _population_icon_composite, hud_population_icon_record = _export_composite(
        hud_status_graphic.crop((0, 0, 6, 9)), "hud_population_icon", graphics_dir
    )

    # House.i(Graphics), construction branch B==3, clips resource 17 into
    # a 5px-wide per-floor fill plus a 7x2 bottom cap.  The fill source row
    # shifts by two pixels for L=1..4.  Rails and empty rows are Java2D
    # primitives, exported here as deterministic composites so the GBA scene
    # can reproduce the exact recovered 240x160 geometry.
    construction_meter_fill_records: list[dict[str, object]] = []
    for building_type in range(1, 5):
        source_top = 9 + 2 * (building_type - 1)
        _fill_composite, fill_record = _export_composite(
            hud_status_graphic.crop((0, source_top, 5, source_top + 2)),
            f"construction_meter_fill_{building_type}",
            graphics_dir,
        )
        construction_meter_fill_records.append(fill_record)

    _meter_base_composite, construction_meter_base_record = _export_composite(
        hud_status_graphic.crop((0, 17, 7, 19)), "construction_meter_base", graphics_dir
    )
    construction_meter_empty_image = Image.new("RGBA", (5, 2), (35, 30, 20, 255))
    _meter_empty_composite, construction_meter_empty_record = _export_composite(
        construction_meter_empty_image, "construction_meter_empty", graphics_dir
    )

    construction_meter_rails_records: list[dict[str, object]] = []
    for target_height in (10, 20, 30, 40):
        rail_height = target_height * 2
        rails = Image.new("RGBA", (9, rail_height), (0, 0, 0, 0))
        for y in range(max(0, rail_height - 2)):
            rails.putpixel((0, y), (72, 28, 0, 255))
            rails.putpixel((1, y), (255, 255, 0, 255))
            rails.putpixel((7, y), (255, 255, 0, 255))
            rails.putpixel((8, y), (72, 28, 0, 255))
        _rails_composite, rails_record = _export_composite(
            rails, f"construction_meter_rails_{target_height}", graphics_dir
        )
        construction_meter_rails_records.append(rails_record)
    hud_state_indicator_records = _export_strip_frames(
        hud_state_indicators, 10, "hud_state_indicator", graphics_dir
    )
    _counter_composite, quick_counter_frame_record = _export_composite(
        quick_counter_frame, "quick_counter_frame", graphics_dir
    )
    _city_hanging_composite, city_hanging_ui_record = _export_composite(
        city_hanging_ui, "city_hanging_ui", graphics_dir
    )
    _city_continue_composite, city_continue_arrow_record = _export_composite(
        city_continue_arrow, "city_continue_arrow", graphics_dir
    )

    city_edge_clip_records: list[dict[str, object]] = []
    for left, name in ((0, "city_edge_top_left"), (3, "city_edge_top_right"),
                       (6, "city_edge_bottom_left"), (9, "city_edge_bottom_right")):
        _composite, record = _export_composite(city_hanging_ui.crop((left, 0, left + 3, 23)), name, graphics_dir)
        city_edge_clip_records.append(record)

    city_status_clip_records: list[dict[str, object]] = []
    for left, right, name in ((0, 9, "city_population_icon"), (9, 16, "city_status_placement"),
                              (16, 23, "city_status_browse"), (23, 30, "city_status_aux")):
        _composite, record = _export_composite(city_status_icons.crop((left, 0, right, 9)), name, graphics_dir)
        city_status_clip_records.append(record)

    city_panel_state_records: list[dict[str, object]] = []
    for frame in range(4):
        panel = Image.new("RGBA", (8, 11), (0, 0, 0, 0))
        left = frame * 8
        source = city_panels.crop((left, 0, min(left + 8, city_panels.width), city_panels.height))
        panel.paste(source, (0, 0), source)
        _composite, record = _export_composite(panel, f"city_status_panel_f{frame}", graphics_dir)
        city_panel_state_records.append(record)

    # Keep the previous generic slices for compatibility with older scene code while
    # Fix 11 migrates rendering to the recovered call-site clips above.
    city_status_icon_records = _export_strip_frames(city_status_icons, 5, "city_status_icon", graphics_dir)
    city_panel_records = _export_strip_frames(city_panels, 2, "city_panel", graphics_dir)
    _city_action_composite, city_action_icon_record = _export_composite(
        city_action_icon, "city_action_icon", graphics_dir
    )
    city_effect_records = _export_strip_frames(city_effects, 6, "city_effect", graphics_dir)
    crane_hook_frame_records = _export_strip_frames(crane_hook_frames, 5, "crane_hook_frame", graphics_dir)
    accuracy_star_records = _export_strip_frames(accuracy_stars, 3, "accuracy_star", graphics_dir)
    asset_records.extend(construction_target_badge_records)
    asset_records.extend(hud_white_digit_records)
    asset_records.extend(hud_brown_digit_records)
    asset_records.extend(hud_red_digit_records)
    asset_records.append(hud_status_graphic_record)
    asset_records.append(hud_population_icon_record)
    asset_records.extend(construction_meter_fill_records)
    asset_records.append(construction_meter_base_record)
    asset_records.append(construction_meter_empty_record)
    asset_records.extend(construction_meter_rails_records)
    asset_records.extend(hud_state_indicator_records)
    asset_records.append(quick_counter_frame_record)
    asset_records.append(city_hanging_ui_record)
    asset_records.append(city_continue_arrow_record)
    asset_records.extend(city_edge_clip_records)
    asset_records.extend(city_status_clip_records)
    asset_records.extend(city_panel_state_records)
    asset_records.extend(city_status_icon_records)
    asset_records.extend(city_panel_records)
    asset_records.append(city_action_icon_record)
    asset_records.extend(city_effect_records)
    asset_records.extend(crane_hook_frame_records)
    asset_records.extend(accuracy_star_records)

    city_building_records: list[dict[str, object]] = []
    for building_index, strip in enumerate(city_building_strips, start=1):
        records = _export_strip_frames(strip, 4, f"city_building_{building_index}", graphics_dir)
        city_building_records.extend(records)
        asset_records.extend(records)
    city_lot_records = _export_strip_frames(city_lot_strip, 5, "city_lot", graphics_dir)
    asset_records.extend(city_lot_records)

    # Fix 14.3: all four building strips can coexist in a developed city.
    # Put them on one partial BPP8 palette so the hardware never has to choose
    # between incompatible BPP8 OBJ palettes.
    _share_bpp8_palette(graphics_dir, city_building_records)

    # The two worker strips use 29 opaque colors combined.  Preserve every
    # source color by splitting each frame across two shared BPP4 layers.
    _split_worker_palettes(
        graphics_dir, [*menu_worker_blue_records, *menu_worker_red_records]
    )

    # Fix 14.2: canonicalize Build City UI palettes.  The original per-crop
    # export could occupy all 16 OBJ BPP4 banks on the first placement screen.
    # These groups are exact-color unions that fit in one 4bpp palette each;
    # only palette indices change, never rendered BGR555 colors.
    construction_meter_frame_records = [
        construction_meter_base_record, construction_meter_empty_record, *construction_meter_rails_records,
    ]
    white_ui_records = [
        *hud_white_digit_records, hud_population_icon_record, *construction_meter_frame_records,
    ]
    white_ui_assets = ("tower_font", *_record_asset_names(white_ui_records))
    white_entries = _share_bpp4_palette(graphics_dir, white_ui_assets)
    _set_shared_palette_entries(white_ui_records, white_entries)

    brown_entries = _share_bpp4_palette(
        graphics_dir, _record_asset_names(hud_brown_digit_records)
    )
    _set_shared_palette_entries(hud_brown_digit_records, brown_entries)

    placement_panel_records = [
        *city_edge_clip_records,
        city_panel_state_records[0],
        city_panel_state_records[3],
        city_comparison_panel_active_record,
    ]
    placement_panel_entries = _share_bpp4_palette(
        graphics_dir, _record_asset_names(placement_panel_records)
    )
    _set_shared_palette_entries(placement_panel_records, placement_panel_entries)

    browse_panel_records = [city_panel_state_records[1], city_panel_state_records[2]]
    browse_panel_entries = _share_bpp4_palette(
        graphics_dir, _record_asset_names(browse_panel_records)
    )
    _set_shared_palette_entries(browse_panel_records, browse_panel_entries)

    city_status_entries = _share_bpp4_palette(
        graphics_dir,
        (*_record_asset_names(city_status_clip_records), *_record_asset_names([city_continue_arrow_record])),
    )
    _set_shared_palette_entries(city_status_clip_records, city_status_entries)
    city_continue_arrow_record["palette_entries"] = city_status_entries

    lot_badge_records = [
        *city_type_badge_records, *city_lot_records, city_progress_record, *city_progress_tail_records,
        city_selector_active_slot_record,
    ]
    lot_badge_entries = _share_bpp4_palette(
        graphics_dir, _record_asset_names(lot_badge_records)
    )
    _set_shared_palette_entries(lot_badge_records, lot_badge_entries)

    # The four source fill rows contain exactly 15 opaque BGR555 colors, so
    # they fit one hardware BPP4 bank.  Keep them separate from the common
    # font/digit/population/meter-frame palette above; that common exact-color
    # union is only nine opaque colors and saves two live construction banks.
    construction_meter_fill_entries = _share_bpp4_palette(
        graphics_dir, _record_asset_names(construction_meter_fill_records)
    )
    _set_shared_palette_entries(construction_meter_fill_records, construction_meter_fill_entries)

    adapted = {locale.code: _adapt_instructions(locale) for locale in locales}
    wrapped: dict[str, dict[str, tuple[str, ...]]] = {}
    for locale in locales:
        about = _normalize_display_text(locale.strings[1])
        about = "\n".join(line for line in about.split("\n") if line.strip() != "img=10")
        wrapped[locale.code] = {
            "quick": _wrap_text(font, adapted[locale.code]["quick"]),
            "city": _wrap_text(font, adapted[locale.code]["city"]),
            "about": _wrap_text(font, about),
            "reset_city": _wrap_text(font, locale.strings[98], max_width=208),
        }

    city_modal_wrapped = {
        locale.code: tuple(_wrap_text(font, locale.strings[index], max_width=208) for index in range(36, 61))
        for locale in locales
    }

    font_header = include_dir / "tower_font.h"
    localization_header = include_dir / "tower_localization.h"
    ui_assets_header = include_dir / "tower_ui_assets.h"
    font_header.write_text(_font_header(font), encoding="utf-8", newline="\n")
    localization_header.write_text(
        _localization_header(locales, adapted, wrapped, city_modal_wrapped), encoding="utf-8", newline="\n"
    )
    ui_assets_header.write_text(_logo_header(asset_records), encoding="utf-8", newline="\n")

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
        "font_graphics_count": len(FONT_GRAPHICS_CHARACTERS),
        "font_space_between_characters": font.space_between_characters,
        "font_line_height": font.line_height,
        "adapted_instructions": adapted,
        "city_modal_wrapped_line_counts": {
            locale.code: [len(lines) for lines in city_modal_wrapped[locale.code]] for locale in locales
        },
        "wrapped_line_counts": {
            locale.code: {key: len(value) for key, value in wrapped[locale.code].items()}
            for locale in locales
        },
        "logos": [tower_record, sumea_record],
        "procedural_assets": [
            "menu_highlight", "city_progress_segment", "city_valid_lot_ring", "city_selector_active_slot",
            "city_comparison_panel_active", "city_type_badge_1", "city_type_badge_2",
            "city_type_badge_3", "city_type_badge_4",
        ],
        "menu_highlight": menu_highlight_record,
        "city_progress_segment": city_progress_record,
        "city_progress_tails": city_progress_tail_records,
        "menu_assets": {
            "icons": menu_icon_records,
            "worker_blue_frames": menu_worker_blue_records,
            "worker_red_frames": menu_worker_red_records,
        },
        "hud_assets": {
            "construction_target_badges": construction_target_badge_records,
            "white_digits": hud_white_digit_records,
            "brown_digits": hud_brown_digit_records,
            "red_digits": hud_red_digit_records,
            "status_graphic": hud_status_graphic_record,
            "population_icon": hud_population_icon_record,
            "construction_meter_fills": construction_meter_fill_records,
            "construction_meter_base": construction_meter_base_record,
            "construction_meter_empty": construction_meter_empty_record,
            "construction_meter_rails": construction_meter_rails_records,
            "state_indicators": hud_state_indicator_records,
            "quick_counter_frame": quick_counter_frame_record,
            "crane_hook_frames": crane_hook_frame_records,
            "accuracy_stars": accuracy_star_records,
        },
        "city_assets": {
            "buildings": city_building_records,
            "lots": city_lot_records,
            "supplemental": {
                "hanging_ui": city_hanging_ui_record,
                "continue_arrow": city_continue_arrow_record,
                "edge_clips": city_edge_clip_records,
                "status_clips": city_status_clip_records,
                "panel_states": city_panel_state_records,
                "status_icons": city_status_icon_records,
                "panels": city_panel_records,
                "action_icon": city_action_icon_record,
                "effects": city_effect_records,
                "valid_lot_ring": city_valid_lot_ring_record,
                "selector_active_slot": city_selector_active_slot_record,
                "comparison_panel_active": city_comparison_panel_active_record,
                "type_badges": city_type_badge_records,
            },
        },
        "files": files,
        "tree_hash": tree_digest.hexdigest(),
    }
    _write_json(reference_dir / "ui_assets_manifest.json", manifest)
    return manifest
