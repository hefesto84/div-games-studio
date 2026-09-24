# DEBUG checkpoint 14: variante de build_min2_debug.py. Hijo con SOLO
# x/y += get_distx/get_disty(angle,radio) (builtins 010/011 + lada), angle
# siempre 0 -> sin ruta de rotacion. Si crashea 0xC0000005 -> el bug esta
# en get_disx/get_disy o en el bytecode del hijo; si NO crashea -> sp_rotado.
import struct, zlib

LCAR = 1; LASI = 2; LPTR = 18; LAID = 20
LJMP = 23; LJPF = 24; LFUN = 25; LCAL = 26; LRET = 27; LASP = 28; LFRM = 29
LCBP = 30; LTYP = 32; LCAR2 = 60; LCARAIDCPA = 68; LADA = 42

FUNC_KEY = 1
FUNC_LOAD_FPG = 3
FUNC_GET_DISX = 10
FUNC_GET_DISY = 11

K_ESC = 0x01
_GRAPH = 29
_ANGLE = 32
SCR_RADIO = 11

long_header = 9
system_globals_words = 1588
code_start = long_header + system_globals_words

fpg_str = b"tutor0.fpg\x00"
fpg_str += b"\x00" * ((-len(fpg_str)) % 4)
fpg_addr = code_start
code_base = code_start + len(fpg_str) // 4

codigo = []
fixups = []
labels = {}

def mark(l): labels[l] = len(codigo)
def emit(*w): codigo.extend(w)

emit(LCAR, fpg_addr, LFUN, FUNC_LOAD_FPG, LASP)

emit(LCAR2, 100, 100)
emit(LCAR2, 24, 11250)
idx = len(codigo)
emit(LCAL, 0)
fixups.append((idx + 1, "HIJO"))
emit(LASP)

mark("loop")
idx = len(codigo)
emit(LCAR, K_ESC, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx + 5, "cont"))
emit(LRET)
mark("cont")
emit(LFRM)
idx2 = len(codigo)
emit(LJMP, 0)
fixups.append((idx2 + 1, "loop"))

hijo = []
fix_h = []
lab_h = {}
def emit_h(*w): hijo.extend(w)
def mark_h(l): lab_h[l] = len(hijo)
emit_h(LCBP, 4)
emit_h(LTYP, 2)
emit_h(LCARAIDCPA, 26)
emit_h(LCARAIDCPA, 27)
emit_h(LCARAIDCPA, 11)
emit_h(LCARAIDCPA, 12)
emit_h(LCAR, _GRAPH, LAID, LCAR, 4, LASI, LASP)
emit_h(LCAR, _ANGLE, LAID, LCAR, 0, LASI, LASP)
mark_h("lh")
# x += get_distx(angle,radio)
emit_h(LCAR, 26, LAID)
emit_h(LCAR, _ANGLE, LAID, LPTR)
emit_h(LCAR, SCR_RADIO, LAID, LPTR)
emit_h(LFUN, FUNC_GET_DISX)
emit_h(LADA, LASP)
# y += get_disty(angle,radio)
emit_h(LCAR, 27, LAID)
emit_h(LCAR, _ANGLE, LAID, LPTR)
emit_h(LCAR, SCR_RADIO, LAID, LPTR)
emit_h(LFUN, FUNC_GET_DISY)
emit_h(LADA, LASP)
emit_h(LFRM)
idxh = len(hijo)
emit_h(LJMP, 0)
fix_h.append((idxh + 1, "lh"))

code_end = code_base + len(codigo)
iloc_pub_len = 44
hijo_addr = code_end + iloc_pub_len
for ix, l in fix_h: hijo[ix] = hijo_addr + lab_h[l]
for ix, l in fixups:
    codigo[ix] = hijo_addr if l == "HIJO" else code_base + labels[l]

mem8 = hijo_addr + len(hijo)
plantilla = [0] * iloc_pub_len
plantilla[4] = 2
plantilla[11] = -1
plantilla[31] = 100
plantilla[36] = 1
plantilla[40] = -1
plantilla[41] = -1
plantilla[42] = -1
plantilla[43] = 32

globales = [0] * system_globals_words
payload = (struct.pack('<%di' % system_globals_words, *globales)
            + fpg_str
            + struct.pack('<%di' % len(codigo), *codigo)
            + struct.pack('<%di' % iloc_pub_len, *plantilla)
            + struct.pack('<%di' % len(hijo), *hijo))
n = len(payload)
assert n == (mem8 - long_header) * 4
header9 = struct.pack('<9i', 0, code_base, code_end, 0, 0, 0, iloc_pub_len, 0, mem8)
comprimido = zlib.compress(payload)
with open('test_min3b.div32', 'wb') as f:
    f.write(b'\x00' * 602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)
print(f"OK min3b: entry={code_base} hijo={hijo_addr} mem8={mem8} n={n}")
