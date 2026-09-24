# -*- coding: cp850 -*-
"""Genera prototipos para las funciones de nivel superior de los .cpp de src/div.

Los fuentes originales del IDE se apoyaban en que Watcom C++ aceptaba llamar a
funciones definidas mas abajo en el mismo fichero sin prototipo previo. Al
compilarlos como C (/TC) eso produce declaraciones implicitas `int f()` y luego
C2371 al llegar a la definicion real. En vez de tocar src/, se genera un header
con los prototipos y se incluye desde port/div/ide/global.h.

Uso: python tools/gen_protos.py divpaint.cpp divpalet.cpp > salida.h
"""
import re
import sys

# Definicion de funcion en columna 0: "tipo nombre(args) {" (la llave puede ir
# en la linea siguiente).
DEF = re.compile(
    r'^(?=\S)((?:(?:static|extern|unsigned|signed|const|struct|union|void|int|'
    r'char|long|short|float|double|byte|word|dword|far|__far|\*|[ \t])+?))'
    r'(\w+)[ \t]*\(([^;{}]*)\)\s*\{', re.M)

SKIP = {'main', 'if', 'for', 'while', 'switch', 'do', 'else', 'return',
        # divpaint.cpp define una funcion zoom_porcion() y divbasic.cpp una
        # variable int con el mismo nombre; declararla rompe divbasic.cpp.
        'zoom_porcion',
        # divsetup.cpp: GetFreeMem usa el typedef local meminfo (definido en ese
        # .cpp), que no existe en el ambito de global.h; la unica llamada ocurre
        # despues de la definicion, asi que el prototipo no hace falta.
        'GetFreeMem'}


def strip_comments(src):
    """Quita comentarios en un solo pase, respetando cadenas y caracteres.

    Hay que hacerlo asi (y no con dos regex encadenadas): los fuentes tienen
    lineas `//` que contienen `/*`, y quitar primero los bloques se comia
    trozos enteros de codigo real.
    """
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            while i < n and src[i] != '\n':
                i += 1
        elif c == '/' and i + 1 < n and src[i + 1] == '*':
            i += 2
            while i + 1 < n and not (src[i] == '*' and src[i + 1] == '/'):
                i += 1
            i += 2
            out.append(' ')
        elif c in '"\'':
            q = c
            out.append(c)
            i += 1
            while i < n and src[i] != q:
                if src[i] == '\\' and i + 1 < n:
                    out.append(src[i])
                    i += 1
                out.append(src[i])
                i += 1
            out.append(q)
            i += 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def protos(path):
    src = strip_comments(open(path, 'rb').read().decode('cp850'))
    out = []
    for m in DEF.finditer(src):
        ret, name, args = m.group(1), m.group(2), m.group(3)
        if name in SKIP or not ret.strip():
            continue
        ret = ' '.join(ret.split())
        args = ' '.join(args.split()) or 'void'
        out.append('%s %s(%s);' % (ret, name, args))
    return out


def main():
    print('/* Generado por tools/gen_protos.py -- NO editar a mano. */')
    print('#ifndef PORT_IDE_PROTOS_H')
    print('#define PORT_IDE_PROTOS_H')
    # Los prototipos de divfpg.cpp/fpgfile.cpp usan los tipos FPG/HeadFPG
    # (E6a); este header los define. Se incluye aqui porque el resto de
    # tipos que necesita (byte, t_listboxbr, t_thumb...) ya estan definidos
    # en global.h para cuando se incluye port_ide_protos.h.
    print('')
    print('#include "fpgfile.hpp"')
    for f in sys.argv[1:]:
        print('\n/* ---- %s ---- */' % f.replace('\\', '/').split('/')[-1])
        seen = set()
        for p in protos(f):
            if p in seen:
                continue
            seen.add(p)
            print(p)
    print('\n#endif')


main()
