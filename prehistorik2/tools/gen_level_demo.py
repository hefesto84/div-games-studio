#!/usr/bin/env python3
"""Genera un .PRG que pinta un trozo de un nivel real usando el FPG de
tiles generado por leveltiles2fpg.py -- para poder verlo corriendo dentro
de DIV, no solo como PNG.

Usa el patron PROCESS(X,Y,GRAPH) con bucle de espera, el mismo que ya se
valido visualmente con los sprites de personajes en PRE2.PRG (ver
project_prehistorik2_remake en memoria) -- evita usar PUT() de bajo nivel
directamente, cuyo manejo exacto del punto de control no esta verificado.

Por defecto usa la resolucion original del juego, 320x200, en una ventana
centrada sobre la posicion de inicio del nivel (start_x/start_y del JSON
parseado) -- para dar la sensacion de tamano real, no el nivel entero.

IMPORTANTE: escribe el .PRG en CRLF directamente (ver
feedback_div_compiler_crlf en memoria: el compilador falla con un error
criptico si el fichero esta en LF).

Los COD del FPG de tiles son (valor_lookup + 1) -- ver leveltiles2fpg.py.
"""
import json
import os
import sys

PARSED_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "original", "levels_parsed")


def generate(level_name, out_prg_path, fpg_name, screen_w=320, screen_h=200):
    parsed = json.load(open(os.path.join(PARSED_DIR, f"{level_name}.json")))
    height = parsed["height"]
    lookup = parsed["lookup_table"]
    tilemap = parsed["tilemap_row_major"]
    start_x, start_y = parsed["start_x"], parsed["start_y"]

    tiles_wide = -(-screen_w // 16)  # ceil
    tiles_tall = -(-screen_h // 16)

    # Centra el viewport en la posicion de inicio, con margen a los lados,
    # sin salirse del mapa (0..256 tiles de ancho, 0..height de alto).
    start_tx, start_ty = start_x // 16, start_y // 16
    from_tx = max(0, min(256 - tiles_wide, start_tx - tiles_wide // 2))
    from_ty = max(0, min(height - tiles_tall, start_ty - tiles_tall // 2))

    lines = [
        "PROGRAM LEVELDEMO;",
        "",
        "GLOBAL",
        "    fpg;",
        "",
        "BEGIN",
        f"    SET_MODE({screen_w}{screen_h:03d});",
        f'    fpg = LOAD_FPG("{fpg_name}");',
        "    CLEAR_SCREEN();",
        "",
    ]

    count = 0
    for ty in range(from_ty, min(from_ty + tiles_tall, height)):
        row = tilemap[ty]
        for tx in range(from_tx, min(from_tx + tiles_wide, 256)):
            v = lookup[row[tx]]
            if v == 256:
                continue
            x = (tx - from_tx) * 16 + 8
            y = (ty - from_ty) * 16 + 8
            lines.append(f"    TILE({x}, {y}, {v + 1});")
            count += 1

    lines += [
        "    REPEAT",
        "        FRAME;",
        "    UNTIL (KEY(_esc))",
        "END",
        "",
        "PROCESS TILE(X, Y, GRAPH);",
        "BEGIN",
        "    REPEAT",
        "        FRAME;",
        "    UNTIL (KEY(_esc))",
        "END",
    ]

    text = "\r\n".join(lines) + "\r\n"
    with open(out_prg_path, "wb") as f:
        f.write(text.encode("ascii"))

    print(f"{level_name}: viewport {screen_w}x{screen_h}px (tiles {from_tx}..{from_tx+tiles_wide-1} x "
          f"{from_ty}..{from_ty+tiles_tall-1}, inicio real en tile {start_tx},{start_ty}), "
          f"{count} tiles -> {out_prg_path}")


if __name__ == "__main__":
    level = sys.argv[1] if len(sys.argv) > 1 else "LEVEL1"
    out = os.path.join(os.path.dirname(__file__), "..", "src", "LEVELDEMO.PRG")
    generate(level, out, f"{level}_TILES.FPG")
