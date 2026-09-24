#!/usr/bin/env python3
# Segunda prueba manual de bytecode DIV, hermana de build_test_prg.py (ver
# ese fichero y docs/architecture/12-port-progreso.md checkpoint 8 para el
# analisis completo del formato EML/layout de mem[]). Esta variante existe
# para tener una confirmacion VISUAL de que el runtime funciona: el
# programa de build_test_prg.py (un solo "lret") mata el proceso en el
# primer frame, así que la ventana se abre y cierra casi instantaneamente
# -- facil de confundir con "no hace nada".
#
# Programa: el proceso "main" ejecuta el opcode real lfrm (29, "FRAME" --
# detiene la ejecucion de este proceso hasta el proximo frame, ver
# inter.h/kernel.cpp) repetido N veces seguidas, y termina con lret (27).
# lfrm no lleva operando (una sola palabra por repeticion, confirmado en
# kernel.cpp: el case no hace ningun mem[ip++] extra), asi que basta con
# repetir la palabra de opcode N veces -- no hace falta un salto (ljmp).
# DIV usa 24 fps por defecto (freloj=ireloj=100.0/24.0 en
# inicializacion(), ver checkpoint 9 del documento de progreso), asi que
# con N=180 la ventana debe quedar abierta y respondiendo ~7.5s, y
# cerrarse sola al terminar.
import struct, zlib

LFRM = 29
LRET = 27
N_FRAMES = 180  # ~7.5 segundos a los 24 fps por defecto de DIV

long_header = 9
system_globals_words = 1520 + 68   # = 1588 (ver build_test_prg.py)
code_start = long_header + system_globals_words   # = 1597

codigo = struct.pack('<%di' % (N_FRAMES + 1), *([LFRM] * N_FRAMES + [LRET]))
code_len_words = N_FRAMES + 1
code_end = code_start + code_len_words

iloc_pub_len = 44
iloc_priv = 0
plantilla = struct.pack('<%di' % iloc_pub_len, *([0] * iloc_pub_len))
globales = struct.pack('<%di' % system_globals_words, *([0] * system_globals_words))

mem0 = 0
mem1 = code_start
mem2 = code_end
mem3 = 0
mem4 = 0
mem5 = iloc_priv
mem6 = iloc_pub_len
mem7 = 0
mem8 = code_end + iloc_pub_len

payload = globales + codigo + plantilla
n = len(payload)
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_frame_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: {N_FRAMES} frames de FRAME + 1 lret, code_start={code_start} "
      f"mem8={mem8} fichero total={602+40+len(comprimido)} bytes")
