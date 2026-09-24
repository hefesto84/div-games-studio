#!/usr/bin/env python3
"""Parche byte-exacto (CP850) para src/div/diveffec.cpp: GenExplodes()
guarda valores 0..255 (brillo del pixel) en Buff_exp, declarado "char *"
(con signo por defecto en MSVC): los valores >=128 se guardan como
negativos (wraparound de conversion int->signed char). Al leerlos luego
como indice de ExpDac[256] ("v.mapa->map[x]=ExpDac[Buff_exp[x]];"), un
valor negativo produce una lectura fuera de limites ANTES del array (en la
pila de GenExplodes(), donde vive ExpDac) -- colores practicamente
aleatorios para los pixeles mas brillantes de la explosion (bug real
reportado por el usuario tras cerrar E10, ver docs/architecture/
12-port-progreso.md).

Fix minimo: castear a "unsigned char" en el unico punto de lectura que
importa para el color (no se toca la declaracion de Buff_exp ni el resto de
comparaciones con DEEP, que son un efecto de estetica del "fade" sin
relacion con el bug de color reportado).

Idempotente: si ya esta aplicado, no hace nada.
"""
import sys

PATH = "src/div/diveffec.cpp"

OLD = b"v.mapa->map[x]=ExpDac[Buff_exp[x]];"
NEW = b"v.mapa->map[x]=ExpDac[(unsigned char)Buff_exp[x]];"

def main():
    with open(PATH, "rb") as f:
        data = f.read()
    if NEW in data:
        print("Ya aplicado, nada que hacer.")
        return 0
    n = data.count(OLD)
    if n != 1:
        print(f"ERROR: se esperaba 1 ocurrencia de OLD, encontradas {n}", file=sys.stderr)
        return 1
    data = data.replace(OLD, NEW)
    with open(PATH, "wb") as f:
        f.write(data)
    print("Parche aplicado.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
