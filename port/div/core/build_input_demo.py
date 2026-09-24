#!/usr/bin/env python3
# Quinta prueba manual de bytecode DIV (ver build_test_prg.py,
# build_frame_demo.py, build_sprite_demo.py y build_sound_demo.py, y
# docs/architecture/13-handoff.md). Esta ejercita el INPUT desde bytecode
# real: el programa lee el estado de las flechas con key() (funcion 001)
# cada frame y mueve un sprite en consecuencia; ESC termina el programa
# antes de tiempo. Confirma el camino completo
# key() -> _key (f.cpp) -> kbdFLAGS[] -> tecla() (divkeybo.cpp) ->
# io_poll()/io_key_down() (io_input.c, traduccion raylib->scancode set-1).
#
# Programa equivalente en lenguaje DIV (solo referencia, este script
# codifica el bytecode a mano):
#
#   load_fpg("tutor0.fpg");   # fpg "0"
#   graph=1; x=160; y=100;    # sprite 35x35 centrado
#   loop
#     clear_screen();         # fondo limpio cada frame (DIV no lo borra solo)
#     if (key(1)) break;      # ESC = salir
#     if (key(75)) x-=4;      # Izquierda  (0x4B = 75)
#     if (key(77)) x+=4;      # Derecha    (0x4D = 77)
#     if (key(72)) y-=4;      # Arriba     (0x48 = 72)
#     if (key(80)) y+=4;      # Abajo      (0x50 = 80)
#     frame;
#   end
#
# CONVENCIONES verificadas en kernel.cpp/f.cpp/inter.h:
#  - _key (caso 1 de function()): pila[sp]=key(pila[sp]) con pila[sp] el
#    codigo scan (1..127). key() = kbdFLAGS[scancode], rellenado por
#    tecla()/io_poll() cada frame.
#  - clear_screen (caso 33) NO lleva argumentos: hace memset(copia2,0,...)
#    y deja un 0 en pila (pila[++sp]=0) -> hay que lasp para descartarlo.
#  - ljpf (24): si (pila[sp--]&1) es CERO salta a la direccion ABSOLUTA de
#    mem[] que viene como operando; si es impar (true) sigue con ip++.
#  - ljmp (23): salta a la direccion absoluta de su operando.
#  - ladd/lsub (12/13): pila[sp-1]=X operation pila[sp]; sp-- (operacion
#    binaria; deja el resultado en la pila).
#  - lada/lsua (42/43): pila[sp-1]=mem[pila[sp-1]] +=/-= pila[sp]; sp--.
#    O sea: se empuja primero la DIRECCION del campo y luego el valor.
#  - lret (27): NO consume operando (usa _Param/_NumPar del proceso).
import struct, zlib

# --- Opcodes usados (ver inter.h) ------------------------------------------
LCAR = 1      # carga una constante en pila (lleva 1 palabra de operando)
LASI = 2      # pop addr,valor -> mem[addr]=valor (deja valor en pila)
LMEN = 9      # pila[sp-1] = pila[sp-1] < pila[sp]  (sp--)
LSUB = 13     # pila[sp-1] -= pila[sp] (sp--)
LADD = 12     # pila[sp-1] += pila[sp] (sp--)
LPTR = 18     # pila[sp] = mem[pila[sp]]
LAID = 20     # pila[sp] += id (direccion relativa al proceso actual)
LJMP = 23     # salto incondicional (lleva 1 palabra: direccion abs. de mem[])
LJPF = 24     # salta si (pila[sp--]&1)==0 (lleva 1 palabra: direccion abs.)
LFUN = 25     # llamada a funcion interna (lleva 1 palabra: el codigo)
LRET = 27     # fin del proceso
LASP = 28     # descarta el valor de pila (sp--)
LFRM = 29     # FRAME: cede la ejecucion hasta el proximo frame
LADA = 42     # add-asignacion: mem[addr]+=val, deja resultado
LSUA = 43     # sub-asignacion: mem[addr]-=val, deja resultado

FUNC_KEY = 1         # key(scan) -> 1 mientras esta pulsada
FUNC_LOAD_FPG = 3    # load_fpg(fichero) -> id de fpg
FUNC_CLEAR_SCREEN = 33

# --- Offsets de campo de proceso (inter.h, _Campo) -------------------------
_GRAPH = 29
_X = 26
_Y = 27

# --- Scancodes de teclado (divkeybo.h, juego de teclas de DIV) -------------
K_ESC   = 0x01
K_LEFT  = 0x4B
K_RIGHT = 0x4D
K_UP    = 0x48
K_DOWN  = 0x50

long_header = 9
system_globals_words = 1520 + 68     # = 1588 (ver build_test_prg.py)
code_start = long_header + system_globals_words   # = 1597

# --- Cadenas de texto embebidas (load_fpg) ---------------------------------
fpg_str = b"tutor0.fpg\x00"
fpg_str += b"\x00" * ((-len(fpg_str)) % 4)
fpg_addr = code_start
code_base = code_start + len(fpg_str) // 4

# --- Ensamblador minimo con labels y direcciones absolutas de mem[] --------
codigo = []          # (lista de palabras, indices relativos a code_base)
fixups = []          # (indice_dentro_de_codigo, nombre_label)
labels = {}

def mark(label):
    labels[label] = len(codigo)

def emit(*words):
    codigo.extend(words)

def emit_set_field(field, value):
    emit(LCAR, field, LAID, LCAR, value, LASI, LASP)

def emit_key_jump(scancode, fallto_label):
    """key(scancode); si se cumple (true) sigue, si no salta a label."""
    idx = len(codigo)
    emit(LCAR, scancode, LFUN, FUNC_KEY, LJPF, 0)
    fixups.append((idx + 5, fallto_label))   # operando del ljpf

def emit_addsub(field, value, op):
    """mem[id+field] += value (op=LADA) o -= value (op=LSUA); descarta resultado."""
    emit(LCAR, field, LAID, LCAR, value, op, LASP)

# --- Programa ---------------------------------------------------------------
# load_fpg("tutor0.fpg");  (fpg id 0; descartamos el id devuelto)
emit(LCAR, fpg_addr, LFUN, FUNC_LOAD_FPG, LASP)

# graph=1; x=160; y=100;
emit_set_field(_GRAPH, 1)
emit_set_field(_X, 160)
emit_set_field(_Y, 100)

mark("loop")
# clear_screen();  (no lleva arg; deja un 0 en pila que hay que descartar)
emit(LFUN, FUNC_CLEAR_SCREEN, LASP)

# if (key(1)) break;  -> si ESC pulsado, lret (fin del programa)
idx_escj = len(codigo)
emit(LCAR, K_ESC, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_escj + 5, "continue"))
emit(LRET)                       # ESC pulsado: termina aqui
mark("continue")

# if (key(75)) x-=4;
idx_jl = len(codigo)
emit(LCAR, K_LEFT, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_jl + 5, "skipL"))
emit_addsub(_X, 4, LSUA)
mark("skipL")

# if (key(77)) x+=4;
idx_jr = len(codigo)
emit(LCAR, K_RIGHT, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_jr + 5, "skipR"))
emit_addsub(_X, 4, LADA)
mark("skipR")

# if (key(72)) y-=4;
idx_ju = len(codigo)
emit(LCAR, K_UP, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_ju + 5, "skipU"))
emit_addsub(_Y, 4, LSUA)
mark("skipU")

# if (key(80)) y+=4;
idx_jd = len(codigo)
emit(LCAR, K_DOWN, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_jd + 5, "skipD"))
emit_addsub(_Y, 4, LADA)
mark("skipD")

# frame;  -> vuelta al bucle
emit(LFRM)
idx_jl2 = len(codigo)
emit(LJMP, 0)
fixups.append((idx_jl2 + 1, "loop"))

for idx, label in fixups:
    if label not in labels:
        raise SystemExit(f"label no definida: {label}")
    codigo[idx] = code_base + labels[label]

code_words = codigo
code_end = code_base + len(code_words)

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
mem1 = code_base            # entry point: primer opcode de "main"
mem2 = code_end             # iloc (molde) justo despues del codigo
mem3 = 0
mem4 = 0
mem5 = iloc_priv
mem6 = iloc_pub_len
mem7 = 0
mem8 = code_end + iloc_pub_len

payload = (struct.pack('<%di' % system_globals_words, *globales)
           + fpg_str
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % iloc_pub_len, *plantilla))
n = len(payload)
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_input_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: entry={mem1} iloc={mem2} mem8={mem8} n={n} "
      f"comprimido={len(comprimido)} bytes, codigo={len(code_words)} palabras, "
      f"fichero total={602+40+len(comprimido)} bytes")
print("Scancodes: ESC=1  IZQ=75  DER=77  ARR=72  ABA=80 "
      "(ESC termina el programa; Ctrl+ESC termina el runtime)")