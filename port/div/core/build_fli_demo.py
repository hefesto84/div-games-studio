#!/usr/bin/env python3
# Prueba manual de bytecode DIV para el decodificador FLI/FLC nuevo
# (port_topflc.c, ver docs/architecture/12-port-progreso.md). Reproduce un
# .FLI real de principio a fin:
#
#   n = start_fli("intro.fli", 0, 0);   # numero de frames devuelto
#   repetir n veces:
#     frame_fli();                      # decodifica el siguiente frame en copia2
#     frame;                            # cede el frame -> copia2 se copia a copia y se presenta
#   end_fli();
#   loop frame; end   # se queda con el ultimo frame visible unos segundos
#
# start_fli() dibuja directamente en copia2 (el buffer de fondo, ver
# DIV_export("background",...) en i.cpp) -- el pipeline normal de frame_end()
# ya copia copia2 a copia cada frame, asi que no hace falta scroll ni nada
# mas para que se vea.
import struct, sys, zlib

LCAR = 1
LASI = 2
LAID = 20
LJMP = 23
LJPF = 24
LFUN = 25
LASP = 28
LFRM = 29
LRET = 27

FUNC_START_FLI = 43
FUNC_FRAME_FLI = 44
FUNC_END_FLI = 45

long_header = 9
system_globals_words = 1520 + 68
code_start = long_header + system_globals_words

# Nombre del fichero .FLI que se copia a fli/intro.fli en el directorio de
# pruebas (ver instrucciones al final del script).
fli_str = b"intro.fli\x00"
fli_str += b"\x00" * ((-len(fli_str)) % 4)
fli_addr = code_start

N_FRAMES = 121       # coincide con INTRO.FLI (div2/DIV2/DATA/FLI/ALIEN/)
TAIL_FRAMES = 48     # ~2s extra con el ultimo frame en pantalla, para verlo bien

codigo = []

def emit_lcar(v):
    codigo.append(LCAR); codigo.append(v)

# n_var: se guarda el contador de frames restantes en una variable local del
# proceso. Offset 44 en adelante son campos privados/locales (iloc_priv);
# para esta prueba basta con un contador en la pila via un bucle "for"
# implementado a mano con saltos.

# start_fli("intro.fli", 0, 0); -> pila[sp] = numero de frames (se descarta,
# el script ya sabe cuantos frames tiene el fichero de prueba)
emit_lcar(fli_addr)
emit_lcar(0)   # x
emit_lcar(0)   # y
codigo.append(LFUN); codigo.append(FUNC_START_FLI)
codigo.append(LASP)

# N_FRAMES veces: frame_fli(); frame;
for _ in range(N_FRAMES):
    codigo.append(LFUN); codigo.append(FUNC_FRAME_FLI)
    codigo.append(LASP)   # descarta el "sigue/termino" (no lo necesitamos, sabemos cuantos hay)
    codigo.append(LFRM)

# end_fli();
codigo.append(LFUN); codigo.append(FUNC_END_FLI)
codigo.append(LASP)

# unos frames mas con el ultimo cuadro en pantalla, luego salir
codigo += [LFRM] * TAIL_FRAMES
codigo.append(LRET)

code_words = codigo
code_start_real = code_start + len(fli_str) // 4
code_end = code_start_real + len(code_words)

iloc_pub_len = 44
iloc_priv = 0
plantilla = [0] * iloc_pub_len
plantilla[11] = -1
plantilla[31] = 100
plantilla[36] = 1
plantilla[40] = -1
plantilla[41] = -1
plantilla[42] = -1
plantilla[43] = 32

globales = [0] * system_globals_words
# restore_type/dump_type (inter.h: mem[end_struct+17]/mem[end_struct+18],
# end_struct=1529) -- el compilador real los inicializa a 1 (ver
# system/ltobj.def: "global restore_type=1", "global dump_type=1" =
# volcado/restauracion COMPLETOS). Con 0 (el default de este script para
# el resto de globales) el motor usa restauracion PARCIAL por regiones
# sucias (scan[], ver i.cpp restore()), que solo se marcan cuando algun
# proceso dibuja un sprite -- sin ningun sprite en escena, copia2 (donde
# start_fli/frame_fli pintan) nunca se copia a copia (el buffer visible)
# y la pantalla se queda negra aunque el FLI se decodifique bien.
end_struct = 1529
globales[end_struct + 17 - long_header] = 1   # restore_type = complete_dump
globales[end_struct + 18 - long_header] = 1   # dump_type = complete_dump

mem0 = 0
mem1 = code_start_real
mem2 = code_end
mem3 = 0
mem4 = 0
mem5 = iloc_priv
mem6 = iloc_pub_len
mem7 = 0
mem8 = code_end + iloc_pub_len

payload = (struct.pack('<%di' % system_globals_words, *globales)
           + fli_str
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % iloc_pub_len, *plantilla))
n = len(payload)
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_fli_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: fli_addr={fli_addr} code_start={code_start_real} iloc={mem2} "
      f"mem8={mem8} n={n} comprimido={len(comprimido)} bytes, "
      f"fichero total={602+40+len(comprimido)} bytes")
print("Copiar un .FLI real (320x200) a fli/intro.fli antes de ejecutar, "
      "p.ej. div2/DIV2/DATA/FLI/ALIEN/INTRO.FLI (121 frames, coincide con "
      "N_FRAMES de este script).")
