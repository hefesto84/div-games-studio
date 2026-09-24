#!/usr/bin/env python3
"""Renderiza un nivel completo de Prehistorik 2 (ya descomprimido y
parseado) a un PNG, para poder verificar visualmente que el parser y el
descifrado de los tiles gráficos son correctos.

Formato de tile (docs/format_notes.md / techdocs.htm):
  - 16x16 px, 4 bits por pixel (16 colores), planar: 4 planos de 32 bytes
    cada uno (16 filas x 2 bytes/fila = 16 bits = 16 pixeles monocromos),
    planos consecutivos. Color 0 = transparente.
  - Tiles propios del nivel: en el propio LEVEL*.SQZ, justo despues de la
    tabla de lookup, offset = 256*alto + 512 + valor*128.
  - Tiles compartidos (valor de lookup > 256): en UNION.SQZ,
    offset = (valor-256)*128.

Paleta: PRE2.PAL tiene 10 paletas de 16 colores en formato B,G,R,0 (8 bits).
Que paleta usa cada nivel: cadena "0134568899222298" (un digito por nivel,
en orden 1,2,3,4,5,6,7,8,9,A,B,C,D,E,F,G).
"""
import argparse
import json
import os
import sys
from PIL import Image

ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets", "original")
DECOMP_DIR = os.path.join(ASSETS, "decompressed")
PARSED_DIR = os.path.join(ASSETS, "levels_parsed")
OUT_DIR = os.path.join(ASSETS, "levels_rendered")

LEVEL_ORDER = ["LEVEL1", "LEVEL2", "LEVEL3", "LEVEL4", "LEVEL5", "LEVEL6", "LEVEL7",
               "LEVEL8", "LEVEL9", "LEVELA", "LEVELB", "LEVELC", "LEVELD", "LEVELE",
               "LEVELF", "LEVELG"]
PALETTE_PER_LEVEL = "0134568899222298"


def load_palettes():
    data = open(os.path.join(ASSETS, "PRE2.PAL"), "rb").read()
    assert len(data) == 10 * 16 * 4, len(data)
    palettes = []
    for p in range(10):
        colors = []
        for c in range(16):
            o = (p * 16 + c) * 4
            b, g, r, _ = data[o:o + 4]
            colors.append((r, g, b))
        palettes.append(colors)
    return palettes


def decode_tile(raw128):
    """128 bytes -> lista de 16x16 indices de paleta (0-15)."""
    pixels = [[0] * 16 for _ in range(16)]
    for plane in range(4):
        plane_data = raw128[plane * 32:(plane + 1) * 32]
        for row in range(16):
            b0, b1 = plane_data[row * 2], plane_data[row * 2 + 1]
            word = (b0 << 8) | b1
            for col in range(16):
                bit = (word >> (15 - col)) & 1
                pixels[row][col] |= bit << plane
    return pixels


def load_union_tiles():
    path = os.path.join(DECOMP_DIR, "UNION.SQZ")
    data = open(path, "rb").read()
    count = len(data) // 128
    return [decode_tile(data[i * 128:(i + 1) * 128]) for i in range(count)]


def render_level(name, out_path):
    parsed = json.load(open(os.path.join(PARSED_DIR, f"{name}.json")))
    level_raw = open(os.path.join(DECOMP_DIR, f"{name}.SQZ"), "rb").read()

    height = parsed["height"]
    lookup = parsed["lookup_table"]
    tilemap = parsed["tilemap_row_major"]

    own_tiles_off = 256 * height + 512
    palette_idx = int(PALETTE_PER_LEVEL[LEVEL_ORDER.index(name)])
    palette = load_palettes()[palette_idx]
    union_tiles = load_union_tiles()

    own_tile_cache = {}
    img = Image.new("RGB", (256 * 16, height * 16), (0, 0, 0))
    px = img.load()

    transparent_tiles = 0
    own_used = 0
    union_used = 0

    for ty in range(height):
        row = tilemap[ty]
        for tx in range(256):
            raw_index = row[tx]
            value = lookup[raw_index]
            if value == 256:
                transparent_tiles += 1
                continue
            if value < 256:
                if value not in own_tile_cache:
                    o = own_tiles_off + value * 128
                    own_tile_cache[value] = decode_tile(level_raw[o:o + 128])
                tile = own_tile_cache[value]
                own_used += 1
            else:
                uidx = value - 256
                if uidx >= len(union_tiles):
                    continue
                tile = union_tiles[uidx]
                union_used += 1

            ox, oy = tx * 16, ty * 16
            for row_i in range(16):
                trow = tile[row_i]
                for col_i in range(16):
                    ci = trow[col_i]
                    if ci == 0:
                        continue  # color 0 = transparente
                    px[ox + col_i, oy + row_i] = palette[ci]

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    img.save(out_path)
    print(f"{name}: {img.width}x{img.height}px, paleta #{palette_idx}, "
          f"tiles propios usados={own_used} compartidos={union_used} transparentes={transparent_tiles} "
          f"-> {out_path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("level", nargs="?")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()

    names = LEVEL_ORDER if args.all else [args.level]
    if not names or names == [None]:
        ap.error("indica un nivel (p.ej. LEVEL1) o usa --all")

    for name in names:
        out_path = os.path.join(OUT_DIR, f"{name}.png")
        render_level(name, out_path)


if __name__ == "__main__":
    sys.exit(main())
