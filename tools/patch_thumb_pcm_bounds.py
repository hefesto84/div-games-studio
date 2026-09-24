#!/usr/bin/env python3
"""Parche byte-exacto (CP850) para src/div/divbrow.cpp: crear_un_thumb_PCM(),
rama "else" (ficheros grandes, filesize >= 3*an), escribia directamente
thumb[num].ptr[x+y*an]=c_g_low sin comprobar limites -- a diferencia de la
rama "if" (usa wline()/linea_pixel(), que si los comprueba). `y` sale de
"temp[p0]*al/256" con temp[] un char con signo: para una muestra por debajo
de cero, y es NEGATIVO, y x+y*an cae ANTES del buffer asignado
(malloc(an*al)) -- escritura fuera de limites real (STATUS_HEAP_CORRUPTION
al listar sonidos en el navegador de ficheros con miniaturas, ver
docs/architecture/12-port-progreso.md, hito "audio del IDE").

Fix minimo: clampar y0/y1 al rango valido [0, al-1] antes del bucle de
escritura, igual que ya hace linea_pixel() para la otra rama. No cambia el
resultado visual para las filas que ya caian dentro de rango.

Idempotente: si ya esta aplicado, no hace nada.
"""
import sys

PATH = "src/div/divbrow.cpp"

OLD = (b"          y0=y1=temp[p0]*thumb[num].al/256;\r\n"
       b"\r\n"
       b"          do\r\n"
       b"          {\r\n"
       b"            y=temp[p0]*thumb[num].al/256;\r\n"
       b"            if (y<y0) y0=y; else if (y>y1) y1=y;\r\n"
       b"            p0+=2;\r\n"
       b"          } while (p0<p1);\r\n"
       b"\r\n"
       b"          y=y0;\r\n")

NEW = (b"          y0=y1=temp[p0]*thumb[num].al/256;\r\n"
       b"\r\n"
       b"          do\r\n"
       b"          {\r\n"
       b"            y=temp[p0]*thumb[num].al/256;\r\n"
       b"            if (y<y0) y0=y; else if (y>y1) y1=y;\r\n"
       b"            p0+=2;\r\n"
       b"          } while (p0<p1);\r\n"
       b"\r\n"
       b"          if (y0<0) y0=0;\r\n"
       b"          if (y1>=thumb[num].al) y1=thumb[num].al-1;\r\n"
       b"          y=y0;\r\n")

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
