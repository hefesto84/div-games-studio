#!/usr/bin/env python3
"""Convierte los sprites GIF extraidos del fan site (001.gif..460.gif) a un
unico fichero .FPG de DIV Games Studio.

Formato FPG (deducido de src/div/fpgfile.cpp y fpgfile.hpp del propio port):

  Cabecera del fichero:
    8   bytes  magic "fpg\x1a\x0d\x0a\x00\x00"
    768 bytes  paleta (256 x RGB, rango 0-63, escala VGA)
    576 bytes  "reglas" (16 x struct tipo_regla de 36 bytes) - gamas de color
               del editor original; no las usa el runtime, se dejan a cero.

  Por cada sprite (repetido hasta EOF):
    HeadFPG (64 bytes):
      int  COD       (4)  codigo/id del grafico
      int  LONG      (4)  longitud total de esta entrada (64 + nPuntos*4 + Ancho*Alto)
      char Descrip[32]
      char Filename[12]
      int  Ancho     (4)
      int  Alto      (4)
      int  nPuntos   (4)
    nPuntos*4 bytes: puntos de control (short x, short y), punto 0 = centro
    Ancho*Alto bytes: pixeles, 1 byte = indice de paleta, color 0 = transparente

Todos los sprites de esta coleccion comparten exactamente los mismos 16
colores RGBA (confirmado por analisis previo), con (255,255,255,alpha=0)
como color transparente -> se mapea siempre al indice de paleta 0.
"""
import glob
import os
import struct
import sys
from PIL import Image

SRC_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "original", "sprites_extracted")
OUT_FPG = os.path.join(os.path.dirname(__file__), "..", "assets", "PRE2SPR.FPG")

FPG_HEAD = 64


def build_palette(files):
    """Recorre todos los sprites y construye la paleta global de 16 colores,
    con el color transparente forzado al indice 0."""
    colors = {}  # RGBA -> total de pixeles (para ordenar por frecuencia)
    transparent_rgba = None
    for f in files:
        im = Image.open(f)
        trans_idx = im.info.get("transparency")
        im_rgba = im.convert("RGBA")
        for count, rgba in im_rgba.getcolors(maxcolors=100000):
            colors[rgba] = colors.get(rgba, 0) + count
            if rgba[3] == 0:
                transparent_rgba = rgba

    if transparent_rgba is None:
        raise SystemExit("No se encontro ningun color transparente en el set de sprites")
    if len(colors) > 256:
        raise SystemExit(f"Demasiados colores unicos ({len(colors)}) para una paleta FPG de 256")

    ordered = [transparent_rgba] + sorted(
        (c for c in colors if c != transparent_rgba), key=lambda c: -colors[c]
    )
    palette_index = {rgba: i for i, rgba in enumerate(ordered)}
    return ordered, palette_index


def dac_bytes(ordered_colors):
    """256 entradas RGB en rango 0-63 (escala VGA). Los colores no usados
    se dejan a negro."""
    dac = bytearray(768)
    for i, (r, g, b, a) in enumerate(ordered_colors):
        dac[i * 3 + 0] = round(r * 63 / 255)
        dac[i * 3 + 1] = round(g * 63 / 255)
        dac[i * 3 + 2] = round(b * 63 / 255)
    return bytes(dac)


def sprite_entry(cod, filename, im, palette_index):
    im_rgba = im.convert("RGBA")
    ancho, alto = im_rgba.size
    pixels = im_rgba.load()

    data = bytearray(ancho * alto)
    for y in range(alto):
        for x in range(ancho):
            data[y * ancho + x] = palette_index[pixels[x, y]]

    descrip = f"SPRITE {cod}".encode("ascii")[:32].ljust(32, b"\x00")
    base = os.path.splitext(os.path.basename(filename))[0].upper()
    dos_name = (base[:8] + ".MAP").encode("ascii")[:12].ljust(12, b"\x00")

    n_puntos = 1
    puntos = struct.pack("<2h", ancho // 2, alto // 2)  # punto 0 = centro

    long_total = FPG_HEAD + n_puntos * 4 + ancho * alto
    head = struct.pack("<ii32s12siii", cod, long_total, descrip, dos_name, ancho, alto, n_puntos)
    return head + puntos + bytes(data)


def main():
    files = sorted(glob.glob(os.path.join(SRC_DIR, "*.gif")))
    if not files:
        raise SystemExit(f"No se encontraron GIFs en {SRC_DIR}")

    print(f"Sprites encontrados: {len(files)}")
    ordered_colors, palette_index = build_palette(files)
    print(f"Paleta global: {len(ordered_colors)} colores (indice 0 = transparente)")

    with open(OUT_FPG, "wb") as out:
        out.write(b"fpg\x1a\x0d\x0a\x00\x00")
        out.write(dac_bytes(ordered_colors))
        out.write(bytes(576))  # reglas, sin usar

        for f in files:
            cod = int(os.path.splitext(os.path.basename(f))[0])
            im = Image.open(f)
            out.write(sprite_entry(cod, f, im, palette_index))

    size = os.path.getsize(OUT_FPG)
    print(f"Escrito {OUT_FPG} ({size} bytes)")


if __name__ == "__main__":
    sys.exit(main())
