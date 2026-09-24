# 14 — Formato `.div32` / bytecode EML: especificación mínima de referencia

> **Para qué sirve este documento**: es la referencia independiente del formato de bytecode que produce `divc.cpp` y consume `DIV32RUN`. Nace como "mano derecha" del port del compilador (backlog §8 del handoff): permite verificar la salida del compilador portado byte a byte sin tener que releer `divc.cpp`/`i.cpp`/`kernel.cpp`. Todo lo aquí afirmado está verificado en caliente por los 7 demos `port/div/core/build_*_demo.py`, que construyen `.div32` válidos a mano siguiendo exactamente esta especificación.
>
> Fuentes: `13-handoff.md` §5, `07-modelo-ejecucion.md` §"Formato de bytecode", `src/div32run/inter.h`, `system/ltobj.def`, y los scripts de demo citados.

Última actualización: 2026-09-18 (checkpoint 14, tag `0.0.6-runtime+api`).

---

## 1. Formato en disco del fichero

```
┌────────────┬──────────────────────────────────────────────────────────┐
│ Offset     │ Contenido                                                │
├────────────┼──────────────────────────────────────────────────────────┤
│ 0          │ 602 bytes: div_stub (binario DOS 16-bit, firma "MZ").    │
│            │ En ficheros de prueba manual: todo ceros (el runtime     │
│            │ nunca lo ejecuta; solo hace fseek(602) para saltarlo)    │
│ 602        │ 10 enteros LE (40 bytes): mem[0..8] + n                  │
│ 642        │ n bytes comprimidos con zlib compress()                  │
└────────────┴──────────────────────────────────────────────────────────┘
```

Cabecera de 10 enteros (little-endian, 4 bytes cada uno):

| Campo | Nombre | Significado |
|---|---|---|
| `mem[0]` | `program_type` + banderas | 0 = normal. El compilador le suma banderas: `+1024` build SHARE/demo, `+128` modo traza, `+512` ignorar errores. El runtime las separa restando (`i.cpp` ~1351) |
| `mem[1]` | entry point | `_IP` inicial del proceso "main": índice de `mem[]` del primer opcode |
| `mem[2]` | `iloc` | offset del "molde" de campos públicos (§4) — **no** es la ubicación del proceso en ejecución |
| `mem[3]` | `max_process` | 0 = heurística automática del runtime (256 KB–512 KB de `mem[]`) |
| `mem[4]` | — | sin uso confirmado en la carga |
| `mem[5]` | `iloc_priv` | nº de campos "privados"; 0 si no hay `PRIVATE` |
| `mem[6]` | `iloc_pub_len` | nº de campos "públicos"; mínimo 44 (§4) |
| `mem[7]` | — | sin uso confirmado en la carga |
| `mem[8]` | `imem` final | "longitud del cmp": código+locales+textos. De aquí arranca la asignación dinámica de procesos en ejecución |
| `n` (10º) | tamaño descomprimido | bytes exactos del payload tras `uncompress()` |

Payload descomprimido = `mem[9 .. mem[8]-1]` concatenado (se descomprime **directamente sobre `&mem[9]`** en el runtime). Orden de las secciones dentro del payload: globales de sistema (§2) → textos/literales → código → molde(s) de proceso.

`divc.cpp` escribe este formato en dos sitios idénticos (`system\EXEC.EXE` e `install\setup.ovl`): `fwrite(div_stub,1,602)` + 9 enteros + buffer `(mem[9..imem-1] ++ loc[])` comprimido + `n`.

## 2. Layout de `mem[]`: las globales de sistema (1588 palabras)

`long_header = 9`. Tras la cabecera, **antes** de cualquier dato/código de usuario, el compilador reserva un bloque fijo de **1588 palabras** para las globales de sistema que expone el lenguaje (`mouse`, `scroll`, `m7`, `joy`, `setup`, `net`, `m8`, `dirinfo`, `fileinfo`, `video_modes`, `timer[]`, `text_z`, `fading`, `shift_status`, `ascii`, `scan_code`, `joy_filter`, `joy_status`, `restore_type`, `dump_type`, `max_process_time`, `fps`, `argc`, `argv[]`, `channel[]`, `vsync`, `draw_z`, `num_video_modes`, `unit_size`). Exacto en `inter.h`:

```c
#define end_struct long_header+14+10*10+10*7+8+11+9+10*4+1026+146+32*3   // = long_header + 1520
#define unit_size mem[end_struct+67]   // 68 palabras despues de end_struct
```

**El primer hueco libre para datos/código de usuario es `mem[9 + 1588] = mem[1597]`.** Escribir código de usuario en `mem[9]` corrompe el struct `mouse` y crashea dentro del scheduler.

Direcciones absolutas útiles (verificadas en caliente, checkpoint 14): `fading` = `mem[1540]` (= 9 + 1520 + 11), `scan_code` = `mem[1543]`.

## 3. El proceso "main" y el molde (`iloc`)

`inicializacion()` (`i.cpp` ~213-229) copia con `memcpy` los `iloc_pub_len` (`mem[6]`) campos desde `mem[iloc]` hacia el nuevo proceso, y **después** fija explícitamente `_Id`, `_IdScan`, `_Status`, `_IP` (con `mem[1]`). El molde es un bloque de **valores por defecto**, no la posición del proceso.

Los 44 campos de proceso (offsets `_Campo` 0-43, `inter.h`):

```
0  _Id            10 _Painted       20 _Father        30 _Flags         40 _M8_Wall
1  _IdScan        11 _Dist1/_M8_Object  21 _Son      31 _Size          41 _M8_Sector
2  _Bloque        12 _Dist2/_Old_Ctype  22 _SmallBro 32 _Angle         42 _M8_NextSector
3  _BlScan        13 _Frame         23 _BigBro        33 _Region        43 _M8_Step
4  _Status        14 _x0            24 _Priority      34 _File
5  _NumPar        15 _y0            25 _Ctype         35 _XGraph
6  _Param         16 _x1            26 _X             36 _Height
7  _IP            17 _y1            27 _Y             37 _Cnumber
8  _SP            18 _FCount        28 _Z             38 _Resolution
9  _Executed      19 _Caller        29 _Graph         39 _Radius
```

**Defaults NO nulos obligatorios en el molde** (`system/ltobj.def`, sección `local`): sin ellos el comportamiento es incorrecto de formas no obvias (sprite invisible sin error, etc.):

```
offset 4  (_Status)        = 2    (vivo)
offset 11 (_M8_Object)     = -1
offset 31 (_Size)          = 100  (% de tamaño; en 0 el sprite es INVISIBLE)
offset 36 (_Height)        = 1
offset 40 (_M8_Wall)       = -1
offset 41 (_M8_Sector)     = -1
offset 42 (_M8_NextSector) = -1
offset 43 (_M8_Step)       = 32
```

## 4. Cadenas de texto en bytecode

Un "string" es un **entero = índice de `mem[]`**; su contenido reinterpretado como bytes (`(char*)&mem[offset]`) es una cadena C terminada en NUL. Basta colocar los bytes ASCII+NUL en cualquier hueco word-alineado y pasar el índice con `lcar`. La marca `0xDAD0...` de `nullstring[]` solo la necesitan los opcodes de manipulación de cadenas (`lstrcpy`/`lstrcat`/...), no la carga de ficheros.

## 5. Recursos: la ruta pasada al builtin se IGNORA

`open_file()` (`f.cpp`, versión sin DEBUG) hace `_splitpath()` sobre la ruta y **se queda solo con nombre+extensión**, reconstruyendo `<ext_sin_punto>\<nombre>.<ext>` relativo al cwd. Consecuencia: `load_fpg("tutor0.fpg")` exige `<cwd>\fpg\tutor0.fpg`; `load_fnt("tutor1.fnt")` exige `<cwd>\fnt\tutor1.fnt`; etc. Es el mecanismo real y original de organización de recursos de un juego DIV.

## 6. ISA: opcodes usados por los demos (tabla completa en `inter.h` ~154-293)

El opcode ocupa el **byte bajo** de una palabra de 32 bits (`switch((byte)mem[ip++])`).

| Opcode | Valor | Operandos | Efecto |
|---|---|---|---|
| `lnop` | 0 | — | no-op |
| `lcar` | 1 | 1 palabra (valor) | `pila[++sp]=valor` |
| `lasi` | 2 | — | pop addr (en `sp-1`) y valor (en `sp`): `mem[addr]=valor`; deja `valor` en pila |
| `ligu`/`ldis` | 6/7 | — | `pila[sp-1] = (pila[sp-1]==/!=pila[sp])` → 0/1 puro; `sp--` |
| `ladd`/`lsub` | 12/13 | — | `pila[sp-1]=pila[sp-1]±pila[sp]; sp--` |
| `lptr` | 18 | — | `pila[sp]=mem[pila[sp]]` |
| `laid` | 20 | — | `pila[sp]+=id` (offset de campo → dirección real del proceso actual) |
| `lcid` | 21 | — | `pila[++sp]=id` |
| `ljmp` | 23 | 1 palabra (dir. absoluta) | `ip=mem[ip]` |
| `ljpf` | 24 | 1 palabra (dir. absoluta) | si `(pila[sp--]&1)==0` salta entonces; si no, `ip++` |
| `lfun` | 25 | 1 palabra (código función) | invoca `function()` (`f.cpp`) |
| `lcal` | 26 | 1 palabra (dir. **absoluta** del código del proceso) | crea proceso: copia el molde, el hijo corre hasta su primer `lfrm` y devuelve el control al padre dejando su id en pila |
| `lret` | 27 | — | auto-elimina el proceso actual |
| `lasp` | 28 | — | `sp--` |
| `lfrm` | 29 | — | **FRAME**: cede la ejecución hasta el próximo fotograma |
| `lcbp` | 30 | 1 palabra (num_par) | `mem[_NumPar]=x; mem[_Param]=sp-x+1` |
| `ltyp` | 32 | 1 palabra (tipo) | `mem[_Bloque]=tipo` (el que usa `collision()`) |
| `lada`/`lsua` | 42/43 | — | `pila[sp-1]=mem[pila[sp-1]]+=pila[sp]` (o `-=`); la dirección se empuja primero |
| `lcar2` | 60 | 2 palabras | empuja dos constantes consecutivas |
| `lcaraidcpa` | 68 | 1 palabra (offset de campo) | `mem[campo+id]=pila[_Param++]` — lee el parámetro n-ésimo en un campo |

**Convención de pila para expresiones/asignaciones**: asignar un campo de proceso = `lcar <campo>; laid; lcar <valor>; lasi; lasp;`. Incrementar un campo = `lcar <campo>; laid; lcar <valor>; lada; lasp;`.

## 7. Funciones internas (`lfun <código>`) — verificadas por los demos

Tabla completa de ~165 builtins en `system/ltobj.def` (líneas `function NNN ...`), implementada en el switch de `function()` (`f.cpp` ~4245). Argumentos: se empujan con `lcar` **de izquierda a derecha**; la implementación desapila en orden inverso; el primer argumento queda en `pila[sp]` como slot de retorno.

| Código | Nombre | Firma y notas |
|---|---|---|
| 0 | `signal` | `(tipo, código)` — `s_kill=0` → `_Status=1` a todos los del tipo |
| 1 | `key` | `(scan_code)` → 1 mientras pulsada |
| 3 | `load_fpg` | `(fichero)` → id de fpg |
| 8 | `collision` | `(tipo)` → id del proceso solapado (AABB, ignora rotación) |
| 9 | `get_id` | `(tipo)` → id del primer vivo del tipo (dirección **par**: testear con `ligu`/`ldis`, nunca con `ljpf` directo) |
| 10/11 | `get_distx`/`get_disty` | `(ángulo, distancia)` → inc X/Y (vuelta = 360000) |
| 14 | `fade` | `(r,g,b,speed)` — `fading` (`mem[1540]`) = 1 mientras dure |
| 15 | `load_fnt` | `(fichero)` → id de fuente (slot libre desde 1) |
| 16/17 | `write`/`write_int` | `(font,x,y,centro,ptr)` → id de texto |
| 18 | `delete_text` | `(id)` — 0 = todos |
| 21 | `random` | `(min,max)` → entero en rango |
| 24 | `put` | `(file,graf,x,y)` → id de gráfico |
| 28 | `put_pixel` | `(x,y,color)` — escribe en `copia2` |
| 33 | `clear_screen` | `()` → 0 (sin argumentos) |
| 36 | `set_mode` | `(código=ancho*1000+alto)` |
| 37 | `load_pcm`/`load_wav` | `(fichero,loop)` → id de sonido |
| 39 | `sound` | `(id_sonido,volumen 0..256,frecuencia 0..256)` → id de canal |
| 66 | `let_me_alone` | `()` — mata a todos menos al llamante |

## 8. Render automático de proceso

Un proceso con `_Ctype==0` (default) y `_Graph>0` se pinta **solo** cada fotograma (`frame_end()` en `i.cpp` ~975-1059 ordena por `_Z` todo lo pintable y llama a `pinta_sprite()`). Mostrar algo = fijar `_File`/`_Graph`/`_X`/`_Y`/`_Z` del proceso; no hace falta ningún builtin de dibujo. Si `_Angle!=0` y `_XGraph<=0` entra la ruta de rotación (`sp_rotado`).

## 9. Creación de procesos desde bytecode (receta `lcal`)

Secuencia canónica (ver `build_collision_demo.py` / `build_api_demo.py`):

```
lcar2 <arg1> <arg2>      ; argumentos del hijo (tantos lcar/lcar2 como parámetros)
lcal <dir_absoluta_hijo> ; el operando ES la dirección del código, no un puntero intermedio
lasp                     ; descarta el id del hijo
```

El hijo empieza con:

```
lcbp <num_par>                ; imprescindible para que su lfrm restituya la pila
ltyp <tipo>                   ; opcional (collision/get_id/signal por tipo)
lcaraidcpa <campo> × num_par  ; lee cada parámetro en un campo (p.ej. _X, _Y)
```

Errores que producen `0xC0000005` (verificados, checkpoints 13-14): doble indirección en `lcal` (operando → palabra de datos → código), y desbalances de pila en bytecode escrito a mano.

## 10. Verificación automatizada: `e()` sale con código 26

`e()` (errores no críticos, `i.cpp:1262`) imprime `Error NNN (función) texto` y hace `exit(26)` — **el mismo código que la terminación normal**. Para distinguir aborto de fin normal: mirar stdout (línea `Error`) o medir duración (un aborto es inmediato).

## 11. Los 7 demos como implementación canónica

| Script | Qué demuestra de esta spec |
|---|---|
| `build_test_prg.py` | Cabecera + `lret` mínimo |
| `build_frame_demo.py` | `lfrm` × 180 |
| `build_sprite_demo.py` | Molde con defaults (§3), render automático (§8), `load_fpg` (§5) |
| `build_sound_demo.py` | `load_pcm`+`sound`, cadenas en `mem[]` (§4) |
| `build_input_demo.py` | `key`, `ljpf`/`ljmp`, `lada`/`lsua`, `clear_screen` |
| `build_collision_demo.py` | `lcal`/`lcbp`/`ltyp`/`lcaraidcpa` (§9), `collision`, `signal` |
| `build_api_demo.py` | `set_mode`, `load_fnt`, `write`/`write_int`/`delete_text`, `put_pixel`/`random`, `fade`/`fading`, `let_me_alone`/`get_id`, `get_distx`/`get_disty`, globales absolutas (§2) |
