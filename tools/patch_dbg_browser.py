#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
patch_dbg_browser.py -- TEMPORAL de diagnostico (no commitear).

Instrumenta el flujo de "Mapas -> Abrir mapa..." y la navegacion del browser
para cazar el bug de peteo al subir de directorio (13-handoff.md, bug abierto 1).
Anclas byte-exactas (fuentes CP850). Revertir restaurando la copia de
seguridad que haga el harness (NO usar git checkout, pisaria otros cambios).
"""
import os
import sys


def patch(path, anchor, fmt, args):
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

# menu_mapas2: transiciones de boton, para ver si el click de "Abrir mapa..."
# llega y con que v.estado
ok &= patch(
    os.path.join("src", "div", "divhandl.cpp"),
    b"void menu_mapas2(void) {\r\n  int n,m;\r\n",
    b"mapas2 mb=%d omb=%d est=%d",
    b"mouse_b,old_mouse_b,v.estado",
)

# browser0: entrada al dialogo, v_tipo y el path de partida
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"void browser0(void) {\r\n  unsigned n,m,x;\r\n",
    b"browser0 v_tipo=%d path=%s",
    b"v_tipo,tipo[v_tipo].path",
)

# navegacion "subir/entrar" de directorio: el nombre elegido y el path final
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"} else if (ldirectoriosbr.zona>=10) { v.volcar=1;\r\n"
    b"      if (tipo[v_tipo].path[strlen(tipo[v_tipo].path)-1]!='\\\\')\r\n"
    b"        strcat(tipo[v_tipo].path,\"\\\\\");\r\n"
    b"      strcat(tipo[v_tipo].path,directorio+(ldirectoriosbr.zona-10+\r\n"
    b"        ldirectoriosbr.inicial)*an_directorio);\r\n",
    b"nav-dir zona=%d inicial=%d sel=%s path_pre=%s",
    b"ldirectoriosbr.zona,ldirectoriosbr.inicial,"
    b"directorio+(ldirectoriosbr.zona-10+ldirectoriosbr.inicial)*an_directorio,"
    b"tipo[v_tipo].path",
)

# granularidad fina: un checkpoint despues de cada paso, para acotar
# exactamente donde revienta chdir/getcwd/imprime_rutabr/dir_abrirbr.
# Cada ancla es un tramo CONSECUTIVO y unico (verificado con count==1);
# no se reutiliza texto entre anclas para que insertar una no rompa la
# siguiente.
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"      chdir(tipo[v_tipo].path);\r\n"
    b"      getcwd(tipo[v_tipo].path,PATH_MAX+1);\r\n",
    b"nav-dir 1-tras-chdir-getcwd path=%s",
    b"tipo[v_tipo].path",
)
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"      imprime_rutabr();\r\n"
    b"      larchivosbr.creada=0;\r\n"
    b"      ldirectoriosbr.creada=0;\r\n"
    b"      tipo[v_tipo].inicial=0;\r\n",
    b"nav-dir 2-tras-imprime_rutabr",
    b"",
)
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"      dir_abrirbr();\r\n"
    b"\r\n"
    b"      crear_listboxbr(&larchivosbr);\r\n"
    b"      crear_listbox(&ldirectoriosbr);\r\n",
    b"nav-dir 3-tras-dir_abrirbr-y-listbox",
    b"",
)

# dir_abrirbr: contadores de archivos/directorios encontrados
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"  } larchivosbr.maximo=n;\r\n"
    b"  qsort(archivo,larchivosbr.maximo,an_archivo,strcmp);\r\n"
    b"\r\n"
    b"  n=0; m=_dos_findfirst(\"*.*\",_A_SUBDIR,&fileinfo);\r\n",
    b"dir_abrirbr archivos=%d",
    b"larchivosbr.maximo",
)
ok &= patch(
    os.path.join("src", "div", "divbrow.cpp"),
    b"  } ldirectoriosbr.maximo=n;\r\n"
    b"  qsort(directorio,ldirectoriosbr.maximo,an_directorio,strcmp);\r\n",
    b"dir_abrirbr directorios=%d",
    b"ldirectoriosbr.maximo",
)

# lista de unidades (drive letters) -- determina_unidades(): resultado final
# (ancla SIN la '}' de cierre de funcion, para no insertar a ambito de fichero)
ok &= patch(
    os.path.join("src", "div", "div.cpp"),
    b"  } while (++n<=26);\r\n"
    b"  unidades[uni]=0;\r\n",
    b"determina_unidades resultado=[%s] uni=%d",
    b"unidades,uni",
)

sys.exit(0 if ok else 1)
