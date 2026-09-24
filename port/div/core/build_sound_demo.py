#!/usr/bin/env python3
# Cuarta prueba manual de bytecode DIV (ver build_test_prg.py,
# build_frame_demo.py y build_sprite_demo.py, y docs/architecture/
# 13-handoff.md §8 backlog item 1). Esta es la primera que ejercita el
# canal de AUDIO con bytecode real: carga un .pcm con load_pcm() y lo
# reproduce con sound(), confirmando de punta a punta el camino
# load_pcm -> LoadSound -> io_load_sound -> sound -> PlaySound ->
# io_play_sound (raylib) que divsound.cpp/io_audio.c ya exponen.
#
# Como el .pcm de referencia del proyecto (help/help.pcm) es un sample
# corto y de volumen incierto, este script SINCRONIZA ademas su propio
# fichero "sound_demo.pcm" (PCM bruto de 8 bits mono a 22050 Hz: tres
# bip-bips a 880Hz seguidos de un tono sostenido a 440Hz, ~3.2s) para
# que la confirmacion auditiva sea inequivoca. Reutiliza exactamente el
# mismo camino de codigo que un .pcm real (io_load_sound primero intenta
# WAV y hace fallback a raw PCM, igual que el LoadSound original con
# judas_loadwav_mem/judas_loadrawsample_mem -- ver debajo).
#
# Programa (equivalente DIV fuente, no compilable con esta herramienta,
# solo para referencia de lo que codifica a mano el bytecode):
#
#   load_fpg("tutor0.fpg");   # fondo que llena toda la pantalla
#   graph=2; x=160; y=100;
#   s = load_pcm("sound_demo.pcm", 0);   # 0 = no hacer loop
#   sound(s, 256, 256);                  # 256 = max volumen / frec original
#   loop
#     frame;
#   end
#
# CONVENCION DE ARGUMENTOS (ver 13-handoff.md §5.8): se empujan con lcar
# en orden izquierda a derecha; la implementacion los desapila en orden
# inverso. load_pcm(fichero,loop): lo que hay en pila[sp] tras la llamada
# es el id de sonido (slot de retorno), y _sound(id,vol,fre) lo
# sobreescribe con el id de canal -- por eso el id de sonido devuelto por
# load_pcm se usa directamente como primer argumento de sound sin tener
# que guardarlo en ningun sitio.
import math, os, struct, zlib

# --- Opcodes usados (ver inter.h) ------------------------------------------
LCAR = 1    # carga una constante en pila (lleva 1 palabra de operando)
LASI = 2    # pop addr,valor -> mem[addr]=valor (deja valor en pila)
LAID = 20   # pila[sp] += id (direccion relativa al proceso actual)
LFUN = 25   # llamada a funcion interna (lleva 1 palabra: el codigo)
LASP = 28   # descarta el valor de pila (sp--)
LFRM = 29   # FRAME: cede la ejecucion hasta el proximo frame
LRET = 27   # fin del proceso

FUNC_LOAD_FPG = 3
FUNC_LOAD_PCM = 37
FUNC_SOUND = 39

# --- Offsets de campo de proceso (inter.h, _Campo) -------------------------
_GRAPH = 29
_X = 26
_Y = 27

N_FRAMES = 240  # ~10 segundos a los 24 fps por defecto de DIV

long_header = 9
system_globals_words = 1520 + 68   # = 1588 (ver build_test_prg.py)
code_start = long_header + system_globals_words   # = 1597

# --- Cadenas de texto (nombres de fichero) embebidas como datos ------------
# Igual mecanismo que en build_sprite_demo.py: un "string" es un indice de
# mem[] cuyo contenido es una cadena C terminada en NUL. open_file() en
# produccion reconstruye la ruta como "<ext>\<nombre>.<ext>" relativa al
# directorio de trabajo, asi que basta el nombre de fichero a secas y hay
# que ejecutar con el CWD en una carpeta que tenga fpg/ y pcm/ (ver
# build/test_game/).
fpg_str = b"tutor0.fpg\x00"
fpg_str += b"\x00" * ((-len(fpg_str)) % 4)
pcm_str = b"sound_demo.pcm\x00"
pcm_str += b"\x00" * ((-len(pcm_str)) % 4)

fpg_addr = code_start
pcm_addr = code_start + len(fpg_str) // 4

# --- Sintesis del .pcm de prueba (8 bits mono 22050 Hz, el formato que
# asume el fallback raw de io_audio.c) --------------------------------------
PCM_RATE = 22050

def synth(freq, dur, amp=100):
    """Onda senoidal de 8 bits sin signo (mid=128) con fade de 10ms."""
    n = int(PCM_RATE * dur)
    fade = int(0.010 * PCM_RATE)
    out = bytearray()
    for i in range(n):
        env = 1.0
        if i < fade: env = i / fade
        if i > n - fade: env = (n - i) / fade
        v = amp * math.sin(2 * math.pi * freq * i / PCM_RATE)
        out.append(int(128 + v * env))
    return out

def silence(dur):
    return bytearray(int(PCM_RATE * dur)) + b"\x80"

def make_pcm():
    pcm = bytearray()
    for _ in range(3):                       # tres bip-bips cortos a 880Hz
        pcm += synth(880, 0.20)
        pcm += silence(0.18)
    pcm += synth(440, 2.20)                  # tono sostenido a 440Hz
    return bytes(pcm)

pcm_bytes = make_pcm()
pcm_dir = os.path.join("pcm")
os.makedirs(pcm_dir, exist_ok=True)
with open(os.path.join(pcm_dir, "sound_demo.pcm"), "wb") as f:
    f.write(pcm_bytes)
print(f"OK: pcm/sound_demo.pcm escrito ({len(pcm_bytes)} bytes, "
      f"{len(pcm_bytes)/PCM_RATE:.2f}s a {PCM_RATE} Hz)")

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

# load_fpg("tutor0.fpg"); (descarta el id de fpg devuelto)
emit_lcar(fpg_addr)
codigo.append(LFUN); codigo.append(FUNC_LOAD_FPG)
codigo.append(LASP)

# graph=2; x=160; y=100;  (proceso "main", _File ya es 0 por defecto)
emit_set_field(_GRAPH, 2)
emit_set_field(_X, 160)
emit_set_field(_Y, 100)

# sound_id = load_pcm("sound_demo.pcm", 0);  -> id queda en pila[sp]
emit_lcar(pcm_addr)
emit_lcar(0)                                 # loop = 0
codigo.append(LFUN); codigo.append(FUNC_LOAD_PCM)

# sound(sound_id, 256, 256);  -> sobreescribe pila[sp] con el id de canal
emit_lcar(256)                               # volumen maximo (0..256)
emit_lcar(256)                               # frecuencia = la original
codigo.append(LFUN); codigo.append(FUNC_SOUND)
codigo.append(LASP)                          # descarta el id de canal

# loop frame; end -- N_FRAMES veces, para poder oirlo antes de que termine
codigo += [LFRM] * N_FRAMES
codigo.append(LRET)

code_words = codigo
code_start_real = code_start + (len(fpg_str) + len(pcm_str)) // 4
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
           + fpg_str
           + pcm_str
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % iloc_pub_len, *plantilla))
n = len(payload)
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_sound_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: fpg_addr={fpg_addr} pcm_addr={pcm_addr} "
      f"code_start={code_start_real} iloc={mem2} mem8={mem8} n={n} "
      f"comprimido={len(comprimido)} bytes, "
      f"fichero total={602+40+len(comprimido)} bytes")