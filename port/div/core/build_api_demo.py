#!/usr/bin/env python3
# Septima prueba manual de bytecode DIV (ver build_test_prg.py,
# build_frame_demo.py, build_sprite_demo.py, build_sound_demo.py,
# build_input_demo.py y build_collision_demo.py, y
# docs/architecture/13-handoff.md). Esta ejercita, desde bytecode real, el
# resto de builtins que usa un juego tipo STEROID (ver STEROID.PRG) y que
# aun no se habian probado en el intérprete: put_pixel, random, write,
# write_int, delete_text, fade/fading, set_mode, let_me_alone, get_id y las
# trigonometricas get_distx/get_disty (builtins 010/011).
#
# El programa se auto-cierra (~2.5 s a 60 fps): tras ~130 frames entra en un
# ciclo de fade (fade -> while(fading)frame -> let_me_alone -> espera a que
# get_id(TYPE 2) devuelva 0 -> delete_text -> fade de vuelta -> fin normal).
# Si alguna builtin falla, el programa se queda colgado (fallo ruidoso).
#
# Programa equivalente en lenguaje DIV (solo referencia, este script
# codifica el bytecode a mano):
#
#   program api_demo;
#   global
#       cont=0; titulo=0;
#   begin
#       set_mode(640480);           # m640x480 (codigo de modo en DIV)
#       load_fpg("tutor0.fpg");
#       load_fnt("tutor1.fnt");     # devuelve el id 1 (primer slot libre)
#       nave_api(160,120,24,11250); # hijo tipo 2 (x,y,radio,incr_angulo)
#       titulo=write(1,0,0,0,"DIV API REVIEW: put_pixel rand write_int fade get_id let_me_alone delete_text");
#       write_int(1,0,20,0,&cont);
#       write_int(1,0,40,0,&scan_code);
#       write_int(1,0,60,0,&fading);
#       loop
#           cont+=1;
#           for (i=0;i<120;i++) put_pixel(rand(0,639),rand(0,479),rand(1,160)); end
#           if (key(_esc)) break;
#           if (cont==130)
#               fade(0,0,0,8);
#               while (fading) frame; end
#               let_me_alone();                 # mata al hijo (tipo 2)
#               while (get_id(TYPE 2)) frame; end  # el hijo desaparece del scan
#               delete_text(titulo);            # se elimina el titulo
#               fade(100,100,100,8);
#               while (fading) frame; end
#               frame; frame; frame;
#               break;                          # FIN NORMAL (auto-cierre)
#           end
#           frame;
#       end
#   end
#
#   process nave_api(x,y,radio,incr);
#   begin
#       graph=4;
#       loop                                   # circulo con trigonometria real
#           x+=get_distx(angle,radio);         # (builtin 010)
#           y+=get_disty(angle,radio);         # (builtin 011)
#           angle+=incr;                       # 16 pasos/fase -> anillo cerrado
#           frame;
#       end
#   end
#
# CONVENCIONES (kernel.cpp / f.cpp / inter.h):
#  - ljpf (24) salta si (pila[sp--]&1)==0. get_id() devuelve la DIRECCION del
#    proceso (multiplo de iloc_len, siempre PAR), asi que NUNCA se puede
#    testear con ljpf directo (daria falso siempre): hay que convertir con
#    ligu/ldis (==/!=) que devuelven exactamente 0/1. key() si devuelve 0/1.
#  - get_id (009): (?escaneo) devuelve el id del primer proceso vivo
#    (_Status 2 o 4) de ese tipo.
#  - let_me_alone (066): pone _Status=1 a todos los procesos excepto el
#    llamante, y empuja 0.
#  - get_distx/get_disty (010/011): coge (angulo, distancia), angulo en
#    unidades DIV (pi=180000, 2*pi=360000 = vuelta completa). Devuelve el
#    incremento; la orden x+=... usa lada (42) sobre la direccion del campo.
#  - random (021): coge (min, max) y deja un entero en ese rango.
#  - put_pixel (028): coge (x,y,color) y escribe en el buffer copia2.
#      Es la orden que STEROID usa para las estrellas de fondo.
#  - write (016) / write_int (017): cogen (font,x,y,centro,ptr). write dejan
#    el id del texto en pila. delete_text (018) lo elimina.
#  - fade (014) coge (r,g,b,speed); la global "fading" esta en mem[1540]
#    (long_header 9 + end_struct 1520 + 11) y es 1 mientras dure el fundido.
#    scan_code vive en mem[1543]. read/write de globales de sistema = lcar
#    con la direccion absoluta + lptr (18).
#  - set_mode (036): en este port reasigna copia/copia2 al (ancho,alto) del
#    codigo 640480 (ancho*1000+alto); la tabla video_modes[] no lo contiene,
#    asi que cae al fallback 640x480 = lo mismo que ya tiene la ventana.
import struct, zlib

# --- Opcodes usados (ver inter.h) ------------------------------------------
LCAR = 1      # carga una constante en pila
LASI = 2      # pop addr,valor -> mem[addr]=valor (deja valor en pila)
LIGU = 6      # pila[sp-1] = pila[sp-1]==pila[sp]  (0/1 puro)
LDIS = 7      # pila[sp-1] = pila[sp-1]!=pila[sp]  (0/1 puro)
LADD = 12     # pila[sp-1] += pila[sp] (sp--)
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
LADA = 42     # pila[sp-1]=mem[pila[sp-1]]+=pila[sp] (sp--)
LCAR2 = 60    # empuja dos constantes consecutivas
LCARAIDCPA = 68  # mem[field+id] = pila[_Param++]  (lee parametro en un campo)

# --- Codigos de funcion (function(), f.cpp 4253-4399) ----------------------
FUNC_SIGNAL      = 0
FUNC_KEY         = 1
FUNC_LOAD_FPG    = 3
FUNC_GET_ID      = 9     # get_id(tipo) -> id del primer proceso vivo del tipo
FUNC_GET_DISX    = 10    # get_distx(angulo,dist) -> inc en X
FUNC_GET_DISY    = 11    # get_disty(angulo,dist) -> inc en Y
FUNC_FADE        = 14    # fade(r,g,b,speed)
FUNC_LOAD_FNT    = 15    # load_fnt(fichero) -> id de fuente
FUNC_WRITE       = 16    # write(font,x,y,centro,str) -> id de texto
FUNC_WRITE_INT   = 17    # write_int(font,x,y,centro,&int)
FUNC_DELETE_TEXT = 18    # delete_text(id)
FUNC_RANDOM      = 21    # random(min,max)  (el "rand" del lenguaje DIV)
FUNC_PUT_PIXEL   = 28    # put_pixel(x,y,color)
FUNC_SET_MODE    = 36    # set_mode(codigo_modo)
FUNC_FADE_ON     = 58
FUNC_FADE_OFF    = 59
FUNC_LET_ME_ALONE = 66   # let_me_alone()

# --- Offsets de campo de proceso (inter.h, SS5.4 del handoff) --------------
_X       = 26
_Y       = 27
_GRAPH   = 29
_ANGLE   = 32
SCR_RADIO = 11    # _Dist1 (sin uso en render normal): "radio" del hijo
SCR_INCR  = 12    # _Dist2 (sin uso en render normal): "incr_angulo" del hijo

# --- Scancodes de teclado (divkeybo.h, juego de teclas de DIV) -------------
K_ESC = 0x01

# --- Direcciones absolutas de globales de sistema (inter.h) ----------------
# long_header=9, end_struct=long_header+1520=1529.
FADING_ADDR   = 1529 + 11    # mem[1540] -- global "fading"
SCAN_CODE_ADDR = 1529 + 14   # mem[1543] -- global "scan_code"

TIPO_HIJO = 2
GRAPH_HIJO = 4
M_640_480 = 640480           # codigo de modo m640x480 (ancho*1000+alto)
ILOC_PUB = 44                # numero de campos publicos del molde de proceso

long_header = 9
system_globals_words = 1520 + 68     # = 1588
code_start = long_header + system_globals_words

# --- Cadenas de texto embebidas (recursos) ---------------------------------
def cstr(s):
    b = s.encode('latin-1') + b"\x00"
    b += b"\x00" * ((-len(b)) % 4)
    return b

fpg_str   = cstr("tutor0.fpg")
fnt_str   = cstr("tutor1.fnt")
title_str = cstr("DIV API REVIEW: put_pixel rand write_int fade get_id "
                 "let_me_alone delete_text")

FONT_ID = 1                  # primer slot libre tras load_fnt

str_area = len(fpg_str) + len(fnt_str) + len(title_str)   # bytes, multiplo de 4
fpg_addr   = code_start
fnt_addr   = code_start + len(fpg_str) // 4
title_addr = code_start + (len(fpg_str) + len(fnt_str)) // 4

RAM_CNT    = code_start + str_area // 4          # "global" cont
RAM_TITLE  = RAM_CNT + 1                         # "global" titulo (id de texto)
code_base  = RAM_CNT + 2

# --- Ensamblador del proceso principal -------------------------------------
codigo = []
fixups = []
labels = {}

def mark(label):
    labels[label] = len(codigo)

def emit(*words):
    codigo.extend(words)

# set_mode(640480);
emit(LCAR, M_640_480, LFUN, FUNC_SET_MODE, LASP)

# load_fpg("tutor0.fpg");   load_fnt("tutor1.fnt");   (descartamos los ids)
emit(LCAR, fpg_addr, LFUN, FUNC_LOAD_FPG, LASP)
emit(LCAR, fnt_addr, LFUN, FUNC_LOAD_FNT, LASP)

# nave_api(160,120,24,11250);  -> lcar2 x2 + lcal (operando = addr del codigo)
emit(LCAR2, 160, 120)
emit(LCAR2, 24, 11250)
idx_cal = len(codigo)
emit(LCAL, 0)
fixups.append((idx_cal + 1, "HIJO"))
emit(LASP)

# titulo=write(1,0,0,0,&title_str);   (lasi: addr en sp-1, valor en sp;
#  por eso se empuja RAM_TITLE ANTES de la llamada, que deja el id arriba)
emit(LCAR, RAM_TITLE)
emit(LCAR, FONT_ID, LCAR, 0, LCAR, 0, LCAR, 0, LCAR, title_addr,
     LFUN, FUNC_WRITE)
emit(LASI, LASP)

# write_int(1,0,20,0,&cont);  write_int(1,0,40,0,&scan_code);
# write_int(1,0,60,0,&fading);
emit(LCAR, FONT_ID, LCAR, 0, LCAR, 20, LCAR, 0, LCAR, RAM_CNT,
     LFUN, FUNC_WRITE_INT, LASP)
emit(LCAR, FONT_ID, LCAR, 0, LCAR, 40, LCAR, 0, LCAR, SCAN_CODE_ADDR,
     LFUN, FUNC_WRITE_INT, LASP)
emit(LCAR, FONT_ID, LCAR, 0, LCAR, 60, LCAR, 0, LCAR, FADING_ADDR,
     LFUN, FUNC_WRITE_INT, LASP)

mark("loop")

# cont+=1;   (mem[RAM_CNT]+=1)
emit(LCAR, RAM_CNT, LCAR, 1, LADA, LASP)

# 120 estrellas aleatorias: put_pixel(rand(0,639),rand(0,479),rand(1,160));
for _ in range(120):
    emit(LCAR, 0, LCAR, 639, LFUN, FUNC_RANDOM)      # x = rand(0,639)
    emit(LCAR, 0, LCAR, 479, LFUN, FUNC_RANDOM)      # y = rand(0,479)
    emit(LCAR, 1, LCAR, 160, LFUN, FUNC_RANDOM)      # color = rand(1,160)
    emit(LFUN, FUNC_PUT_PIXEL, LASP)

# if (key(_esc)) break;
idx_esc = len(codigo)
emit(LCAR, K_ESC, LFUN, FUNC_KEY, LJPF, 0)
fixups.append((idx_esc + 5, "continue"))
emit(LRET)
mark("continue")

# if (cont==130) { ...fase de fuego artificial...; break; }
idx_fase = len(codigo)
emit(LCAR, RAM_CNT, LPTR, LCAR, 130, LIGU, LJPF, 0)
fixups.append((idx_fase + 7, "no_fase"))

#     fade(0,0,0,8);
emit(LCAR, 0, LCAR, 0, LCAR, 0, LCAR, 8, LFUN, FUNC_FADE, LASP)
#     while (fading) frame; end
mark("fade_off")
idx_f1 = len(codigo)
emit(LCAR, FADING_ADDR, LPTR, LJPF, 0)
fixups.append((idx_f1 + 4, "fade_off_done"))
emit(LFRM)
idx_f2 = len(codigo)
emit(LJMP, 0)
fixups.append((idx_f2 + 1, "fade_off"))
mark("fade_off_done")

#     let_me_alone();   (mata al hijo tipo 2)
emit(LFUN, FUNC_LET_ME_ALONE, LASP)

#     while (get_id(TYPE 2)) frame; end   (espera a que el hijo desaparezca)
mark("wait_gone")
idx_g = len(codigo)
emit(LCAR, TIPO_HIJO, LFUN, FUNC_GET_ID, LCAR, 0, LDIS, LJPF, 0)
fixups.append((idx_g + 8, "gone"))
emit(LFRM)
idx_g2 = len(codigo)
emit(LJMP, 0)
fixups.append((idx_g2 + 1, "wait_gone"))
mark("gone")

#     delete_text(titulo);
emit(LCAR, RAM_TITLE, LPTR, LFUN, FUNC_DELETE_TEXT, LASP)

#     fade(100,100,100,8);
emit(LCAR, 100, LCAR, 100, LCAR, 100, LCAR, 8, LFUN, FUNC_FADE, LASP)
#     while (fading) frame; end
mark("fade_on")
idx_f3 = len(codigo)
emit(LCAR, FADING_ADDR, LPTR, LJPF, 0)
fixups.append((idx_f3 + 4, "fade_on_done"))
emit(LFRM)
idx_f4 = len(codigo)
emit(LJMP, 0)
fixups.append((idx_f4 + 1, "fade_on"))
mark("fade_on_done")

#     frame; frame; frame;   (unos frames estaticos para ver el resultado)
emit(LFRM)
emit(LFRM)
emit(LFRM)
emit(LRET)                 # FIN NORMAL: acaba el programa (auto-cierre)
mark("no_fase")

emit(LFRM)
idx_loop = len(codigo)
emit(LJMP, 0)
fixups.append((idx_loop + 1, "loop"))

# --- Ensamblador del proceso "nave_api" (tipo 2) ---------------------------
hijo = []
fixups_h = []
labels_h = {}

def emit_h(*w): hijo.extend(w)
def mark_h(l): labels_h[l] = len(hijo)

emit_h(LCBP, 4)                    # 4 parametros: x,y,radio,incr
emit_h(LTYP, TIPO_HIJO)
emit_h(LCARAIDCPA, _X)             # _X = param1 (x)
emit_h(LCARAIDCPA, _Y)             # _Y = param2 (y)
emit_h(LCARAIDCPA, SCR_RADIO)      # SCR_RADIO = param3
emit_h(LCARAIDCPA, SCR_INCR)       # SCR_INCR = param4
emit_h(LCAR, _GRAPH, LAID, LCAR, GRAPH_HIJO, LASI, LASP)
emit_h(LCAR, _ANGLE, LAID, LCAR, 0, LASI, LASP)
mark_h("loop_h")
# x+=get_distx(angle,radio);
emit_h(LCAR, _X, LAID)                         # direccion de x
emit_h(LCAR, _ANGLE, LAID, LPTR)               # valor de angle
emit_h(LCAR, SCR_RADIO, LAID, LPTR)            # valor de radio
emit_h(LFUN, FUNC_GET_DISX)
emit_h(LADA, LASP)
# y+=get_disty(angle,radio);
emit_h(LCAR, _Y, LAID)
emit_h(LCAR, _ANGLE, LAID, LPTR)
emit_h(LCAR, SCR_RADIO, LAID, LPTR)
emit_h(LFUN, FUNC_GET_DISY)
emit_h(LADA, LASP)
# angle+=incr;
emit_h(LCAR, _ANGLE, LAID, LCAR, SCR_INCR, LAID, LPTR, LADA, LASP)
emit_h(LFRM)
idx_h = len(hijo)
emit_h(LJMP, 0)
fixups_h.append((idx_h + 1, "loop_h"))

# --- Direcciones y resolucion de fixups ------------------------------------
code_words = codigo
code_end = code_base + len(code_words)      # = direccion del molde (iloc)
hijo_addr = code_end + ILOC_PUB             # el hijo va tras el molde

for idx, lab in fixups_h:
    hijo[idx] = hijo_addr + labels_h[lab]

for idx, lab in fixups:
    if lab == "HIJO":
        codigo[idx] = hijo_addr
    else:
        codigo[idx] = code_base + labels[lab]

mem8 = hijo_addr + len(hijo)

# --- Payload: globales de sistema + strings + RAM + codigo + molde + hijo --
plantilla = [0] * ILOC_PUB
plantilla[4] = 2     # _Status=2 (vivo): en un .div32 real lo trae el molde
plantilla[11] = -1   # SCR_RADIO/_Dist1 (default del molde original)
plantilla[12] = -1   # SCR_INCR/_Dist2
plantilla[31] = 100  # size
plantilla[36] = 1    # height
plantilla[40] = -1
plantilla[41] = -1
plantilla[42] = -1
plantilla[43] = 32

globales = [0] * system_globals_words
payload = (struct.pack('<%di' % system_globals_words, *globales)
           + fpg_str + fnt_str + title_str
           + struct.pack('<2i', 0, 0)          # RAM_CNT, RAM_TITLE
           + struct.pack('<%di' % len(code_words), *code_words)
           + struct.pack('<%di' % ILOC_PUB, *plantilla)
           + struct.pack('<%di' % len(hijo), *hijo))
n = len(payload)
assert n == (mem8 - long_header) * 4, (n, (mem8 - long_header) * 4)

mem0, mem1, mem2 = 0, code_base, code_end
mem3, mem4 = 0, 0
mem5, mem6, mem7 = 0, ILOC_PUB, 0
header9 = struct.pack('<9i', mem0, mem1, mem2, mem3, mem4, mem5, mem6, mem7, mem8)
comprimido = zlib.compress(payload)
stub_602 = b'\x00' * 602

with open('test_api_demo.div32', 'wb') as f:
    f.write(stub_602)
    f.write(header9)
    f.write(struct.pack('<I', n))
    f.write(comprimido)

print(f"OK: entry={mem1} iloc={mem2} hijo={hijo_addr} mem8={mem8} "
      f"n={n} comprimido={len(comprimido)} bytes")
print("Durante ~130 frames: estrellas aleatorias (put_pixel+random), texto de "
      "cabecera y un procesito tipo 2")
print("trazando un anillo (get_distx/get_disty). El contador (write_int) sube; "
      "si pulsa una tecla se ve su scand")
print("code en la linea 2 (scan_code). Despues: fade a negro, let_me_alone "
      "(el anillo desaparece), vuelve la")
print("pantalla clara y el programa se auto-cierra por si solo. ESC corta "
      "antes; Ctrl+ESC cierra el runtime.")