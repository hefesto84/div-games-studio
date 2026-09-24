import struct, zlib

LCAR = 1; LASI = 2; LIGU = 6; LADD = 12; LPTR = 18; LAID = 20
LJMP = 23; LJPF = 24; LFUN = 25; LRET = 27; LASP = 28; LFRM = 29; LADA = 42

FUNC_KEY = 1
FUNC_LOAD_FPG = 3
FUNC_RANDOM = 21
FUNC_PUT_PIXEL = 28

K_ESC = 0x01

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

mark("loop")
for _ in range(120):
    emit(LCAR, 0, LCAR, 639, LFUN, FUNC_RANDOM)
    emit(LCAR, 0, LCAR, 479, LFUN, FUNC_RANDOM)
    emit(LCAR, 1, LCAR, 160, LFUN, FUNC_RANDOM)
    emit(LFUN, FUNC_PUT_PIXEL, LASP)

idx = len(codigo)
emit(LCAR, K_ESC, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx + 5, "cont"))
emit(LRET)
mark("cont")
emit(LFRM)
idx2 = len(codigo)
emit(LJMP, 0)
fixups.append((idx2 + 1, "loop"))

for idx, l in fixups:
    codigo[idx] = code_base + labels[l]

code_words = codigo
code_end = code_base + len(code_words)

iloc_pub_len = 44
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
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % iloc_pub_len, *plantilla))
n = len(payload)
mem8 = code_end + iloc_pub_len
assert n == (mem8 - long_header) * 4

header9 = struct.pack('<9i', 0, code_base, code_end, 0, 0, 0, iloc_pub_len, 0, mem8)
comprimido = zlib.compress(payload)
with open('test_min_debug.div32', 'wb') as f:
    f.write(b'\x00' * 602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)
print(f"OK min_debug: entry={code_base} mem8={mem8} n={n}")