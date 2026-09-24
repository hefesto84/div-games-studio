#!/usr/bin/env python3
"""Parche byte-exacto (CP850) para src/div/divsetup.cpp: MemInfo1() sumaba
un valor en bytes (Mi_meminfo.Bloque_mas_grande_disponible, procedente de
la llamada DPMI 0500h emulada en port/div/ide/port_ide_dos.c) con `mem`
(heap libre, tambien en bytes) y dividia el total por 1024 para mostrar KB.

Ese campo DPMI es un `unsigned` de 32 bits -- en bytes, satura sobre ~4 GB,
muy por debajo de la RAM libre real en un PC moderno (ver
docs/architecture/12-port-progreso.md, hito de "Informacion del sistema").
port_ide_dos.c pasa a devolver el valor ya en KB (rango realista, ~4 TB) en
vez de bytes; esta linea deja de re-dividirlo por 1024 y en su lugar solo
convierte `mem` (heap, que si sigue en bytes) a KB antes de sumar.

Idempotente: si ya esta aplicado, no hace nada.
"""
import sys

PATH = "src/div/divsetup.cpp"

OLD = b"mem=(Mi_meminfo.Bloque_mas_grande_disponible+mem)/1024;"
NEW = b"mem=Mi_meminfo.Bloque_mas_grande_disponible+mem/1024;"

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
