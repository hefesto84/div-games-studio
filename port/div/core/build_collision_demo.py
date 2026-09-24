#!/usr/bin/env python3
# Sexta prueba manual de bytecode DIV (ver build_test_prg.py,
# build_frame_demo.py, build_sprite_demo.py, build_sound_demo.py y
# build_input_demo.py, y docs/architecture/13-handoff.md). Esta ejercita la
# DETECCION DE COLISIONES desde bytecode real: crea un segundo proceso
# (tipo 2, "fantasma") mediante lcal/ltyp y, cada frame, el proceso principal
# avanza un sprite hacia el fantasma y comprueba collision(TYPE 2). Cuando se
# solapan, el proceso principal termina (lret): el programa finaliza solo.
# Valida el camino completo collision() -> escaneo de mem[] (id_init..id_end)
# -> caja por grafico contra s.cpp. Si collision() fuese el stub antiguo
# (siempre 0) el programa nunca detectaria el solape y no terminaria.
#
# Programa equivalente en lenguaje DIV (referencia; este script codifica el
# bytecode a mano):
#
#   load_fpg("tutor0.fpg");   # fpg "0"
#   fantasma(100,100);        # crea proceso tipo 2
#   graph=1; x=40; y=100;     # sprite 35x35 -> al 200% (size=200)
#   size=200;                  # arranca a la izquierda, avanza lento
#   loop
#     clear_screen();
#     if (key(1)) break;              # ESC = salir
#     x+=2;                           # avance automatico (lento)
#     if (collision(TYPE 2)) break;   # choque: fin (auto-cierre)
#     frame;
#   end
#
#   process fantasma(x,y);    # tipo 2
#   begin
#     graph=4; x=250; size=200;  # grafico 46x40 al 200%, a la derecha
#     loop frame; end
#   end
#
# CONVENCIONES (ver kernel.cpp):
#  - lcal (26): operando = direccion ABSOLUTA de mem[] donde empieza el codigo
#    del proceso (kernel hace ip=mem[ip], el operando ES la direccion). Cambia
#    de contexto al proceso nuevo; este corre hasta su primer lfrm y entonces
#    devuelve el control al padre justo tras el lcal, dejando el id del hijo en
#    pila (por eso el padre emite lasp a continuacion). El codigo del nuevo
#    proceso empieza directamente en la direccion apuntada (no hay cabecera):
#    kernel.cpp solo mira mem[ip+2]==lnop para distinguir funciones, irrelevante
#    aqui.
#  - lcbp (30): fija _NumPar y _Param del proceso (necesario para que su
#    lfrm devuelva la pila al sitio correcto).
#  - ltypp (32): _Bloque = tipo (identificador de colision).
#  - collision es la funcion interna 008; lee el tipo de pila[sp] y deja el
#    id del proceso colisionante (o 0) en pila[sp].
import struct, zlib

# --- Opcodes usados (ver inter.h) ------------------------------------------
LCAR = 1      # carga una constante en pila
LASI = 2      # mem[addr]=valor (addr,valor en pila)
LPTR = 18     # pila[sp] = mem[pila[sp]]
LAID = 20     # pila[sp] += id
LJMP = 23     # salto incondicional (operando: direccion abs. de mem[])
LJPF = 24     # salta si (pila[sp--]&1)==0
LFUN = 25     # llamada a funcion interna (operando: codigo)
LCAL = 26     # crea un nuevo proceso (operando: addr que apunta al codigo)
LRET = 27     # fin del proceso
LASP = 28     # descarta el valor de pila
LFRM = 29     # FRAME
LCBP = 30     # inicializa el puntero a parametros locales (operando: num_par)
LTYP = 32     # define el tipo de proceso (operando: bloque)
LSUA = 43     # sub-asignacion
LADA = 42     # add-asignacion
LCAR2 = 60    # empuja dos constantes consecutivas
LCARAIDCPA = 68  # mem[field+id] = pila[_Param++]  (lee parametro en un campo)

FUNC_SIGNAL = 0    # signal(tipo, codigo) -> mata/congela procesos por tipo
FUNC_KEY = 1
FUNC_LOAD_FPG = 3
FUNC_COLLISION = 8
FUNC_CLEAR_SCREEN = 33

# --- Offsets de campo de proceso (inter.h, _Campo) -------------------------
_GRAPH = 29
_X = 26
_Y = 27
_SIZE = 31

# --- Scancodes de teclado (divkeybo.h, juego de teclas de DIV) -------------
K_ESC, K_LEFT, K_RIGHT, K_UP, K_DOWN = 0x01, 0x4B, 0x4D, 0x48, 0x50

TIPO_FANTASMA = 2
GRAPH_NAVE = 1
GRAPH_FANTASMA = 4

long_header = 9
system_globals_words = 1520 + 68     # = 1588
code_start = long_header + system_globals_words

# --- Cadena de texto embebida (load_fpg) -----------------------------------
fpg_str = b"tutor0.fpg\x00"
fpg_str += b"\x00" * ((-len(fpg_str)) % 4)
fpg_addr = code_start
code_base = code_start + len(fpg_str) // 4

iloc_pub_len = 44
plantilla = [0] * iloc_pub_len
plantilla[4] = 2     # _Status=2 (vivo): en un .div32 real lo trae el molde;
                     # sin esto, lcal crea el proceso como "hueco libre"
plantilla[11] = -1
plantilla[31] = 100  # size
plantilla[36] = 1    # height
plantilla[40] = -1
plantilla[41] = -1
plantilla[42] = -1
plantilla[43] = 32

# --- Ensamblador del proceso principal -------------------------------------
codigo = []
fixups = []          # (indice_en_codigo, label) : operando a rellenar
labels = {}

def mark(label):
    labels[label] = len(codigo)

def emit(*words):
    codigo.extend(words)

def emit_set_field(field, value):
    emit(LCAR, field, LAID, LCAR, value, LASI, LASP)

def emit_key_jump(scancode, fallto_label):
    idx = len(codigo)
    emit(LCAR, scancode, LFUN, FUNC_KEY, LJPF, 0)
    fixups.append((idx + 5, fallto_label))

def emit_addsub(field, value, op):
    emit(LCAR, field, LAID, LCAR, value, op, LASP)

# load_fpg("tutor0.fpg");  (descartamos el id)
emit(LCAR, fpg_addr, LFUN, FUNC_LOAD_FPG, LASP)

# fantasma(250,100); -> lcar2 250,100 (empuja 2 args) + lcal <dir_codigo>
# (el operando de lcal es directamente la direccion del codigo del proceso:
#  kernel.cpp hace ip=mem[ip] -> ip=operando. Se resuelve con "FANTASMA")
emit(LCAR2, 250, 100)
idx_cal = len(codigo)
emit(LCAL, 0)
fixups.append((idx_cal + 1, "FANTASMA"))
emit(LASP)

# graph=1; x=40; y=100; size=200;   (nave a la izquierda, al 200%; el
#                                    fantasma esta en x=250, avance lento)
emit_set_field(_GRAPH, GRAPH_NAVE)
emit_set_field(_X, 40)
emit_set_field(_Y, 100)
emit_set_field(_SIZE, 200)

mark("loop")
emit(LFUN, FUNC_CLEAR_SCREEN, LASP)

# if (key(1)) break;
idx_esc = len(codigo)
emit(LCAR, K_ESC, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_esc + 5, "continue"))
emit(LRET)
mark("continue")

# x+=2;  (avance automatico lento hacia el fantasma)
emit_addsub(_X, 2, LADA)

# if (collision(TYPE 2)) lret;   -> si NO hay colision, salta a nohit
idx_col = len(codigo)
emit(LCAR, TIPO_FANTASMA, LFUN, FUNC_COLLISION, LJPF, 0)
fixups.append((idx_col + 5, "nohit"))
emit(LCAR2, TIPO_FANTASMA, 0, LFUN, FUNC_SIGNAL)   # signal(2, s_kill): mata al
# fantasma antes de acabar, para que el programa termine (sin hijo vivo)
emit(LRET)          # colision detectada: termina (fin normal del programa)
mark("nohit")

emit(LFRM)
idx_l2 = len(codigo)
emit(LJMP, 0)
fixups.append((idx_l2 + 1, "loop"))

# --- Ensamblador del proceso "fantasma" (tipo 2) ---------------------------
fantasma = []
fixups_f = []
labels_f = {}

def emit_f(*w): fantasma.extend(w)
def mark_f(l): labels_f[l] = len(fantasma)

emit_f(LCBP, 2)              # 2 parametros: x, y
emit_f(LTYP, TIPO_FANTASMA)  # _Bloque = 2
emit_f(LCARAIDCPA, _X)       # _X = param1  (mem[id+_X]=pila[_Param++])
emit_f(LCARAIDCPA, _Y)       # _Y = param2
emit_f(LCAR, _GRAPH, LAID, LCAR, GRAPH_FANTASMA, LASI, LASP)
emit_f(LCAR, _SIZE, LAID, LCAR, 200, LASI, LASP)
mark_f("loop")
emit_f(LFRM)
idx_f = len(fantasma)
emit_f(LJMP, 0)
fixups_f.append((idx_f + 1, "loop"))

# --- Direcciones y resolucion de fixups ------------------------------------
code_words = codigo
code_end = code_base + len(code_words)      # = direccion del molde (iloc)
fantasma_addr = code_end + iloc_pub_len     # el fantasma va tras el molde

# el fantasma va en posicion absoluta fantasma_addr
for idx, lab in fixups_f:
    fantasma[idx] = fantasma_addr + labels_f[lab]

# main: direcciones absolutas de mem[]
for idx, lab in fixups:
    if lab == "FANTASMA":
        codigo[idx] = fantasma_addr
    else:
        codigo[idx] = code_base + labels[lab]

mem8 = fantasma_addr + len(fantasma)

globales = [0] * system_globals_words
payload = (struct.pack('<%di' % system_globals_words, *globales)
           + fpg_str
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % iloc_pub_len, *plantilla)
           + struct.pack('<%di' % len(fantasma), *fantasma))
n = len(payload)
assert n == (mem8 - long_header) * 4, (n, (mem8 - long_header) * 4)

mem0, mem1, mem2 = 0, code_base, code_end
mem3, mem4 = 0, 0
mem5, mem6, mem7 = 0, iloc_pub_len, 0
header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_collision_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: entry={mem1} iloc={mem2} fantasma={fantasma_addr} mem8={mem8} "
      f"n={n} comprimido={len(comprimido)} bytes")
print("La nave 35x35 al 200% avanza lentamente (2px/frame) hacia el 46x40 "
      "al 200%; al solaparse el programa")
print("termina solo (auto-cierre = collision() detectada). Ctrl+ESC cierra "
      "el runtime si algo fuese mal.")
