# Parche byte-exacto (idempotente) sobre src/div/divdsktp.cpp.
#
# Arregla el desbordamiento al restaurar una sesion (system\session.dtf):
# nueva_ventana_carga() reserva el buffer de la ventana con la geometria
# recien calculada y DESPUES sobrescribe v.an/v.al con la guardada en la
# sesion. Si la sesion viene de un build con otra escala de letra (big2),
# esos an/al son mayores que el bloque reservado y cada volcado posterior
# (wvolcado desde actualiza_caja) lee fuera -> 0xC0000005.
#
# El fichero es CP850: se parchea en binario, nunca con un editor de texto
# (ver 13-handoff.md, incidente U+FFFD).
#
# Uso:  python tools\patch_session_geom.py

import sys

RUTA = "src/div/divdsktp.cpp"

PARCHES = [
    # 1) variable nueva con el tamano realmente reservado
    (b"  int n,m,x,y,an,al;\r\n  int vtipo;",
     b"  int n,m,x,y,an,al;\r\n  int vtipo;\r\n"
     b"  long lon_ptr; /* PORT: bytes realmente reservados para v.ptr */"),

    # 2) recordarlo en el momento del malloc
    (b"    if ((ptr=malloc(an*al))!=NULL)",
     b"    lon_ptr=(long)an*al;\r\n"
     b"    if ((ptr=malloc(lon_ptr))!=NULL)"),

    # 3) usar el mismo tamano al limpiar el buffer
    (b"      memset(ptr,c0,an*al);",
     b"      memset(ptr,c0,lon_ptr);"),

    # 4) aceptar la geometria de la sesion solo si cabe en ese buffer
    (b"        v.an=ventana_aux.an;\r\n"
     b"        v.al=ventana_aux.al;\r\n"
     b"        v._x=ventana_aux._x;\r\n"
     b"        v._y=ventana_aux._y;\r\n"
     b"        v._an=ventana_aux._an;\r\n"
     b"        v._al=ventana_aux._al;\r\n",
     b"        /* PORT: la geometria guardada en session.dtf solo se acepta si\r\n"
     b"         * cabe en el buffer que se acaba de reservar. Una sesion escrita\r\n"
     b"         * por un build con otra escala de letra (big2) trae an/al mayores\r\n"
     b"         * que los recalculados ahora, y entonces cada volcado posterior\r\n"
     b"         * (wvolcado desde actualiza_caja) lee fuera del bloque. */\r\n"
     b"        if (ventana_aux.an>0 && ventana_aux.al>0 &&\r\n"
     b"            (long)ventana_aux.an*ventana_aux.al<=lon_ptr &&\r\n"
     b"            (long)ventana_aux._an*ventana_aux._al<=lon_ptr) {\r\n"
     b"          v.an=ventana_aux.an;\r\n"
     b"          v.al=ventana_aux.al;\r\n"
     b"          v._an=ventana_aux._an;\r\n"
     b"          v._al=ventana_aux._al;\r\n"
     b"        }\r\n"
     b"        v._x=ventana_aux._x;\r\n"
     b"        v._y=ventana_aux._y;\r\n"),
]


def main():
    datos = open(RUTA, "rb").read()

    if b"lon_ptr" in datos:
        print("ya parcheado, no se toca nada")
        return 0

    for viejo, nuevo in PARCHES:
        n = datos.count(viejo)
        if n != 1:
            print("ERROR: %d ocurrencias de %r (se esperaba 1)" % (n, viejo[:48]))
            return 1
        datos = datos.replace(viejo, nuevo)

    open(RUTA, "wb").write(datos)
    print("parcheado %s (%d bytes)" % (RUTA, len(datos)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
