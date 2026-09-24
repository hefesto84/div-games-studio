#!/usr/bin/env python3
"""Parsea los ficheros LEVEL*.SQZ originales de Prehistorik 2 (ya
descomprimidos con DIET, ver decompress_diet.ps1) y vuelca su contenido a
JSON legible.

Formato deducido de https://pre2.mine.nu/techdocs.htm (Jesses y Dorten),
documentado con detalle en docs/format_notes.md. Los offsets de la tabla de
"estructuras" estan verificados: cada seccion empieza justo donde acaba la
anterior, sin huecos, hasta el offset 5028 (ultimo byte del fichero).

Uso:
    python parse_level.py LEVEL1          # nivel 1, alto=49 (de HEIGHTS)
    python parse_level.py LEVEL1 --out level1.json
    python parse_level.py --all           # los 16 niveles a la vez
"""
import argparse
import json
import os
import struct
import sys

DECOMP_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "original", "decompressed")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "original", "levels_parsed")

# Alto de cada nivel, en tiles. No esta en el fichero, solo en el ejecutable
# original (PRE2.EXE) -- tomado de la tabla de techdocs.htm.
HEIGHTS = {
    "LEVEL1": 49, "LEVEL2": 104, "LEVEL3": 49, "LEVEL4": 45, "LEVEL5": 128,
    "LEVEL6": 128, "LEVEL7": 128, "LEVEL8": 86, "LEVEL9": 110, "LEVELA": 12,
    "LEVELB": 24, "LEVELC": 51, "LEVELD": 51, "LEVELE": 38, "LEVELF": 173,
    "LEVELG": 84,
}

STRUCT_SIZE = 5029

ENEMY_TYPE_NAMES = {
    0: "falls_then_walks", 1: "static_decoration", 2: "web_spider",
    3: "spider_spawner", 4: "pendulum_spider", 5: "line_of_sight_diagonal",
    6: "flyer_smart", 7: "line_of_sight_diagonal_simple", 8: "jumper",
    9: "walker_ignores_walls", 10: "ground_spawner", 11: "flying_squirrel",
    12: "fast_runner",
}


def read_u8(b, o): return b[o]
def read_s16(b, o): return struct.unpack_from("<h", b, o)[0]
def read_u16(b, o): return struct.unpack_from("<H", b, o)[0]


def parse_tile_properties(struct_bytes):
    b1 = struct_bytes[0:256]
    b2 = struct_bytes[256:512]
    b3 = struct_bytes[512:768]
    b4 = struct_bytes[4031:4287]
    tiles = []
    for i in range(256):
        tiles.append({
            "horizontal": b1[i],
            "vertical": b2[i],
            "misc_bits": b3[i],
            "slope_bits": b4[i],
        })
    return tiles


def _is_empty(rec):
    """Los registros no usados se rellenan con 0xFF (sentinela de 'vacio'),
    no con ceros -- confirmado inspeccionando niveles reales."""
    return all(v == 0xFF for v in rec)


def parse_gates(struct_bytes):
    gates = []
    base = 1289
    for i in range(20):
        o = base + i * 7
        rec = struct_bytes[o:o + 7]
        if _is_empty(rec):
            continue
        gates.append({
            "xin": rec[0], "yin": rec[1],
            "xscreen": rec[2], "yscreen": rec[3],
            "xout": rec[4], "yout": rec[5],
            "scroll": rec[6],
        })
    return gates


def parse_moving_blocks(struct_bytes):
    blocks = []
    base = 1429
    for i in range(15):
        o = base + i * 10
        rec = struct_bytes[o:o + 10]
        if _is_empty(rec):
            continue
        blocks.append({
            "x": rec[0], "y": rec[1], "width": rec[2], "height": rec[3],
            "xact": rec[4], "yact": rec[5],
            "dist": rec[8],
        })
    return blocks


ENEMY_EXTRA_LEN = {0: 13, 1: 13, 2: 15, 3: 14, 4: 17, 5: 16, 6: 21, 7: 15,
                   8: 17, 9: 19, 10: 14, 11: 16, 12: 15}


def parse_enemies(struct_bytes, enemy_sprite_offset):
    base = 1579
    region = struct_bytes[base:base + 2048]
    enemies = []
    pos = 0
    while pos < len(region):
        length = region[pos]
        if length == 0 or length == 0xFF:
            break
        rec = region[pos:pos + length]
        if len(rec) < 13:
            break
        type_expert = rec[1]
        etype = type_expert & 0x7F
        expert_only = bool(type_expert & 0x80)
        sprite_stored = read_u16(rec, 2)
        enemy = {
            "type": etype,
            "type_name": ENEMY_TYPE_NAMES.get(etype, f"unknown_{etype}"),
            "expert_only": expert_only,
            "sprite_stored": sprite_stored,
            "sprite_actual": sprite_stored + 312 - enemy_sprite_offset,
            "unknown1": rec[4],
            "hitpoints": rec[5],
            "pause": rec[6],
            "score_code": rec[8],
            "x": read_s16(rec, 9),
            "y": read_s16(rec, 11),
            "record_length": length,
            "raw_extra_hex": rec[13:].hex(),
        }
        enemies.append(enemy)
        pos += length
    return enemies


def parse_secrets(struct_bytes):
    secrets = []
    base = 3631
    for i in range(80):
        o = base + i * 5
        rec = struct_bytes[o:o + 5]
        if _is_empty(rec):
            continue
        secrets.append({
            "from_tile": rec[0], "to_tile": rec[1],
            "bonus_raw": rec[2], "x": rec[3], "y": rec[4],
        })
    return secrets


def parse_items(struct_bytes, item_sprite_offset):
    items = []
    base = 4287
    for i in range(70):
        o = base + i * 7
        rec = struct_bytes[o:o + 7]
        if _is_empty(rec):
            continue
        sprite_stored = read_u16(rec, 4)
        items.append({
            "x": read_s16(rec, 0), "y": read_s16(rec, 2),
            "sprite_stored": sprite_stored,
            "sprite_actual": sprite_stored + 53 - item_sprite_offset,
        })
    return items


def parse_platforms(struct_bytes):
    platforms = []
    base = 4777
    for i in range(16):
        o = base + i * 15
        rec = struct_bytes[o:o + 15]
        if _is_empty(rec):
            continue
        sprite_stored = read_u16(rec, 4)
        platforms.append({
            "x": read_s16(rec, 0), "y": read_s16(rec, 2),
            "sprite_stored": sprite_stored,
            "behavior": rec[6], "speed": rec[7],
            "drop_delay": rec[9] if len(rec) > 9 else None,
        })
    return platforms


def parse_level(name):
    path = os.path.join(DECOMP_DIR, f"{name}.SQZ")
    if not os.path.isfile(path):
        raise SystemExit(f"No existe {path} (¿corriste decompress_diet.ps1?)")
    data = open(path, "rb").read()

    height = HEIGHTS[name]
    tilemap_size = 256 * height
    lookup_off = tilemap_size
    lookup_size = 512
    struct_off = len(data) - STRUCT_SIZE

    if struct_off < lookup_off + lookup_size:
        raise SystemExit(
            f"{name}: tamaño de fichero ({len(data)}) incompatible con alto={height} "
            f"(offset de estructuras {struct_off} < fin de lookup {lookup_off + lookup_size})"
        )

    tilemap = list(data[0:tilemap_size])
    lookup = [read_u16(data, lookup_off + i * 2) for i in range(256)]
    small_num = max((v for v in lookup if v < 256), default=0)

    tile_bitmaps_off = lookup_off + lookup_size
    tile_bitmaps_size = struct_off - tile_bitmaps_off

    struct_bytes = data[struct_off:struct_off + STRUCT_SIZE]

    item_sprite_offset = read_u16(struct_bytes, 3627)
    enemy_sprite_offset = read_u16(struct_bytes, 3629)

    result = {
        "level": name,
        "height": height,
        "width": 256,
        "file_size": len(data),
        "small_num": small_num,
        "own_tile_bitmaps_bytes": tile_bitmaps_size,
        "own_tile_bitmaps_count_expected": tile_bitmaps_size // 128,
        "start_x": read_s16(struct_bytes, 770),
        "start_y": read_s16(struct_bytes, 772),
        "h_scroll_limit_tiles": struct_bytes[774],
        "scroll_behavior_bits": struct_bytes[776],
        "item_sprite_offset": item_sprite_offset,
        "enemy_sprite_offset": enemy_sprite_offset,
        "tile_properties": parse_tile_properties(struct_bytes),
        "gates": parse_gates(struct_bytes),
        "moving_blocks": parse_moving_blocks(struct_bytes),
        "enemies": parse_enemies(struct_bytes, enemy_sprite_offset),
        "secrets": parse_secrets(struct_bytes),
        "items": parse_items(struct_bytes, item_sprite_offset),
        "platforms": parse_platforms(struct_bytes),
        "kong": {
            "left_border": read_s16(struct_bytes, 5017),
            "right_border": read_s16(struct_bytes, 5019),
            "unknown_5021": struct_bytes[5021],
            "health": read_s16(struct_bytes, 5022),
            "present_5024": struct_bytes[5024],
            "x": read_s16(struct_bytes, 5025),
            "y": read_s16(struct_bytes, 5027) if len(struct_bytes) > 5028 else None,
        },
        "tilemap_row_major": [tilemap[r * 256:(r + 1) * 256] for r in range(height)],
        "lookup_table": lookup,
    }
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("level", nargs="?", help="p.ej. LEVEL1, LEVELA")
    ap.add_argument("--all", action="store_true", help="parsea los 16 niveles")
    ap.add_argument("--out", help="fichero de salida (solo con un nivel)")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)

    names = list(HEIGHTS.keys()) if args.all else [args.level]
    if not names or names == [None]:
        ap.error("indica un nivel o usa --all")

    for name in names:
        result = parse_level(name)
        out_path = args.out if (args.out and not args.all) else os.path.join(OUT_DIR, f"{name}.json")
        with open(out_path, "w") as f:
            json.dump(result, f, indent=1)
        print(f"{name}: alto={result['height']} enemigos={len(result['enemies'])} "
              f"plataformas={len(result['platforms'])} puertas={len(result['gates'])} "
              f"secretos={len(result['secrets'])} items={len(result['items'])} "
              f"small_num={result['small_num']} tile_bitmaps_esperados={result['own_tile_bitmaps_count_expected']} "
              f"-> {out_path}")


if __name__ == "__main__":
    sys.exit(main())
