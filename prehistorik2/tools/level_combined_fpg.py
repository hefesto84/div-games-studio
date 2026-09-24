#!/usr/bin/env python3
"""Empaqueta fondo de nivel + sprites de personaje en UN SOLO .FPG.

Por que: DIV solo mantiene una paleta global activa a la vez; cuando se
cargan dos FPG por separado (LOAD_FPG dos veces), el segundo se "adapta"
(remapea cada pixel al color mas parecido de la paleta activa, ver
adaptar()/find_color()/palcrc en src/div32run/f.cpp). Ya se intento que esa
adaptacion encontrara coincidencia EXACTA metiendo los colores del
personaje en los indices 16-31 de la paleta del fondo (levelbg2fpg.py) y
aun asi salio mal en pruebas reales -- hay algo en ese mecanismo
(find_color salta un "last_c1" protegido, o el checksum palcrc no se
compara como se esperaba) que no se ha terminado de entender. Para no
depender de ese mecanismo nada fiable, este script mete fondo Y sprites en
el MISMO fichero FPG (una sola paleta, sin adaptacion cruzada de por medio,
cero ambiguedad).

Paleta resultante (256 entradas, solo las primeras 32 tienen contenido):
  0-15:  colores de terreno del nivel (de PRE2.PAL, uno de los 10)
  16-31: colores de los sprites de personaje (fijos, de PRE2SPR)

Codigos de grafico:
  1:      fondo del nivel completo (todo el tilemap, una imagen grande)
  2:      capa de colision, mismo tamano en pixeles que el fondo, un byte
          por pixel = categoria (0 = libre, 1 = solido). Pensada para leerse
          en tiempo real desde DIV con MAP_GET_PIXEL(fpg,2,x,y) -- resolucion
          por pixel (no por tile) para no tener que hacer division entera en
          el propio .PRG. Nunca se dibuja en pantalla, solo se consulta.
  500+n:  sprite de personaje n (n = 1..460, el mismo numero que en
          PRE2SPR.FPG / sprites_extracted/NNN.gif), con los indices de
          pixel desplazados +16 (excepto el 0, que sigue siendo
          transparente) para que caigan en el rango 16-31 de la paleta.
          (Base 500, no 1000: ver CHAR_COD_BASE mas abajo.)
  700+n:  version volteada horizontalmente del sprite de personaje n, solo
          para los sprites en PLAYER_ANIM_SPRITES (los que usa la animacion
          del jugador en gen_scroll_demo.py) -- ver build_characters().

Categoria de colision por tile: se deriva de la tabla de propiedades de
tile (docs/format_notes.md) indexada por el byte CRUDO del tilemap (0-255),
no por el valor ya resuelto de la tabla de lookup -- son dos lookups en
paralelo a partir del mismo byte. "Solido" = lado horizontal solido (byte1
== 1) o techo/suelo solido (byte2 == 1) o bit 0 del byte3 (suelo solido).
De momento no se distingue solido-desde-abajo, letal, resbaladizo ni
plataformas de un solo sentido -- solo "solido o no", primer paso de
colision. Ampliar aqui cuando se necesiten esos matices.
"""
import glob
import os
import struct
import sys
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from render_level import (ASSETS, DECOMP_DIR, PARSED_DIR, LEVEL_ORDER,
                           PALETTE_PER_LEVEL, load_palettes, decode_tile, load_union_tiles)
import gif2fpg
import json

FPG_HEAD = 64
CHAR_SLOT = 16
# ¡Ojo! el loader del runtime (load_fpg en f.cpp) exige COD<1000 (usa un
# array lst[1000] y corta el bucle de lectura en cuanto ve un COD>=1000),
# así que la base no puede ser 1000: con 460 sprites, 500+460=960 cabe.
CHAR_COD_BASE = 500


def dac_bytes(terrain_rgb, char_rgb):
    dac = bytearray(768)
    for i, (r, g, b) in enumerate(terrain_rgb):
        dac[i * 3:i * 3 + 3] = (round(r * 63 / 255), round(g * 63 / 255), round(b * 63 / 255))
    for i, (r, g, b) in enumerate(char_rgb):
        o = CHAR_SLOT + i
        dac[o * 3:o * 3 + 3] = (round(r * 63 / 255), round(g * 63 / 255), round(b * 63 / 255))
    return bytes(dac)


def entry(cod, ancho, alto, n_puntos, puntos_bytes, pixel_bytes, descrip=b"", filename=b""):
    descrip = descrip[:32].ljust(32, b"\x00")
    filename = filename[:12].ljust(12, b"\x00")
    long_total = FPG_HEAD + n_puntos * 4 + len(pixel_bytes)
    head = struct.pack("<ii32s12siii", cod, long_total, descrip, filename, ancho, alto, n_puntos)
    return head + puntos_bytes + pixel_bytes


def build_background(level_name):
    parsed = json.load(open(os.path.join(PARSED_DIR, f"{level_name}.json")))
    level_raw = open(os.path.join(DECOMP_DIR, f"{level_name}.SQZ"), "rb").read()
    height = parsed["height"]
    lookup = parsed["lookup_table"]
    tilemap = parsed["tilemap_row_major"]
    own_tiles_off = 256 * height + 512
    union_tiles = load_union_tiles()

    width_px, height_px = 256 * 16, height * 16
    pixels = bytearray(width_px * height_px)
    own_cache = {}

    for ty in range(height):
        row = tilemap[ty]
        for tx in range(256):
            v = lookup[row[tx]]
            if v == 256:
                continue
            if v < 256:
                if v not in own_cache:
                    o = own_tiles_off + v * 128
                    own_cache[v] = decode_tile(level_raw[o:o + 128])
                tile = own_cache[v]
            else:
                uidx = v - 256
                if uidx >= len(union_tiles):
                    continue
                tile = union_tiles[uidx]
            ox, oy = tx * 16, ty * 16
            for ry in range(16):
                base = (oy + ry) * width_px + ox
                pixels[base:base + 16] = bytes(tile[ry])

    return entry(1, width_px, height_px, 0, b"", bytes(pixels), b"LEVEL BG", b"BG.MAP"), (width_px, height_px)


def is_solid(raw_index, lookup, tile_properties):
    """Lectura literal de docs/format_notes.md: solido = lado horizontal
    solido (byte1==1) o techo/suelo solido (byte2==1) o bit0 del byte3
    (suelo solido).

    Se probo primero simplificar esto a "cualquier tile con grafico es
    solido" (ignorando las propiedades) porque instintivamente la roca
    de fondo se ve solida por todas partes. Eso rompio la colision de
    verdad: el jugador empezaba "bloqueado" (project_prehistorik2_remake
    en memoria). Verificado con datos reales: en LEVEL1, la columna donde
    arranca el nivel (start_x=42, tile 2) tiene roca decorativa (con
    grafico, pero horizontal=vertical=misc_bits=0) en casi todas las filas,
    y SOLO las filas 42-43 (exactamente donde start_y=672 coloca los pies
    del jugador) tienen vertical==1. O sea: la mayor parte de la roca
    visible es puro decorado de fondo, no solida -- solo una fina capa de
    "superficie funcional" bloquea de verdad. La definicion literal por
    propiedades es la correcta; no simplificar esto otra vez."""
    if lookup[raw_index] == 256:
        return False
    props = tile_properties[raw_index]
    return props["horizontal"] == 1 or props["vertical"] == 1 or (props["misc_bits"] & 1) != 0


def build_collision_layer(level_name):
    parsed = json.load(open(os.path.join(PARSED_DIR, f"{level_name}.json")))
    height = parsed["height"]
    lookup = parsed["lookup_table"]
    tilemap = parsed["tilemap_row_major"]
    tile_properties = parsed["tile_properties"]

    width_px, height_px = 256 * 16, height * 16
    pixels = bytearray(width_px * height_px)  # 0 = libre

    solid_cache = {}
    for ty in range(height):
        row = tilemap[ty]
        for tx in range(256):
            raw_index = row[tx]
            if raw_index not in solid_cache:
                solid_cache[raw_index] = is_solid(raw_index, lookup, tile_properties)
            if not solid_cache[raw_index]:
                continue
            ox, oy = tx * 16, ty * 16
            row_bytes = bytes([1]) * 16
            for ry in range(16):
                base = (oy + ry) * width_px + ox
                pixels[base:base + 16] = row_bytes

    return entry(2, width_px, height_px, 0, b"", bytes(pixels), b"LEVEL COL", b"COL.MAP")


MIRROR_COD_BASE = 970  # ver build_characters(): "mirando a la izquierda" = volteado, no un sprite distinto
# ¡Ojo! los sprites de personaje normales ocupan CHAR_COD_BASE+n = 501..960
# (n=1..460). Un MIRROR_COD_BASE de 700 colisionaba con esos codigos (p.ej.
# 700+1=701 == 500+201, un sprite real de fruta) y la entrada que aparece
# despues en el fichero pisaba a la anterior en la tabla de carga del
# runtime (load_fpg en f.cpp) -- por eso se veian sprites random al mirar a
# la izquierda. 970 esta por encima de 960 y por debajo del limite de 1000.


def build_characters(mirror_sprites=()):
    """mirror_sprites: numeros de sprite (1..460) de los que ademas se
    genera una copia volteada horizontalmente, en COD MIRROR_COD_BASE+n.

    Por que: no hay ningun catalogo oficial de que sprite es cada pose (es
    lectura visual de la hoja de contacto), y los sprites 1-460 NO son un
    par derecha/izquierda limpio y simetrico -- se probo usar sprites
    "distintos, dibujados a mano" para cada direccion (p.ej. 35-38 para
    mirar a la izquierda) y salio fatal: esos frames eran una mezcla de
    poses de reposo, carrera y hasta un sprite roto/parcial (el 40), no un
    ciclo de carrera coherente. Voltear los MISMOS frames que ya se sabe
    que funcionan mirando a la derecha garantiza la pose exacta en ambas
    direcciones, sin tener que catalogar los 460 sprites a mano."""
    files = sorted(glob.glob(os.path.join(gif2fpg.SRC_DIR, "*.gif")))
    ordered_colors, palette_index = gif2fpg.build_palette(files)
    entries = []
    for f in files:
        cod = int(os.path.splitext(os.path.basename(f))[0])
        im = Image.open(f).convert("RGBA")
        ancho, alto = im.size
        px = im.load()
        data = bytearray(ancho * alto)
        for y in range(alto):
            for x in range(ancho):
                idx = palette_index[px[x, y]]
                data[y * ancho + x] = 0 if idx == 0 else idx + CHAR_SLOT
        puntos = struct.pack("<2h", ancho // 2, alto // 2)
        entries.append(entry(CHAR_COD_BASE + cod, ancho, alto, 1, puntos, bytes(data),
                              f"SPRITE {cod}".encode("ascii"), f"S{cod}.MAP".encode("ascii")))

        if cod in mirror_sprites:
            im_flip = im.transpose(Image.FLIP_LEFT_RIGHT)
            px_flip = im_flip.load()
            data_flip = bytearray(ancho * alto)
            for y in range(alto):
                for x in range(ancho):
                    idx = palette_index[px_flip[x, y]]
                    data_flip[y * ancho + x] = 0 if idx == 0 else idx + CHAR_SLOT
            entries.append(entry(MIRROR_COD_BASE + cod, ancho, alto, 1, puntos, bytes(data_flip),
                                  f"SPRITE {cod} MIRROR".encode("ascii")[:32],
                                  f"S{cod}M.MAP".encode("ascii")))

    char_rgb = [(r, g, b) for (r, g, b, a) in ordered_colors]
    return entries, char_rgb


# Sprites de personaje que usa la animacion del jugador (gen_scroll_demo.py)
# -- los unicos de los que hace falta generar tambien la copia volteada.
PLAYER_ANIM_SPRITES = {1, 6, 7, 8, 10, 12}


def build(level_name, out_path):
    palette_idx = int(PALETTE_PER_LEVEL[LEVEL_ORDER.index(level_name)])
    terrain_rgb = load_palettes()[palette_idx]

    bg_entry, (w, h) = build_background(level_name)
    col_entry = build_collision_layer(level_name)
    char_entries, char_rgb = build_characters(PLAYER_ANIM_SPRITES)

    with open(out_path, "wb") as out:
        out.write(b"fpg\x1a\x0d\x0a\x00\x00")
        out.write(dac_bytes(terrain_rgb, char_rgb))
        out.write(bytes(576))
        out.write(bg_entry)
        out.write(col_entry)
        for e in char_entries:
            out.write(e)

    print(f"{level_name}: fondo {w}x{h}px (paleta terreno #{palette_idx}) + capa de colision + "
          f"{len(char_entries)} sprites de personaje, un solo FPG -> {out_path} "
          f"({os.path.getsize(out_path)} bytes)")


if __name__ == "__main__":
    level = sys.argv[1] if len(sys.argv) > 1 else "LEVEL1"
    out = os.path.join(ASSETS, f"{level}_ALL.FPG")
    build(level, out)
