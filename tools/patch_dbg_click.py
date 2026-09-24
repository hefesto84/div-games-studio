#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
patch_dbg_click.py -- TEMPORAL de diagnostico (no commitear).

Inserta logging a ide_dbg.log (cwd del IDE) en handlers de menu, para ver si
el clic llega y con que estado. Anclas byte-exactas (fuentes CP850).
Quitar con: git checkout -- src/div/divhandl.cpp
"""
import os
import sys


def patch(path, anchor, fmt, args):
    """Inserta tras `anchor`: { FILE *db=...; fprintf(db, fmt "\r\n", args); ... }"""
    ins = (
        b'\r\n  { FILE *db=fopen("ide_dbg.log","ab"); if(db){fprintf(db,"'
        + fmt + b'\\r\\n"'
        + (b"," + args if args else b"")
        + b"); fflush(db); fclose(db);} }\r\n"
    )
    with open(path, "rb") as f:
        data = f.read()
    n = data.count(anchor)
    if n != 1:
        print("ERROR %s: ancla aparece %d veces" % (path, n))
        print("  ancla:", anchor[:80])
        return False
    data = data.replace(anchor, anchor + ins)
    with open(path, "wb") as f:
        f.write(data)
    print("OK %s <- %s" % (path, fmt.decode("ascii")[:60]))
    return True


ok = True

ok &= patch(
    os.path.join("src", "div", "divhandl.cpp"),
    b"void menu_mapas2(void) {\r\n  int n,m;\r\n",
    b"mapas2 mb=%d omb=%d est=%d tipo=%d items=%d",
    b"mouse_b,old_mouse_b,v.estado,v.tipo,v.items",
)

ok &= patch(
    os.path.join("src", "div", "divhandl.cpp"),
    b"void menu_graficos2(void) {\r\n  FPG *Fpg;\r\n  int n;\r\n",
    b"graficos2 mb=%d omb=%d est=%d tipo=%d",
    b"mouse_b,old_mouse_b,v.estado,v.tipo",
)

# FPG2 (handler de la ventana FPG): la zona de la listbox y el arrastre
ok &= patch(
    os.path.join("src", "div", "divfpg.cpp"),
    b"void FPG2(void)\r\n{\r\nint x,y,n;\r\n",
    b"FPG2 mb=%d omb=%d zona=%d act=%d arrastre=%d",
    b"mouse_b,old_mouse_b,((FPG *)v.aux)->lInfoFPG.zona,v.active_item,arrastrar",
)

# FPG2: carga del grafico al arrastrar (path y exito del fopen)
ok &= patch(
    os.path.join("src", "div", "divfpg.cpp"),
    b"if ((arrastrar==3)&&(MiFPG->lInfoFPG.zona>=10))\r\n        {\r\n",
    b"FPG2 dragload file=%s",
    b"((FPG *)v.aux)->ActualFile",
)

# drop en el escritorio (div.cpp): entra en el handler de soltar
ok &= patch(
    os.path.join("src", "div", "div.cpp"),
    b"if (arrastrar==4 && (n==max_windows || ventana[n].tipo==2)) {\r\n      arrastrar=5; free_drag=0;\r\n",
    b"drop-tapiz vtipo=%d",
    b"v.tipo",
)

# menu principal: que opcion se elige
ok &= patch(
    os.path.join("src", "div", "divhandl.cpp"),
    b"void menu_principal2(void) {\r\n  actualiza_menu(750,1,0); if ((old_mouse_b&1) && !(mouse_b&1)) {\r\n    switch (v.estado) {\r\n",
    b"menu_principal est=%d",
    b"v.estado",
)

sys.exit(0 if ok else 1)
