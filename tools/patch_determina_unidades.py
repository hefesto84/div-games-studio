# Parche byte-exacto (idempotente) sobre src/div/div.cpp.
#
# determina_unidades() (menu Ficheros/Mapas -> lista de unidades <X:> del
# browser) usaba INT 21h AX=440Eh via intdos() para enumerar las unidades de
# disco. El shim intdos() de este port (port_ide_pre.h) es un no-op literal
# (`#define intdos(inregs,outregs) 0`) que no toca `r`: el bucle leia
# `r.w.cflag`/`r.h.al` sin inicializar (warning C4700 del propio compilador),
# y en la practica terminaba SIEMPRE con una unica letra bogus en unidades[]
# (ej. "N:", segun el bit de basura de la pila), no las unidades reales del
# equipo. Se sustituye por GetLogicalDrives() (Win32), el equivalente real y
# determinista en Windows.
#
# El fichero es CP850: se parchea en binario, nunca con un editor de texto.
#
# Uso:  python tools\patch_determina_unidades.py

import sys

RUTA = "src/div/div.cpp"

VIEJO = (
    b"void determina_unidades(void) {\r\n"
    b"  int n,m,uni=0;\r\n"
    b"  union REGS r;\r\n"
    b"\r\n"
    b"  n=1; do {\r\n"
    b"    r.h.bl=n; r.w.ax=0x440e; intdos(&r,&r);\r\n"
    b"    if (r.w.cflag&INTR_CF)\r\n"
    b"      if (r.h.al==0xf) continue;\r\n"
    b"      else unidades[uni++]='A'+n-1;\r\n"
    b"    else if (r.h.al==0) unidades[uni++]='A'+n-1;\r\n"
    b"    else unidades[uni++]='A'+r.h.al-1;\r\n"
    b"    if (uni>1) for (m=0;m<uni-1;m++)\r\n"
    b"      if (unidades[m]==unidades[uni-1]) uni--;\r\n"
    b"  } while (++n<=26);\r\n"
    b"  unidades[uni]=0;\r\n"
    b"}\r\n"
)

NUEVO = (
    b"void determina_unidades(void) {\r\n"
    b"  /* PORT: el original enumeraba unidades con INT 21h AX=440Eh via\r\n"
    b"   * intdos(); el shim intdos() de este port es un no-op literal que no\r\n"
    b"   * toca `r` (port_ide_pre.h), asi que el bucle original conducia por\r\n"
    b"   * memoria de pila sin inicializar y unidades[] quedaba con basura\r\n"
    b"   * (una unica letra bogus repetida, no las unidades reales). Se\r\n"
    b"   * sustituye por GetLogicalDrives(), el equivalente real en Win32. */\r\n"
    b"  unsigned long mapa=GetLogicalDrives();\r\n"
    b"  int n,uni=0;\r\n"
    b"  for (n=0;n<26;n++) if (mapa&(1UL<<n)) unidades[uni++]=(char)('A'+n);\r\n"
    b"  unidades[uni]=0;\r\n"
    b"}\r\n"
)


def main():
    datos = open(RUTA, "rb").read()

    if b"GetLogicalDrives" in datos:
        print("ya parcheado, no se toca nada")
        return 0

    n = datos.count(VIEJO)
    if n != 1:
        print("ERROR: %d ocurrencias del cuerpo original (se esperaba 1)" % n)
        return 1

    datos = datos.replace(VIEJO, NUEVO)
    open(RUTA, "wb").write(datos)
    print("parcheado %s (%d bytes)" % (RUTA, len(datos)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
