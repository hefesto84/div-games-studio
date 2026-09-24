# Parche byte-exacto (idempotente) sobre src/div/divbrow.cpp.
#
# Arregla el crash del browser (bug abierto 1 de 13-handoff.md) al navegar a
# un directorio con ficheros SIN extension (LICENSE, makefile, Vagrantfile...
# la raiz del propio repo DIV es un caso real). El patron repetido 16 veces
# en este fichero es:
#
#   strcmp(strupr(strchr(l->lista+(l->lista_an*num),'.')), ".EXT")
#
# strchr(...,'.') devuelve NULL si el nombre no tiene punto. strupr(NULL) no
# es un caso que Watcom comprobara (su CRT de DOS simplemente no crasheaba),
# pero la UCRT de MSVC valida el parametro de las funciones "inseguras" y
# aborta el proceso con __fastfail (excepcion 0xC0000409, la misma que un
# fallo de cookie /GS -- por eso parecia un desbordamiento de pila) en vez de
# devolver NULL o vacio.
#
# El fichero es CP850: se parchea en binario, nunca con un editor de texto.
#
# Uso:  python tools\patch_browser_ext_crash.py

import sys

RUTA = "src/div/divbrow.cpp"

ANCLA_INSERCION = b"void crear_un_thumb_MAP(struct t_listboxbr * l){\r\n"

HELPER = (
    b"/* PORT: strchr(nombre,'.') devuelve NULL si el fichero no tiene\r\n"
    b" * extension (LICENSE, makefile, Vagrantfile...); strupr(NULL) hace\r\n"
    b" * abortar el proceso en la UCRT de MSVC (fastfail 0xC0000409), a\r\n"
    b" * diferencia del CRT de Watcom/DOS original. Mismo comportamiento que\r\n"
    b" * el original para nombres CON extension; sin extension se compara\r\n"
    b" * contra una cadena vacia, que nunca coincide con ninguna extension\r\n"
    b" * real (tratado igual que un tipo de fichero desconocido). */\r\n"
    b"char * mayus_extension(char * nombre) {\r\n"
    b"  char * p = strchr(nombre,'.');\r\n"
    b"  return p ? strupr(p) : \"\";\r\n"
    b"}\r\n"
)

PATRON_VIEJO = b"strupr(strchr(l->lista+(l->lista_an*num),'.'))"
PATRON_NUEVO = b"mayus_extension(l->lista+(l->lista_an*num))"


def main():
    datos = open(RUTA, "rb").read()

    if b"mayus_extension" in datos:
        print("ya parcheado, no se toca nada")
        return 0

    n_ancla = datos.count(ANCLA_INSERCION)
    if n_ancla != 1:
        print("ERROR: ancla de insercion aparece %d veces (se esperaba 1)" % n_ancla)
        return 1

    n_patron = datos.count(PATRON_VIEJO)
    if n_patron < 1:
        print("ERROR: no se encontro el patron a reemplazar")
        return 1

    datos = datos.replace(ANCLA_INSERCION, HELPER + ANCLA_INSERCION)
    datos = datos.replace(PATRON_VIEJO, PATRON_NUEVO)

    open(RUTA, "wb").write(datos)
    print("parcheado %s: 1 helper insertado, %d llamadas reemplazadas"
          % (RUTA, n_patron))
    return 0


if __name__ == "__main__":
    sys.exit(main())
