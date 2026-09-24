#!/usr/bin/env python3
"""DIAGNOSTICO TEMPORAL, no permanente: desactiva crear_un_thumb_PCM()
(divbrow.cpp) con un return inmediato, para confirmar o descartar que es la
causa de la corrupcion de heap al listar sonidos con miniaturas en el
navegador de ficheros (bug en investigacion, ver docs/architecture/
12-port-progreso.md, hito "audio del IDE"). Revertir con
tools/revert_diag_disable_thumb_pcm.py en cuanto se tenga el diagnostico.
"""
import sys

PATH = "src/div/divbrow.cpp"

OLD = b"void crear_un_thumb_PCM(struct t_listboxbr * l)\r\n{\r\n"
NEW = b"void crear_un_thumb_PCM(struct t_listboxbr * l)\r\n{\r\nreturn; /* DIAGNOSTICO TEMPORAL */\r\n"

def main():
    with open(PATH, "rb") as f:
        data = f.read()
    if NEW in data:
        print("Ya aplicado.")
        return 0
    n = data.count(OLD)
    if n != 1:
        print(f"ERROR: {n} ocurrencias de OLD", file=sys.stderr)
        return 1
    data = data.replace(OLD, NEW)
    with open(PATH, "wb") as f:
        f.write(data)
    print("Parche de diagnostico aplicado.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
