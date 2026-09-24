# Repara la corrupcion U+FFFD de los ficheros de src/div (CP850).
#
# Sintoma: un editor no byte-safe leyo el fichero como UTF-8, no supo decodificar
# los bytes CP850 (acentos, ñ, y las lineas de separacion ═) y los reescribio
# como U+FFFD (EF BF BD). Ver 13-handoff.md §24.3.
#
# Metodo: se trabaja A NIVEL DE BYTES, nunca decodificando el fichero entero.
# Decodificarlo seria peligroso porque algunos pares de bytes CP850 (p.ej.
# C4 BF = "─┐") forman por casualidad UTF-8 valido y siguen INTACTOS en disco;
# re-codificarlos los destruiria. Aqui lo unico que se toca son las secuencias
# EF BF BD: se alinea cada linea con su original en HEAD (esqueleto ASCII, con
# todo byte >= 0x80 y toda secuencia EF BF BD reducidos a \x01) y se devuelve a
# su sitio el byte original. Todo lo demas se copia byte a byte.
#
# Las lineas nuevas de esta sesion no tienen pareja en HEAD; para esas se aplica
# la tabla MANUAL de abajo (comentarios del hito §41, escritos ya rotos).
#
# Uso:  python tools\fix_cp850.py            (solo informa)
#       python tools\fix_cp850.py --apply    (escribe)

import difflib
import subprocess
import sys

MAL = b'\xef\xbf\xbd'

# Palabras de los comentarios nuevos (§41), con sus bytes CP850 correctos:
#   a1=A0  e1=82  i1=A1  o1=A2  u1=A3  n~=A4
MANUAL = [
    (b'resoluci' + MAL + b'n', b'resoluci\xa2n'),
    (b'gr' + MAL + b'ficos', b'gr\xa0ficos'),
    (b'est' + MAL + b'ndar', b'est\xa0ndar'),
    (b'tama' + MAL + b'o', b'tama\xa4o'),
    (b'm' + MAL + b'scara', b'm\xa0scara'),
    (b'f' + MAL + b'sico', b'f\xa1sico'),
    (b'l' + MAL + b'gica', b'l\xa2gica'),
    (b'p' + MAL + b'xel', b'p\xa1xel'),
    (b'ser' + MAL + b'n', b'ser\xa0n'),
    (b'm' + MAL + b's', b'm\xa0s'),
]

SEPARADOR = b'\xcd'   # '═', el que usan las cabeceras de bloque de estos .cpp


def esqueleto(linea):
    s = linea.replace(MAL, b'\x01')
    return bytes(b if b < 0x80 else 0x01 for b in s)


def arregla_linea(w, h):
    """Devuelve w con cada EF BF BD sustituido por el byte que toca de h."""
    out = bytearray()
    i = j = 0
    while i < len(w):
        if w[i:i + 3] == MAL:
            out.append(h[j])
            i += 3
        else:
            out.append(w[i])
            i += 1
        j += 1
    return bytes(out)


def manual(linea):
    for viejo, nuevo in MANUAL:
        linea = linea.replace(viejo, nuevo)
    # separador de bloque: '//' seguido solo de marcas rotas
    cuerpo = linea.lstrip()
    if cuerpo.startswith(b'//'):
        resto = cuerpo[2:]
        if resto and resto.replace(MAL, b'') == b'':
            linea = linea.replace(MAL, SEPARADOR)
    return linea


def repara(ruta):
    crudo = open(ruta, 'rb').read()
    if MAL not in crudo:
        return None

    fin = b'\r\n' if b'\r\n' in crudo else b'\n'
    w = crudo.split(fin)
    h = subprocess.run(['git', 'show', 'HEAD:' + ruta],
                       capture_output=True).stdout.replace(b'\r\n', b'\n').split(b'\n')

    emparejada = {}
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(
            None, [esqueleto(x) for x in h], [esqueleto(x) for x in w]).get_opcodes():
        if op == 'equal':
            for k in range(i2 - i1):
                emparejada[j1 + k] = i1 + k

    de_head = a_mano = 0
    pendientes = []
    for idx, ln in enumerate(w):
        if MAL not in ln:
            continue
        src = emparejada.get(idx)
        if src is not None and esqueleto(h[src]) == esqueleto(ln):
            w[idx] = arregla_linea(ln, h[src])
            de_head += 1
            continue
        nueva = manual(ln)
        if MAL in nueva:
            pendientes.append((idx + 1, ln))
        else:
            w[idx] = nueva
            a_mano += 1

    return fin.join(w), de_head, a_mano, pendientes


def main():
    aplicar = '--apply' in sys.argv
    ficheros = subprocess.run(['git', 'diff', '--name-only', '--', 'src/div/'],
                              capture_output=True, text=True).stdout.split()
    tot_head = tot_mano = escritos = 0
    pend_todas = []

    for f in ficheros:
        r = repara(f)
        if r is None:
            continue
        datos, de_head, a_mano, pend = r
        tot_head += de_head
        tot_mano += a_mano
        pend_todas += [(f, n, ln) for n, ln in pend]
        print('%-28s desde_HEAD=%-5d a_mano=%-3d sin_resolver=%d'
              % (f, de_head, a_mano, len(pend)))
        if aplicar and not pend:
            open(f, 'wb').write(datos)
            escritos += 1

    print()
    print('lineas restauradas desde HEAD: %d' % tot_head)
    print('lineas de comentario nuevas arregladas a mano: %d' % tot_mano)
    print('ficheros escritos: %d' % escritos)
    if pend_todas:
        print('SIN RESOLVER (no se escribe ese fichero): %d' % len(pend_todas))
        for f, n, ln in pend_todas:
            print('   %s:%d  %s' % (f, n, ln.replace(MAL, b'<?>').decode('ascii', 'replace').strip()[:110]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
