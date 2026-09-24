#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
patch_e6a_divforma.py -- Hito E6a del port del IDE.

Deja divforma.cpp compilable en el port (PORT_IDE):
 1. jpeglib fuera (JPG queda stub de momento): includes + es_JPG +
    descomprime_JPG + cargadac_JPG se excluyen con #ifndef PORT_IDE.
    Motivo: jpeglib 6.x define 'boolean' como enum (int) y choca con el
    'unsigned char boolean' de rpcndr.h (windows.h entra por force-include);
    y jconfig.h es el de Watcom/DOS. Se aborda aparte si hace falta JPG.
 2. RGBQUAD / BITMAPFILEHEADER / BITMAPINFOHEADER: DIV las redefine y chocan
    con wingdi.h. Las de Windows son identicas campo a campo, y divforma las
    usa solo leyendo campos sueltos o con memcpy de 40 bytes (sin padding en
    ninguno de los dos compiladores), asi que se excluyen las de DIV bajo
    PORT_IDE y se usan las del sistema.

CP850 + CRLF: solo parches byte-exactos (regla 12-port-progreso.md 24.3).
Idempotente.
"""
import os
import sys

PATH = os.path.join("src", "div", "divforma.cpp")
GUARD_B = b"#ifndef PORT_IDE\r\n"
GUARD_E = b"#endif\r\n"

# (descripcion, ancla_inicio, ancla_fin_o_None)
# Si ancla_fin es None: solo se inserta GUARD_B antes del ancla (para pares
# donde el cierre se hace con otra regla de la lista).
RULES = [
    (
        "includes jpeglib",
        b'  #include "jpeglib/jpeglib.h"\r\n  #include "jpeglib/cdjpeg.h"\r\n',
        None,  # se envuelve el propio bloque
        b'  /* PORT (E6a): jpeglib fuera del build del IDE de momento; JPG queda\r\n'
        b'   * en stub (port_ide_stubs.c). El include arrastra el choque de\r\n'
        b'   * "boolean" con rpcndr.h y el jconfig.h es de Watcom/DOS. */\r\n'
        b'#ifndef PORT_IDE\r\n'
        b'  #include "jpeglib/jpeglib.h"\r\n  #include "jpeglib/cdjpeg.h"\r\n'
        b'#endif\r\n',
    ),
    (
        "RGBQUAD",
        b"typedef struct tagRGBQUAD\r\n{",
        b"} RGBQUAD;\r\n",
        None,
    ),
    (
        "BITMAPFILEHEADER",
        b"typedef struct tagBITMAPFILEHEADER\r\n{",
        b"} BITMAPFILEHEADER;\r\n",
        None,
    ),
    (
        "BITMAPINFOHEADER",
        b"typedef struct tagBITMAPINFOHEADER\r\n{",
        b"} BITMAPINFOHEADER;\r\n",
        None,
    ),
    (
        "es_JPG + descomprime_JPG",
        b"int es_JPG(byte *buffer, int img_filesize)\r\n{",
        b"  return(1);\r\n}\r\n\r\n//" + b"\xcd" * 8,
        None,
    ),
    (
        "cargadac_JPG",
        b"int cargadac_JPG(char *name)\r\n{",
        b"  jpeg_destroy_decompress(&cinfo);\r\n  free(buffer);\r\n\r\n  return(1);\r\n}\r\n",
        None,
    ),
]


def main():
    with open(PATH, "rb") as f:
        data = f.read()

    if data.count(b"#ifndef PORT_IDE") >= 6:
        print("SKIP: ya parcheado")
        return 0

    for desc, start, end, custom in RULES:
        n = data.count(start)
        if n != 1:
            print("ERROR: '%s' ancla inicio aparece %d veces" % (desc, n))
            return 1
        if custom is not None:
            data = data.replace(start, custom)
            print("OK: %s (bloque reemplazado)" % desc)
            continue
        data = data.replace(start, GUARD_B + start)
        if end is not None:
            m = data.count(end)
            if m != 1:
                print("ERROR: '%s' ancla fin aparece %d veces" % (desc, m))
                return 1
            data = data.replace(end, end + GUARD_E)
        print("OK: %s" % desc)

    with open(PATH, "wb") as f:
        f.write(data)
    print("divforma.cpp parcheado")
    return 0


if __name__ == "__main__":
    sys.exit(main())
