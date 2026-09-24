#!/usr/bin/env python3
"""Empaqueta los tiles graficos de un nivel (propios + compartidos de
UNION.SQZ, los que realmente usa ese nivel) en un .FPG de DIV, usando como
COD el valor resuelto de la tabla de lookup (256=transparente se omite,
<256=tile propio, >256=tile de UNION.SQZ) -- asi el tilemap del nivel se
puede pintar directamente con PUT(x,y,valor) sin tener que traducir codigos.

Reutiliza el decodificador de tiles y la paleta de render_level.py.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from render_level import (ASSETS, DECOMP_DIR, PARSED_DIR, LEVEL_ORDER,
                           PALETTE_PER_LEVEL, load_palettes, decode_tile, load_union_tiles)

FPG_HEAD = 64


def dac_bytes(palette_rgb):
    """256 entradas RGB 0-63 (VGA). Solo rellena las primeras 16 (paleta del nivel)."""
    dac = bytearray(768)
    for i, (r, g, b) in enumerate(palette_rgb):
        dac[i * 3 + 0] = round(r * 63 / 255)
        dac[i * 3 + 1] = round(g * 63 / 255)
        dac[i * 3 + 2] = round(b * 63 / 255)
    return bytes(dac)


def tile_to_fpg_pixels(tile16x16):
    data = bytearray(16 * 16)
    for y in range(16):
        for x in range(16):
            data[y * 16 + x] = tile16x16[y][x]
    return bytes(data)


def sprite_entry(cod, pixel_bytes, ancho=16, alto=16):
    """cod nunca puede ser 0: el loader del runtime (load_fpg en f.cpp) usa
    COD==0 como fin de la lista de graficos del FPG, así que un tile con
    cod 0 truncaría silenciosamente el resto del fichero. build() ya suma 1
    a cada valor por esto (ver llamada a sprite_entry)."""
    assert cod > 0, f"COD no puede ser 0 (tile FPG usa 0 como terminador): {cod}"
    descrip = f"TILE {cod}".encode("ascii")[:32].ljust(32, b"\x00")
    dos_name = f"T{cod}.MAP".encode("ascii")[:12].ljust(12, b"\x00")
    n_puntos = 1
    puntos = struct.pack("<2h", ancho // 2, alto // 2)  # punto 0 = centro, igual que gif2fpg.py
    long_total = FPG_HEAD + n_puntos * 4 + ancho * alto
    head = struct.pack("<ii32s12siii", cod, long_total, descrip, dos_name, ancho, alto, n_puntos)
    return head + puntos + pixel_bytes


def build(level_name, out_path):
    parsed = json.load(open(os.path.join(PARSED_DIR, f"{level_name}.json")))
    level_raw = open(os.path.join(DECOMP_DIR, f"{level_name}.SQZ"), "rb").read()
    height = parsed["height"]
    lookup = parsed["lookup_table"]
    tilemap = parsed["tilemap_row_major"]
    own_tiles_off = 256 * height + 512

    union_tiles = load_union_tiles()
    palette_idx = int(PALETTE_PER_LEVEL[LEVEL_ORDER.index(level_name)])
    palette = load_palettes()[palette_idx]

    used_values = set()
    for row in tilemap:
        for raw_index in row:
            v = lookup[raw_index]
            if v != 256:
                used_values.add(v)

    entries = []
    for v in sorted(used_values):
        if v < 256:
            o = own_tiles_off + v * 128
            tile = decode_tile(level_raw[o:o + 128])
        else:
            uidx = v - 256
            if uidx >= len(union_tiles):
                continue
            tile = union_tiles[uidx]
        entries.append((v + 1, tile_to_fpg_pixels(tile)))  # +1: ver nota en sprite_entry sobre cod 0

    with open(out_path, "wb") as out:
        out.write(b"fpg\x1a\x0d\x0a\x00\x00")
        out.write(dac_bytes(palette))
        out.write(bytes(576))
        for cod, pixels in entries:
            out.write(sprite_entry(cod, pixels))

    print(f"{level_name}: {len(entries)} tiles (paleta #{palette_idx}) -> {out_path} "
          f"({os.path.getsize(out_path)} bytes)")


if __name__ == "__main__":
    level = sys.argv[1] if len(sys.argv) > 1 else "LEVEL1"
    out = os.path.join(ASSETS, f"{level}_TILES.FPG")
    build(level, out)
