#!/usr/bin/env python3
"""DIAGNOSTICO TEMPORAL: traza por stderr justo despues de judas_playsample()
en PCM2() (divpcm.cpp, case 8: Play), para confirmar si el cuelgue tras
reproducir un PCM esta dentro de esa llamada o justo al volver de ella. Ver
docs/architecture/12-port-progreso.md, hito "audio del IDE"."""
import sys

PATH = "src/div/divpcm.cpp"

OLD = b"      judas_playsample(&sample, 0, mypcminfo->SoundFreq, 64*256, MIDDLE);\r\n      return;\r\n"
NEW = (b"      judas_playsample(&sample, 0, mypcminfo->SoundFreq, 64*256, MIDDLE);\r\n"
       b"      fprintf(stderr, \"[PCM2] tras judas_playsample\\n\"); fflush(stderr);\r\n"
       b"      return;\r\n")

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
