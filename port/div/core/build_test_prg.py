#!/usr/bin/env python3
# Construye a mano un bytecode DIV minimo (formato EML), replicando
# exactamente save_exec_bin() de src/div/divc.cpp y la lectura simetrica
# en main()/inicializacion() de src/div32run/i.cpp. Programa: un solo
# proceso "main" cuyo unico opcode es lret (27) -- termina inmediatamente.
# Ver docs/architecture/12-port-progreso.md checkpoint 8 para el analisis
# completo del formato (incluye el bug de la primera version de este
# script, que asumia que el codigo de usuario empezaba en mem[9]).
import struct, zlib

LRET = 27

long_header = 9  # mem[0..8]: cabecera de 9 campos (mas 1 campo extra "n"
                  # justo despues, ver mas abajo -- 40 bytes en total)

# --- Region de "globales de sistema" (system\ltobj.def) --------------------
# precarga_obj() (divc.cpp) parsea system\ltobj.def antes que el programa
# del usuario y reserva en mem[] TODAS las variables "global" alli
# declaradas (mouse, scroll, m7, joy, setup, net, m8, dirinfo, fileinfo,
# video_modes, timer, text_z..fps, argc, argv, channel, vsync, draw_z,
# num_video_modes, unit_size). inter.h expone el tamano exacto de esa
# region via la macro end_struct + los offsets fijos de las variables
# posteriores (ver 12-port-progreso.md checkpoint 8):
#   end_struct = long_header + 14+10*10+10*7+8+11+9+10*4+1026+146+32*3
#              = long_header + 1520
#   unit_size (la ultima)  = mem[end_struct+67]  -> +68 palabras mas
# Es decir: el codigo del usuario NO puede empezar en mem[long_header] (=9)
# -- ese hueco esta ocupado por estas estructuras. El primer hueco libre
# real es:
system_globals_words = 1520 + 68   # = 1588
code_start = long_header + system_globals_words   # = 1597

# --- Codigo del programa -----------------------------------------------
codigo = struct.pack('<i', LRET)   # 1 sola palabra: lret
code_len_words = 1
code_end = code_start + code_len_words   # = 1598

# --- "Molde" (plantilla) de variables locales del proceso ----------------
# mem[2] (iloc) NO es donde vive el proceso en tiempo de ejecucion -- es la
# posicion de un bloque de solo lectura con los valores por defecto de los
# campos "publicos" de un proceso nuevo, que inicializacion() copia via
# memcpy a id_init/id_start (ver i.cpp lineas 213-229). Su longitud
# (iloc_pub_len = mem[6]) es como minimo el numero de campos del sistema
# definidos en inter.h (_Id.._M8_Step): el ultimo es _M8_Step=43, es decir
# 44 campos (0..43). Sin PROCESS propios que anadan mas campos "local",
# 44 es el valor real minimo que usaria el compilador.
iloc_pub_len = 44   # mem[6]
iloc_priv = 0        # mem[5]
plantilla = struct.pack('<%di' % iloc_pub_len, *([0] * iloc_pub_len))

globales = struct.pack('<%di' % system_globals_words, *([0] * system_globals_words))

# --- Cabecera de 9 campos + longitud descomprimida (10 campos = 40 bytes) --
mem0 = 0                 # program_type
mem1 = code_start        # entry point (_IP inicial del proceso "main")
mem2 = code_end          # iloc: offset del molde de variables locales
mem3 = 0                 # max_process (0 = heuristica automatica del runtime)
mem4 = 0
mem5 = iloc_priv
mem6 = iloc_pub_len
mem7 = 0
mem8 = code_end + iloc_pub_len   # imem final: "longitud del cmp" (codigo+locales+textos)

payload = globales + codigo + plantilla
n = len(payload)
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)

comprimido = zlib.compress(payload)

stub_602 = b'\x00' * 602

with open('test_program.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: code_start={code_start} iloc={mem2} iloc_pub_len={iloc_pub_len} "
      f"mem8={mem8} n={n} comprimido={len(comprimido)} bytes, "
      f"fichero total={602+40+len(comprimido)} bytes")
