#!/usr/bin/env python3
"""Genera un .PRG con scroll DE VERDAD (START_SCROLL nativo de DIV) y
colision jugador-terreno de verdad: gravedad, salto, y bloqueo de
movimiento contra la capa de colision (COD 2) generada por
level_combined_fpg.py.

Patron de scroll tomado de build/Release/ZELDA.PRG (unico ejemplo real de
scroll en el repo): START_SCROLL(snum,fichero,graf1,graf2,region,flags); el
proceso que hace de camara necesita CTYPE=C_SCROLL y SCROLL.CAMERA=ID.

Colision: MAP_GET_PIXEL(fpg,2,x,y) lee la capa de colision a resolucion de
pixel (1 = solido, 0 = libre) -- se comprueban las dos esquinas del lado
que avanza (arriba/abajo del lado que se mueve en horizontal, izq/dcha del
lado que se mueve en vertical) ANTES de aplicar el movimiento, para no
atravesar paredes/suelo. Caja de colision fija (hw/hh de PLAYER_HALF_W/H),
no ligada al tamano real del sprite todavia.

Animacion: identificada a ojo mirando una hoja de contacto de los primeros
60 sprites (assets/original/sprites_extracted/001..060.gif) -- no hay
ninguna tabla oficial de que sprite es cada pose, es lectura visual directa.
Sprites 6,7,8,10 = ciclo de carrera mirando a la derecha; 12 = salto/caida;
1 = pose de pie, hace de "quieto" (no hay un frame de reposo claro entre
los 60 primeros). Para mirar a la izquierda NO se usan otros sprites
dibujados a mano (se probo con 35-38 y salio fatal -- resultaron ser una
mezcla de reposo/carrera/un frame roto, no un ciclo coherente); en vez de
eso, level_combined_fpg.py genera copias volteadas horizontalmente de estos
mismos sprites en COD MIRROR_COD_BASE+n, garantizando la pose exacta en
ambas direcciones.

IMPORTANTE: CRLF directo (ver feedback_div_compiler_crlf en memoria).
"""
import json
import os
import sys

PARSED_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "original", "levels_parsed")

SCREEN_W, SCREEN_H = 320, 200
CHAR_COD_BASE = 500    # ver level_combined_fpg.py
MIRROR_COD_BASE = 970  # ver level_combined_fpg.py (¡no 700! colisiona con codigos de sprite reales 501-960)

MOVE_SPEED = 2
GRAVITY = 1
MAX_FALL = 10
JUMP_VY = -12
PLAYER_HALF_W = 8
PLAYER_HALF_H = 16

WALK_SPRITES = [6, 7, 8, 10]  # mismos sprites para las dos direcciones, ver nota de mas arriba
JUMP_SPRITE = 12
IDLE_SPRITE = 1
ANIM_SPEED = 6  # frames de juego por pose


def generate(level_name, out_prg_path, fpg_name):
    parsed = json.load(open(os.path.join(PARSED_DIR, f"{level_name}.json")))
    height = parsed["height"]
    width_px, height_px = 256 * 16, height * 16
    start_x, start_y = parsed["start_x"], parsed["start_y"]

    def cod(n):
        return CHAR_COD_BASE + n

    def codm(n):
        return MIRROR_COD_BASE + n

    hw, hh = PLAYER_HALF_W, PLAYER_HALF_H
    x_min, x_max = hw, width_px - 1 - hw
    y_min, y_max = hh, height_px - 1 - hh

    lines = [
        "PROGRAM SCROLLDEMO;",
        "",
        "GLOBAL",
        "    fpg;",
        "",
        "BEGIN",
        f"    SET_MODE({SCREEN_W}{SCREEN_H:03d});",
        f'    fpg = LOAD_FPG("{fpg_name}");',
        f"    DEFINE_REGION(0, 0, 0, {SCREEN_W}, {SCREEN_H});",
        "    START_SCROLL(0, fpg, 1, 0, 0, 0);",
        "",
        f"    PLAYER({start_x}, {start_y});",
        "",
        "    REPEAT",
        "        FRAME;",
        "    UNTIL (KEY(_esc))",
        "END",
        "",
        "PROCESS PLAYER(X, Y);",
        "PRIVATE",
        "    vx; vy; on_ground; facing; anim_timer; anim_frame;",
        "",
        "BEGIN",
        "    CTYPE = C_SCROLL;",
        "    SCROLL.CAMERA = ID;",
        f"    GRAPH = {cod(IDLE_SPRITE)};",
        f"    Y = Y - {hh};",  # start_x/start_y del nivel son la posicion de los PIES, no el centro
        "    vx = 0; vy = 0; on_ground = 0; facing = 1; anim_timer = 0; anim_frame = 0;",
        "",
        "    REPEAT",
        "        vx = 0;",
        f"        IF (KEY(_LEFT)) vx = -{MOVE_SPEED}; facing = -1; END",
        f"        IF (KEY(_RIGHT)) vx = {MOVE_SPEED}; facing = 1; END",
        "",
        f"        vy = vy + {GRAVITY};",
        f"        IF (vy > {MAX_FALL}) vy = {MAX_FALL}; END",
        "        IF (on_ground)",
        f"            IF (KEY(_UP)) vy = {JUMP_VY}; END",
        "        END",
        "",
        "        // colision horizontal (esquinas del lado que avanza)",
        "        IF (vx < 0)",
        f"            IF (MAP_GET_PIXEL(fpg, 2, X - {hw} + vx, Y - {hh} + 2) == 1) vx = 0;",
        "            ELSE",
        f"                IF (MAP_GET_PIXEL(fpg, 2, X - {hw} + vx, Y + {hh} - 2) == 1) vx = 0; END",
        "            END",
        "        END",
        "        IF (vx > 0)",
        f"            IF (MAP_GET_PIXEL(fpg, 2, X + {hw} + vx, Y - {hh} + 2) == 1) vx = 0;",
        "            ELSE",
        f"                IF (MAP_GET_PIXEL(fpg, 2, X + {hw} + vx, Y + {hh} - 2) == 1) vx = 0; END",
        "            END",
        "        END",
        "        X = X + vx;",
        "",
        "        // colision vertical (esquinas del lado que avanza)",
        "        on_ground = 0;",
        "        IF (vy > 0)",
        f"            IF (MAP_GET_PIXEL(fpg, 2, X - {hw} + 2, Y + {hh} + vy) == 1)",
        "                vy = 0; on_ground = 1;",
        "            ELSE",
        f"                IF (MAP_GET_PIXEL(fpg, 2, X + {hw} - 2, Y + {hh} + vy) == 1)",
        "                    vy = 0; on_ground = 1;",
        "                END",
        "            END",
        "        END",
        "        IF (vy < 0)",
        f"            IF (MAP_GET_PIXEL(fpg, 2, X - {hw} + 2, Y - {hh} + vy) == 1) vy = 0;",
        "            ELSE",
        f"                IF (MAP_GET_PIXEL(fpg, 2, X + {hw} - 2, Y - {hh} + vy) == 1) vy = 0; END",
        "            END",
        "        END",
        "        Y = Y + vy;",
        "",
        f"        IF (X < {x_min}) X = {x_min}; END",
        f"        IF (X > {x_max}) X = {x_max}; END",
        f"        IF (Y < {y_min}) Y = {y_min}; END",
        f"        IF (Y > {y_max}) Y = {y_max}; END",
        "",
        "        // animacion: quieto / ciclo de carrera / salto, segun estado y direccion",
        "        // (mismos sprites en ambas direcciones; a la izquierda se usa la version",
        "        // volteada horizontalmente que genera level_combined_fpg.py)",
        "        IF (on_ground)",
        "            IF (vx == 0)",
        "                anim_timer = 0; anim_frame = 0;",
        f"                IF (facing == 1) GRAPH = {cod(IDLE_SPRITE)}; ELSE GRAPH = {codm(IDLE_SPRITE)}; END",
        "            ELSE",
        "                anim_timer = anim_timer + 1;",
        f"                IF (anim_timer >= {ANIM_SPEED})",
        "                    anim_timer = 0;",
        "                    anim_frame = anim_frame + 1;",
        f"                    IF (anim_frame >= {len(WALK_SPRITES)}) anim_frame = 0; END",
        "                END",
        "                IF (facing == 1)",
    ] + [
        f"                    IF (anim_frame == {i}) GRAPH = {cod(s)}; END"
        for i, s in enumerate(WALK_SPRITES)
    ] + [
        "                ELSE",
    ] + [
        f"                    IF (anim_frame == {i}) GRAPH = {codm(s)}; END"
        for i, s in enumerate(WALK_SPRITES)
    ] + [
        "                END",
        "            END",
        "        ELSE",
        f"            IF (facing == 1) GRAPH = {cod(JUMP_SPRITE)}; ELSE GRAPH = {codm(JUMP_SPRITE)}; END",
        "        END",
        "",
        "        FRAME;",
        "    UNTIL (KEY(_esc))",
        "END",
    ]

    text = "\r\n".join(lines) + "\r\n"
    with open(out_prg_path, "wb") as f:
        f.write(text.encode("ascii"))

    print(f"{level_name}: nivel {width_px}x{height_px}px, ventana {SCREEN_W}x{SCREEN_H}, "
          f"inicio ({start_x},{start_y}), caja jugador {hw*2}x{hh*2}px -> {out_prg_path}")


if __name__ == "__main__":
    level = sys.argv[1] if len(sys.argv) > 1 else "LEVEL1"
    out = os.path.join(os.path.dirname(__file__), "..", "src", "SCROLLDEMO.PRG")
    generate(level, out, f"{level}_ALL.FPG")
