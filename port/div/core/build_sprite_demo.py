#!/usr/bin/env python3
# Tercera prueba manual de bytecode DIV (ver build_test_prg.py y
# build_frame_demo.py, y docs/architecture/12-port-progreso.md checkpoint
# 10 para el analisis completo). Esta es la primera que ejercita de verdad
# el interprete "para lo que sirve": carga un .fpg real y hace que el
# propio proceso "main" se pinte con uno de sus graficos, usando la misma
# ruta de render automatico por proceso (_Graph/_X/_Y/_Ctype) que usa
# cualquier juego DIV real -- no llama a put()/xput() manualmente.
#
# Programa (equivalente DIV fuente, no compilable con esta herramienta,
# solo para referencia de lo que codifica a mano el bytecode):
#
#   load_fpg("resource/fpg/tutorial/tutor0.fpg");
#   graph=2; x=160; y=100;
#   loop
#     frame;
#   end
#
# graf=2 en tutor0.fpg es una imagen de fondo completa (320x200, 64000
# bytes de pixeles + 64 de cabecera = 64064, confirmado leyendo el fichero
# directamente), asi que deberia llenar toda la pantalla -- confirmacion
# visual inequivoca si funciona.
#
# HALLAZGO IMPORTANTE (ver checkpoint 10): el "molde" de 44 campos
# publicos NO puede ser todo ceros como en las dos pruebas anteriores.
# system/ltobj.def declara valores por defecto explicitos y no nulos para
# varios campos del proceso (size=100, height=1, m8_wall=-1,
# m8_sector=-1, m8_nextsector=-1, m8_step=32, m8_object=-1) -- si el
# molde no los replica, _Size queda en 0 ("grafico al 0% de tamano" ==
# invisible) y el sprite nunca se veria aunque todo lo demas funcione.
import struct, zlib

# --- Opcodes usados (ver inter.h) ------------------------------------------
LCAR = 1    # carga una constante en pila (lleva 1 palabra de operando)
LASI = 2    # pop addr,valor -> mem[addr]=valor (deja valor en pila)
LAID = 20   # pila[sp] += id (direccion relativa al proceso actual)
LFUN = 25   # llamada a funcion interna (lleva 1 palabra: el codigo)
LASP = 28   # descarta el valor de pila (sp--)
LFRM = 29   # FRAME: cede la ejecucion hasta el proximo frame
LRET = 27   # fin del proceso

FUNC_LOAD_FPG = 3

# --- Offsets de campo de proceso (inter.h, _Campo) -------------------------
_GRAPH = 29
_X = 26
_Y = 27

N_FRAMES = 240  # ~10 segundos a los 24 fps por defecto de DIV

long_header = 9
system_globals_words = 1520 + 68   # = 1588 (ver build_test_prg.py)
code_start = long_header + system_globals_words   # = 1597

# --- Cadena de texto (nombre de fichero) embebida como datos ---------------
# Un "string" en DIV es simplemente un indice de mem[] cuyo contenido,
# reinterpretado como bytes (memb), es una cadena C terminada en NUL --
# confirmado leyendo load_fpg()/open_file() en f.cpp: usan
# (byte*)&mem[pila[sp]] con fopen() normal, sin comprobar ninguna
# marca especial (esa marca 0xDAD0... solo la usan los opcodes de
# manipulacion de cadenas lstrcpy/lstrcat, no la carga de ficheros).
#
# IMPORTANTE (ver checkpoint 10): open_file() en la build de produccion
# (f.cpp, rama #else de la version DEBUG) IGNORA cualquier ruta que se le
# pase -- con _splitpath() se queda solo con el nombre+extension y
# reconstruye la ruta como "<extension>\<nombre>.<extension>" relativo al
# directorio de trabajo (asi organiza DIV los recursos de un juego
# compilado: subcarpetas fpg\, pcm\, etc. junto al .prg). Por eso aqui
# basta con el nombre de fichero a secas -- y hay que ejecutar el .exe
# con el directorio de trabajo puesto en una carpeta que tenga una
# subcarpeta fpg\ con este fichero dentro (ver build/test_game/ generado
# aparte, no versionado).
filename = b"tutor0.fpg\x00"
pad = (-len(filename)) % 4
filename += b"\x00" * pad
filename_words = len(filename) // 4
string_addr = code_start  # los datos van justo antes del codigo ejecutable

codigo = []

def emit_lcar(v):
    codigo.append(LCAR); codigo.append(v)

def emit_set_field(field_offset, value):
    # mem[id+field_offset] = value ; descarta el resultado de la asignacion
    emit_lcar(field_offset)
    codigo.append(LAID)
    emit_lcar(value)
    codigo.append(LASI)
    codigo.append(LASP)

# load_fpg("resource/fpg/tutorial/tutor0.fpg"); (descarta el id de fpg devuelto)
emit_lcar(string_addr)
codigo.append(LFUN); codigo.append(FUNC_LOAD_FPG)
codigo.append(LASP)

# graph=2; x=160; y=100;  (proceso "main", _File ya es 0 por defecto)
emit_set_field(_GRAPH, 2)
emit_set_field(_X, 160)
emit_set_field(_Y, 100)

# loop frame; end -- N_FRAMES veces, para poder verlo antes de que termine
codigo += [LFRM] * N_FRAMES
codigo.append(LRET)

code_words = codigo
code_start_real = code_start + filename_words
code_end = code_start_real + len(code_words)

# --- Molde de 44 campos publicos, con los defaults reales de ltobj.def ----
iloc_pub_len = 44
iloc_priv = 0
plantilla = [0] * iloc_pub_len
plantilla[11] = -1   # m8_object
plantilla[31] = 100  # size
plantilla[36] = 1    # height
plantilla[40] = -1   # m8_wall
plantilla[41] = -1   # m8_sector
plantilla[42] = -1   # m8_nextsector
plantilla[43] = 32   # m8_step

globales = [0] * system_globals_words

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
           + filename
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % iloc_pub_len, *plantilla))
n = len(payload)
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_sprite_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: string_addr={string_addr} code_start={code_start_real} "
      f"iloc={mem2} mem8={mem8} n={n} comprimido={len(comprimido)} bytes, "
      f"fichero total={602+40+len(comprimido)} bytes")
