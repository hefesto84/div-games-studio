# Progreso del port a Windows 11 nativo (raylib) — bitácora técnica

> Este documento registra, paso a paso y con el mayor detalle posible, el trabajo real de implementación del port descrito en [`11-port-windows11-mikedx.md`](11-port-windows11-mikedx.md) (Fase 0 y Fase 1). Está pensado para que otro modelo de IA (o vos mismo en otra sesión, con otra herramienta) pueda retomar exactamente donde se dejó, sin tener que re-descubrir nada de lo ya investigado. Se actualiza incrementalmente: cada sección nueva se añade al final con fecha, no se reescribe el historial.

---

## Estado general (actualizado 2026-09-18, checkpoint 13)

| Pieza | Estado |
|---|---|
| Fase 0 — toolchain (MSVC + raylib) | **Hecho**, committeado en `eb36c5f` |
| Fase 1 — capa de I/O (`port/io/`) sobre raylib | **Hecho**, committeado en `eb36c5f` |
| Fase 1 — intérprete portado (`port/div/core/`) | **11 ficheros del núcleo compilan limpio** (exit 0) con MSVC/x64 en modo C: `i.cpp`+`kernel.cpp`, `f.cpp`, `s.cpp`, `v.cpp`, `det_vesa.cpp`, `mouse.cpp`, `divkeybo.cpp`, `divlengu.cpp`, `divsound.cpp`, `port_stubs.c`, `port_misc.cpp`. |
| Integración `port/div/core/` ↔ `CMakeLists.txt` | **Hecho**: target `div_core` (`OBJECT` library) compila vía `cmake --build . --target div_core`. |
| Truncamientos de puntero reales en x64 (`FILE*`/`tabfiles`, pilas guardadas, `mem[]`, `divmalloc`) | **Corregidos** (checkpoint 3, ver §7) — salvo `kernel.cpp:254` (Fase 4) y un riesgo residual documentado en `divmalloc` (§7.4). |
| Backend de vídeo (`v.cpp` → `port/io`) | **Hecho** (checkpoint 5, ver §9). |
| Ratón/teclado (`mouse.cpp`/`divkeybo.cpp` → `port/io`) | **Hecho** (checkpoint 6, ver §11) — confirmado por enlace de prueba: ya no aparecen como símbolos sin resolver. |
| **Input confirmado con bytecode DIV** (`key()`, función 001) | **Confirmado interactivamente por el usuario** (checkpoint 12, ver §20): un bytecode de prueba (`build_input_demo.py`) mueve un sprite con las flechas y cierra con ESC. Cierra el círculo gráficos + input + sonido ejercitados de punta a punta desde el intérprete. |
| Sonido (`divsound.cpp` → `port/io`) | **Hecho**: efectos vía raylib (checkpoint 8, ver §15.1) y música de tracker (MOD/S3M/XM) con libmikmod vendored (ver §39). |
| **Audio real confirmado con bytecode DIV** (`load_pcm` + `sound`, funciones 037/039) | **Confirmado auditivamente por el usuario** (checkpoint 11, ver §19): un bytecode de prueba (`build_sound_demo.py`) carga un `.pcm` real y lo reproduce (~10 s de programa, 3 bips 880 Hz + tono 440 Hz). De paso se arregló una colisión de símbolos raylib/div_core que silenciaba todo el audio (ver §19). |
| Stubs de subsistemas no portados (`port_stubs.c`) | **Hecho** (checkpoint 8, ver §15.2): DLLs/plugins (pool `DIV_export`/`DIV_import` real), Modo-8, FLI, CD, red, sistema de objetos, `mul_24`/`mul_16` (implementación real), memoria DOS/DPMI, `_heapshrink`. |
| zlib para MSVC/x64 | **Hecho** (checkpoint 8, ver §15.3): compilado como fuente propia dentro de `div_core` (no existía ninguna `.lib` precompilada para MSVC). |
| **Enlace completo, cero símbolos sin resolver** | **Hecho** (checkpoint 8, ver §15.4): `div32run_port.exe` enlaza limpio. `main()` ya existía en `i.cpp` — no hizo falta escribir uno nuevo. |
| **Ejecución real confirmada** (banner + bytecode de prueba) | **Hecho** (checkpoint 8, ver §15.5-§15.7): el binario imprime el banner real de DIV32RUN, y un bytecode DIV mínimo construido a mano (`build_test_prg.py`) corre de punta a punta a través de `inicializacion()` → `frame_start()` → `exec_process()`/`nucleo_exec()` (opcode real `lret`) → `frame_end()` → `finalizacion()`, verificado tanto con un build instrumentado como con el `.exe` de producción. |
| Limitador de fotogramas (`frame_start()`) | **Corregido** (checkpoint 9, ver §16): `reloj` nunca avanzaba (hueco real del port, sin IRQ de temporizador restaurada), disparando en cada frame la rama de "hardware de sonido roto" y reiniciando el dispositivo de audio sin parar. Arreglado con un reloj real (`port_get_reloj`) + una cesión de CPU por vuelta (`port_frame_yield`, `Sleep(1)`) — verificado con un bytecode de demostración visual (`build_frame_demo.py`) que ahora mantiene la ventana abierta ~7.5s (180 frames a los 24fps por defecto de DIV) con un único `AUDIO: Device initialized` en todo el log. |
| **Carga de recursos reales + render de sprite** (`load_fpg` + pipeline automático de proceso) | **Confirmado visualmente por el usuario** (checkpoint 10, ver §17): un bytecode de prueba (`build_sprite_demo.py`) carga un `.fpg` real y pinta un gráfico de 320×200 en pantalla durante ~10s, usando `_Graph`/`_X`/`_Y` del proceso "main" (el mecanismo normal de cualquier juego DIV, sin llamar a `put()` a mano). De paso se documentó una convención real de DIV (no un bug): `open_file()` en producción ignora la ruta pasada y busca en `<extensión>\<nombre>.<extensión>` relativo al directorio de trabajo. |
| **`collision()` portada y confirmada con bytecode DIV** (función 008) | **Hecho** (checkpoint 13, ver §21): AABB por el rectángulo del gráfico (`_X`/`_Y`/`_Size`/pivot y `_Ctype`/`_Resolution` ya incluidos), reemplaza al stub. Se probó con un bytecode (`build_collision_demo.py`) con dos procesos (`main` + proceso hijo creado con `lcal`, dos tipos distintos) que avanza uno hacia el otro y **el programa se auto-cierra al colisionar** (~600 ms, justo cuando los rectángulos se tocan). De paso: los parámetros a procesos (`lcar2`+`lcbp`+`lcaraidcpa`) y `signal(0)` quedaron verificados. |
| **Vídeo FLI/FLC en el runtime** (`start_fli`/`frame_fli`/`end_fli`/`reset_fli`, funciones 043-046) | **Confirmado visualmente por el usuario** (ver §51): decodificador FLI/FLC nuevo (`port_topflc.c`, API compatible con TopFLC v1.0 — la librería original no está vendored) + `divfli.cpp` copiado sin tocar. Bug real encontrado en el script de prueba, no en el port: los globales `restore_type`/`dump_type` deben inicializarse a 1 (volcado completo), no a 0, para que `copia2` se refleje en pantalla sin depender de sprites en escena. Confirmado con `INTRO.FLI` real (juego ALIEN, 121 frames). |

---

## 1. Qué se encontró al retomar el trabajo (contexto heredado de otro modelo)

Una sesión anterior (con otra herramienta, "opencode") dejó, sin commitear, la carpeta `port/div/` con:

- `port/div/core/{i.cpp,kernel.cpp,f.cpp,inter.h,divlengu.cpp}` — **copias byte a byte idénticas** a los originales de `src/div32run/` (confirmado con `diff`, 0 líneas de diferencia en el momento de retomar el trabajo). Estrategia correcta: no tocar el núcleo, interceptar sus `#include <dos.h>/<bios.h>/<i86.h>/<graph.h>` con headers "shim" propios.
- `port/div/core/{cdrom.h,divdll.h,divfli.h,divkeybo.h,divmixer.hpp,divsound.h,dll.h,pe_load.h,sysdac.h,06x08.h,include.div}` — copias también idénticas de sus contrapartes en `src/div32run/`.
- `port/div/core/{netlib.h,math_.h}` — reescritos/recortados a propósito (networking fuera de alcance; matemáticas dependientes de ASM simplificadas a la `<math.h>` estándar).
- `port/div/shim/{dos.h,bios.h,i86.h,graph.h}` — los headers de sustitución. **Estaban a medio escribir**: contenían errores reales de sintaxis y quedaban dos punteros vacíos (`union REGS {}`, `struct SREGS {}`) sin campos. Los timestamps de archivo mostraban que el trabajo se cortó literalmente a mitad de escribir `bios.h`/`dos.h`, antes de intentar compilar ni una vez.
- `port/div/judas/{judas.h,timer.h}` — copia del SDK público de JUDAS (de `3rdparty/judas/`), para que `inter.h` pueda `#include "judas/judas.h"`.
- **Nada de esto estaba enganchado a `CMakeLists.txt`** ni se había compilado nunca (no había `port_misc.cpp`, ni build dir, ni `.obj`).

## 2. Qué se corrigió/añadió en el checkpoint 1

Todos los cambios están pensados para tocar **lo mínimo posible** el código original. Regla seguida: primero intentar resolver cualquier incompatibilidad en un *header shim* (`port/div/shim/*.h` o `port/div/core/port_pre.h`/`port_forward_decls.h`); solo tocar `i.cpp`/`f.cpp`/`kernel.cpp` directamente cuando la incompatibilidad es imposible de resolver de otra forma (y en ese caso, con un comentario `/* PORT: ... */` explicando exactamente qué y por qué, para que sea trivial de localizar y revertir).

### 2.1 `port/div/shim/dos.h` — reescrito

- **Antes**: `union REGS { /* i86.h */ };` y `struct SREGS { /* i86.h */ };` vacíos (placeholders sin campos) — cualquier código que hiciera `regs.w.ax` o `regs.x.eax` no compilaba.
- **Ahora**: layout clásico Watcom/Borland completo — `struct WORDREGS`/`BYTEREGS`/`DWORDREGS` dentro de `union REGS` (acceso `.w`/`.h`/`.x`), y `struct SREGS` con `es/cs/ss/ds/fs/gs`.
- Se **elimina** la definición propia de `struct diskfree_t` (antes chocaba: `<direct.h>` de MSVC moderno tiene `#define diskfree_t _diskfree_t`, así que nuestra definición se reescribía a `struct _diskfree_t {...}` y colisionaba con la del propio CRT — error `C2011`). Ahora simplemente se **usa** el `struct diskfree_t` real del CRT (vía esa misma macro de alias), no se redefine.
- Se añade `struct find_t` (eliminado del UCRT moderno junto con toda la familia `_dos_*`) con campos `attrib`, `wr_time`, `wr_date`, `size`, `name[260]` — implementado sobre `_findfirst64`/`_findnext64` en `port_misc.cpp` (ver 2.5).
- Se añaden las declaraciones de `_dos_findfirst`, `_dos_findnext`, `_dos_setfileattr`, `_dos_getdrive`, `_dos_setdrive`, `_dos_getdiskfree`, `int386`, `int386x` (implementadas en `port_misc.cpp`).
- Se añaden `_HARDERR_IGNORE`/`_RETRY`/`_ABORT`/`_FAIL` (constantes de retorno clásicas de `_harderr`, usadas en `i.cpp:84`).
- Los macros `_A_NORMAL`/`_A_RDONLY`/... se guardan con `#ifndef` porque `port_misc.cpp` incluye además el `<io.h>` real (que ya los define con los mismos valores) — evita choque de redefinición en esa unidad concreta.

### 2.2 `port/div/shim/bios.h` — reescrito, simplificado

- Se **retiraron** las declaraciones especulativas de teclado vía BIOS (`bim_key_ready`/`bim_keybrd`/`bim_key_shift`) que estaban ahí antes: no las usa ningún fichero portado en este commit (confirmado por grep en `i.cpp`/`f.cpp`/`kernel.cpp`/`divlengu.cpp`), y tenían sintaxis inválida (`word bim_key_shift(void implicit NONE);` — "implicit NONE" no es C válido; probablemente el punto exacto donde se cortó el trabajo anterior). Cuando se porte el input de teclado, se hará directamente contra `port/io` (`io_key_down`/`io_key_pressed`), no vía un BIOS falso.
- Queda solo lo que **sí** se usa: `_bios_timeofday(int cmd, long *timerticks)` + `_TIME_GETCLOCK`, para sembrar el generador de números aleatorios en `i.cpp:241`.

### 2.3 `port/div/shim/i86.h` — dos cambios

1. Se añaden `int386`/`int386x` (declarados aquí originalmente en el diseño, pero movidos a `dos.h` junto a `union REGS`/`struct SREGS` para que estén todos juntos — ver 2.1).
2. **Bug encontrado al compilar en modo C** (`/TC`, ver §4): `inp`/`outp`/`inpw`/`outpw`/`_inp`/`_outp` son **intrínsecos reservados** del compilador MSVC en modo C (error `C2169: función intrínseca, no se puede definir`). Antes estaban implementados directamente con esos nombres (`static __inline unsigned inp(...)`), lo cual compilaba en C++ pero no en C. Se renombraron las implementaciones a `div_port_inp`/`div_port_outp` y se redirigen con macros (`#define inp(p) div_port_inp(p)`, etc.) — el código del núcleo sigue llamando a `inp(...)`/`outp(...)` tal cual, sin tocarlo.

### 2.4 `port/div/core/port_forward_decls.h` — nuevo

Header nuevo, incluido desde `port_pre.h`, con declaraciones adelantadas de funciones que el C de Watcom podía usar antes de definir (declaración implícita de K&R C) pero que MSVC en C estricto/C++ estricto rechaza. Por ahora solo tiene una entrada:

- `busca_packfile(void)` — definida en `i.cpp:1408` (ahora `port/div/core/i.cpp`, la misma línea), usada en `i.cpp:314` (dentro de `inicializacion()`, bajo `#ifndef DEBUG`) **antes** de su definición en el mismo fichero. Watcom C lo permitía (asumía `int` de retorno implícito para funciones no declaradas); MSVC no.

**Diseñado para crecer**: según avance la compilación de `f.cpp`/`kernel.cpp`, es muy probable que aparezcan más casos iguales (funciones grandes de 4000+ líneas suelen tener este patrón). Cuando aparezca uno nuevo, añadir aquí una entrada con el mismo formato (dónde se define, dónde se usa antes), no dispersar declaraciones sueltas por otros ficheros.

### 2.5 `port/div/core/port_misc.cpp` — nuevo

Unidad de compilación **aislada a propósito** (no incluye `port_pre.h` ni los headers del núcleo DIV) que implementa todo lo declarado en los shims de `dos.h`/`bios.h`. Al estar aislada puede incluir `<windows.h>`/`<io.h>` sin arriesgar colisión de macros con el resto del núcleo (`far`/`near`/`byte`/`word`, etc., que sí están redefinidos para el lado del núcleo). Contenido:

- `_bios_timeofday`: sobre `GetTickCount64()`. Solo se usa para sembrar un RNG, no necesita ser el reloj PIT real.
- `_dos_findfirst`/`_dos_findnext`/`_dos_setfileattr`: sobre `_findfirst64`/`_findnext64`/`SetFileAttributesA`. Empaqueta fecha/hora en el formato DOS de 16 bits que ya interpretan las macros `YEAR()`/`MONTH()`/... de `f.cpp` (sin tocar esas macros).
- `_dos_getdrive`/`_dos_setdrive`/`_dos_getdiskfree`: sobre `_getdrive`/`_chdrive`/`_getdiskfree` (misma numeración de unidad 1=A,2=B,... en ambos mundos).
- `_setvideomode`: no-op que devuelve éxito (el vídeo real lo gestiona `port/io`).
- `int386`/`int386x`: implementación **específica**, no genérica, para los **dos únicos puntos de uso reales** localizados por grep en `f.cpp` (no hay más en `i.cpp`/`kernel.cpp`/`divlengu.cpp`):
  1. `int386(0x21, ...)` con `AX=0x4409` dentro de `disk_free()` (`f.cpp` ~3281): consulta IOCTL "¿es un dispositivo de bloques con E/S genérica?". Se responde siempre que sí, para que el código siga por la rama que llama a `_dos_getdiskfree`.
  2. `int386x(0x31, ...)` con `EAX=0x0500` dentro de `GetFreeMem()`/`memory_free()` (`f.cpp` ~3259-3268): función DPMI "Get Free Memory Information", escribe 12 `unsigned long` (48 bytes) en `ES:EDI`. Se reconstruye el puntero plano con `FP_SEG`/`FP_OFF` y se rellena con datos reales de `GlobalMemoryStatusEx`, en unidades de página de 4 KB.
  - Cualquier otra interrupción no reconocida devuelve "error" (`cflag=1`) en vez de fallar en silencio con datos basura.
  - **LIMITACIÓN CONOCIDA, no resuelta, documentada en el propio código** (`port_misc.cpp`, comentario junto a `int386x`): `FP_SEG`/`FP_OFF` empaquetan un puntero partiéndolo en dos mitades de 16 bits (esquema real-mode). En un proceso x64 esto solo reconstruye la dirección original correctamente si esa dirección cabe en 32 bits, cosa que el SO **no garantiza** para una variable de pila corriente. Si el puntero real de `meminfo` (una variable local dentro de `memory_free()` en `f.cpp`) cae por encima de 4 GB, `GetFreeMem()` escribiría en una dirección incorrecta. Mitigación pendiente: lo correcto a medio plazo es que `memory_free()` no pase por `int386x` en el port (tocar `f.cpp` directamente, con el mismo criterio de "cambio mínimo documentado" que el resto). **No se ha tocado todavía porque `memory_free()` es un builtin raro del lenguaje DIV, poco usado en juegos reales** — se prioriza avanzar con la compilación general antes de perseguir este caso concreto.

### 2.6 Cambios directos en `i.cpp` (los únicos, documentados con `/* PORT: ... */` en el propio código)

1. **Línea ~199** (`ghost=...`): el original hace `(char*)((int)(ghost_inicial+512) & 0xFFFFFF00)` para alinear un puntero a un límite de 256 bytes. En x64, castear a `(int)` (32 bits) trunca la dirección real — no es solo un aviso del compilador, es un bug de memoria genuino en 64 bits. Se cambió a `(uintptr_t)(...) & ~(uintptr_t)0xFF` — **misma operación de alineación exacta**, sin perder los bits altos de la dirección.
2. **Líneas ~362/364** (`COM_export=CNT_export;` / `COM_export=CMP_export;`): `CNT_export`/`CMP_export` toman `char*`, pero `COM_export_t` (de `divdll.h`) espera `const char*`. Watcom permitía la conversión implícita; MSVC no la permite para punteros a función que solo difieren en la constancia de un parámetro. Se añadió un cast explícito `(COM_export_t)` en las dos asignaciones — no se tocó ninguna de las dos firmas reales.

### 2.7 `port/div/core/netlib.h` — un añadido

`i.cpp:144-145` declara `MAINSRV_Packet`/`MAINNOD_Packet` (funciones de red nunca implementadas ni llamadas en este commit) usando los tipos `WORD`/`BYTE`. En DOS los aporta `<dos.h>`; en Windows normalmente `<windows.h>`, que no incluimos en la unidad de `i.cpp`. Se añadieron los typedefs (`unsigned short`/`unsigned char`) a `netlib.h`, guardados con `#ifndef _WORD_DEFINED_PORT` por si en el futuro se acaba incluyendo `windows.h` en la misma unidad.

### 2.8 `port/div/core/port_pre.h` — el fix más importante de este checkpoint

**Causa raíz real encontrada** (después de una sesión larga de bisección — ver §5 si hace falta repetir el proceso): `inter.h` define, como parte legítima y original del código DIV, `#define _Size 31` (línea 466 — es el offset del campo "tamaño del gráfico" dentro del bloque de proceso en `mem[]`, documentado en `07-modelo-ejecucion.md`). Ese mismo nombre, `_Size`, lo usa **internamente** `<time.h>`/`corecrt.h` de MSVC como nombre de parámetro de plantilla:

```c
#define __DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(...) \
    extern "C++" { template <size_t _Size> inline ... }
```

Si `inter.h` se procesa **antes** que `<time.h>`, el preprocesador sustituye textualmente `_Size` por `31` dentro de la propia cabecera de `<time.h>`, generando `template <size_t 31>` (inválido) y una cascada larguísima de errores de sintaxis en `time.h`/`corecrt_wtime.h` que **no tienen nada que ver con el error real** (llevó tiempo diagnosticarlo por eso).

**Fix**: `port_pre.h` ahora fuerza la inclusión de **todas** las cabeceras estándar/CRT que `i.cpp`/`f.cpp`/`kernel.cpp` acaban incluyendo (`conio.h`, `stdlib.h`, `stdio.h`, `string.h`, `malloc.h`, `ctype.h`, `signal.h`, `errno.h`, `time.h`) **antes** de que `inter.h` tenga ocasión de definir sus constantes de campo. Como `port_pre.h` se fuerza con `/FI` (compile con `cl /FI port_pre.h ...`), esto pasa siempre antes que nada del propio `i.cpp`. La re-inclusión posterior de esas mismas cabeceras desde dentro de `i.cpp`/`inter.h` es un no-op inofensivo (ya están protegidas por sus propios include-guards).

**Riesgo latente no resuelto**: `inter.h` define muchas constantes cortas con prefijo `_` (`_Id`, `_X`, `_Y`, `_Z`, `_File`, `_Height`, etc. — lista completa en `inter.h` líneas 420-478). Front-cargar las cabeceras estándar en `port_pre.h` cubre las colisiones con esas cabeceras concretas, pero **no hay garantía de que no aparezca otra colisión igual** con otra cabecera del sistema más adelante (por ejemplo, si algún día se incluye `<windows.h>` en la misma unidad que `inter.h`). Si vuelve a aparecer una cascada de errores de sintaxis rara en un header del sistema que no tiene relación aparente con el código tocado, **sospechar primero de una colisión de macro de una sola palabra con una constante de campo de `inter.h`**, antes de asumir un bug del shim.

## 3. Receta exacta de compilación (fuera de CMake, para iterar rápido)

Válida en este checkpoint para compilar `i.cpp` (que incluye `kernel.cpp`) de forma aislada:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\Users\Dani\Documents\DIV
cl /c /nologo /TC /D_CRT_SECURE_NO_WARNINGS /FI port_pre.h ^
   /I port\div\core /I port\div /I port\div\shim /I 3rdparty\zlib /I 3rdparty\judas ^
   port\div\core\i.cpp
```

Notas sobre las flags, para no tener que re-derivarlas:

- **`/TC` (compilar como C, no C++)**: decisión clave de este checkpoint. `i.cpp`/`f.cpp`/`kernel.cpp`/`divlengu.cpp` no usan **ninguna** construcción real de C++ (confirmado con `grep -c "class \|template<\|new \|delete \|::\|operator\b" i.cpp f.cpp kernel.cpp divlengu.cpp` → 0 en los cuatro ficheros), pese a la extensión `.cpp`. Compilando como C++ estricto aparecían decenas de errores de conversión implícita `char*`↔`unsigned char*` y `void*`→puntero-a-función que Watcom permitía sin más (son exactamente el tipo de laxitud que un compilador C tolera y C++ no). Compilando como C (`/TC`), esos mismos casos vuelven a ser válidos (o como mucho warnings), tal como los escribió originalmente el código. **Recomendación fuerte para lo que sigue**: seguir compilando `f.cpp`/`kernel.cpp`/`divlengu.cpp` también con `/TC`, no intentar mantenerlos como C++.
- **`/FI port_pre.h`**: fuerza la inclusión de nuestro header de adaptación antes que nada (ver §2.8 sobre por qué el orden importa).
- **`/I port\div` (no solo `port\div\core`)**: necesario para que resuelvan los `#include "shim/bios.h"` (relativos, buscan primero junto al fichero que los incluye — `port/div/core/` — y si no lo encuentran ahí, recorren los `/I` en orden) y `#include "judas/judas.h"` de `inter.h`.
- **`/I 3rdparty\zlib` y `/I 3rdparty\judas`**: `inter.h` hace `#include <zlib.h>` y `port/div/judas/judas.h` hace `#include "judascfg.h"`/`"judaserr.h"`, que viven en `3rdparty/judas/` (no se copiaron, se referencian directamente — son C portable sin nada DOS-específico).
- **`/D_CRT_SECURE_NO_WARNINGS`**: silencia los avisos de "usa la versión `_s` de esta función" del CRT moderno (el código usa `strcpy`/`sprintf`/etc. a secas por todas partes; no es un problema de compilación, solo ruido).

## 4. Warnings que quedan tras compilar `i.cpp`/`kernel.cpp` (no bloquean, pero son riesgo real en x64)

```
port\div\judas/judas.h(74,96): warning C4081 (esperaba newline, encontró ';')
   -> #pragma pack (push,1); / (pop); con ; sobrante. Inocuo (confirmado
      con repro aislado): NO afecta el resultado del pragma. No tocado.
port\div\core\i.cpp(111,125): warning C4068 (pragma "aux" desconocida)
   -> #pragma aux es una extensión de Watcom para fijar la convención de
      llamada de una función en ASM inline; MSVC no la entiende y la
      ignora. Revisar si el código que sigue a esos #pragma aux depende
      de verdad de esa convención de llamada antes de dar por bueno que
      "ignorarlo sin más" es seguro (no confirmado en este checkpoint).
port\div\core\i.cpp(462,470,480,1339): warning C4311/C4312
   -> Mismo patrón que el bug de "ghost" ya corregido (ver 2.6.1):
      conversiones puntero<->int de 32 bits. NO CORREGIDAS TODAVIA en
      estas líneas -- son candidatas a bug real en x64 si el puntero cae
      por encima de 4 GB. Pendiente de revisar una a una con el mismo
      criterio (cambiar a uintptr_t conservando la operación exacta).
port\div\core\kernel.cpp(254): warning C4311
   -> Idem, dentro del kernel.cpp incluido. Pendiente de revisar.
```

**Recomendación para la siguiente sesión**: antes de dar el núcleo por "portado", localizar y corregir cada uno de estos sitios (son pocos, la lista de arriba es completa para `i.cpp`/`kernel.cpp` en este checkpoint) con el mismo patrón que `ghost` en 2.6.1. Cuando se compile `f.cpp` es esperable que aparezcan más — **no ignorar estos warnings en bloque**, cada uno es candidato a corromper memoria en una build de 64 bits real.

## 5. Próximos pasos concretos (en orden) — actualizado tras checkpoint 3

1. ~~Compilar `f.cpp`~~ y ~~compilar `divlengu.cpp`~~ — **hecho, ver §6**.
2. ~~Añadir target de CMake~~ — **hecho, ver §6** (`div_core`, `OBJECT` library).
3. ~~Revisar y corregir los warnings C4311/C4312 de truncamiento de puntero (`tabfiles`/`FILE*`, `guarda_pila`/`carga_pila`/`actualiza_pila`, alineación de `mem[]`, `divmalloc`)~~ — **hecho, ver §7**. Queda **un** warning de esta familia, deliberadamente sin tocar: `kernel.cpp:254` (opcode `lext`, llamada a función de DLL vía `call()`/`a.asm`) — pertenece al mecanismo de carga de DLLs, todavía sin implementar en el port (Fase 4 del plan), no tiene sentido arreglarlo hasta que se rediseñe ese mecanismo completo.
4. **Siguiente paso real: empezar a conectar el núcleo con `port/io`** (sustituir el backend de vídeo/sonido que hoy se resuelve vía `DIV_import()`/DLL por llamadas directas a `io_*`, ver `07-modelo-ejecucion.md` sección "Resolución de DLLs" para entender el mecanismo original) y escribir un `main()` que una todo — ese es ya el trabajo de "Fase 2: el editor" o, si se prioriza probar el runtime primero, un ejecutable de prueba mínimo.

## 6. Checkpoint 2 (2026-09-18, misma sesión) — `f.cpp`, `divlengu.cpp` y la integración con CMake

### 6.1 `f.cpp` — mismo patrón de forward-declarations que `busca_packfile`

Al compilar `f.cpp` con la receta del §3 aparecieron 12 casos más del mismo patrón exacto que `busca_packfile` (§2.4): funciones definidas más abajo en el propio `f.cpp` pero llamadas antes, dentro de otra función definida antes en el fichero. Watcom en modo C asumía `int` de retorno implícito para la llamada y no se quejaba luego al ver la definición real; MSVC sí (`error C2371`, "nueva definición; tipos básicos distintos"). Añadidas a `port_forward_decls.h` con sus firmas reales: `load_pal`, `adaptar`, `put_screen`, `texn2`, `expres0`..`expres5`, `get_token`, `_encriptar`, `_comprimir` (línea de definición y de primer uso anotadas en el propio header).

**Detalle no obvio**: las firmas de estas declaraciones adelantadas usan `unsigned char *` en vez de `byte *`, aunque el código real (`f.cpp`) sí usa `byte`. Motivo: `port_forward_decls.h` se incluye desde `port_pre.h`, que se fuerza con `/FI` **antes** de que `inter.h` tenga ocasión de ejecutar su `#define byte unsigned char` — en el momento en que se parsean estas declaraciones adelantadas, `byte` todavía no existe como tipo/macro. Si se añaden más declaraciones en el futuro que usen `byte`/`word`/`ulong`/etc., recordar usar el tipo real (`unsigned char`/`unsigned short`/`unsigned long`) en su lugar.

También se añadió `#define INTR_CF 1` a `port/div/shim/dos.h` (bit de acarreo x86 tras un `int386`, usado en `disk_free()` — ver `f.cpp` ~3282, ya documentado como uno de los dos usos reales de `int386`/`int386x` en §2.5).

Con estos dos cambios, **`f.cpp` compila limpio** (exit 0) con la misma receta del §3.

### 6.2 `divlengu.cpp` — mismo patrón, 5 casos más

`analiza_textos`, `an_numero`, `an_comentario`, `an_texto`, `coder` — mismo patrón exacto, añadidas a `port_forward_decls.h`. **`divlengu.cpp` compila limpio** (exit 0).

### 6.3 Inventario completo de warnings C4311/C4312 (truncamiento de puntero) tras compilar los tres ficheros

Esta es la lista completa a día de hoy (antes de este checkpoint solo se conocía la de `i.cpp`/`kernel.cpp`; con `f.cpp` compilado aparece un patrón más serio):

```
i.cpp:462,470,480,1339   -> int* <-> int               (ya inventariado en checkpoint 1, §4)
kernel.cpp:254           -> void* -> unsigned int       (ya inventariado en checkpoint 1, §4)
f.cpp:2895,2923,2945,2973,2995,3011,3026,3027,3028,3029
                          -> FILE* <-> int   *** IMPORTANTE ***
f.cpp:3896,3898          -> unsigned char* <-> int, int* <-> int
```

**El bloque `FILE* <-> int` de `f.cpp` es el más importante de corregir a continuación.** Es el mismo patrón de bug que `ghost` (§2.6.1) pero mucho más extendido: el lenguaje DIV expone al programa un "número de fichero" (`int`) para las operaciones de fichero (`fopen`/`fread`/`fwrite`/`fclose`-equivalentes del lenguaje), y la implementación original simplemente truncaba el `FILE*` real de la librería C a un `int` para guardarlo en `mem[]` (que es un array de `int`/32-bit en el diseño original). **En x64, un `FILE*` real casi seguro no cabe en 32 bits** — esto no es un aviso cosmético, es corrupción de memoria/crash esperable en cuanto un programa DIV portado abra un fichero. No se ha tocado todavía: antes de arreglarlo hay que decidir el diseño correcto (lo más razonable: una tabla de handles indexada por `int` que el código de `f.cpp` consulte, en vez de truncar el puntero real — parecido a como un descriptor de fichero POSIX es un `int` que el SO resuelve internamente a una estructura real; aquí tocaría que el port mantenga esa tabla, no el SO). Es un cambio de diseño más grande que un simple cast, por eso se pospone a un checkpoint dedicado en vez de improvisarlo ahora.

Los warnings `C4090`/`C4113` en `f.cpp:2810-2822` (calificadores `const` distintos al pasar funciones de comparación a `qsort`) son ruido de estilo (Watcom no exigía que el comparador de `qsort` tuviera exactamente `const void*,const void*`) — no representan riesgo de corrupción como los de arriba, se pueden posponer sin urgencia.

El warning `C4477` en `f.cpp:2320` (formato `%s` de `printf` recibiendo un `int*`) sí merece una mirada rápida en el próximo checkpoint — puede ser un bug real preexistente en el propio DIV32RUN (imprimir un puntero como si fuera texto) o un caso donde el formato y el argumento están descoordinados a propósito por algún motivo no evidente; no se ha investigado todavía.

### 6.4 `port_misc.cpp` — dos bugs propios, encontrados al compilarlo por primera vez

Este checkpoint fue la **primera vez que `port_misc.cpp` se compiló de verdad** (en el checkpoint 1 se había escrito pero nunca probado, porque los cl.exe manuales solo compilaban `i.cpp`/`f.cpp`/`divlengu.cpp`; recién apareció en la compilación al añadir el target `div_core` a CMake, que sí incluye `port_misc.cpp` en la biblioteca). Dos errores propios encontrados y corregidos:

1. **Nombre de tipo equivocado**: se usó `_finddata64_t` (que no existe en el UCRT moderno) en vez de `struct __finddata64_t` (doble guion bajo — el nombre real, ver `corecrt_io.h`). Existe una confusión de nombres parecida en el UCRT real: `_finddatai64_t` (con una "i" de "int64") sí es un alias de `__finddata64_t`, pero `_finddata64_t` a secas no existe. Corregido en `port_misc.cpp`.
2. **Enlace C++ vs C inconsistente** (`error C2732`, "la especificación de vinculación se contradice"): `port_misc.cpp` **define** `int386`, `int386x`, `_dos_findfirst`, `_dos_findnext`, `_dos_setfileattr`, `_dos_getdrive`, `_dos_setdrive`, `_dos_getdiskfree` como `extern "C"` (necesario para que el núcleo, compilado como C, pueda enlazarlas por nombre sin decorar) — pero sus **declaraciones** en `port/div/shim/dos.h` no estaban envueltas en `extern "C" { ... }`, así que al incluir ese mismo `dos.h` desde `port_misc.cpp` (que sí es C++ real, sin `/TC`), declaración y definición quedaban con enlace distinto. **Corregido envolviendo esas declaraciones concretas en `dos.h` con `#ifdef __cplusplus extern "C" { ... } #endif`** (no afecta a las unidades compiladas como C, donde `__cplusplus` ni siquiera existe). `_bios_timeofday` (`bios.h`) y `_setvideomode` (`graph.h`) ya estaban bien desde el checkpoint 1.

**Lección para lo que sigue**: cualquier función nueva que se añada a `port_misc.cpp` con `extern "C"` necesita su declaración correspondiente en el shim envuelta de la misma forma. Si en el futuro aparece de nuevo `error C2732`, es casi seguro este mismo problema.

### 6.5 Integración con `CMakeLists.txt` — target `div_core`

Se añadió un target `add_library(div_core OBJECT ...)` en el `CMakeLists.txt` raíz, con `port/div/core/{i.cpp,f.cpp,divlengu.cpp,port_misc.cpp}` como fuentes. **Todavía no se enlaza a nada** (ni al ejecutable `div_port` de la Fase 1, ni a un `.exe` propio) — es deliberadamente solo una biblioteca objeto para validar que el núcleo compila también vía `cmake --build`, no solo con `cl.exe` a mano.

Detalles de la configuración (ver el propio `CMakeLists.txt` para el texto exacto):
- `i.cpp`/`f.cpp`/`divlengu.cpp` se marcan con `set_source_files_properties(... PROPERTIES LANGUAGE C COMPILE_OPTIONS "/TC;/FIport_pre.h")` — el equivalente de CMake a la receta manual del §3.
- `port_misc.cpp` se deja **fuera** de esa propiedad a propósito: es C++ real (usa `extern "C"` desde un archivo `.cpp`, necesita compilarse como C++, no como C).
- Include dirs: `port/div/core`, `port/div` (para que resuelvan los `#include "shim/..."` y `#include "judas/..."` relativos), `port/div/shim`, `3rdparty/zlib`, `3rdparty/judas`.

**Verificado**: `cmake --build build --target div_core --config Release` termina con `div_core.vcxproj -> ...\div_core.lib` (el nombre `.lib` es un artefacto de cómo MSBuild empaqueta una `OBJECT` library de CMake; no es una librería estática enlazable de verdad todavía, solo el contenedor de los `.obj`).

## 7. Checkpoint 3 (2026-09-18, misma sesión) — corregidos todos los truncamientos de puntero reales (`FILE*`/`tabfiles`, pilas guardadas, `mem[]`, `divmalloc`)

A pedido explícito del usuario ("vamos a reparar lo del FILE*"), se corrigió toda la familia de warnings C4311/C4312 inventariada en §6.3, salvo el único caso deliberadamente fuera de alcance (`kernel.cpp:254`, mecanismo de DLLs de la Fase 4). Cuatro sitios distintos, cada uno con su propia naturaleza:

### 7.1 `tabfiles[32]` (`inter.h` + `f.cpp`) — el caso original pedido

**Primer toque directo a `inter.h`** en todo este trabajo (hasta ahora se había mantenido byte a byte idéntico al original). Justificado porque `tabfiles` es una tabla puramente nativa —nunca la toca el bytecode DIV, solo las funciones C de manejo de fichero (`_fopen`/`_fclose`/`_fread`/`_fwrite`/`_fseek`/`_ftell`/`div_filelength`)— así que ampliar su tipo no cambia ni el formato de bytecode ni el "handle" de 6 bits que sí ve el lenguaje DIV.

- `inter.h`: `int tabfiles[32]` → `intptr_t tabfiles[32]`.
- `i.cpp` y `f.cpp`: los dos `memset(tabfiles, 0, 32*4)` (que asumían `sizeof(int)==4` por entrada) pasan a `memset(tabfiles, 0, sizeof(tabfiles))` — si no, con `tabfiles` ya como `intptr_t` (8 bytes en x64), esos `memset` solo habrían limpiado la mitad de la tabla.
- `f.cpp` (`_fopen`, hallazgo más importante de este apartado): el original hacía `if (pila[sp]=(int)f) { ... tabfiles[x]=pila[sp]; ... }` — un único truco de C que a la vez probaba si `f` era no-nulo Y truncaba el `FILE*` a 32 bits dentro de `pila[sp]` (la palabra de la VM, que sigue siendo `int` a propósito). El bug real era que `tabfiles[x]` copiaba ese valor **ya truncado**, no el puntero real. Se separó el test (`if (f)`, sin truncar) de lo que efectivamente se guarda (`tabfiles[x]=(intptr_t)f`), preservando exactamente el mismo comportamiento de `pila[sp]` en los tres caminos posibles (éxito, tabla de 32 llena, `f==NULL`) que tenía el original.
- Los sitios de **lectura** (`_fclose`, `_fread`, `_fwrite`, `_fseek`, `_ftell`, `div_filelength` — 9 warnings en total) no necesitaron ningún cambio: el warning desaparece solo al ensanchar `tabfiles`.

### 7.2 `guarda_pila`/`carga_pila`/`actualiza_pila` (`i.cpp`) — necesitó una tabla de handles nueva, no solo ensanchar un tipo

Caso distinto de `tabfiles`: aquí el puntero (`int* p`, un fragmento de pila "aparcado" mientras un proceso está a mitad de una llamada a función con `FRAME` de por medio — ver `07-modelo-ejecucion.md`, "Multi-pila para funciones") se guardaba con `mem[id+_SP]=(int)p`, es decir, **dentro de una celda de `mem[]`**, el array de palabras de 32 bits de la propia VM. A diferencia de `tabfiles`, `mem[]` **no se puede ensanchar** sin romper el formato de bytecode/proceso — cada celda tiene que seguir siendo exactamente un `int`.

Se creó **`port/div/core/port_native_handles.h`** (nuevo, pequeño, autocontenido): una tabla genérica de 256 entradas que traduce un puntero nativo a un handle pequeño (`port_native_handle_alloc`/`_get`/`_free`) que sí cabe en una celda de `mem[]`. `mem[id+_SP]` ahora guarda el handle, no el puntero — el significado de "0 = no hay pila guardada" se conserva exactamente igual. Incluido desde `port_pre.h`, junto al resto de headers de adaptación.

Un matiz que se corrigió durante la implementación (no estaba en el bug original, lo introduciría este mismo arreglo si no se cuidaba): si `malloc()` tiene éxito pero la tabla de 256 handles está llena, hay que liberar `p` inmediatamente y devolver "sin memoria" — si no, se perdería esa memoria (fuga) y el proceso quedaría con un `mem[id+_SP]=0` que en `carga_pila` se interpreta como "no hay nada que restaurar", perdiendo silenciosamente el contexto de pila guardado. Implementado así en `guarda_pila`.

**Límite documentado, no un bug**: 256 handles simultáneos es un margen holgado para el uso típico del lenguaje DIV (acotado por cuántos procesos pueden estar a la vez a mitad de una llamada a función con `FRAME` dentro), pero es un límite real. Si algún juego lo agotara, falla de forma segura (mismo camino que "sin memoria" que ya tenía el código original), no crashea.

### 7.3 Alineación inicial de `mem[]` (`i.cpp`)

Mismo patrón que el bug `ghost` ya corregido en el checkpoint 1: `mem=(int*)((((int)mem+3)/4)*4)` truncaba el puntero recién reservado con `malloc()` a 32 bits antes de alinear a un múltiplo de 4. Cambiado a `(uintptr_t)` — misma operación exacta, sin perder los bits altos de la dirección en x64.

### 7.4 `divmalloc`/`imem1` (`f.cpp`) — arreglo parcial, riesgo arquitectónico de fondo documentado explícitamente

Este es el único de los cuatro casos que **no queda completamente resuelto**, y se documenta así a propósito en vez de dar una falsa sensación de que está arreglado del todo. `divmalloc[con].imem1` pretende ser un índice dentro de `mem[]` que el bytecode DIV pueda usar como si la memoria reservada por el builtin `malloc()` del lenguaje DIV viviera dentro del propio array `mem[]` — pero esa memoria en realidad viene de un `malloc()` de C completamente aparte. En el DOS original, todo el proceso vivía en un único espacio plano de 32 bits, así que la resta de punteros (`p - mem`) siempre caía dentro de un `int` válido por construcción del propio entorno DOS/DPMI. **En un proceso x64 no hay ninguna garantía de eso.**

Se corrigió lo que sí es un bug claro y objetivamente peor: el original truncaba `p` y `mem` **por separado** a `(int)` antes de restar (`((int)p-(int)mem)/4`), lo que en x64 da una diferencia esencialmente arbitraria/basura, no relacionada con la distancia real entre ambos punteros. Ahora la resta se hace primero en `uintptr_t` (ancho completo) y solo se trunca a `int` al final (`(int)(((uintptr_t)p-(uintptr_t)mem)/4)`) — esto es correcto siempre que la distancia real entre `p` y `mem` quepa en un `int` tras dividir por 4, que es el caso normal cuando ambos bloques están razonablemente cerca en el heap del proceso, pero **no es una garantía matemática en x64**. Si algún día un programa DIV portado usa este builtin (`malloc()`/`memory_free()` del lenguaje) y falla de forma extraña con punteros grandes, este es el primer sitio a revisar. Arreglo de fondo pendiente, no trivial: reservar la memoria de `divmalloc` desde dentro del propio array `mem[]` en vez de con un `malloc()` aparte (tocaría también el dimensionado de `mem[]` en `i.cpp`).

### 7.5 Verificación

Recompilados `i.cpp`, `f.cpp`, `divlengu.cpp` (receta del §3) y el target `div_core` de CMake: los tres compilan limpio (exit 0), y de toda la familia de warnings C4311/C4312 solo queda `kernel.cpp:254` (deliberadamente diferido, ver §5).

## 8. Checkpoint 4 (2026-09-18, misma sesión) — `s.cpp` portado + hallazgo importante sobre el backend de vídeo

### 8.1 `s.cpp` compiló al primer intento real

Copiado sin cambios desde `src/div32run/s.cpp` (2143 líneas). Solo hicieron falta 5 declaraciones adelantadas más en `port_forward_decls.h` (mismo patrón K&R de siempre: `caja`, `caja_rellena`, `circulo`, `line`, `line_pixel`) y **cero** warnings de truncamiento de puntero — coincide exactamente con lo que ya predecía `08-aspectos-tecnicos.md` (`s.cpp` tiene 0 coincidencias de `outp`/`inp`/`int86`/`_dos_*` en todo el fichero). Añadido a `div_core` en `CMakeLists.txt`.

### 8.2 Hallazgo importante: el "backend de vídeo" que se resuelve vía `DIV_import` es casi todo opcional, con fallback nativo ya presente

Antes de este checkpoint se asumía (siguiendo `07-modelo-ejecucion.md`) que las ~18 funciones que `frame_end()`/`inicializacion()` resuelven con `DIV_import("set_video_mode")`, `DIV_import("put_sprite")`, etc. eran una pieza de código perdida (como el stub de 602 bytes) que había que reconstruir entera desde cero. **Investigación más a fondo revela que no es así**, y esto cambia sustancialmente el tamaño real del trabajo que queda:

- Se confirmó por grep exhaustivo que **ninguna de esas ~18 funciones tiene implementación real en ningún sitio del repo** (ni `src/div32run/`, ni `src/vpe/`, ni `dll/`) — eso sigue siendo cierto, es un hueco real de la restauración, igual que el stub de 602 bytes.
- Pero casi todos los puntos de llamada en `i.cpp`/`f.cpp` están escritos como `if (funcion!=NULL) funcion(); else { /* camino nativo real */ }` — es decir, son **ganchos opcionales** para un backend alternativo (aceleración por hardware, etc.), no dependencias obligatorias. Confirmado caso por caso:
  - `set_video_mode` → fallback `svmode()` (`v.cpp`).
  - `background_to_buffer` → fallback `restore()`/`memcpy()` (`v.cpp`/`s.cpp`).
  - `buffer_to_video` → fallback `volcado()` (`v.cpp`).
  - `pre_process_buffer`/`post_process_buffer`/`post_process` → simplemente se saltan si son `NULL`.
  - `ss_init`/`ss_frame`/`ss_end` (protector de pantalla) → todo el bloque que los llama está condicionado a `ss_status && ss_frame!=NULL`; en un juego normal (no un `.scr`) nunca se ejecuta.
  - `process_fpg`/`process_map`/`process_fnt` → son notificaciones opcionales tras cargar el recurso; la carga real (parseo del fichero, tablas, paletas) **ya vive dentro de `f.cpp`**, que ya está portado y compilando (ver p.ej. `load_fpg()` en `f.cpp`, que llama a `process_fpg` solo como aviso posterior, no como quien hace el trabajo).
  - `process_sound` → se asigna con `DIV_import` pero **no se llama en ningún sitio** de `i.cpp`/`f.cpp`/`kernel.cpp`. Muerto, sin impacto.
- Conclusión práctica: **el trabajo real pendiente para tener render funcionando no es "escribir ~15 funciones nuevas desde cero", es portar `v.cpp`** (848 líneas) — el único fichero donde vive el camino nativo de verdad (`svmode`, `volcado`/variantes por resolución, `restore`, `set_dac`) — y ahí sí hace falta reescribir el cuerpo de esas funciones para que llamen a `port/io` (`io_video_init`, `io_present`, `io_palette_set`) en vez de escribir puertos VGA directamente, dado que `08-aspectos-tecnicos.md` ya había confirmado que `v.cpp` sí toca hardware de verdad (a diferencia de `s.cpp`).
- Dato adicional para dimensionar lo que falta después de `v.cpp`: `readmouse()` (usada por `f.cpp`/`i.cpp`) vive en un fichero pequeño y todavía no copiado, `src/div32run/mouse.cpp` — un cuarto fichero a portar, previsiblemente chico.
- Para poder *ejecutar* algo de verdad hace falta además bytecode DIV ya compilado. `resource/prg/tutor/*.prg` en este repo son **código fuente DIV sin compilar** (no bytecode) — el compilador (`divc.cpp`, ~7700 líneas) vive en `src/div` (el IDE), no en `src/div32run`, y no se ha tocado. Para una primera validación del pipeline sin esperar al compilador, la opción más rápida es construir a mano un bytecode mínimo de prueba en el formato EML ya documentado en `07-modelo-ejecucion.md`, en vez de portar el compilador ya mismo.

## 9. Checkpoint 5 (2026-09-18, misma sesión) — `v.cpp` reescrito contra `port/io` + primer intento de enlace

### 9.1 `v.cpp` — reescritura real, no port mecánico

A diferencia de `i.cpp`/`f.cpp`/`s.cpp`/`divlengu.cpp` (copiados sin tocar salvo ediciones puntuales), `v.cpp` **se reescribió de verdad**, porque es el único módulo de `div32run` que toca hardware de vídeo real (confirmado ya en `08-aspectos-tecnicos.md`). Se copió primero sin cambios para inspeccionarlo completo, y luego:

- **Funciones que se mantienen exactamente igual** (sin hardware, pura composición de datos): `set_paleta` (cálculo de fundido de paleta), `snapshot`/`graba_PCX`/`graba_MAP` (captura a fichero PCX/MAP, solo E/S de fichero), `restore`/`init_volcado`/`volcado_parcial` (gestión de rectángulos sucios vía `scan[]`, usada por el fallback de `background_to_buffer` que ya vimos en `i.cpp`), y toda la tabla ghost (`init_ghost`, `crear_ghost`, `crear_ghost_vc`, `crear_ghost_slow`, `find_color`, `media` — mezcla de colores, pura matemática).
- **Funciones reescritas** (bridge a `port/io`):
  - `set_dac`/`set_dac2`: antes escribían `dac[]` (paleta VGA de 6 bits/canal) al DAC por puertos `0x3c8`/`0x3c9`, y `set_dac` además fijaba un "color de borde" vía `int386(0x10,...)`. Ahora ambas convierten `dac[]` a RGB de 8 bits (`*4`, la misma conversión que ya usaba el propio código en `dac4[]`) y llaman a `io_palette_set()`. El color de borde se omite (no tiene sentido en una ventana).
  - `retrazo`: antes esperaba el retrazo vertical por puerto `0x3da`; ahora no-op.
  - `svmode`: antes enumeraba y programaba modos VESA/SVGA reales (SciTech SVGA Kit) o, en su defecto, Modo-X de VGA pura (`svmodex`, eliminada). Ahora es una única llamada a `io_video_init(vga_an, vga_al, ...)`, guardando el framebuffer devuelto en la variable `vga` (que antes apuntaba al segmento físico `0xA0000`, ahora es simplemente el puntero que da `port/io`).
  - `volcado`: antes despachaba a una de 6 rutinas distintas según resolución/modo de vídeo (SVGA con *banking*, Mode-X con escritura por planos, VGA lineal). Como `port/io` siempre expone un framebuffer lineal de 8 bits sin importar la resolución lógica, todo eso se colapsa en un único `memcpy` al framebuffer + `io_present()`.
  - `rvmode`: antes restauraba el modo texto de DOS al salir; ahora no-op (el cierre de ventana lo gestionará el futuro `main()` vía `io_video_close()`).
- **Funciones eliminadas** (dependían de hardware/SciTech SVGA Kit que no existe en este port y quedan inalcanzables tras reescribir `svmode`/`volcado`): `svmodex`, `volcadop320200`, `volcadoc320200`, `volcadopsvga`, `volcadocsvga`, `volcadopx`, `volcadocx`, `vgacpy`. Se retiraron los `#include <svga.h>`/`#include "vesa.h"` correspondientes.

**Limitación conocida y documentada en el propio código**: `io_video_init()` (ver `port/io/io_video.c`) solo puede llamarse una vez con éxito — no soporta cambiar de resolución en caliente. Si un programa DIV llama a `set_mode()` más de una vez con resoluciones distintas, la segunda llamada no tiene efecto hoy. No bloqueaba nada para este checkpoint; queda anotado para cuando haga falta.

**Compilación**: solo hicieron falta 3 declaraciones adelantadas más en `port_forward_decls.h` (`snapshot`, `crear_ghost_vc`, `crear_ghost_slow` — mismo patrón K&R de siempre). Quedan 3 warnings `C4028` (parámetro distinto de la declaración) entre `volcado`/`restore` y sus prototipos en `inter.h` (`char*` vs `byte*`) — es una inconsistencia **preexistente en el código original** (no introducida por el port), inocua en la práctica, no se tocó.

### 9.2 `det_vesa.cpp` — portado con un recorte mínimo y documentado

Archivo pequeño (63 líneas originales) que puebla la tabla fija de 6 resoluciones nativas de DIV (`video_modes[]`/`num_video_modes`, usada también por `f.cpp`/`i.cpp` ya portados). Se conservó esa parte tal cual y se eliminó únicamente el sondeo real de modos VESA/SVGA adicionales por BIOS (`vbeInit`/`vbeGetModeInfo`/`VbeInfoBlock`) — equivalente a lo que el propio original hacía cuando no había VESA disponible (`VersionVesa=0`, se queda solo con los 6 modos fijos). Compiló limpio sin necesitar ninguna declaración adelantada.

### 9.3 Primer intento de enlace — diagnóstico, no un intento de completarlo

Con los 7 ficheros del núcleo ya compilando (`i.cpp`+`kernel.cpp`, `f.cpp`, `s.cpp`, `v.cpp`, `det_vesa.cpp`, `divlengu.cpp`, `port_misc.cpp`) más la capa `port/io` ya existente, se hizo un enlace de prueba (`link.exe` manual, fuera de CMake, con un `main()` vacío) **con el único propósito de enumerar qué símbolos faltan de verdad** — no para completarlo en este checkpoint. Resultado: una lista larga, agrupable así:

| Categoría | Símbolos (ejemplos) | Origen / qué falta |
|---|---|---|
| Carga de DLLs / plugins | `DIV_LoadDll`, `DIV_UnLoadDll`, `DIV_export`, `DIV_import`, `LookForAutoLoadDlls`, `ExternDirs`, `pe`, `nDLL`, `call` | `divdll.c`/`dll.c`/`pe_load.c` (no portados) + `a.asm` (`call`, salto a función de DLL). Es la Fase 4 del plan (§11-port-windows11-mikedx.md). |
| Sonido | `InitSound`, `EndSound`, `LoadSong`, `PlaySong`, `StopSong`, `ChangeSound`, `ChangeChannel`, `IsPlayingSound`, `judascfg_device`, `sonido`, `cancion`, `set_mixer`, `SetVocVolume`, `SetCDVolume` | El bridge de `divsound.h` contra `port/io` audio, todavía no escrito (mencionado como pendiente en checkpoints anteriores). |
| Motor Modo-8 (3D) | `loop_mode8`, `start_mode8`, `set_sector_height`, `get_sector_height`, `go_to_flag`, `set_point_m8`, `get_point_m8` | Subsistema completo no tocado — vive fuera de `src/div32run` (ver `src/vpe`/`src/div32run/vpe`, este último vacío según investigamos en `11-port-windows11-mikedx.md`). Alcance no dimensionado todavía. |
| Input (teclado/ratón) | `kbdInit`, `kbdReset`, `set_mouse`, `readmouse`, `check_mouse` | `mouse.cpp` (pequeño, ya identificado en checkpoint 4) + la parte de teclado de `divkeybo.cpp`/equivalente, tampoco portada. |
| Vídeo FLI/FLC | `StartFLI`, `Nextframe`, `EndFli`, `ResetFli` | `divfli.cpp`, no portado — reproducción de animaciones FLI, probablemente baja prioridad. |
| CD-ROM | `Init_CD`, `Play_CD`, `Stop_CD` | `cdrom.cpp`, no portado — baja prioridad casi con certeza. |
| Red | `net_join_game`, `net_get_games`, `net_end`, `_net_loop`, `inicializacion_red` | Fuera de alcance por decisión explícita del usuario (§0.2 de `11-port-windows11-mikedx.md`) — habrá que poner stubs inertes, no implementarla. |
| Gestión de "objetos" | `_object_data_input`, `_object_destroy`, `create_object`, `_object_data_output`, `out_region`, `graphic_info` | Módulo pequeño no identificado todavía en qué fichero original vive; pendiente de localizar. `collision` ya está portado (era la pieza viva de ese grupo, ver §21). |
| Compresión | `uncompress`, `compress` | **No falta código**, falta enlazar `3rdparty/zlib` (la lib ya existe en el repo, solo hay que añadirla al link del target real). |
| Memoria DOS | `DOSalloc4k`, `DPMIalloc4k` | Usadas por `GetFree4kBlocks()` en `i.cpp` — probablemente se puedan stubear de forma segura (devolver "sin memoria adicional") sin implementar DPMI real. |
| Varios menores | `alt_x`, `find_status`, `COM_export` (ver 2.5, ya declarado pero sin definición propia fuera de `divdll.c`) | A revisar caso por caso, previsiblemente pequeños. |

**Conclusión honesta de este checkpoint**: "conectar el núcleo" para tener un `.exe` que enlace y corra un juego real es un proyecto notablemente más grande que "portar `v.cpp`" — incluye como mínimo sonido, input y el motor Modo-8 antes de poder ejecutar la mayoría de programas DIV típicos (muchos usan sonido y/o Modo-8). **Lo que sí se cerró en este checkpoint es el render 2D puro** (sprites/scroll normal vía `s.cpp`+`v.cpp`), que ya no depende de nada más para funcionar salvo el propio enlace y un `main()`.

## 11. Checkpoint 6 (2026-09-18, misma sesión) — `mouse.cpp`/`divkeybo.cpp` reescritos contra `port/io`, segundo intento de enlace

### 11.1 `mouse.cpp` — reescrito (mismo criterio que `v.cpp`)

El original consultaba el driver de ratón real de DOS vía `int386(0x33,...)` (API estándar INT 33h). Reescrito contra `port/io` (`io_get_mouse`/`io_mouse_button`), que ya lee el ratón real de Windows a través de raylib.

- `check_mouse()`: antes fijaba `mouse->cursor=1` cuando **no** había driver de ratón instalado, para activar un control del cursor por teclado (flechas) más abajo en `readmouse()`. En Windows siempre hay un ratón real, así que ahora fija `mouse->cursor=0` de forma fija — esa rama de control por teclado queda inalcanzable y se eliminó junto con las variables que solo usaba (`vx`/`vy`/`vmax`), confirmado por grep que no las usa ningún otro fichero.
- `readmouse()`: reescrito para usar `io_get_mouse()`/`io_mouse_button()`. **Cambio de comportamiento deliberado**: el original acumulaba movimiento *relativo* del ratón (contador de motion de INT 33h) con un divisor de "velocidad" configurable; `io_get_mouse()` da la posición *absoluta* ya en el espacio lógico del framebuffer, que es el equivalente moderno natural y evita reinventar la escala de velocidad — el resultado percibido por quien juegue es equivalente o mejor.
- Se mantiene el recorte a los límites `vga_an`/`vga_al` y la llamada a `tecla()` al principio, igual que el original.

### 11.2 `divkeybo.cpp` — reescrito por completo

El original instalaba un manejador de la IRQ9 (interrupción de teclado) manipulando directamente los vectores de interrupción de DOS (`int386x(0x21,...)`, AH=0x25/0x35) y leía además el buffer de teclado real vía BIOS (`int 16h`) como respaldo — nada de eso existe en Windows.

- `kbdInit()`/`kbdReset()`: instalaban/desinstalaban el manejador de IRQ9 → no-op.
- `tecla()`: ahora llama a `io_poll()` y rellena `kbdFLAGS[128]` con un simple bucle sobre `io_key_down(0..127)` — funciona sin tabla de traducción porque `port/io` ya usa la misma convención de scancodes BIOS/set-1 que las constantes `_XXX` de `divkeybo.h` (confirmado en el propio `div_io.h`). También deriva `shift_status` (bits shift/ctrl/alt) a partir de `io_key_down()` sobre las teclas modificadoras, y recalcula `alt_x` igual que el original.
- **Limitación conocida y documentada en el propio código**: `ascii`/`scan_code` (macros sobre `mem[]`) se dejan siempre a 0. El original les daba semántica de "evento: última tecla pulsada" con su propio buffer circular (`tecla_bios()`), que no tiene equivalente trivial sobre una API de *polling* por estado como `io_key_down()`. Los juegos que solo usan `key()`/`KEY()` (estado mantenido — con diferencia el uso más común en DIV) no se ven afectados. Si hace falta soporte real de "última tecla"/entrada de texto más adelante (relevante sobre todo para el editor del IDE), hay que extender `port/io` con una cola de eventos de teclado real — anotado como trabajo futuro, no resuelto aquí.
- `IrqHandler`/`Irq23`/`Irq1b`/`GetIRQVector`/`SetIRQVector` (todo lo específico de vectores de interrupción DOS) se eliminó por completo, inalcanzable tras los cambios de arriba.

**Compilación**: ambos ficheros compilaron limpio (exit 0) **en el primer intento real**, sin necesitar ninguna declaración adelantada nueva — la racha de sorpresas de compilación de los checkpoints anteriores no se repitió aquí.

### 11.3 Segundo intento de enlace — confirma el bridge de input y refina la lista de lo que falta

Mismo procedimiento del checkpoint 5 (enlace manual, diagnóstico, con un `main()` vacío), ahora con los 9 ficheros del núcleo. Confirmado: **`readmouse`, `set_mouse`, `check_mouse`, `kbdInit`, `kbdReset` ya no aparecen como símbolos sin resolver** — el bridge de input funciona a nivel de enlace. Quedan **80 símbolos sin resolver**, dentro de las mismas categorías de §9.3, con algunos detalles nuevos que vale la pena anotar:

- **Modo-8 resultó más grande de lo que sugería la lista corta anterior**: además de `loop_mode8`/`start_mode8`, aparecen `stop_mode8`, `load_wld`, `set_fog`, `set_sector_texture`/`get_sector_texture`, `set_wall_texture`/`get_wall_texture`, `set_env_color`, `path_find`/`path_line`/`path_free` (búsqueda de caminos), `set_point_m8`/`get_point_m8`, `go_to_flag`. Es un subsistema genuinamente grande (motor 3D completo con pathfinding), no un puñado de funciones.
- **Nuevos, pequeños, no vistos antes**: `mul_24`/`mul_16` (`s.cpp`, usadas en `pinta_modo7` — multiplicaciones de punto fijo, casi con certeza implementadas en algún `.asm` no localizado todavía); `fli_palette_update` (declarada `extern` en `v.cpp` pero sin definición en ningún fichero ya portado — probablemente vive en `divfli.cpp`, no portado); `_heapshrink` (función de heap específica del CRT de Watcom, no existe en el CRT de MSVC — necesitará un shim o eliminarse si no es crítica); `get_cd_error`, `MusicChannels` (más superficie de CD-audio/sonido); `_object_avance` (sistema de objetos).
- El resto (DLLs, sonido, FLI, CD, red, sistema de objetos) coincide con lo ya inventariado en §9.3.

**Conclusión de este checkpoint**: input (ratón + teclado) está resuelto de punta a punta a nivel de enlace. El render 2D + input básico ya no tiene bloqueos propios; lo que queda para un `.exe` jugable de verdad sigue siendo sonido + Modo-8 (más grande de lo estimado) + la infraestructura de DLLs, en ese orden de prioridad probable si el objetivo es que corran juegos DIV típicos.

## 13. Checkpoint 7 (2026-09-18, misma sesión) — verificación en caliente real de `v.cpp`/`mouse.cpp`/`divkeybo.cpp`/`det_vesa.cpp`

Hasta este checkpoint, "funciona" significaba únicamente "compila limpio y enlaza" — nunca se había *ejecutado* código real del port. A pedido explícito del usuario ("¿hay alguna manera de probar lo que tenemos hecho antes de tomar una decisión?"), se escribió `port/div/core/smoke_test.c`, un ejecutable de diagnóstico aparte (target CMake `div_smoke_test`, no forma parte de `div_core`) que llama **directamente a las funciones reales ya portadas** (`svmode`, `volcado`, `set_dac`, `readmouse`, `tecla`, `detectar_vesa`) sin pasar por el intérprete completo ni necesitar un fichero de bytecode.

### 13.1 Diseño de la prueba

Reutiliza el mismo patrón que `i.cpp`: define `DEFINIR_AQUI` e incluye `inter.h`, así obtiene almacenamiento real para las variables `GLOBAL` que `v.cpp`/`mouse.cpp`/`divkeybo.cpp` esperan (`vga_an`, `paleta`, `dac`, `mouse`, `kbdFLAGS`, `mem`, etc.) sin arrastrar el resto de dependencias de `i.cpp` (intérprete, DLLs, sonido — los 80 símbolos de §11.3). Bucle de 360 frames (~6s a 60 fps) dibujando un patrón de prueba + paleta animada + el cursor real del ratón, con salida a consola cada 60 frames; termina solo o con ESC.

### 13.2 Dos bugs reales encontrados escribiendo la prueba (no en el propio port, en el arnés de prueba)

Al ejecutar por primera vez, **crash inmediato (SIGSEGV) dentro de `detectar_vesa()`**, sin ninguna salida por consola (pista falsa: un `SIGSEGV` no vacía el buffer de stdio, así que "no hay salida" no significa "crashea en la primera línea" — hubo que forzar `setvbuf(stdout, NULL, _IONBF, 0)` para localizar el punto real). Causas, ambas del mismo tipo — variables `GLOBAL` que en el runtime real (`i.cpp`) se inicializan apuntando *dentro* de un bloque de memoria reservado en el arranque, y que la prueba aislada nunca reservaba:

1. `video_modes` es `GLOBAL struct _video_modes * video_modes;` — un puntero, `NULL` por defecto. `detectar_vesa()` escribe `video_modes[0].ancho=...` sin comprobar nada → escritura a través de `NULL`. Arreglado apuntándolo a un array local (`vmodes_storage[10]`) en vez de replicar el esquema real de offset dentro de `mem[]`.
2. `mem` es `GLOBAL int *mem;`, también `NULL` por defecto. La macro `num_video_modes` (`#define num_video_modes mem[end_struct+66]`) escribe a través de `mem` sin que la prueba lo supiera — el propio `detectar_vesa()` la usa en su primera línea. Arreglado con `mem=(int*)calloc(20000,sizeof(int));` antes de llamar a nada.

**Importante para quien retome esto**: estos dos arreglos son **solo del arnés de prueba** (`smoke_test.c`), no del port en sí — en el runtime real, `i.cpp` ya reserva `mem[]` como corresponde y apunta `video_modes` dentro de él (`i.cpp:164`) como parte de su propia inicialización, así que no hace falta "arreglar" nada ahí. Se documentan aquí porque son una demostración concreta de que **cualquier prueba aislada de una porción del núcleo necesita replicar, aunque sea mínimamente, la inicialización que normalmente hace `i.cpp`** — buena lección para el próximo intento de prueba parcial.

### 13.3 Resultado: funciona de verdad

Tras los dos arreglos, la prueba corrió limpio (código de salida 0) tanto compilada a mano con `cl.exe` como con el target `div_smoke_test` de CMake. Evidencia concreta (no solo "no crashea"):

- raylib inicializó de verdad: ventana abierta, GPU detectada (NVIDIA GeForce RTX 5090), pipeline OpenGL 3.3 completo, textura de 320×200 creada.
- `svmode()` devolvió un framebuffer real y no nulo.
- **`readmouse()` rastreó la posición real del ratón del sistema** durante la ejecución (`mouse=(305,93)`, con el valor cambiando entre bloques de frames según se movía el ratón de verdad) — confirma que `io_get_mouse()`/`io_mouse_button()` funcionan end-to-end, no solo que compilan.
- `set_dac()`/`volcado()` corrieron sin fallos en los 360 frames (paleta animada + patrón de prueba presentado cada frame).
- `tecla()`/`key()` no crashearon (no se pulsó ESC/A durante la corrida automática, comportamiento esperado: `key_A=0` en toda la salida).

### 13.4 Qué NO prueba esto (alcance honesto)

- No ejercita el intérprete (`i.cpp`/`kernel.cpp`/`f.cpp`) en absoluto — ni bytecode, ni procesos DIV, ni sprites reales del lenguaje. Es una prueba de la **capa de bridging de vídeo/input**, no del runtime completo.
- No prueba sonido (no tocado en `smoke_test.c`).
- Los valores de `ascii`/`scan_code` siguen sin probarse (la limitación de §11.2 sigue vigente, no se ejercitó ese camino).

## 15. Checkpoint 8 (2026-09-18, misma sesión) — sonido + stubs + enlace completo + primer bytecode DIV ejecutado de verdad

A pedido explícito del usuario ("dale al sonido + stubs + main + bytecode, así tendremos un exe de verdad"), se cerraron **todas** las piezas que faltaban para tener un ejecutable Windows nativo del runtime DIV32RUN que enlace sin ningún símbolo pendiente y ejecute bytecode real. Es el checkpoint más grande de la sesión; se detalla en sub-secciones porque toca 4 áreas bastante independientes.

### 15.1 `port/div/core/divsound.cpp` (nuevo) — bridge de sonido real contra `port/io`

Reescritura completa (mismo criterio que `v.cpp`/`mouse.cpp`/`divkeybo.cpp`: el original, 486 líneas, llamaba a JUDAS —biblioteca con acceso directo a Sound Blaster/DMA— para *todo*, carga y reproducción). `port/io` (`io_audio.c`) ya exponía una API 1:1 con `divsound.h` desde la Fase 1, así que el bridge es casi directo:

- **Funcionan de verdad, vía raylib**: `InitSound` (`io_audio_init()`), `LoadSound`→`io_load_sound`, `UnloadSound`→`io_unload_sound`, `PlaySound`→`io_play_sound`, `StopSound`→`io_stop_sound`, `ChangeSound`→`io_change_sound`, `ChangeChannel`→`io_change_channel`, `IsPlayingSound`→`io_is_playing_sound`, `EndSound`→`io_audio_close()`.
- **Limitación conocida y documentada en el propio código**: música de tracker (`LoadSong`/`PlaySong`/`StopSong`/`UnloadSong`/`SetSongPos`/`GetSongPos`/`GetSongLine`/`IsPlayingSong`, formatos MOD/S3M/XM) queda sin implementar — `io_load_song()` siempre devuelve -1 porque raylib no trae decodificador de tracker. Es exactamente la misma limitación ya documentada para el fork de MikeDX en `11-port-windows11-mikedx.md` §4.2 (que la resuelve sumando `libmikmod`, una dependencia externa que este port no ha incorporado todavía). Si un programa DIV llama a estos builtins se comportan como "no hay canción cargada" de forma seguro, no crashean.
- Detalle menor: `judascfg_device` estaba declarada `extern` en el SDK real de `judas.h` pero nunca tenía almacenamiento en ningún fichero portado — se le da definición real aquí (`unsigned judascfg_device=0;`), fijada a `DEV_NOSOUND` permanente en `InitSound()`, con lo que la rama `if (judascfg_device!=DEV_NOSOUND) set_init_mixer();` de `i.cpp` nunca se ejecuta (`set_init_mixer()` es un stub, ver §15.2).

### 15.2 `port/div/core/port_stubs.c` (nuevo) — todo lo demás que el enlazador pedía

Consolidado en un único fichero, con una regla explícita documentada en su propia cabecera: si una función está protegida en el código real por una condición que en el caso de prueba nunca se cumple (p.ej. "solo se llama si el proceso es Modo-8"), el cuerpo puede ser un no-op sin riesgo; si además hace falta mantener la contabilidad de pila del lenguaje DIV (funciones `void X(void)` invocadas por el *dispatcher* de `f.cpp`, que sacan sus propios argumentos de `pila[]`), se documenta explícitamente que el stub **no** es correcto si un programa llega a invocarlas de verdad, porque no se ha intentado adivinar cuántos argumentos esperan (el código original de esos builtins no está en este repositorio).

- **DLLs/plugins**: `DIV_export`/`DIV_import` son una implementación **real**, no un stub — el pool nombre→puntero ya descrito en `11-port-windows11-mikedx.md` §4.5, lógica portable sin dependencia de hardware ni de un *loader* PE. `DIV_LoadDll`/`DIV_ImportDll`/`DIV_UnLoadDll`/`DIV_UnImportDll`/`LookForAutoLoadDlls`/`call()` sí son stubs (fallan/no-op limpiamente, no hay *loader* PE en este port).
- **Modo-8 (3D)**: todas las `void X(void)` (`loop_mode8`, `start_mode8`, `stop_mode8`, `load_wld`, `set_fog`, `set_sector_texture`/`get_sector_texture`, `set_wall_texture`/`get_wall_texture`, `set_env_color`, `path_find`/`path_line`/`path_free`, `set_point_m8`/`get_point_m8`, `go_to_flag`, `set_sector_height`/`get_sector_height`) más `out_region`/`graphic_info` — no-op, con el aviso de arriba. (`collision` ya no está aquí: portada de verdad en `s.cpp`, ver §21.)
- **Sistema de "objetos"**: `_object_data_input`/`_object_destroy`/`create_object`/`_object_data_output`/`_object_avance` — seguros de verdad (gateados por `_Ctype==3` en `i.cpp`, nunca se alcanzan sin Modo-8).
- **FLI**: `fli_palette_update=0`, `StartFLI` (firma real tomada de `divfli.h`, devuelve -1), `Nextframe`/`EndFli`/`ResetFli`.
- **CD-audio**: `Init_CD`/`Play_CD`/`Stop_CD`/`IsPaying_CD`/`get_cd_error`.
- **Red**: `inicializacion_red=0`, `find_status=0`, `net_end`/`_net_loop`/`net_join_game`/`net_get_games` — fuera de alcance por decisión explícita (§0.2 de `11-port-windows11-mikedx.md`), inertes a propósito.
- **Mezclador**: `SetVocVolume`/`SetCDVolume`/`set_mixer`/`set_init_mixer` — no-op (el volumen real ya lo gestiona `port/io` vía `io_change_sound`/`io_change_channel`).
- **Implementación real, no stub**: `mul_24`/`mul_16` (`s.cpp`, usadas en `pinta_modo7`). El original las declaraba con `#pragma aux` (ensamblador embebido de Watcom, ignorado en silencio por MSVC — de ahí que aparecieran como símbolos sin resolver en vez de como un warning de pragma). Equivalen exactamente a `imul edx` + `shrd eax,edx,N`: multiplicar dos enteros de 32 bits en un producto de 64 y quedarse con el resultado desplazado 24 o 16 bits (punto fijo 8.24 y 16.16). Implementadas con `(int)(((long long)a*(long long)b)>>N)`.
- **Memoria DOS/DPMI**: `DOSalloc4k`/`DPMIalloc4k` devuelven 0 — solo las usa `GetFree4kBlocks()`, código muerto en la práctica (nada lo llama).
- **`_heapshrink`**: no-op — función de heap específica del CRT de Watcom sin equivalente en el CRT de MSVC; solo la usa el builtin `SYSTEM()` del lenguaje DIV antes/después de lanzar un proceso externo, innecesaria con memoria virtual moderna.

### 15.3 zlib compilado como fuente propia

Confirmado que `3rdparty/zlib` (1.2.11) no trae ninguna `.lib` precompilada para MSVC/x64 (solo las de Watcom 386/586, ya documentado en `05-dependencias-terceros.md`). Como `f.cpp`/`divc.cpp` solo usan la API simple `compress()`/`uncompress()` (no streaming), se compila directamente el subconjunto estándar de zlib como fuente C ANSI portable dentro de `div_core` (`adler32.c`, `compress.c`, `crc32.c`, `deflate.c`, `inflate.c`, `inftrees.c`, `inffast.c`, `trees.c`, `uncompr.c`, `zutil.c` — se excluyen `gz*.c`/`infback.c`, de la API de fichero `.gz`, no usada aquí).

### 15.4 Enlace completo — `main()` ya existía, cero símbolos sin resolver

**No hizo falta escribir ningún `main()` nuevo**: `i.cpp` ya tiene el `main()` real de DIV32RUN (carga el `.div32`, llama a `inicializacion()`/`interprete()`/`finalizacion()`). Con `divsound.cpp` + `port_stubs.c` + zlib añadidos a `div_core` (`CMakeLists.txt`), el enlace de `div_core` contra `port/io` + `raylibdll.lib` produce `div32run_port.exe` **sin ningún símbolo sin resolver** — el primer ejecutable completo y enlazado del port. Ejecutado sin argumentos, imprime el banner real original ("DIV32RUN Run time library - version 1.03b - Freeware by Hammer Technologies") y el mensaje de error esperado ("Needs a DIV32RUN executable to load."), confirmando que el binario es genuinamente el runtime original, no un stub vacío.

### 15.5 El formato de bytecode tenía un campo más de lo que se había documentado: cabecera de 10 enteros, no 9

Al construir a mano un `.div32` mínimo para probar el intérprete (`resource/prg/tutor/*.prg` son fuente DIV sin compilar, no bytecode — ver checkpoint 4 §8.2 — y `divc.cpp`, el compilador, no está portado), se releyó con cuidado la carga en `i.cpp` (`main()`, ~línea 1333-1368):

```c
len=ftell(f)-602-4*10;      // 602 = stub original + 4*10 = 40 bytes de cabecera
fseek(f,602,SEEK_SET);
fread(mimem,4,10,f);        // <- lee 10 enteros, no 9
...
memcpy(mem,mimem,40);       // copia los 10 al principio de mem[]
len_descomp=mem[9];         // el 10º entero (mimem[9]/mem[9]) es el tamaño descomprimido
if (!uncompress((unsigned char *)&mem[9],&len_descomp,ptr,len)) { ... }
```

Es decir: tras los 602 bytes de stub vienen **10 enteros (40 bytes)** = `mem[0..8]` (9 campos de programa) + un 10º campo que es el tamaño en bytes del payload descomprimido, inmediatamente seguidos por el payload comprimido con zlib (sin ningún campo intermedio adicional). El script anterior (`build_test_prg.py`, primera versión) ya escribía exactamente estos 40 bytes en la práctica (9 enteros + un `uint32` más, bit a bit idéntico a "10 enteros"), así que **esta parte no era el bug** — se documenta aquí porque durante la investigación se llegó a sospechar de ella antes de encontrar el problema real (§15.6).

### 15.6 El bug real: el código de usuario no puede empezar en `mem[9]` — ahí vive un bloque de "globales de sistema" reservado por `system\ltobj.def`

La primera versión de `build_test_prg.py` asumía que el código del programa (un único opcode `lret`) podía escribirse en `mem[long_header]` = `mem[9]`, justo después de la cabecera. Al ejecutar el `.div32` resultante, el runtime crasheaba (SIGSEGV) dentro del *scheduler* de `exec_process()`, después de `inicializacion()` y `frame_start()` pero antes de llegar a ejecutar el opcode. Para diagnosticarlo se copió `i.cpp` a un fichero de *scratch* fuera del repositorio (`%TEMP%\dbgcopy\i_debug.c` — **nunca se tocó el `i.cpp` real**) con trazas `fprintf(stderr,...)` en puntos clave (fin de `inicializacion()`, cada iteración del bucle principal, antes/después de `exec_process()`/`nucleo_exec()`), y se compiló/enlazó a mano un segundo ejecutable (`div32run_debug.exe`) contra el resto de `div_core` sin tocar nada más.

La traza confirmó que el crash ocurría **dentro** de `exec_process()`, antes de llegar siquiera a `nucleo_exec()`. Revisando `divc.cpp` (`precarga_obj()`, que carga y parsea `system\ltobj.def` **antes** que el programa del usuario) y cruzándolo con `inter.h`, se confirmó la causa: el propio DIV, como parte de su diseño (no algo específico de este port), reserva en `mem[]`, empezando justo en `mem[long_header]`, un bloque fijo con las estructuras globales que expone `system\ltobj.def` (`mouse`, `scroll`, `m7`, `joy`, `setup`, `net`, `m8`, `dirinfo`, `fileinfo`, `video_modes`, `timer`, `text_z`..`fps`, `argc`/`argv`, `channel`, `vsync`, `draw_z`, `num_video_modes`, `unit_size`). `inter.h` ya expone el tamaño exacto de ese bloque:

```c
#define end_struct long_header+14+10*10+10*7+8+11+9+10*4+1026+146+32*3   // = long_header + 1520
#define unit_size mem[end_struct+67]   // el último campo, +68 palabras más allá de end_struct
```

Es decir: el primer hueco realmente libre para código/datos de usuario es `mem[long_header + 1520 + 68]` = `mem[1597]`, **no** `mem[9]`. Al escribir el `lret` en `mem[9]`, el hand-crafted bytecode corrompía silenciosamente el struct `mouse` (que `inicializacion()` apunta ahí, `mouse=(struct _mouse*)&mem[long_header]`), y el *scheduler* terminaba leyendo basura.

Segundo hallazgo relacionado: `mem[2]` (`iloc`) **no** es la dirección de ejecución del proceso — es el offset de un bloque de solo lectura (el "molde") con los valores por defecto de los campos *públicos* de un proceso nuevo, que `inicializacion()` copia via `memcpy` a `id_init`/`id_start` (`i.cpp` líneas 213-229: `memcpy(&mem[id_start],&mem[iloc],iloc_pub_len<<2)`, luego `mem[id_start+_IP]=mem[1]`). Su longitud (`iloc_pub_len`, `mem[6]`) tiene un mínimo real: en `inter.h`, el último campo del "reservado" de proceso es `_M8_Step=43` — es decir, **44 campos** (0..43) como mínimo, no un número arbitrario (la primera versión del script usaba 64, sin ninguna base).

### 15.7 Corrección y verificación: bytecode mínimo ejecutado de punta a punta

`build_test_prg.py` (reescrito) ahora calcula: `code_start = long_header + 1588 = 1597` (dónde vive el `lret`), `iloc (mem[2]) = code_start + 1 = 1598` (el molde, justo después del único opcode), `iloc_pub_len (mem[6]) = 44`, y `mem[1]` (entry point) `= code_start`. El payload total pasado a `zlib.compress()` es: 1588 palabras de globales de sistema (todo a 0) + 1 palabra de código (`lret`) + 44 palabras de molde (todo a 0) = 1633 palabras.

Con esto, `div32run_debug.exe test_program.div32` corrió de punta a punta sin crashear, con esta traza completa:

```
[dbg] inicializacion() OK, procesos=1 id_start=1687 id_end=1687 iloc_len=44
[dbg] loop iter=0 procesos=1
[dbg] frame_start() OK
[dbg] antes de nucleo_exec: id=1687 ip=1597 opcode=27
[dbg] despues de nucleo_exec
[dbg] exec_process() OK, ide=1687
[dbg] exec_process() OK, ide=0
[dbg] frame_end() OK
[dbg] saliendo del while principal, procesos=0
[dbg] finalizacion() OK
```

`ip=1597 opcode=27` confirma que el intérprete leyó el opcode `lret` exactamente en la dirección esperada. El proceso "main" termina (opcode `lret`), `procesos` cae a 0, el bucle principal sale limpio y `finalizacion()` corre sin errores. **Repetido con éxito idéntico en el ejecutable de producción** `div32run_port.exe` (sin ninguna instrumentación de depuración) — código de salida 26 en ambos casos, que es el camino de salida *normal* de DIV32RUN tras `interprete()` (`i.cpp` línea 1419: `exit(26); // exit sin borrar la pantalla`), no un error.

**Esto es la primera ejecución real, de punta a punta, de bytecode DIV en el port**: carga de fichero → descompresión zlib → inicialización de procesos → *scheduler* → ejecución de un opcode real de la VM → cierre limpio.

### 15.8 Respuesta a la pregunta lateral: ¿qué falta para compilar un `.PRG` de verdad a un `.exe`?

Todo lo de este checkpoint ejecuta bytecode ya compilado (formato `.div32`/EML). Para partir de un `.prg` fuente (como los de `resource/prg/tutor/`) hace falta **el compilador** (`divc.cpp`, ~7700 líneas, vive en `src/div`, no en `src/div32run`) — no se ha tocado ni evaluado su portabilidad todavía. Es *IDE-side*, no runtime: por tamaño y naturaleza (parser + generador de code + el mismo tipo de dependencias DOS que ya se resolvieron en `f.cpp`) es un esfuerzo comparable al de portar `f.cpp`, quizás algo mayor por ser más grande. No es parte del alcance de este checkpoint; queda para cuando se aborde el compilador/editor (Fase 2 del plan, prioridad ya fijada por el usuario en §0 de `11-port-windows11-mikedx.md`).

## 16. Checkpoint 9 (2026-09-18, misma sesión) — el temporizador de `frame_start()` no avanzaba nunca: la ventana se cerraba (casi) al instante y el sonido se reiniciaba en cada frame

El usuario ejecutó `div32run_port.exe` sin argumentos ("no parece hacer nada" — comportamiento esperado y ya documentado: sin un `.div32` real solo imprime el error y sale, sin ventana). Para darle una confirmación visual real se construyó un segundo bytecode de prueba (`build_frame_demo.py`, nuevo, hermano de `build_test_prg.py`): el proceso "main" ejecuta el opcode real `lfrm` (29, "FRAME" — cede la ejecución hasta el próximo frame, ver `inter.h`/`kernel.cpp`) repetido 180 veces seguidas y termina con `lret`. `lfrm` no lleva operando (confirmado en `kernel.cpp`: su `case` no hace ningún `mem[ip++]` extra), así que basta con repetir la palabra de opcode, sin necesitar un salto (`ljmp`).

Al ejecutarlo apareció un problema real, no relacionado con el bytecode: la ventana se quedaba abierta, pero el log mostraba el dispositivo de audio **cerrándose y reabriéndose en cada uno de los 180 frames** (`AUDIO: Device closed/initialized successfully` repetido ~162 veces), y 180 "frames" tardaban ~6-8s reales en vez de acercarse a lo esperado.

### 16.1 Causa raíz: `reloj` nunca se incrementaba — hueco real del port, no algo introducido en este checkpoint

`frame_start()` (código original, sin tocar hasta ahora) limita la velocidad de fotogramas comparando la variable global `reloj` contra un objetivo (`freloj`), incrementado cada frame según `ireloj` (fijado a `100.0/24.0` en `inicializacion()` — es decir, DIV asume que `reloj` avanza a ~100 "ticks" por segundo, y el valor por defecto es **24 fps**, no 60). En el DOS original, `reloj` lo incrementaba una interrupción de temporizador real (IRQ0/IRQ8 reprogramada a 100Hz) que vive en un `.asm` no presente en este repositorio restaurado — el mismo tipo de hueco que el stub de 602 bytes o el motor Modo-8. Grep exhaustivo confirmó que **ningún fichero portado incrementa `reloj` en ningún sitio** (`reloj=0;` una sola vez en `inicializacion()`, y después solo lecturas). Consecuencia: el bucle de espera de `frame_start()`,

```c
n=0; old_reloj=get_reloj();
do {
  n++;
  if (n>60000 && get_reloj()==old_reloj) { retrazo(); n=-2; }
  else if (n<0 && get_reloj()==old_reloj) { EndSound(); InitSound(); break; }
} while (get_reloj()<(int)freloj);
```

siempre veía `reloj` congelado, entraba en la rama pensada para "el hardware del temporizador ha fallado de verdad" (un caso límite real de MS-DOS con ciertas tarjetas de sonido, según el propio código), y esa rama reinicia el dispositivo de sonido — de ahí el reinicio de audio en cada frame.

### 16.2 Primer intento (insuficiente): un `reloj` en vivo por sí solo no basta

Se añadió `port_get_reloj()` (`port_misc.cpp`, nuevo, mismo patrón que el resto de bridges nativos de ese fichero) usando `QueryPerformanceCounter` para calcular un valor equivalente a 100 ticks/segundo, y se conectó en dos sitios:
- `get_reloj()` (`f.cpp`) ahora llama a `port_get_reloj()` en vez de devolver el campo `reloj` cacheado — confirmado por grep que `get_reloj()` **solo** se usa dentro del bucle de espera de `frame_start()`, así que este cambio es autocontenido.
- `frame_start()` (`i.cpp`) sincroniza `reloj=get_reloj();` nada más entrar, antes de que el resto de la función lo lea directamente (control de screen saver, contabilidad de `fps`/`timer()`, decisión de saltar el volcado).

Con solo esto, el problema **persistía** (162 reinicios de audio, ahora tardando incluso más: ~7.9s). Causa: el umbral `n>60000` del "¿está el temporizador congelado?" es una constante calibrada para CPUs de ~1998 (386/486), donde 60000 vueltas de un bucle vacío tardaban varias decenas de milisegundos — tiempo de sobra para que un temporizador real de 100Hz avanzara al menos un tick. En una CPU moderna, 60000 vueltas de ese mismo bucle (una llamada a `QueryPerformanceCounter` cada una) tardan un puñado de **microsegundos**, muchísimo menos que la resolución de 10ms de `reloj` — así que la rama de "hardware roto" se disparaba en la práctica totalidad de los frames, con o sin reloj en vivo.

### 16.3 Fix real: ceder CPU en cada vuelta del spin (`port_frame_yield`)

Se añadió `port_frame_yield()` (`port_misc.cpp`), que simplemente llama a `Sleep(1)`, declarada en `port/div/shim/dos.h` junto al resto de bridges. Se llama una vez por vuelta dentro del mismo bucle de espera de `frame_start()` (`i.cpp`), sin tocar ni el umbral `n>60000` ni ninguna otra lógica original — cada iteración ahora representa una cantidad de tiempo real comparable a la de 1998, así que la rama de "temporizador congelado" vuelve a ser (correctamente) casi inalcanzable en operación normal, y de paso se deja de quemar un núcleo de CPU al 100% solo esperando al siguiente frame.

**Verificado**: con el mismo bytecode de 180×`lfrm`+`lret`, el dispositivo de audio ahora se inicializa **una sola vez** en toda la ejecución (antes: ~162), y el programa tarda ~7.9s reales — que **coincide** con el cálculo esperado (180 frames ÷ 24 fps por defecto de DIV ≈ 7.5s), confirmando que el limitador de fotogramas ahora funciona con precisión real, no que antes fuera "más rápido por error". Repetido el test de regresión del checkpoint 8 (bytecode de un solo `lret`) sin cambios de comportamiento.

### 16.4 Ficheros tocados en este checkpoint

- `port/div/core/port_misc.cpp`: `port_get_reloj()` y `port_frame_yield()` (nuevas).
- `port/div/shim/dos.h`: declaraciones de ambas, con el análisis completo en el propio comentario (mismo patrón que el resto del fichero).
- `port/div/core/f.cpp`: `get_reloj()` ahora es un valor en vivo (antes leía el campo `reloj` nunca actualizado).
- `port/div/core/i.cpp`: dos ediciones documentadas con `/* PORT: */` dentro de `frame_start()` — sincronización de `reloj` al entrar, y `port_frame_yield()` dentro del bucle de espera.
- `port/div/core/build_frame_demo.py` (nuevo): script hermano de `build_test_prg.py` que genera el bytecode de 180×`lfrm`+`lret` usado para encontrar y verificar este arreglo.

### 16.5 Para quien retome esto: cómo probar la ventana de verdad

```
python port/div/core/build_frame_demo.py   # genera test_frame_demo.div32
cmake --build build --target div32run_port --config Release
build/Release/div32run_port.exe build/Release/test_frame_demo.div32   # (o donde se copie el .div32)
```
Debe abrirse una ventana raylib real que se queda visible ~7.5s (180 frames a 24 fps) y se cierra sola — sin ningún mensaje repetido de `AUDIO: Device closed/initialized` en el log. Ejecutar sin argumentos (`div32run_port.exe` a secas) sigue siendo, a propósito, un camino sin ventana: imprime el error real de DIV32RUN y sale — no es un bug, es el comportamiento original ante la ausencia de un fichero de bytecode.

## 17. Checkpoint 10 (2026-09-18, misma sesión) — primer bytecode que carga un recurso real (`.fpg`) y pinta un sprite de verdad, vía el pipeline automático de proceso

A petición del usuario ("bytecode de prueba más rico"), se construyó un tercer bytecode de prueba (`build_sprite_demo.py`, nuevo) que ya no es solo un banco de pruebas del *scheduler* — ejercita el intérprete "para lo que sirve": carga un `.fpg` real (`resource/fpg/tutorial/tutor0.fpg`) y hace que el propio proceso "main" se pinte con uno de sus gráficos, usando el **pipeline automático de proceso** (`_Graph`/`_X`/`_Y`/`_Ctype`, el mismo mecanismo que usa cualquier juego DIV real) en vez de llamar a `put()`/`xput()` a mano — se confirmó leyendo `frame_end()` (`i.cpp` ~975-1059) que un proceso con `_Ctype==0` y `_Graph>0` se pinta solo cada frame vía `pinta_sprite()` (`s.cpp`), sin que el programa tenga que invocar ningún builtin de dibujo explícito.

### 17.1 Opcodes nuevos usados (más allá de `lret`/`lfrm` de los checkpoints anteriores)

Confirmados leyendo `kernel.cpp` y `f.cpp`:
- `lcar <valor>` (1): empuja una constante en `pila[]` — 2 palabras (opcode + operando).
- `laid` (20): `pila[sp]+=id` — convierte una constante de campo en una dirección real de `mem[]`, sumando el id del proceso que se está ejecutando ahora mismo.
- `lasi` (2): `mem[pila[sp-1]]=pila[sp]` (dirección en `sp-1`, valor en `sp`, empujados en ese orden) — la asignación real a un campo de proceso.
- `lfun <código>` (25): llamada a un builtin interno — 2 palabras (opcode + código de función, este segundo lo consume `function()` internamente leyendo `mem[ip++]`, no el `case lfun` de `kernel.cpp`).
- `lasp` (28): descarta el valor superior de la pila (`sp--`) — usado tras cada `lfun`/`lasi` para tirar el valor de retorno/expresión que no nos interesa.

Con esto, `graph=2; x=160; y=100;` (asignación directa a los campos del propio proceso) se codifica como tres repeticiones de `lcar <campo>; laid; lcar <valor>; lasi; lasp;` (7 palabras cada una), y `load_fpg("tutor0.fpg");` como `lcar <dirección_string>; lfun 3; lasp;` (5 palabras).

### 17.2 Las cadenas de texto en DIV son solo un índice de `mem[]` con bytes C detrás

Confirmado leyendo `load_fpg()`/`open_file()` (`f.cpp`): un argumento "string" es simplemente un entero (offset de `mem[]`) que, reinterpretado como bytes (`(char*)&mem[offset]`), es una cadena C terminada en NUL — sin ninguna marca especial que comprobar para *leerla* (la marca `0xDAD0...` vista en `nullstring[]` solo la usan los opcodes de manipulación de cadenas `lstrcpy`/`lstrcat`, no la carga de ficheros). Basta con colocar los bytes ASCII+NUL en cualquier hueco libre de `mem[]` (en este caso, justo antes del código ejecutable) y pasar su índice como una constante `lcar` normal.

### 17.3 Hallazgo importante: el "molde" de 44 campos no puede ser todo ceros si hay que pintar algo

Las dos pruebas anteriores (checkpoints 8 y 9) usaban un molde de proceso completamente a cero, y funcionaban porque nunca dependían de ningún campo salvo los que `inicializacion()` fija explícitamente (`_Id`, `_IdScan`, `_Status`, `_IP`). Para pintar un sprite hace falta bastante más, y `system\ltobj.def` (la sección `local`, ya citada en el checkpoint 8) declara valores por defecto **no nulos** para varios campos:

```
local size=100          // offset 31 (_Size) -- "tamaño del gráfico en %"
local height=1          // offset 36 (_Height)
local m8_wall=-1        // offset 40
local m8_sector=-1      // offset 41
local m8_nextsector=-1  // offset 42
local m8_step=32        // offset 43
local struct reserved[0] ... m8_object=-1  // offset 11
```

Si el molde no replica esto, `_Size` queda en 0 — "gráfico al 0% de tamaño", es decir, invisible aunque todo el resto (carga del fpg, `_Graph`/`_X`/`_Y` correctos, `pinta_sprite()` sin errores) funcione perfectamente. Se corrigió construyendo el molde de 44 palabras con estos 7 valores explícitos (offsets 11, 31, 36, 40-43) en vez de todo a cero — confirmado por conteo exacto de campos contra `inter.h` (`_M8_Step=43` es el último, 44 campos en total, 0..43) cruzado con el orden de declaración de `system\ltobj.def`.

### 17.4 Hallazgo importante: `open_file()` en producción ignora la ruta que se le pasa — así organiza DIV los recursos de un juego compilado

Primer intento: `load_fpg("resource/fpg/tutorial/tutor0.fpg")` (ruta completa desde la raíz del repo) → `Error 105 (load_fpg)` — fallo real de apertura de fichero, no un problema de bytecode. Leyendo `open_file()` en `f.cpp` (la versión **sin** `#ifdef DEBUG`, la que compila en una build de producción):

```c
FILE * open_file(byte * file) {
  ...
  strcpy(full,(char*)file);
  if (_fullpath(full,(char*)file,_MAX_PATH)==NULL) return(NULL);
  _splitpath(full,drive,dir,fname,ext);
  strcpy(full,fname); strcat(full,ext);          // *** tira la ruta, se queda solo con "tutor0.fpg" ***
  if ((f=fopen(full,"rb"))==NULL) {
    if (strchr(ext,'.')==NULL) strcpy(full,ext); else strcpy(full,strchr(ext,'.')+1);
    if (strlen(full)) strcat(full,"\\");
    strcat(full,fname); strcat(full,ext);        // "fpg\tutor0.fpg"
    if ((f=fopen(full,"rb"))==NULL) { strcpy(full,""); return(NULL); }
    else return(f);
  } else return(f);
}
```

Es decir: **cualquier ruta que se le pase se descarta por completo** — la función se queda solo con el nombre+extensión y reconstruye la ruta como `<extensión_sin_punto>\<nombre>.<extensión>`, relativa al directorio de trabajo del proceso. Esto no es un bug del port: es el mecanismo real y original con el que DIV organiza los recursos de un juego ya compilado (subcarpetas `fpg\`, `pcm\`, `map\`, `fnt\`, etc. junto al `.prg`/`.exe`, un nivel de anidamiento exacto, sin subcarpetas propias). El repo restaurado organiza sus assets de ejemplo con un nivel extra (`resource/fpg/tutorial/`), que es una convención de **este repositorio para catalogar recursos fuente**, no la disposición que espera el runtime.

**Arreglo (no es un cambio de código, es de cómo se invoca la prueba)**: se creó `build/test_game/` (no versionado — vive dentro de `build/`, ya ignorado por completo) con `fpg/tutor0.fpg` y `pcm/help.pcm` copiados desde `resource/`, y se cambió el bytecode para pasar solo `"tutor0.fpg"` (sin ruta) — así es como lo escribiría un programa DIV real. Ejecutando `div32run_port.exe` con el directorio de trabajo puesto en `build/test_game/`, la carga funciona limpia (sin `Error 105`) y el programa corre sus 240 frames completos sin errores.

### 17.5 Resultado: confirmado visualmente por el usuario

`graf=2` de `tutor0.fpg` es una imagen de fondo completa (320×200, 64000 bytes de píxeles + 64 de cabecera = exactamente los 64064 bytes vistos al inspeccionar el fichero) — se asigna a `_Graph` del proceso "main", centrada en pantalla (`_X=160,_Y=100`). El usuario confirmó una ventana real mostrando esa imagen durante ~10 segundos (240 frames a 24fps), cerrándose sola al terminar. **Primera confirmación visual de contenido gráfico real (no solo una ventana vacía) producido por el intérprete portado.**

### 17.6 Para quien retome esto: cómo reproducir la prueba

```
python port/div/core/build_sprite_demo.py     # genera test_sprite_demo.div32 en la raiz
mkdir -p build/test_game/fpg build/test_game/pcm
cp resource/fpg/tutorial/tutor0.fpg build/test_game/fpg/tutor0.fpg
cp help/help.pcm build/test_game/pcm/help.pcm  # no usado todavia por este bytecode, ver §18
cp test_sprite_demo.div32 build/test_game/
cd build/test_game
../Release/div32run_port.exe test_sprite_demo.div32
```

### 17.7 Ficheros tocados en este checkpoint

- `port/div/core/build_sprite_demo.py` (nuevo): construye el bytecode de esta prueba. Documentado en el propio fichero con el mismo nivel de detalle que aquí.
- `.gitignore`: patrón de logs de prueba ampliado a `run_*.txt` (antes solo cubría `run_dbg*.txt`/`run_out*.txt`/`run_real.txt`, y este checkpoint generó nombres nuevos que no encajaban).
- Ningún fichero del núcleo portado (`i.cpp`/`f.cpp`/etc.) se tocó en este checkpoint — todo el trabajo fue en el bytecode de prueba y en cómo se invoca.

## 18. Lo que falta y NO se ha empezado a mirar todavía (para que quede explícito) — actualizado tras checkpoint 10

- Sonido real todavía no ejercitado con bytecode (`load_pcm`/`sound`, funciones 037/039) — `build/test_game/pcm/help.pcm` ya está preparado para esto (ver §17.6) pero el bytecode del checkpoint 10 no lo usa todavía; sería el siguiente paso natural para "bytecode más rico".
- Música de tracker (MOD/S3M/XM) en `divsound.cpp` — necesitaría una dependencia externa tipo `libmikmod` (misma limitación que MikeDX, ver §15.1).
- El motor Modo-8 (3D) — sigue siendo solo stubs no-op; cualquier programa DIV que lo use realmente fallará de forma silenciosa/incorrecta, no hay implementación real (ver §15.2, límite explícito).
- Infraestructura real de carga de DLLs/plugins (*loader* PE) — el pool `DIV_export`/`DIV_import` funciona, pero `DIV_LoadDll` nunca encuentra nada que cargar (Fase 4 del plan).
- FLI/FLC, CD-audio, red — fuera de alcance actual, stubs inertes (red, además, fuera de alcance por decisión explícita del usuario).
- **El compilador (`divc.cpp`, ~7700 líneas) y el editor (`div.cpp`/`divwindo.cpp`/`divedit.cpp`/`divpaint.cpp`)** — ninguno de estos se ha copiado a `port/div/` todavía; es el siguiente bloque de trabajo grande, y coincide con la prioridad ya fijada explícitamente por el usuario (el editor es la razón de ser del proyecto, ver `11-port-windows11-mikedx.md` §0.1/§10 "Fase 2").
- Limpiar/descartar el enlace manual usado para depurar (`div32run_debug.exe`, la copia `i_debug.c` en `%TEMP%`, fuera del repo) — son artefactos de diagnóstico, no parte del port.
- **Ya resuelto en este mismo checkpoint**: se añadió el target real `div32run_port` a `CMakeLists.txt` (`add_executable` sobre `$<TARGET_OBJECTS:div_core>` + `port/io`), verificado con `cmake --build . --target div32run_port --config Release` → enlaza limpio y reproduce exactamente el mismo resultado (exit 26 con el bytecode de prueba) que el enlace manual. Queda un warning `LNK4098` (conflicto `LIBCMT` con el runtime por defecto) — inocuo en la práctica (el binario corre bien), pendiente de revisar con calma si se quiere una build sin warnings.

## 19. Checkpoint 11 (2026-09-18): audio real confirmado con bytecode DIV — y la colisión raylib/div_core que lo silenciaba

### 19.1 Qué se quería probar

Cerrar el paso pendiente de §18 con "bytecode más rico": un programa DIV real que llamara a `load_pcm` + `sound` (funciones 037/039, ya preparadas en §5.8) y reprodujera audio de verdad. Nuevo generador `port/div/core/build_sound_demo.py`:

- Bytecode `test_sound_demo.div32` (240 frames ~10 s), el más rico hasta ahora: `load_fpg("tutor0.fpg")`, fija `_Graph=2`/`_X=160`/`_Y=100` del proceso "main" (fondo del tutor0), `load_pcm("sound_demo.pcm",0)` y `sound(id,256,256)`.
- `build/test_game/pcm/sound_demo.pcm`: PCM bruto 8-bit mono 22050 Hz sintetizado por el propio script (3 bips 880 Hz + tono sostenido 440 Hz, ~3,3 s, 73650 bytes). No lleva cabecera RIFF a propósito, para ejercitar el fallback de `io_audio.c`.

### 19.2 Síntoma y caza del bug: la colisión de símbolos raylib/div_core

La pila del bridge funcionaba (raiza `load_pcm` a handle 0, `sound()` invocado 1 vez con id/vol/fre correctos) pero **no se oía nada**. Instrumentación temporal (`PORT: DEBUG-TEMP`, retirada tras el arreglo) mostró el mecanismo exacto:

```
[dbg] _sound before PlaySound ip=... id=0 vol=256 fre=256
[dbg-aud] io_play_sound(handle=0 vol=256 pit=256)          <- raylib PlaySound planteado
[dbg-aud] about to raylib PlaySound(snd) frameCount=160327
[dbg-aud] io_play_sound(handle=-2089291456 vol=-1296629760 pit=-4)  <- REENTRADA con basura
[portdbg] PlaySound(id=0 ...) -> channel -1                <- ruido de raylib nunca oído
```

Causa raíz: **los nombres de la API de audio del runtime DIV coinciden literalmente con los de raylib** (`LoadSound`/`PlaySound`/`StopSound`/`UnloadSound`). Al incluir `raylib.h` **sin** dllimport, `RLAPI` queda vacío, todas las llamadas raylib se emiten como símbolo de nombre plano, y el linker las resuelve contra el primer símbolo con ese nombre que ve — el de `divsound.obj`. Así, `PlaySound(Sound snd)` de `io_audio.c` se enlazaba contra `int PlaySound(int,int,int)` de `divsound.cpp`, que descompone la struct `Sound` en tres ints basura y reintroduce en `io_play_sound` (vol/–1) — el mismo patrón de colisión ya visto con `COM_export` en `i.cpp` (checkpoint 8).

### 19.3 La corrección (dos piezas, sin tocar el núcleo)

1. **`CMakeLists.txt` — `USE_LIBTYPE_SHARED`** en los targets que consumen raylib (`div_port`, `div_core`, `div_smoke_test`, `div32run_port`). Con ese define, `raylib.h` (línea 98 de 5.0) declara toda su API como `__declspec(dllimport)`, así que **cualquier** llamada raylib (presente o futura) se resuelve por `__imp_<nombre>` contra `raylib.dll` y jamás contra símbolos del runtime DIV.
2. **`port/div/core/port_pre.h` — renombrado de símbolos por macro** de las 4 funciones que colisionan, para eliminar el choque con los *thunks* de nombre plano que trae el import lib (`raylibdll.lib`):
   - `LoadSound → divLoadSound`, `PlaySound → divPlaySound`, `StopSound → divStopSound`, `UnloadSound → divUnloadSound`.
   - El macro reescribe por igual la declaración (divsound.h), la definición (divsound.cpp) y todos los llamadores (f.cpp) **sin tocar ninguno de esos ficheros** — la única pieza nueva es el bloque de macros en `port_pre.h` (que ya es el punto de enganche `/FI` de todo el núcleo).

`divsound.cpp` y `f.cpp` quedaron sin ningún diff neto (se restauraron byte a byte; la única diferencia que quedaba, una línea comentada `// if (pila[sp]==-1) e(129);` en `_sound`, también se restauró).

### 19.4 Verificación

- Build: `cmake --build build --target div32run_port --config Release` → el `LNK2005: PlaySound/StopSound ya definido en divsound.obj` desaparece (prueba de que ninguna llamada raylib apunta ya a símbolos del núcleo).
- Run desde `build/test_game`: `..\Release\div32run_port.exe test_sound_demo.div32` → `exit 26` (salida normal del runtime), 240 frames completos.
- Log raylib: `AUDIO: Device initialized successfully` (miniaudio/WASAPI); `WARNING: WAVE: Failed to load WAV data` + `WAVE: Data loaded successfully (22050 Hz, 16 bit, 1 channels)` → el fallback de PCM bruto de `io_audio.c` funcionó (el demo usa un `.pcm` sin cabecera RIFF a propósito).
- **Confirmación auditiva del usuario**: se oyen los 3 bips a 880 Hz y el tono sostenido de 440 Hz. Primer audio real (no silencioso) producido por el intérprete portado a partir de un bytecode DIV.

### 19.5 Ficheros tocados en este checkpoint

- `port/div/core/build_sound_demo.py` (nuevo): generador del bytecode de demostración de audio + sintetizador del `.pcm`.
- `CMakeLists.txt`: define `USE_LIBTYPE_SHARED` en los 4 targets de raylib (comentario `PORT` documentando el porqué).
- `port/div/core/port_pre.h`: macros de renombrado de los 4 símbolos colisionantes (bloque de macros + comentario `PORT`).
- `port/io/io_audio.c`, `port/div/core/divsound.cpp`, `port/div/core/f.cpp`: instrumentación `PORT: DEBUG-TEMP` añadida y **retirada** en este mismo checkpoint (restaurados).
- `build/test_game/test_sound_demo.div32` + `build/test_game/pcm/sound_demo.pcm`: artefactos generados (no versionados, viven dentro de `build/`).

## 20. Checkpoint 12 (2026-09-18): input confirmado con bytecode DIV — se cierra el círculo gráficos + input + sonido

### 20.1 Qué se quería probar

Ejercitar el teclado desde el intérprete: el único sentido del "trío" (gráficos, input, sonido) que aún solo estaba verificado por el `smoke_test` (que llamaba a `tecla()`/`readmouse()` directamente, sin pasar por bytecode). Nuevo generador `port/div/core/build_input_demo.py` que codifica a mano este programa DIV:

```
load_fpg("tutor0.fpg");   # fpg "0"
graph=1; x=160; y=100;    # sprite 35x35 (graf 1 del tutor0) centrado
loop
  clear_screen();         # DIV no borra el buffer solo: hay que limpiarlo cada frame
  if (key(1)) break;      # ESC = salir
  if (key(75)) x-=4;      # Izquierda (0x4B)
  if (key(77)) x+=4;      # Derecha   (0x4D)
  if (key(72)) y-=4;      # Arriba    (0x48)
  if (key(80)) y+=4;      # Abajo     (0x50)
  frame;
end
```

### 20.2 Piezas nuevas que hubo que dominar para este bytecode (todas verificadas contra `kernel.cpp`/`f.cpp`)

- **`_key` (caso 1 de `function()`)** → `if (pila[sp]<=0 || pila[sp]>=128) e(101); pila[sp]=key(pila[sp]);` — el código de tecla debe ir 1..127 (son los scancodes set-1 de DIV, p.ej. ESC=0x01, flechas 0x48/0x4B/0x4D/0x50) y el resultado **sustituye** al argumento en `pila[sp]` (mismo patrón que `_sound`).
- **`key()`** es la macro `kbdFLAGS[scancode]` (`inter.h:955`); `kbdFLAGS[]` lo rellena `tecla()` (`divkeybo.cpp`) llamando a `io_key_down()` por cada código — y `tecla()` corre **cada frame** vía `readmouse()` desde `i.cpp` (líneas 708/809/1029). Por eso el estado de teclas se lee en vivo entre `frame;` y `frame;`.
- **`clear_screen()` (caso 33)** NO lleva argumentos: `memset(copia2,0,vga_an*vga_al); pila[++sp]=0;` — hay que `lasp` tras la llamada para descartar ese 0.
- **`ljpf` (24)**: `if (pila[sp--]&1) ip++; else ip=mem[ip];` — salta con operando **absoluto** (dirección de `mem[]`) y la condición es impar=verdadero.
- **`lada`/`lsua` (42/43)**: `pila[sp-1]=mem[pila[sp-1]]+=pila[sp]; sp--` — se empuja primero la **dirección del campo** (`lcar _X; laid`) y después el valor; deja el resultado en la pila (se descarta con `lasp`).
- **`lret` (27)** no consume operando (restaura `sp` con `_Param`/`_NumPar` del proceso).
- **Ctrl+ESC** termina el runtime de forma nativa (`i.cpp:510`: `while (procesos && !(kbdFLAGS[_ESC] && kbdFLAGS[_L_CTRL]) && !alt_x)`); **ESC solo** no — por eso el programa lo maneja por bytecode.

### 20.3 Verificación

- Generación: `python port/div/core/build_input_demo.py` → `test_input_demo.div32` (823 bytes, 91 palabras de código, `entry=1600`). Desensamblado de spot-check: todas las rutas de salto absolutas (1636/1649/1662/1675/1688) caen exactamente en los labels esperados.
- Ejecución: `div32run_port.exe test_input_demo.div32` desde `build/test_game`. Ventana abierta con fondo negro + sprite 35×35 centrado.
- **Confirmación interactiva del usuario**: el sprite se mueve con las flechas en tiempo real (4 px/frame) y ESC cierra el programa.
- Se descubre de paso una convención DIV real (no del port): **el buffer NO se auto-limpia** — los juegos DIV llaman a `clear_screen()` (o pintan un fondo) cada frame; sin ello los sprites dejan estelas acumuladas.

### 20.4 Ficheros tocados en este checkpoint

- `port/div/core/build_input_demo.py` (nuevo): generador del bytecode de demostración de input (estilo y estructura idénticos a `build_sound_demo.py`).
- Ningún fichero de código del núcleo/port o CMake se tocó en este checkpoint — todo fue bytecode de prueba (los kills de `tecla()`/`readmouse()` ya estaban cableados desde los checkpoints 5/6).
- `build/test_game/test_input_demo.div32`: artefacto generado (no versionado).

---

## 21. Checkpoint 13 (2026-09-18, misma sesión): `collision()` portada de verdad + proceso hijo por `lcal`

### 21.1 Qué se quería probar

La pieza viva que quedaba como stub en el grupo de "gestión de objetos" era **`collision(tipo)`** (función interna 008), el reto de colisión más usado de DIV. El objetivo era portarla de verdad y demostrarla con bytecode: dos procesos (el main, tipo 1, y un proceso "fantasma", tipo 2, creado desde el main con `lcal`) que avanzan uno hacia el otro y el programa se **auto-cierra el instante en que colisionan**.

```
load_fpg("tutor0.fpg");
graph=1; x=40; y=100;      # main: sprite 35x35, avanza +4/frame
loop
  clear_screen();
  if (key(1)) break;
  x+=4;
  if (collision(TYPE 2)) begin
    signal(TYPE 2, s_kill);   # mata al fantasma para que el programa termine
    break;
  end
  frame;
end

process fantasma(x,y);      # tipo 2
begin
  graph=4;                  # grafico 46x40
  loop frame; end
end
```

### 21.2 `collision()` — implementación real en `s.cpp`

El original (asm en `scr_man.cpp`) recalcula la caja del gráfico y compara AABBs contra todos los procesos vivos del tipo pedido, devolviendo el id del primero que choca (0 si ninguno). El port (`s.cpp:2157` `col_caja` + `s.cpp:2200` `collision`) respeta esa semántica:

- **`col_caja`**: lee `_File`/`_Graph` → descriptor del gráfico en `g[file].grf[graph]` (con límites `max_fpgs`/2000/1000 y nulo-check como en `sp_escalado`), `ptr[13]`/`ptr[14]` = ancho/alto reales, pivot por defecto centro (`ptr[15]==0 || pivot==0xFFFF` → `an/2`,`al/2`) o el pivot almacenado (palabras 32/33 del descriptor, como `s.cpp:806`), volteos `_Flags`, y **escala** `_Size` con la misma matemática que el pintado (`sp_escalado` §checkpoint 4): `x0=x-(xg*size)/100`, `x1=x0+(an*size)/100-1`.
- `_Resolution` y `_Ctype==1` (scroll) se aplican igual que en `sp_escalado` (ver `s.cpp:1087-1099`).
- **`collision`**: lee el tipo de `pila[sp]`; si `<=0` devuelve 0. Recorre `i=id_init..id_end` a saltos de `iloc_len`, se omite a sí mismo (`i==id`), exige `_Bloque==tipo` y estado vivo (`_Status==2` o 4), y con `col_caja` de ambos compara los AABBs (`p0<=j1 && j0<=p1 && q0<=k1 && k0<=q1`); el primer solape deja el id del candidato en `pila[sp]`.
- **Las cajas se verificaron numéricamente** en vivo: main 35×35 en `x=60` → `(43,83)-(77,117)`; fantasma 46×40 en `(100,100)` → `(77,80)-(122,119)`. El solape (x `77<=77`) coincide exactamente con el frame en que el programa se cerró solo.
- Se **eliminó el stub** `collision` de `port_stubs.c` (se conserva la nota de que es un stub en el resto de la "gestión de objetos").
- Limitación documentada igual que en la referencia de DIV: DIV **ignora la rotación** en `collision()` (usa la caja sin rotar), así que un AABB es fiel a la original.

### 21.3 Proceso hijo — convenciones de `lcal` que hubo que dominar

- **`lcal` (26)**: el operando (palabra del flujo de código) ES la dirección absoluta de `mem[]` donde empieza el bytecode del proceso — el kernel hace `ip=mem[ip]` y *entra en el proceso*; éste corre hasta su primer `lfrm` y devuelve el control al padre justo tras el `lcal`, dejando el id del hijo en la pila (por eso el padre emite `lasp` después). Un primer intento con una *palabra de datos intermedia* (operando→palabra de datos→código) era DOBLE indirección: el VM ejecutaba el contenido de la palabra de datos como opcode y crasheaba (`0xC0000005`). Confirmado con el dump del bytecode + logging puntual en el kernel.
- **`lcar2` (60)**: `pila[++sp]=mem[ip++]; pila[++sp]=mem[ip++]` — empuja dos constantes consecutivas: aquí los dos argumentos del proceso.
- **`lcbp` (30)** en el hijo: `mem[id+_NumPar]=mem[ip++]; mem[id+_Param]=sp-_NumPar+1` — necesario para que su `lfrm` restituya la pila a donde debe.
- **`lcaraidcpa` (68)**: `mem[mem[ip++]+id]=pila[mem[id+_Param]++]` — lee el parámetro `n` en un campo del proceso actual (`x=param1`, `y=param2`).
- El **molde (`iloc`)** es el plantilla de 44 palabras (`iloc_pub_len`) que `lcal` copia al nuevo proceso; sin `_Status=2` en `plantilla[4]` el proceso nace muerto (el main lo fija en `i.cpp:220`).

### 21.4 `signal` (función 000) para cerrar el programa

Tras detectar la colisión, el main muere a sí mismo con `lret`; para que el `.exe` acabe (el bucle de `interprete()` corre `while (procesos && ...)`), el hijo debe morir también: `signal(TIPO, 0)` (`_signal` en `f.cpp:71`, `s_kill=0` ⇒ `_Status=0+1=1` = muerto) mata al proceso tipo 2 en el frame de la colisión. Sin ese paso el programa "terminaba" (main acabado) pero quedaba el fantasma vivo y la ventana no se cerraba (timeout en la prueba).

### 21.5 Verificación

- Generación: `python port/div/core/build_collision_demo.py` → `test_collision_demo.div32` (195 bytes comprimidos, `entry=1600`, `iloc=1664`, fantasma en `1708`).
- Ejecución: `div32run_port.exe test_collision_demo.div32`. **Se auto-cierra a los ~630 ms** (`exit code 26` = fin normal DIV), exactamente en el frame en que `self.x1==77` toca `cand.x0==77` (5-6 frames con avance de 4px desde `x=40` hasta `x=60`).
- Todo el logging temporal de trazado (bloques `PORT: DEBUG-TEMP` en `kernel.cpp` `lcal`/`lfrm` y en `s.cpp`) se retiró antes del commit; verificado que el binario final no genera `collision_debug.txt`.

### 21.6 Ficheros tocados en este checkpoint

- `port/div/core/s.cpp`: `col_caja` + `collision` (implementación real, sustituye al stub).
- `port/div/core/port_stubs.c`: eliminado el stub de `collision` (la referencia en el comentario de cabecera se actualizó).
- `port/div/core/build_collision_demo.py` (nuevo): generador del bytecode de demostración (2 procesos, tipos 1 y 2, parámetros por `lcal`).
- `build/test_game/test_collision_demo.div32`: artefacto generado (no versionado).

## 22. Checkpoint 14 (2026-09-18, sesión siguiente): `build_api_demo.py` — las builtins de un juego real verificadas, y el crash `0xC0000005` que no era

### 22.1 Qué se quería probar

Séptimo bytecode de prueba: ejercitar desde bytecode real el resto de builtins que usa un juego tipo STEROID (la cabecera de `build_api_demo.py` tiene el programa DIV equivalente comentado): `set_mode` (036), `load_fnt` (015), `write` (016), `write_int` (017), `delete_text` (018), `put_pixel` (028), `random` (021), `fade` (014) + global `fading`, `let_me_alone` (066), `get_id` (009) y las trigonométricas `get_distx`/`get_disty` (010/011). El programa: 130 frames de estrellas aleatorias + título + tres contadores en pantalla (`cont`, `scan_code`, `fading`) + proceso hijo tipo 2 trazando un anillo con trigonometría real; después `fade` a negro, `let_me_alone` (mata al hijo), espera con `get_id` a que desaparezca del scan, `delete_text`, `fade` de vuelta y auto-cierre. Si alguna builtin falla, el programa se queda colgado (fallo ruidoso), no "pasa en falso".

### 22.2 El crash `0xC0000005` heredado de la sesión anterior — investigación y conclusión

La sesión anterior había dejado el trabajo a medias: comentario `<<< DEBUG: movimiento desactivado para aislar el crash 0xC0000005` en `build_api_demo.py` (el bloque `get_distx`/`get_disty`/`angle+=incr` del hijo comentado) y dos reproducers mínimos (`build_min_debug.py`: solo `put_pixel`+`random`; `build_min2_debug.py`: + hijo `lcal` + contador `lada`). Resultados de esta sesión, con el exe vigente (build de las 8:59, posterior a todas las fuentes):

- **Ningún reproducer crashea** (min_debug, min2: vivos a los 10 s, kill manual).
- **`api_demo` completo (movimiento re-activado) termina normal**: exit 26 en ~6,9 s, sin líneas `Error` en stdout, estable en 5/5 ejecuciones.
- Bisección adicional con variantes: `min3a` (hijo solo con `angle+=incr` → ruta `sp_rotado`), `min3b` (hijo solo con `x/y+=get_distx/get_disty`, angle=0), `min3c` (movimiento + rotación + trayectoria que sale de pantalla por la esquina inferior derecha → `sp_rotado` + clipping en los 4 bordes): **todas robustas ≥10 s**.
- Se descartó además la hipótesis de truncamiento de puntero en el sistema de textos: `texto[].font` es `byte*` de verdad (inter.h:661), no un `int`.

**Conclusión**: el crash no es reproducible con exe + script actuales. La hipótesis más probable es un estado intermedio del propio bytecode durante la edición del script (desbalance de pila → `sp` corrupto → `0xC0000005`), la misma clase de fallo que la doble indirección de `lcal` del checkpoint 13 (§21.3). El `.div32` de las 9:42 (1059 bytes) no coincide con ninguna versión regenerable del script actual, lo que apoya que el script seguía mutando entre ejecuciones. **No hay bug abierto**; no se tocó ningún fichero del núcleo.

### 22.3 Detalle importante para verificación automatizada: `e()` sale con código 26

`e()` (i.cpp:1262, errores no críticos en build sin DEBUG) imprime `Error NNN (función) texto` y hace `exit(26)` — **el mismo código de salida que la terminación normal del intérprete**. Para distinguir un aborto por error de un fin normal en pruebas automatizadas hay que mirar stdout (línea `Error`) o medir la duración (un aborto es inmediato; el ciclo completo del demo dura ~7 s).

### 22.4 Verificación

- Generación: `python port/div/core/build_api_demo.py` → `test_api_demo.div32` (`entry=1625`, `iloc=4311`, hijo en `4355`, `mem8=4423`).
- Ejecución: `div32run_port.exe test_api_demo.div32` → exit 26 en ~6,9 s, stdout sin líneas de error, 5/5 ejecuciones idénticas.
- Builtins confirmadas desde bytecode en este checkpoint: `set_mode`, `load_fnt` (con `fnt/tutor1.fnt` real), `write`, `write_int` ×3, `delete_text`, `put_pixel` ×120/frame, `random`, `fade` + global `fading`, `let_me_alone`, `get_id`, `get_distx`/`get_disty` (más `key` y `load_fpg`, ya conocidas).
- Direcciones de globales de sistema confirmadas en caliente: `fading` = `mem[1540]` (long_header 9 + end_struct 1520 + 11), `scan_code` = `mem[1543]`.
- Nota: `load_fnt` exige `fnt\tutor1.fnt` bajo el directorio de trabajo (misma regla de `open_file()` que §5.5 del handoff); si falta, `e(114)` aborta limpio (exit 26 inmediato, con línea `Error 114` en stdout).

### 22.5 Ficheros tocados en este checkpoint

- `port/div/core/build_api_demo.py` (nuevo): generador del séptimo demo (con el bloque de movimiento activo).
- `port/div/core/build_min_debug.py`, `build_min2_debug.py`, `build_min3a_debug.py`, `build_min3b_debug.py`, `build_min3c_debug.py` (nuevos): reproducers de bisección del crash; se conservan como andamiaje de depuración.
- Ningún fichero del núcleo (`i.cpp`/`f.cpp`/`s.cpp`/...): no había bug que arreglar.

## 23. Port del compilador `divc.cpp` — hitos C0/C1 (2026-09-18, sesión siguiente)

El backlog §8 del handoff pasa a su bloque grande: portar el compilador (`src/div/divc.cpp`, 288 KB / ~6700 líneas) para poder compilar `.prg` reales. Plan de hitos: **C0** spec EML → **C1** compila con MSVC → **C2** driver CLI enlaza y genera `.div32` → **C3** el `.div32` ejecuta en `div32run_port` → **C4** corpus `resource/prg/tutor/*.prg`.

### 23.1 Hito C0 (checkpoint 15): especificación de referencia

`docs/architecture/14-formato-div32-eml.md`: destila todo lo verificado por los 7 demos (formato en disco, layout de `mem[]`, molde de 44 campos, ISA usada, builtins, receta `lcal`, gotchas de verificación). Es la referencia para validar la salida del compilador portado byte a byte. Sin código nuevo.

### 23.2 Reconocimiento previo (qué es `divc.cpp` y cómo se acopla al IDE)

- Punto de entrada real: `compilar()` (divc.cpp:954) — lee `system\ltlex.def`/`ltobj.def`, parsea `source_ptr`/`source_len` (buffer que el IDE llena desde el editor; global.h:895) en dos pasadas (`psintactico`/`sintactico`) y escribe `system\exec.lin`/`.pgm`/`.dbg` + `system\EXEC.EXE` (= el `.div32`) vía `save_exec_bin()` (divc.cpp:7099). `compilar_programa()` (7358) es solo el wrapper de diálogo del IDE — no hace falta para el driver CLI.
- Includes: `global.h` (monolítica del IDE, 1032 líneas — los tipos base `byte`/`word` se autodefinen dentro; cientos de declaraciones de funciones del IDE que son inocuas si no se llaman), `divdll.h` (de `src/`, solo se usa `CMP_export`/`DIV_ImportDll` para la maquinaria IMPORT de DLLs — stub-able, DLLs fuera de alcance igual que en el runtime), `zlib.h`, `div_stub.h` (el array de 602 bytes).
- `divfrm.cpp` es una copia paralela muerta del compilador (el formateador de listados retirado, cambio 49 de la lista) — **no** se porta.
- Autocontenido en opcodes/constantes (define su propio `lcar`..., `long_header`, `default_buffer`); no usa `inter.h` ni `div.h` (éste es del DIV1 viejo, `long_header 36`).
- `texto[]` es GLOBAL_DATA de `global.h`; `divlengu.cpp` (141 líneas, carga `system/LENGUAJE.DIV`) es idéntico en espíritu al del runtime pero se copia desde `src/div` (el del core quedó adaptado al runtime).
- `lower[256]` (tabla de caracteres válidos para identificadores) se define en `div.cpp:48` — habrá que inyectarla en el driver (C2).

### 23.3 Hito C1 (checkpoint 16): `divc.cpp` compila con MSVC

Esqueleto nuevo `port/div/compiler/` (patrón del port: copias con comentarios `PORT:`, los originales en `src/` intactos):

- `divc.cpp`, `divlengu.cpp`, `divkeybo.h`, `div_stub.h` — copias verbatim de `src/div/`.
- `divdll.h` (de `src/`), `pe_load.h` (de `port/div/core/`) — copias verbatim.
- `global.h` — copia **recortada**: fuera includes DOS (`i86`/`bios`/`dos`/`graph`), JUDAS y `divmap3d.hpp` (el compilador no los usa); `SAMPLE*`→`void*` y `tmap`→`int` en los structs `pcminfo`/`M3D_info` (tipos de esas librerías, no usados por divc.cpp).
- `port_compiler_pre.h` — force-include `/FI` (mismo patrón que `port_pre.h`): CRT adelantado, `PATH_MAX 260`, y dos fixes específicos:
  - **`#undef _DLL`**: MSVC predefine `_DLL=1` con `/MD`; divc.cpp declara `typedef struct _DLL {...} DLL;` (línea 3069) y la macro lo convertía en `struct 1{...}` → cascada de errores de sintaxis en toda la sección DLL. Bug de los difíciles de diagnosticar (el error aparecía 200 líneas después como `.nParms` sin tipo struct).
  - **`math.h` NO se incluye**: divc.cpp no usa ninguna función matemática, y su parser define su propia `exp2()` que colisiona con el `exp2(double)` de C99.
- `port_compiler_forward_decls.h` — declaraciones adelantadas de **todas** las funciones de divc.cpp (66) y divlengu.cpp (5), generadas mecánicamente con un script de extracción (patrón K&R que Watcom aceptaba y MSVC no: C2371 en `g1`/`gen`/`condicion`/`exp0..6`/`factor`/...). Ojo: usa `unsigned char`/`unsigned short` en vez de `byte`/`word` porque se procesa antes que el typedef de `global.h`.
- `CMakeLists.txt`: target `divc_core` (OBJECT) con `/TC` + `/FIport_compiler_pre.h` + zlib (mismo subconjunto `ZLIB_SOURCES` que `div_core`).

**Resultado**: `cmake --build build --target divc_core` compila limpio (0 errores).

### 23.4 Warnings pendientes del compilador (riesgos x64 a revisar en hitos posteriores)

- **C4311 truncamiento de puntero a int** en varios sitios, clasificados por riesgo:
  - `save_dbg` (7073-7087): escribe punteros como ints en `exec.dbg` — solo afecta al fichero de depuración, no al bytecode. Inocuo para C2/C3.
  - Handlers de diálogo del IDE (7351-7359: `v.paint_handler=(int)compilar1`): el driver CLI no usa diálogos. Inocuo.
  - `plexico`/`gen` (7610-7667): `lex_case[]` mezcla punteros `lex_ele*` reales con constantes pequeñas `l_???` cast a puntero, y `switch((int)lex_case[*_source])` trunca a int para comparar. **Riesgo real teórico en x64** (un puntero truncado podría aliasar una constante `l_???`): si el compilador produce bytecode incorrecto sin errores, sospechar primero de aquí. Análogo al riesgo documentado para `divmalloc` en el runtime (handoff §7).
- El `switch((int)...)` de función a int en handlers de diálogo ya citado.

## 24. Hitos C2/C3 (checkpoint 17, 2026-09-18): `divc_port.exe` compila un `.prg` real y el bytecode generado **ejecuta en el runtime portado**

### 24.1 Qué se construyó

- `port/div/compiler/divc_main.c` — driver CLI: carga el `.prg` en `source_ptr`/`source_len` (papel del editor del IDE), carga `system\LENGUAJE.DIV` (textos de error), llama a `comp()` y renombra la salida fija `system\EXEC.EXE` al destino pedido. Distingue éxito de error por `numero_error` (-1 = éxito, patrón del propio IDE en `compilar2`). **Tanto éxito como error salen de `compilar()` por `longjmp`** (`comp_exit()`): no hay "retorno normal" que distinguir.
- `port/div/compiler/port_compiler_stubs.c` — `DEFINIR_AQUI` + `global.h` (define todas las GLOBAL_DATA en una TU), stubs no-op de la UI del IDE (`wbox`/`wwrite`/`vuelca_ventana`/`volcado_copia`/`_button`/`dialogo`/`tecla`/...), `get_error` funcional (rellena `cerror` desde `texto[n]`), `IsWAV` **real** (de `divpcm.cpp:1940`, la usa `lexico` para marcar recursos empaquetables), y stubs de la maquinaria DLL (`DIV_ImportDll`=NULL → `IMPORT` emite su error 63, fallo ruidoso correcto).
- `wwrite` imprime los mensajes de progreso del compilador (canal CLI natural de `mensaje_compilacion`).
- `CMakeLists.txt`: target `divc_port` = driver + stubs + `$<TARGET_OBJECTS:divc_core>`. Sin raylib (consola pura). Hay que ejecutarlo con cwd = raíz del repo (rutas relativas `system\...`).

### 24.2 Bugs encontrados y arreglados (todos en `divc.cpp` salvo el primero)

| Bug | Síntoma | Causa | Fix |
|---|---|---|---|
| `big2=0` en CLI | `0xC0000094` (STATUS_INTEGER_DIVIDE_BY_ZERO) antes de cualquier mensaje | `mensaje_compilacion` hace `v.an/big2`; el IDE fija `big2` al arrancar, el driver no | `big=0; big2=1;` en `divc_main.c` antes de `comp()` |
| Layout del nodo `vhash` en x64 | `0xC0000005` en `precarga_obj`/`psintactico` | Nodo = `[next(ptr)][token(ptr)][nombre]`; los `strcmp` leían el nombre a `+8` (offset de 32 bits) en vez de `+16` → nunca coincidía | `_ivnom+2*sizeof(byte*)` en los 3 lexers (`next_lexico`, `lexico`, `plexico`) |
| Escritura "id nuevo" desalineada | `0xC0000005` en la siguiente búsqueda de la cadena | `ptr_o=(void*)(_ivnom+4)` y `name=_ivnom+8` (offsets de 32 bits) → el puntero al objeto pisaba el slot `next` | `_ivnom+sizeof(byte*)` y `_ivnom+2*sizeof(byte*)` |
| `lower['\xa5']` con char con signo | (latente) índice `lower[-91]` | MSVC: `char` con signo → constante `'\xa5'` = -91 | `lower[(byte)'\xa5']` (3 sitios) |

Nota: la "corta-cadena en NULL" que se añadió en los 3 lexers resultó **equivalente** al bucle original (que nunca desreferencia NULL: comprueba `*ptr` antes de avanzar); se conserva por claridad, es inocua.

### 24.3 Incidente serio (lección para futuras sesiones): la herramienta de edición NO es byte-safe con latin-1

Al editar la copia de `divc.cpp` con la herramienta `edit` habitual, ésta decodificó el fichero CP850/latin-1 como UTF-8 y lo reescribió con **U+FFFD** en cada byte acentuado: 9816 sustituciones en `divc.cpp` (y 6106 en `global.h`). Consecuencia: `lower['\xa5']` pasó a ser una constante **multicarácter** de 3 bytes → índice salvaje → `0xC0000005` en la fase final de la compilación. **Regla a partir de aquí: cualquier fichero derivado de `src/` (CP850/latin-1) se edita SOLO con parches byte-exactos en Python** (patrón y reemplazo ASCII puros, verificación de conteo de ocurrencias). Los ficheros nacidos en el port (UTF-8) se pueden editar con la herramienta normal. La recuperación fue: copia fresca de `src/` + script de parches byte-exacto que reaplica los 4 fixes de §24.2 + los 3 recortes de `global.h`.

### 24.4 Verificación

- `divc_port.exe resource/prg/tutor/tutor0a.prg build/test_game/tutor0a.div32` → exit 0, 918 bytes. Estructura validada con Python: cabecera coherente (`entry=1602`, `iloc=1685`, `iloc_pub_len=44` — como la spec del doc 14), payload zlib que descomprime exacto (`n=6880`).
- **Ejecución en el runtime**: `div32run_port.exe tutor0a.div32` (desde `build/test_game/`, con `fpg/tutor0.fpg`) → **vivo y estable 10 s** (bucle infinito con `put_screen` + nave siguiendo `mouse.x` + disparos con `mouse.left`). El círculo `.prg` → compilador → bytecode → runtime queda cerrado.
- Camino de error: un `.prg` con error de sintaxis → `Error 10 (linea 1, columna 14): ...`, exit 1 (no crash).

### 24.5 Ficheros tocados

- `port/div/compiler/divc_main.c`, `port_compiler_stubs.c` (nuevos); `divc.cpp`, `global.h` (parches §24.2 vía script byte-exacto); `CMakeLists.txt` (target `divc_port`).

## 25. Hito C4 (checkpoint 18, 2026-09-18): corpus de tutoriales completo — 11/11 `.prg` compilan y ejecutan

### 25.1 El último crash del compilador: `tglo_init2` y la inicialización `dup`

Al compilar el corpus `resource/prg/tutor/*.prg`: 10/11 OK, `tutor6` crasheaba con `0xC0000005`. Bisección con mini-programas (ojo: **los `.prg` de prueba deben escribirse con CRLF** — el lexer de DIV espera `cr`; con LF-only todo el fichero es "línea 1" y cualquier compilación da Error 10 espurio): el culpable es `GLOBAL tablero[99]= 100 dup (1);` (inicialización de datos con `dup`).

`tglo_init2` (divc.cpp:4664+) está construida sobre aritmética de punteros de 32 bits. Tres clases de expresiones, tres destinos distintos en x64:

- `(int)imemptr-(int)mem` (diferencias): **seguras** — `(int)a-(int)b == a-b (mod 2^32)`, exacto para offsets pequeños. Sin tocar.
- `*(imemptr-(int)mem+(int)frm)`: **accidentalmente segura** — el operando exterior es puntero, así que la aritmética es de 64 bits y las truncaciones se cancelan (`imemptr - low32(mem) + low32(frm) == frm + off` si `mem`/`frm` comparten región de 4 GB). Sin tocar (reescribirlas sería diff grande por cero beneficio).
- `*(byte*)((int)imemptr-(int)mem+(int)frm)` (4854-4861): **ROTA** — toda la suma es aritmética `int` y el resultado pierde la mitad alta de `frm` → puntero salvaje → `0xC0000005`. Fix: `*(byte*)((byte*)frm+(imemptr-(byte*)mem))` (6 sitios).
- `imemptr=(byte*)(((int)imemptr+1)&-2)` (4743, 4789, "lo hace par"): **ROTA** — trunca el puntero a 32 bits. Fix: `(((size_t)imemptr+1)&~(size_t)1)` (2 sitios).

### 25.2 Verificación

- Compilación del corpus: **11/11 exit 0** (`tutor0a..tutor7`).
- Ejecución en `div32run_port` (con `fpg/tutor0-7.fpg` y `fnt/tutor1.fnt`/`tutor6.fnt` en `build/test_game`): **11/11 vivos y estables a los 10 s** (son juegos interactivos con bucle `FRAME` infinito; ningún crash, ninguna línea `Error` en stdout).
- Verificación de datos `dup` (invisible en un crash-test): en `tutor6.div32`, la tabla `tablero[99]=100 dup (1)` aparece en el payload como **exactamente 100 enteros a 1 consecutivos** (en `mem[1617]`, tras los literales de texto) — la expansión `dup` es byte-exacta.
- El driver CLI reporta errores de compilación con nº/línea/columna y exit 1 (probado con un `.prg` malformado).

### 25.3 Estado del compilador tras C4

Compila y genera bytecode correcto para: declaraciones `GLOBAL`/`LOCAL`/`PRIVATE`, tablas y `dup`, structs de sistema (`mouse.graph`), procesos con parámetros, `FROM`/`FOR`/`WHILE`/`LOOP`/`REPEAT`, `IF`/`ELSE`, `collision(TYPE ...)`, `return()`, `exit()`, literales de cadena, `write`, `load_fpg`/`load_fnt`, `key`, `let_me_alone`... (todo lo que usa el corpus tutorial). Sin probar aún: `PRIVATE` extensivo, `STRUCT` de usuario, strings como datos, punteros `POINTER`, `IMPORT` (DLLs — stub con error 63 deliberado), `compiler_options`, programas grandes reales (STEROID etc.).


## 26. Hito E1 (checkpoint 19, 2026-09-18): nucleo UI del IDE compila y enlaza

### 26.1 Estrategia

El IDE completo (`src/div/*.cpp`, ~52K lineas) es demasiado grande para atacar de golpe. Para E1 se redujo el target a los ficheros imprescindibles del nucleo UI:

- `div.cpp` — shell principal
- `divwindo.cpp`, `divhandl.cpp` — sistema de ventanas y menus
- `divedit.cpp` — editor de codigo `.prg`
- `divbasic.cpp`, `divdsktp.cpp` — primitivas graficas y escritorio
- `mem.cpp`, `divlengu.cpp`, `divcolor.cpp`, `divgama.cpp`, `divbin.cpp` — utilidades

El resto (video, input, audio y todos los editores de recursos) se stubbea desde `port/div/ide/port_ide_stubs.c`.

### 26.2 Infraestructura creada en `port/div/ide/`

- `port_ide_pre.h` — force-include (`/FI`) con: headers estandar, macros para palabras clave de 16 bits (`__far`, `__cdecl`, `_near`, `huge`, `__loadds`), tipos `REGS`/`SREGS`/`TIRQHandler`, macros DOS/BIOS inline (`int86`, `intdos`, `peekb`, etc.), y `extern FILE *stdprn`.
- `global.h` y `div.h` adaptadas del IDE (sin headers DOS/JUDAS/map3d).
- `svga.h`, `i86.h`, `bios.h`, `dos.h`, `graph.h` — stubs minimos para los includes DOS/Watcom que usa `src/div/global.h`.
- `port_ide_forward_decls.h` — forward declarations auto-generadas para funciones del nucleo UI definidas despues de su primera llamada, evitando los errores C2371 por implicit int.
- `port_ide_stubs.c` — stubs auto-generados para las ~200 funciones declaradas en `global.h` que no estan en el nucleo UI, mas punteros nulos para los simbolos de datos de los editores de recursos.

### 26.3 Problemas principales resueltos

- **Include-path y global.h**: los fuentes de `src/div` incluyen `"global.h"`; MSVC busca primero en el directorio del fuente, asi que siempre cargaban `src/div/global.h` original. La solucion fue dejarlos usar el original pero dar stubs para todos sus includes DOS/JUDAS, y aplicar los cambios necesarios via `port_ide_pre.h`.
- **Forward declarations**: el IDE usa muchas funciones antes de definirlas. Sin prototipos, MSVC infiere `int` en x64 y luego C2371 al encontrar la definicion real. Se genero un header de forward-decls limpio.
- **Tipos DIV en forward-decls/stubs**: `byte`/`word`/`dword` se normalizan a `unsigned char`/`unsigned short`/`unsigned int` para no depender de que `global.h` se haya incluido antes.
- **Simbolos de editores de recursos**: el menu handler referencia muchos dialogos/estructuras de `divfpg`, `divfont`, `divpcm`, `divmap3d`, etc. Se resolvieron iterativamente extrayendo los unresolved external del log y anadiendo punteros nulos o stubs de funcion.

### 26.4 Verificacion

```powershell
cmake --build build --target div_ide_port --config Release
# -> exit 0, cero errores
```

El ejecutable aun no arranca funcionalmente (todos los subsistemas de video/input/audio estan stubeados); el objetivo de E1 era puramente compilar y enlazar el nucleo UI.

### 26.5 Ficheros tocados

- `CMakeLists.txt` — nuevo target `div_ide_port`.
- `port/div/ide/*` — nuevo directorio con pre.h, headers shim, stubs y generadores.
- `docs/architecture/15-port-ide-assessment.md` — assessment del esfuerzo del IDE.

### 26.6 Siguientes pasos

- **E2**: reemplazar stubs de video/input por la capa `port/io` (raylib): `svmode`, `set_dac`, `read_mouse`, teclado, timer.
- **E3**: editor `.prg` usable + boton "compilar" que invoque `compilar()` del compilador ya portado + "probar" que lance `div32run_port`.
- **E4**: editores de recursos (fpg, paleta, paint, font, sonido, browser, ayuda).


## 27. Hito E2.1 (2026-09-18): el escritorio del IDE se ve

Primer subhito de E2. Objetivo: sustituir los stubs de video por la capa
`port/io` (raylib) hasta que el escritorio de DIV 2.01 se dibuje de verdad.
**Conseguido**: se ve el tapiz, la barra inferior con el boton "menu", el
rotulo "DIV 2.01" y el cursor del raton, a 640x480. Todavia **no es
interactuable** (raton y teclado son E2.2/E2.3).

### 27.1 Codigo nuevo en `port/div/ide/`

| Fichero | Sustituye a | Contenido |
|---|---|---|
| `port_ide_video.c` | `divvideo.cpp` + `det_vesa.cpp` | `vga`, `scan[]`, `detectar_vesa()`, `set_dac()`, `svmode()`/`rvmode()`, `retrazo()`, `volcado()`, `init_volcado()`, `volcado_parcial()` |
| `port_ide_dos.c` | interrupciones y libc de DOS | `reloj` (100 Hz), `port_int386()` (INT 33h contra `io_get_mouse`), `_dos_findfirst/next`, unidades, `DOSalloc4k`, stubs de sonido |
| `port_ide_asm.c` | `src/a.asm` | `call()`, `memcpyb()`, `get_t()` |
| `port_ide_mem.c` | — | `malloc/calloc/realloc` sobredimensionados (los nodos con punteros ocupan el doble en x64) |
| `port_ide_setup.c` | — | validacion de `system\setup.bin` |
| `port_ide_protos.h` | — | prototipos generados por `tools/gen_protos.py` |

Ademas se anadieron a `DIV_IDE_SOURCES` los dos modulos que realmente pintan
el escritorio: `src/div/divpaint.cpp` y `src/div/divpalet.cpp`.

### 27.2 Los cinco problemas que costaron el hito

**1. Punteros guardados en `int` (306 avisos C4311).** Todo el sistema de
ventanas del IDE hace `v.paint_handler=(int)mi_funcion;` y luego `call(...)`;
`divcolor.cpp` mete punteros de heap en ints. En DOS 32-bit era gratis; en x64
trunca. En vez de reescribir ~300 sitios de `src/div` se fuerza que el proceso
entero viva por debajo de 2 GB:

```cmake
target_link_options(div_ide_port PRIVATE
    /LARGEADDRESSAWARE:NO /BASE:0x10000000 /DYNAMICBASE:NO /FIXED)
```

Verificado: codigo en `0x10001000` y todas las reservas de heap < 2 GB, asi que
la truncacion a `int` es reversible.

**2. `call()` estaba stubeado como no-op**, es decir *toda* la UI era inerte.
Era el `JMP NEAR PTR EAX` de `src/a.asm`; ahora reconstruye el puntero desde
los 32 bits bajos y llama.

**3. Crash `0xC0000005` en `col_analiza_ltlex()`.** La tabla `lower[256]`
(div.cpp:48) se inicializa con espacios, y es `inicializa_compilador()`
(divc.cpp:939) quien convierte los espacios en `0`. Como `divc.cpp` no esta en
el target, esa funcion estaba stubeada vacia y el bucle
`while (*icvnom.b = lower[*buf++])` no terminaba nunca -> lectura fuera del
buffer. Se porto la funcion de verdad en `port_ide_stubs.c`.

De paso, en `clexico()` los nodos `{siguiente, token, asciiz}` se indexaban con
`_ivnom+8` (dos punteros de 32 bits); parcheado a `_ivnom+2*sizeof(byte*)`.

**4. `system\setup.bin` del DIV original.** `Load_Cfgbin()` hace un `fread`
directo sobre `struct SetupFile`. El fichero heredado tiene el layout de
Watcom (`_MAX_PATH` = 144 frente a 260 en MSVC), asi que todos los campos
salian desplazados: `colors_rgb` a cero -> `c0..c4 = 0` -> **escritorio negro**,
y una resolucion arbitraria (1280x1024). `port_ide_setup.c` lo descarta si el
tamano no coincide con `sizeof(Setupfile)`; se usan los valores por defecto y
`Save_Cfgbin()` lo reescribe con el layout actual.

**5. `zoom_porcion` duplicado.** `divpaint.cpp` define una *funcion* y
`divbasic.cpp` una *variable int* con el mismo nombre. En el C++ original
convivian (la funcion iba mangled); al compilar como C chocan. Se renombra solo
en ese TU con `COMPILE_DEFINITIONS "zoom_porcion=zoom_porcion_paint"`.

### 27.3 `tools/gen_protos.py`

`divpaint.cpp` y `divpalet.cpp` llaman a funciones definidas mas abajo en el
propio fichero sin prototipo previo. Como C eso da declaracion implicita y
luego C2371. El script extrae las definiciones de nivel superior y genera
`port/div/ide/port_ide_protos.h`, que `src/div/global.h` incluye bajo
`#ifdef PORT_IDE`.

Regenerar tras tocar esos fuentes:

```powershell
python tools\gen_protos.py src\div\divpaint.cpp src\div\divpalet.cpp > port\div\ide\port_ide_protos.h
```

Ojo: `#include "global.h"` desde `src/div/*.cpp` resuelve **siempre** a
`src/div/global.h` (MSVC busca primero el directorio del propio fuente), no a
la copia de `port/div/ide`. Por eso el include va ahi y no en la copia del port.

### 27.4 Autoverificacion del render

`port/io` expone `io_screenshot()`. Con dos variables de entorno el IDE vuelca
un frame a disco, imprime un resumen por stderr y termina:

```powershell
cd build\Release
$env:DIV_IDE_SHOT = "..\shot.png"; $env:DIV_IDE_SHOT_FRAME = "120"
.\div_ide_port.exe
# [shot] 640x480 px_no_cero=306959 suma_dac=23059 src_no_cero=306959
#        c0..c4=0,211,215,220,15 tapiz=0000000000000000 volcados=120
```

### 27.5 Diagnostico sin depurador (reutilizable)

No hay cdb/windbg en la maquina. Flujo empleado:

1. `Get-WinEvent -FilterHashtable @{LogName='Application';ProviderName='Application Error'} -MaxEvents 1` -> offset de fallo.
2. Recompilar con `/MAP` y resolver el offset sobre `build/Release/div_ide_port.map` sumando la base `0x10000000`.
3. `dumpbin /disasm:nobytes /section:.text` para ver la instruccion exacta.

### 27.6 Entorno de ejecucion

`main()` hace `chdir` al directorio del `.exe` y luego exige `system\lenguaje.div`,
`system\pequeno.fon`, `system\graf_p.div`, `system\tab_cuad.div`,
`system\sys06x08.bin`, `system\ltlex.def` y `help\help.fig`. En desarrollo se
crean junctions dentro de `build/Release/`:

```powershell
cmd /c mklink /J build\Release\system   <ruta_div2>\system
cmd /c mklink /J build\Release\help     <ruta_div2>\help
cmd /c mklink /J build\Release\resource <ruta_div2>\resource
```

### 27.7 Pendiente

- **E2.2** raton: anadir `src/div/divmouse.cpp` al target (el emulador de INT 33h
  ya esta en `port_ide_dos.c`), ocultar el cursor del SO, menus clicables.
- **E2.3** teclado + timer: ampliar `port/io` con cola de caracteres, scancodes
  y `shift_status`; portar `divkeybo.cpp`; enganchar `port_ide_tick()`.

---

## 28. Hito E2.2 del port del IDE — raton (el escritorio ya es clicable)

Objetivo del subhito: que el cursor del IDE siga al puntero real de la ventana
y que los menus respondan al clic. Cerrado con el tag `0.0.12-ide+raton`.

### 28.1 `divmouse.cpp` es casi puro

`src/div/divmouse.cpp` (209 lineas) no toca hardware salvo en `read_mouse2()`,
que hace dos llamadas a INT 33h:

- `AX=3` -> estado de botones en `BX` (bit 0 izq, 1 der, 2 medio) y posicion.
- `AX=0Bh` -> incremento (`CX`,`DX`) desde la lectura anterior.

Todo lo demas (`mouse_in`, `wmouse_in`, `set_mouse`, `read_mouse`,
`libera_drag`) es logica de UI. Basto con anadirlo a `DIV_IDE_SOURCES` y
borrar de `port_ide_stubs.c` los stubs `mouse_in`, `read_mouse`, `set_mouse`
y `wmouse_in`.

### 28.2 C2371 otra vez -> regenerar los prototipos

Igual que con `divpaint.cpp`/`divpalet.cpp`, `divmouse.cpp` llama a
`libera_drag()` y `read_mouse2()` antes de definirlas. Se regenera el header:

```powershell
python tools\gen_protos.py src\div\divpaint.cpp src\div\divpalet.cpp src\div\divmouse.cpp > port\div\ide\port_ide_protos.h
```

**Regla**: cada `.cpp` de `src/div` que entre en `DIV_IDE_SOURCES` hay que
anadirlo a esa linea de comando y regenerar `port_ide_protos.h`.

### 28.3 El problema de fondo: deltas relativos vs puntero absoluto

La emulacion inicial de `AX=0Bh` devolvia `posicion_actual - posicion_anterior`
de `io_get_mouse()`. Es fiel al driver DOS, pero en una ventana da un cursor
**desfasado**: `m_x` arranca donde DIV lo deje con `set_mouse()` y a partir de
ahi solo acumula incrementos, asi que el cursor del IDE y el puntero real
divergen. Ademas, cada vez que el puntero se sale de la ventana `io_get_mouse()`
**clampea**, se pierden incrementos y el desfase se vuelve permanente.

Solucion en `port_ide_dos.c`: modo absoluto con auto-correccion. Se declara
`extern float m_x, m_y;` (el acumulador de `divmouse.cpp`) y el incremento que
se devuelve es exactamente el que deja ese acumulador sobre el puntero real:

```c
double ratio = 1.0 + (double)Setupfile.mouse_ratio / 3.0;
out->w.cx = (short)lround((x - (double)m_x) * ratio);
out->w.dx = (short)lround((y - (double)m_y) * ratio);
```

El factor `ratio` compensa la division que hace `read_mouse2()`
(`m_x += (float)ix / (1.0 + mouse_ratio/3.0)`), asi que el resultado es
`m_x == x` sea cual sea la sensibilidad configurada.

**Matiz importante**: el incremento solo se calcula **si el puntero real se ha
movido** desde la lectura anterior; si no, se devuelve 0. Sin esa condicion, el
desplazamiento del cursor por teclado (`read_mouse()` mueve `mouse_x`/`mouse_y`
con las flechas y llama a `set_mouse()`) quedaria anulado en el mismo frame.

### 28.4 Cursor del SO oculto

El IDE compone su propio cursor dentro de `copia`, asi que se verian dos.
Se anadio `io_show_cursor(bool)` a `port/io/div_io.h` + `io_video.c`
(`ShowCursor()`/`HideCursor()` de raylib) y `svmode()` la llama con `false`
tras crear la ventana.

### 28.5 Verificacion

Con `DIV_IDE_SHOT` se comprobo el seguimiento absoluto de forma determinista:
se posiciona el puntero con `SetCursorPos` en coordenadas de cliente conocidas
(dividiendo por la escala de ventana, 2x a 640x480) y se compara con el cursor
dibujado en el PNG. Pedido virtual (100,400) -> dibujado en (100,400), exacto.
El clic sobre la barra "menu" y la navegacion por los menus desplegables los
confirmo el usuario en caliente.

**Gotcha de la verificacion**: si el usuario esta usando la maquina, sus propios
movimientos de raton contaminan la prueba (`GetCursorPos` devuelve algo distinto
de lo pedido y pueden colarse clics reales — en una de las pruebas se abrio el
menu Sistema y se selecciono "Salir de DIV", con el consiguiente fundido a
negro que parecia un bug de render). Conviene mirar siempre `GetCursorPos`
despues de `SetCursorPos` para descartarlo.

### 28.6 Por que "todo se queda negro" al abrir el editor de PRG

No es un bug del ratón ni del vídeo: es el comportamiento esperado en E2.2.

`dialogo(int init_handler)` (`div.cpp:2623`) crea la ventana modal con valores
por defecto **a pantalla completa** y con `dummy_handler` como pintor:

```c
v.paint_handler = (int)dummy_handler;
v.an = vga_an;  v.al = vga_al;
```

Es el `init_handler` quien debe encogerla y poner el pintor real. Y el menú
`Programas -> Nuevo / Abrir` (`divhandl.cpp:110` y siguientes) llama a
`dialogo((int)browser0)`, pero **`browser0()` sigue siendo un stub vacío** en
`port_ide_stubs.c`: vive en `divbrow.cpp` (58 KB), que todavía no está en
`DIV_IDE_SOURCES`. Resultado: ventana modal de 640x480 pintada con
`dummy_handler` -> pantalla en negro.

Modulos del IDE que faltan y que hacen falta para llegar al editor de PRG:

| Modulo | Tamano | Por que hace falta |
|---|---|---|
| `divbrow.cpp` | 58 KB | `browser0()` — el selector de ficheros de "Nuevo"/"Abrir" |
| `divfont.cpp` | 57 KB | `ShowText`, `crea_barratitulo`, `Text1..Text3` — el texto del editor |
| `divkeybo.cpp` | 6 KB | `tecla()`, `key()`, `kbdFLAGS` — escribir codigo (hito E2.3) |

Hasta que esos tres entren, el escritorio, los menus y el arrastre de ventanas
funcionan, pero cualquier accion que abra un dialogo de fichero pinta negro.
Esto se aborda en **E3**.

### 28.7 Herramientas de diagnostico anadidas

- `DIV_IDE_SHOT_EVERY=N` (nuevo): modo rafaga. En vez de volcar un PNG y salir,
  escribe `<base>_NNN.png` cada N llamadas a `volcado()` y deja el IDE vivo.
  Sirve para seguir una secuencia de interaccion completa. **Ojo**: el contador
  va por volcados, no por tiempo — si nada cambia en pantalla no hay volcado.
- `tools/drive_ide.ps1` (versionado): lanza el IDE e inyecta clics sinteticos
  en coordenadas del framebuffer virtual (convierte con la escala de ventana que
  devuelve `GetClientRect`), capturando en rafaga. Uso:
  `powershell -File tools\drive_ide.ps1 -Clicks "25,472;40,30"`.

### 28.8 Pendiente

- **E2.3** teclado + timer: ampliar `port/io` con cola de caracteres, scancodes
  y `shift_status`; portar `divkeybo.cpp` (`kbdInit`, `tecla()`, `vacia_buffer`,
  `key()`, `kbdFLAGS`, `shift_status`); enganchar `port_ide_tick()` al reloj de
  100 Hz. Nota: `read_mouse()` ya llama a `tecla()` y a `key()`, asi que parte de
  la ruta de teclado esta viva pero apuntando a stubs.


---

## 29. Hito E2.3 del port del IDE — teclado y reloj (el IDE responde a las teclas)

### 29.1 Resultado

El IDE recibe el teclado completo: caracteres, scancodes, modificadores y
combinaciones. Verificado end-to-end con `ALT+X`, que abre el dialogo
"¿Salir de DIV?" — es decir, la tecla llega a `tecla()`, se convierte en
`ascii`/`scan_code`/`shift_status`, el nucleo UI la enruta al handler del menu
y el handler abre una ventana modal.

Ademas se ha arreglado el reloj: `port_ide_tick()` estaba definido pero **no lo
llamaba nadie**, asi que `reloj` valia 0 permanentemente.

### 29.2 `divkeybo.cpp` NO se porta, se sustituye

El original no tiene nada reaprovechable:

- instala un handler de la **IRQ 9** que lee el puerto `0x60` para mantener
  `kbdFLAGS[]`,
- lee los caracteres con `int386x` contra la **INT 16h** de la BIOS,
- y manipula los vectores DOS `0x1b` (Ctrl+Break) y `0x23` (Ctrl+C).

Se aplica la misma receta que con `divvideo.cpp` → `port_ide_video.c`: el fichero
queda fuera de `DIV_IDE_SOURCES` y se escribe **`port/div/ide/port_ide_keybo.c`**
reproduciendo su contrato exacto.

Simbolos que hay que seguir exportando con la misma semantica:

| Simbolo | Definido en | Quien lo usa |
|---|---|---|
| `kbdFLAGS[128]` | `global.h:803` (macro `key(x)`) | todo el IDE |
| `ascii`, `scan_code`, `shift_status` | `global.h:750-751` | `divedit.cpp`, `div.cpp` |
| `buf[256]`, `ibuf`, `fbuf` | cola circular de 64 eventos | `divedit.cpp:791` |
| `ctrl_c` | flag del handler DOS | residual, siempre 0 |

`ibuf`/`fbuf` **no son un detalle interno**: `divedit.cpp:791` hace
`if (v.volcar && ibuf!=fbuf)` para saltarse volcados cuando el usuario escribe
mas rapido de lo que se redibuja. Por eso la cola se conserva tal cual en vez de
leer los eventos directamente de `port/io`.

### 29.3 El problema de las colas de raylib

`PollInputEvents()` **vacia** `GetKeyPressed()` y `GetCharPressed()` al principio
de cada pasada, y se ejecuta dentro de `EndDrawing()`. Si nadie drena justo
despues, los eventos se pierden. Agravante: el IDE tiene bucles de espera activa
que **no dibujan**, por ejemplo `divpaint.cpp:2807`:

```c
do { tecla(); } while (key(_H));
```

Sin bombeo propio eso se colgaria para siempre. Solucion en dos piezas:

- **cola propia persistente** de 64 eventos en `port/io/io_keyboard.c`, que
  sobrevive al vaciado de raylib;
- **`io_input_drain()`** se llama tras `EndDrawing()` en `io_present_ui()`, y
  **`io_input_pump()`** (= `PollInputEvents()` + drain) lo llama `tecla()`, de
  modo que los bucles sin dibujado siguen recibiendo teclas.

### 29.4 Emparejado ascii ↔ scancode

La BIOS entregaba ascii y scancode **juntos** en el mismo evento (se ve en
`divedit.cpp:1186`, donde `f_backspace()` hace `ascii=0` nada mas entrar porque
da por hecho que venian emparejados). raylib los da por canales separados, asi
que `io_input_drain()` los recompone en 4 pasos:

1. drena `GetCharPressed()` a un array temporal;
2. recorre `GetKeyPressed()` en orden; cada tecla **que produce texto**
   (`io_keymap_is_text`) consume un caracter del array — asi una flecha pulsada
   a la vez no roba la letra;
3. las teclas de control sintetizan su ascii BIOS: Enter→13, Backspace→8,
   Tab→9, ESC→27;
4. los caracteres sobrantes (teclas muertas, AltGr, repeticion) se emiten con
   `scan=0`.

raylib **no** encola repeticiones en `GetKeyPressed()`, por eso las teclas sin
texto se anaden aparte consultando `IsKeyPressedRepeat()`.

La traduccion vive en `port/io/io_keymap.c`: tabla raylib → scancode PC set 1
(~110 entradas, expuestas como lista compacta con `io_keymap_entries()` en vez
de barrer 512 posiciones) y tabla Unicode → CP850 (los 96 codigos de Latin-1
mas `U+0192`, `U+0131`, `U+2017` y `U+25A0`).

### 29.5 `kbdFLAGS` funciona por FLANCOS, no por snapshot

Es el detalle mas facil de romper. El IDE **consume** teclas poniendo el flag a
cero a mano, por ejemplo `divmouse.cpp:76`:

```c
kbdFLAGS[_C_RIGHT]=0;
```

Si `kbdFLAGS` fuese una copia del estado actual del teclado, el flag resucitaria
en el acto mientras la tecla siguiera hundida. Por eso `io_keys_state()` devuelve
**dos** tablas:

- `down[]` — teclas mantenidas,
- `made[]` — pulsacion o repeticion, y **se limpia al leerse**.

Y `refresca_kbdflags()` aplica: `made → 1`, `!down → 0`, en cualquier otro caso
no toca nada.

### 29.6 Filtro de ALT GR (critico para escribir DIV)

Windows inyecta un `LCTRL` sintetico junto a `ALT GR`. Sin filtrarlo,
`divedit.cpp:781` (`if (!(shift_status&SS_CTRL) && ascii)`) **descartaria**
`[ ] { } @ #` en un teclado espanol, que son justo los caracteres que necesita la
sintaxis de DIV. `tecla()` replica el filtro: si `SS_RIGHT_ALT` y `SS_LEFT_CTRL`
estan ambos activos, limpia `SS_CTRL|SS_LEFT_CTRL`.

Los bits de bloqueo (Caps/Num/Scroll) se dejan a 0 **a proposito**: raylib no los
expone y no hacen falta, porque `GetCharPressed()` ya entrega el texto resuelto
por el SO (distribucion, mayusculas, AltGr).

Se conserva del original el caso del **teclado numerico**: si llega un digito
ASCII se pone `scan_code=0` y se limpian `kbdFLAGS[0x4B/0x4D/0x48/0x50]`, porque
los digitos del keypad comparten scancode con las flechas.

### 29.7 `reloj` estaba muerto

`port_ide_tick()` (`port_ide_dos.c`) existia desde E1 pero no se llamaba desde
ningun sitio, asi que `reloj` valia siempre 0. Eso afectaba al watchdog de sonido
(`div.cpp:659`/`1309`) y al spray de `divpaint.cpp:816`. Ahora lo llama `tecla()`,
que se ejecuta exactamente **una vez por vuelta** del bucle principal.

Que no se llame dos veces por iteracion no es casualidad: `read_mouse()`
(`divmouse.cpp:47`) solo llama a `tecla()` si `modo<100 && hotkey &&
!help_paint_active`, y en el escritorio `modo=101` (`div.cpp:2917`).

Medido: `reloj` avanza ~212 unidades en 2,1 s → 100 Hz correcto.

### 29.8 Verificacion

`tools/drive_ide.ps1` se ha ampliado con inyeccion de teclado via `keybd_event`
(GLFW recibe por mensajes de Windows, asi que funciona):

```
powershell -File tools\drive_ide.ps1 -Keys "alt+x"
powershell -File tools\drive_ide.ps1 -Clicks "25,472" -Keys "down;down;up"
```

Y se ha anadido la traza `DIV_IDE_KEYLOG=1`, que vuelca por stderr cada evento
entregado y cada transicion de `kbdFLAGS`. Resultados:

```
[flg] scan=0x2A -> 1
[key] ascii=  0 ('.') scan=0x2A shift=0x0002     <- LSHIFT
[flg] scan=0x1E -> 1
[key] ascii= 65 ('A') scan=0x1E shift=0x0002     <- SHIFT+a = 'A'
[flg] scan=0x1E -> 0
[flg] scan=0x2A -> 0
[flg] scan=0x1D -> 1
[key] ascii=  0 ('.') scan=0x1D shift=0x0104     <- LCTRL (SS_CTRL|SS_LEFT_CTRL)
[flg] scan=0x26 -> 1
[key] ascii=  0 ('.') scan=0x26 shift=0x0104     <- CTRL+L, ascii suprimido
```

F1 llega como `scan=0x3B ascii=0`, la flecha arriba como `scan=0x48 ascii=0`,
Enter como `ascii=13 scan=0x1C`. Consumo de CPU tras el cambio: ~10%.

**Gotcha**: `CTRL+ESC` (`div.cpp:1235`, `salir_del_entorno=1`) **no sirve como
prueba sintetica** porque Windows lo intercepta para abrir el menu Inicio; la
tecla nunca llega a la aplicacion. Se uso `ALT+X` en su lugar.

**Gotcha 2**: los movimientos y clics reales del usuario contaminan las pruebas
sinteticas. Si un resultado no cuadra, comprobar `GetCursorPos` tras
`SetCursorPos` antes de dar por bueno el diagnostico.

### 29.9 Ficheros

Nuevos:

- `port/io/io_keymap.h` / `port/io/io_keymap.c` — tablas de traduccion.
- `port/io/io_keyboard.c` — cola de eventos estilo INT 16h.
- `port/div/ide/port_ide_keybo.c` — sustituto de `divkeybo.cpp`.

Modificados: `port/io/div_io.h` (bloque de teclado), `port/io/io_input.c`
(reescrito: solo raton + `io_poll`, la tabla se movio a `io_keymap.c`),
`port/io/io_video.c` (drenaje tras `EndDrawing()`),
`port/div/ide/port_ide_stubs.c` (fuera los stubs de teclado), `CMakeLists.txt`
(los **4** targets que usan `io_input.c` necesitan tambien `io_keymap.c` y
`io_keyboard.c`), `tools/drive_ide.ps1`.

### 29.10 Pendiente

- No verificado con acentos y `ñ` reales ni con `AltGr` fisico en teclado
  espanol; la ruta esta implementada pero solo probada con teclas sinteticas.
- Escribir en el editor de PRG sigue sin poder probarse: el editor pinta negro
  hasta que entre **E3** (`divbrow.cpp` + `divfont.cpp`).


---

## 30. Hito E3 del port del IDE — navegador de ficheros y editor de fuentes/texto

### 30.1 Objetivo

Que `Programas -> Nuevo / Abrir` deje de pintar negro: `browser0()` (el selector
de ficheros) vive en `divbrow.cpp` y `ShowText()` (el pintado del texto del
editor y del hipertexto) en `divfont.cpp`. Hasta E2.3 ambos eran stubs vacios y
cualquier dialogo de fichero quedaba como ventana modal 640x480 con `dummy_handler`.

### 30.2 Cambios de build y prototipos

- `CMakeLists.txt`: `src/div/divbrow.cpp` y `src/div/divfont.cpp` entran en
  `DIV_IDE_SOURCES`.
- `port/div/ide/port_ide_protos.h` regenerado incluyendo los **5** modulos del
  target con forward-calls (`divpaint`, `divpalet`, `divmouse`, `divbrow`,
  `divfont`), como exige la regla de §28.2:

  ```powershell
  python tools\gen_protos.py src\div\divpaint.cpp src\div\divpalet.cpp src\div\divmouse.cpp src\div\divbrow.cpp src\div\divfont.cpp > port\div\ide\port_ide_protos.h
  ```

### 30.3 Stubs fuera de `port_ide_stubs.c`

Duplicados que ahora viven en los modulos reales: `browser0`, `ShowFont0`,
`OpenFont`, `OpenGenFont`, `ReloadFont`, `imprime_rutabr`, `dir_abrirbr` y los
datos `larchivosbr`, `num_taggeds`, `thumb`, `input2` (`char input2[32]` esta en
`divbrow.cpp:16`) y los `Text1/2/3*` de `divfont.cpp`.

### 30.4 Stubs NUEVOS (tipados, no `void *`)

Los modulos nuevos referencian simbolos de modulos aun no portados
(`divpcm`, `divsetup`, `ifs.cpp`) y de JUDAS. Se definen con el tipo correcto
en un bloque `E3` de `port_ide_stubs.c`:

- **Audicio**: el audio se mantiene `DEV_NOSOUND` declarando
  `unsigned judascfg_device = DEV_NOSOUND;` (antes era un `void *` que por
  casualidad valia 0). `judas_stopsample`, `judas_freesample`,
  `judas_playsample`, `judas_loadwav`, `judas_loadrawsample` y los *load/play*
  de XM/MOD/S3M quedan como no-ops que devuelven `JUDAS_WRONG_FORMAT`/`NULL`:
  los browsers desactivan la "prueba" (`opc_pru=0`) y las rutas de carga lo
  tratan como error controlado. `judas_error` y `judas_channel[CHANNELS]`
  siguen el layout real de `judas.c`.
- **Musica**: `SongType=0`, `SongCode=10`, `last_mod_clean=1`, `FreeMOD()` no-op
  (mismos valores iniciales que `divpcm.cpp`).
- **Memoria**: `Mem_GetHeapFree()` y `GetFreeMem()` devuelven 16 MB ficticios
  (con el `struct meminfo` de `divbrow.cpp:891`) — suficiente para que los
  thumbnails no aborten por falta de memoria.
- **ifc/FNT** (`ifs.cpp`, no portado): `ifs`, `bodyTexBuffer`,
  `outTexBuffer`, `shadowTexBuffer`, `Jorge_Crea_el_font`, `ShowChar`,
  `GetCharSize` (8x8), `ConvertFntToPal`. `GetCharSizeBuffer`/`ShowCharBuffer`
  ya existian como stubs.
- `ifs.h` y `judas/judas.h` se incluyen directamente en `port_ide_stubs.c`:
  son autocontenidos (`ifs.h` solo son tipos+externs; `judas.h` solo declara).

### 30.5 Verificacion

- Enlace: `cmake --build build --target div_ide_port --config Release` -> exit 0
  (27 simbolos sin resolver + 5 duplicados resueltos; solo warnings C4311
  preexistentes y C4091 de la global.h adaptada).
- Smoke test: el `.exe` arranca y se mantiene vivo (sin crash a los 4 s).
- **Confirmado en caliente**: `Programas -> Nuevo` abre el browser real, se
  puede crear un programa, guardarlo, editarlo y cerrarlo, y el texto del
  editor se pinta (teclado operativo desde E2.3).

### 30.6 Bug corregido: `system_clock` (0xC0000005 al hacer clic en un campo)

El primer intento en caliente fallaba: al hacer clic en la caja de texto del
nombre de programa en "crear programa", el IDE se cerraba con excepcion
0xC0000005 en offset `0x952d`.

- Diagnostico (Event Log + `div_ide_port.map` + desensamblado de `.text` con
  dumpbin): el fallo esta en `get_input()` (`div.cpp:3466`) y la instruccion
  `mov eax, dword ptr [rcx]` con `rcx = *(0x10064000)`.
- Causa raiz: `div.cpp:38` define `int * system_clock = (void*) 0x46c;` (el
  contador de ticks de la BIOS en DOS). Esa direccion no existe en Windows (baja
  memoria, pagina NULL), asi que cualquier `*system_clock` reventaba. Hasta E3
  nunca se llegaba a ejecutar porque el browser no existia.
- Fix: `port_ide_dos.c`, en `port_ide_tick()` (ya llamado cada vuelta del bucle
  via `tecla()`), se reengancha el puntero a `&reloj` (el reloj de 100 Hz real
  del port): `system_clock = &reloj;`. Con 100 Hz los usos del nucleo
  (doble_click, cursor de `get_input`, parpadeo, `oclock`) funcionan igual.
- Sin cambios en `src/div` (CP850 intacto); el fix vive en la capa de port.

### 30.7 Ficheros

Modificados: `CMakeLists.txt`, `port/div/ide/port_ide_protos.h`,
`port/div/ide/port_ide_stubs.c`, `port/div/ide/port_ide_dos.c`.
Sin cambios en `src/div` (CP850 intacto).

### 30.8 Pendiente

- **E4**: boton "compilar" que invoque a `divc_port` (compilar `/purge`) y
  "probar" que lance `div32run_port`; despues los editores de recursos
  (fpg, paleta, paint, font, sonido, ayuda).

---

## 31. Hito E4 del port del IDE — Compilar y Probar

### 31.1 Objetivo

Que el boton "compilar" (F11) invoque `divc_port.exe` y "probar" (F10/F12) lance
`div32run_port.exe`, cerrando el loop edicion→compilacion→ejecucion sin salir
del IDE.

### 31.2 Cambios de build y prototipos

- `CMakeLists.txt`: `port/div/ide/port_ide_compila.c` entra en `DIV_IDE_SOURCES`.
- `port/div/ide/port_ide_forward_decls.h`: prototipo de
  `port_ide_tras_compilar_ok()`.

### 31.3 Parches byte-exactos en `src/div`

Los puntos donde se compila/ejecuta sustituyen el "salir del IDE" por una llamada
al port:

- `src/div/div.cpp:1119` (hotkeys F10/F12 de la ventana PRG):
  `modo_de_retorno=1; salir_del_entorno=1;` → `port_ide_tras_compilar_ok();`
- `src/div/divhandl.cpp:224-225` (menu Programar -> Probar):
  `modo_de_retorno=1;` / `salir_del_entorno=1;` → `port_ide_tras_compilar_ok();`

### 31.4 Nuevo modulo `port_ide_compila.c`

- `compilar_programa()`: vuelca `source_ptr`/`source_len` a `.prg`, invoca
  `divc_port.exe` con `CreateProcessA` (sin shell `cmd.exe` para evitar problemas
  de rutas), captura la salida a un fichero temporal, parsea
  `Error N (linea X, columna Y)` y actualiza `numero_error`/`linea_error`/`columna_error`.
- `port_ide_tras_compilar_ok()`: si `ejecutar_programa` es 1 o 3 y compilo OK,
  cambia el cwd del IDE a la carpeta del `.div32`, lanza `div32run_port.exe` con
  `CreateProcessA`, espera a que termine, y restaura el cwd.
- Los stubs `compilar_programa()`, `numero_error`, `linea_error`, `columna_error`
  se quitan de `port_ide_stubs.c` (ahora definidos aqui).

### 31.5 Verificacion

- Enlace: `cmake --build build --target div_ide_port --config Release` -> exit 0.
- **Confirmado en caliente**:
  - **F11 (Compilar)**: compila el programa actual y muestra el resultado.
  - **F10/F12 (Probar)**: compila y ejecuta; el IDE **no se cierra**; al cerrar
    el juego, se vuelve al IDE.
  - Programa minimo (`PROGRAM test; BEGIN FRAME; END`) compila y ejecuta sin
    errores.

### 31.6 Limitacion conocida

- **Recursos externos (FPG, PCM, etc.)**: el runner falla con `Error 105
  (load_fpg)` cuando el programa usa recursos externos. Esto es porque el runner
  no encuentra los recursos relativos al `.div32` compilado. Se aborda en **E5**
  (sistema generalizado de recursos).

### 31.7 Ficheros

Modificados: `CMakeLists.txt`, `port/div/ide/port_ide_forward_decls.h`,
`port/div/ide/port_ide_stubs.c`, `src/div/div.cpp`, `src/div/divhandl.cpp`.
Nuevo: `port/div/ide/port_ide_compila.c`.

## 32. Hito E5 del port del IDE — recursos al Probar (`Error 105 (load_fpg)`)

### 32.1 El sintoma y la causa real

Tras E4, Probar un programa con recursos externos moria con
`Error 105 (load_fpg)`. Reproducido con el corpus del propio repo: los
tutoriales escriben

```
load_fpg("tutorial\tutor0.fpg");
```

y la instalacion de DIV guarda ese fichero en `FPG\TUTORIAL\TUTOR0.FPG`
(el `install` del makefile copia `resource\<x>\*.*` a `<destino>\<x>`).

La causa **no** era el compilador ni el lanzador, sino que `open_file()`
(`f.cpp`) tiene **dos variantes** y el port compilaba la de produccion:

| Variante | Cadena de busqueda | Resultado con `tutorial\tutor0.fpg` |
|---|---|---|
| Produccion (`#else`) | `nombre.ext`, `ext\nombre.ext` | `fpg\tutor0.fpg` → **no existe** |
| `#ifdef DEBUG` | ruta literal, `ext\ruta`, `nombre.ext`, `ext\nombre.ext` | `fpg\tutorial\tutor0.fpg` → **existe** |

La variante `DEBUG` es la que llevaba `SESSION.386`, **el binario que el IDE
de DOS lanzaba justo al "Probar"**. Es decir: el DIV original ya resolvia esto
usando un interprete distinto al que se empaqueta con el juego. El port tiene
un unico `div32run_port`, asi que hereda la cadena permisiva.

### 32.2 Cambios

1. **`port/div/core/f.cpp`** (parche byte-exacto en Python, fichero derivado de
   `src/`): el selector pasa de `#ifdef DEBUG` a
   `#if defined(DEBUG) || defined(PORT_OPEN_FILE_SEARCH)`, y el ultimo fallo de
   la cadena delega en `port_open_file_extra()` en vez de devolver `NULL`.
2. **`port/div/core/port_pre.h`**: `#define PORT_OPEN_FILE_SEARCH 1`.
3. **`port/div/core/port_res_path.c`** (nuevo): raices adicionales de busqueda.
   Lee `DIV_RES_PATH` (lista de directorios separados por `;`), y para cada una
   hace `_chdir` y **reutiliza `open_file()`** en vez de duplicar su logica (un
   flag estatico corta la reentrada). Restaura el cwd siempre.
4. **`port/div/core/port_forward_decls.h`**: prototipo de
   `port_open_file_extra()` (+ `#include <stdio.h>` para `FILE`).
5. **`port/div/ide/port_ide_compila.c`**: `port_ide_tras_compilar_ok()` lanza
   ahora el runner con **cwd = raiz de DIV** (`tipo[1].path`, la carpeta del
   ejecutable del entorno — es lo que hacia `DIV.BAT`), en vez de la carpeta
   del `.div32`; y fija `DIV_RES_PATH` con las raices adicionales:
   carpeta del `.div32`, raiz del repo y `<raiz>\resource` (en el arbol de
   desarrollo los recursos aun no estan "instalados" en la raiz).
6. **`CMakeLists.txt`**: `port_res_path.c` entra en `DIV_CORE_SOURCES`.

### 32.3 Verificacion

- Reproduccion previa: `.div32` del tutorial 1b en una carpeta con la
  estructura real de instalacion (`fpg\tutorial\`, `fnt\tutorial\`) →
  `Error 105 (load_fpg)`.
- Tras el cambio, misma carpeta → **sin error**, el programa corre.
- Raices extra: `.div32` solo, en una carpeta **sin ningun recurso**, con
  `DIV_RES_PATH` apuntando a la raiz de recursos → **sin error**.
- Combinacion exacta que produce el IDE (cwd = `build\Release`,
  `DIV_RES_PATH=<carpeta del .div32>;<repo>;<repo>\resource`, `.div32`
  compilado con `divc_port` desde `resource\prg\tutor\tutor1b.prg`) →
  **sin error**.

### 32.4 Nota de comportamiento

La cadena permisiva es un **superconjunto** de la de produccion: solo anade
intentos previos, nunca descarta los que ya funcionaban. El riesgo es
teorico (un juego que traiga un fichero con el mismo nombre en la ruta
literal y espere que se ignore), y a cambio es el comportamiento que el
propio DIV usaba al probar desde el entorno.

### 32.5 Ficheros

Modificados: `CMakeLists.txt`, `port/div/core/f.cpp`,
`port/div/core/port_pre.h`, `port/div/core/port_forward_decls.h`,
`port/div/ide/port_ide_compila.c`.
Nuevo: `port/div/core/port_res_path.c`.

## 33. Cierre de la ventana del juego (boton X / ALT+F4)

### 33.1 El sintoma

Detectado nada mas empezar a usar el ciclo de E5: un juego lanzado desde el
IDE **no se puede cerrar de ninguna manera**; hay que matar el proceso. Y
como el IDE espera al hijo con `WaitForSingleObject(..., INFINITE)`, se
queda colgado con el.

### 33.2 La causa

No es una regresion de E4/E5: en DOS un programa DIV solo terminaba porque
se le acababan los procesos, porque llamaba a `exit()` del lenguaje o por un
error. **No existia el concepto de "cerrar la ventana"**, asi que el runtime
original no tiene ningun camino para atender esa peticion.

En el port la ventana si existe, pero:

- `io_window_should_close()` (`port/io/io_video.c`) estaba implementada y
  **nadie la llamaba** desde `div32run_port` (solo `port/main.c`, el
  esqueleto de la Fase 1);
- y `SetExitKey(KEY_NULL)` desactiva a proposito el cierre con ESC de raylib
  (el IDE usa ESC para cerrar dialogos).

Resultado: un juego con bucle infinito -- es decir, casi cualquier juego
antes de tener su propio menu de salida -- era inmortal.

### 33.3 El cambio

- **`port/div/core/port_cierre.c`** (nuevo): `port_check_cierre()` consulta
  `io_window_should_close()` y, si procede, replica el apagado ordenado de
  `_exit_dos()` (la implementacion del `exit()` del lenguaje DIV, `f.cpp`):
  `rvmode()` (restaura el modo de video), `kbdReset()`, vuelta a `divpath` y
  `exit(26)` -- el mismo codigo de salida que la terminacion normal.
- **`port/div/core/v.cpp`** (parche byte-exacto): `volcado()` llama a
  `port_check_cierre()` justo despues de `io_present()`, o sea una vez por
  frame volcado.
- `port_forward_decls.h` y `CMakeLists.txt`: prototipo y fuente.

**Por que en `v.cpp` y no en `port/io`**: la capa io es compartida con el
IDE, que tiene su propia ventana y su propio dialogo de confirmacion
("Salir de DIV?"). El chequeo debe ser exclusivo del runtime.

### 33.4 Verificacion

`tutor1b.div32` lanzado con la configuracion de E5, `WM_CLOSE` enviado al
proceso (`Process.CloseMainWindow()`) a los 6 s: **el proceso termina con
exit 26** en vez de quedarse vivo. Antes del cambio habia que matarlo.

### 33.5 Limitacion conocida

El chequeo vive en el volcado de pantalla, asi que un programa que se cuelgue
**sin volcar ningun frame** sigue necesitando matar el proceso. Es el mismo
caso en que Windows ya marca la ventana como "no responde"; no se ha buscado
una red de seguridad adicional.

Aparte, mientras el juego corre el IDE queda bloqueado (espera al hijo). Es
el comportamiento buscado por ahora -- "probar" es modal, como en el DIV
original, que directamente salia del entorno -- pero se puede revisar si
molesta.

## 34. `set_mode()` en caliente: imagen desdoblada / desplazada

### 34.1 El sintoma

Al arrancar un juego real (STEROID) desde el IDE, la imagen salia
**desdoblada** a 640x480 (el mismo texto repetido a media anchura) y
**desplazada** a 320x200.

### 34.2 La causa

Era una **limitacion conocida y documentada** en el propio `svmode()`
(`v.cpp`): `io_video_init()` solo se podia llamar una vez, asi que la segunda
llamada a `set_mode()` se ignoraba en silencio.

Un juego DIV tipico llama a `set_mode()` **varias veces** (320x200 para la
presentacion, 640x480 al entrar al juego). Al ignorarse el cambio:

- `set_mode()` (`f.cpp:1697`) **si** reasignaba `copia`/`copia2` y `region[0]`
  a la nueva resolucion,
- pero el framebuffer de `port/io` seguia con la anchura vieja,
- y `volcado()` hace `memcpy(vga, p, vga_an*vga_al)`.

Es decir: filas de 640 pixeles escritas en un buffer de 320 de ancho. Cada
fila ocupa dos, y la imagen sale partida/desdoblada. El "stride" no coincide.
Los demos previos del port nunca lo destaparon porque llamaban a `set_mode()`
una sola vez.

### 34.3 El cambio

- **`port/io/io_video.c`**: nueva `io_video_resize(w,h)`. Recrea framebuffer,
  buffer RGBA y textura, y reajusta la ventana a un multiplo entero que quepa
  en el monitor (baja la escala si hace falta: 640x480 x3 no cabe en 1440 de
  alto, asi que pasa a x2). Devuelve el **nuevo** puntero al framebuffer; si
  falla una reserva no toca nada y devuelve NULL. Si falla solo la textura,
  devuelve igualmente el framebuffer nuevo (el viejo ya esta liberado y
  devolver NULL dejaria al nucleo con un puntero colgante).
  Se extrajo `io_video_make_texture()` para no duplicar la creacion.
- **`port/io/div_io.h`**: prototipo, documentando que hay que reasignar
  siempre el puntero devuelto.
- **`port/div/core/v.cpp`** (parche byte-exacto): `svmode()` crea la ventana
  la primera vez y **redimensiona** las siguientes.

### 34.4 Verificacion

Programa de prueba `test_modos.prg` (320x200 con marco + diagonal, luego
640x480 con lo mismo), compilado con `divc_port` y ejecutado midiendo el area
cliente de la ventana con `GetClientRect`:

```
antes:   960x600     (320x200 x3)
despues: 1280x960    (640x480 x2)
exit=26
```

Antes del cambio la ventana se quedaba en 960x600 y el contenido de 640 de
ancho se desdoblaba dentro del buffer de 320.

### 34.5 Ficheros

Modificados: `port/io/io_video.c`, `port/io/div_io.h`,
`port/div/core/v.cpp`.

## 35. El input de un juego real: `scan_code`/`ascii` y el `++` que no incrementaba

Sintoma reportado: STEROID arranca y se ve bien, pero **no reacciona a ninguna
tecla**. Resultaron ser dos fallos independientes, y el segundo es de los
gordos.

### 35.1 Falso sintoma previo: `set_mode(m320x200)`

Antes de esto, el `.prg` del usuario tenia en la linea 32
`set_mode(m320x200)` mientras el juego esta escrito para **640x480**
(`put_pixel(rand(0,639),rand(0,479))`, textos centrados en x=320 y x=640,
`write(1,640,480,...)`). A 320x200 solo se ve el cuadrante superior izquierdo
y los textos se salen: parecia un fallo del port y no lo era. Restaurado a
`m640x480`, la pantalla de presentacion sale exacta.

### 35.2 `scan_code`/`ascii` estaban clavados a 0

Era una **limitacion conocida y documentada** en la cabecera de
`divkeybo.cpp`: el original les daba semantica de evento desde el manejador
de la IRQ 9 y el port no tenia con que alimentarlos, asi que `tecla()` los
ponia a 0 en cada frame.

El problema es que STEROID no usa solo `key()`:

```
scan_code=0;
REPEAT FRAME; UNTIL (scan_code==0)              // espera a que se suelte todo
REPEAT ... FRAME; UNTIL (scan_code<>0 OR salir_==1)   // espera a que se pulse algo
```

Con `scan_code` siempre 0, el segundo bucle no termina nunca: el juego se
queda para siempre en "PRESIONE UNA TECLA PARA JUGAR". Desde fuera parece
"el teclado no funciona", aunque `key()` si funcionaba.

La semantica real del original (se deduce de ese mismo bucle) **no es de
evento sino de estado**: el manejador de la IRQ 9 ponia `scan_code` al pulsar
y lo volvia a 0 al soltar. Por eso "espera a que `scan_code==0`" significa
"espera a que no haya ninguna tecla pulsada".

Solucion: `tecla()` ahora se alimenta de la cola de eventos de
`port/io/io_keyboard.c` -- la que se escribio para el IDE en E2.3 -- mas el
estado mantenido:

- se drena la cola: cada pulsacion fija la "tecla viva" (scancode + ascii en
  CP850). Esto recoge tambien las pulsaciones **mas cortas que un frame**,
  que un polling por estado perderia;
- si no ha llegado ningun evento y la tecla viva ya no esta pulsada, se pasa
  a otra que siga pulsada, o a 0;
- `scan_code`/`ascii` toman ese valor, y `vacia_buffer()` limpia tambien la
  cola.

El IDE no se ve afectado: usa `port/div/ide/port_ide_keybo.c`, no
`divkeybo.cpp` (ver el comentario en `CMakeLists.txt`).

### 35.3 Y entonces: `Error: Too many process!`

Con el teclado arreglado el juego arrancaba... y moria en menos de dos
segundos con `Too many process!` (`exer(2)` desde `kernel.cpp`).

Descartados por experimento, con PRGs minimos compilados con `divc_port` y
leyendo el resultado en pantalla con `write_int`:

| Sospechoso | Prueba | Resultado |
|---|---|---|
| `get_id(TYPE x)` devolviendo 0 (el bucle de subir de nivel crearia asteroides sin parar) | `test_getid.prg` | **Correcto**: devuelve 1801 |
| `collision()` con falsos positivos (cada asteroide crea 2-4 procesos por frame al chocar) | `test_coll.prg` / `test_coll3.prg` | **Correcto**: 0 con sprites separados, y no altera la instruccion siguiente |
| El operador `++` | `test_inc.prg` | **ROTO** |

`test_inc.prg` incrementa cuatro variables globales por frame y las pinta:

```
a++    : 0      <-- roto
++b    : 60
c+=1   : 60
d=e++  : d=0, e=0   <-- roto
```

Es decir: **el post-incremento y el post-decremento no hacian nada**. Y
STEROID los usa donde mas duele:

```
FOR (contador0=0;contador0<2+nivel;contador0++)
    asteroide(-16,-16,3);
END
```

`contador0` nunca sube, el `FOR` no termina y el juego crea asteroides hasta
agotar la tabla de procesos. De ahi el `Too many process!`.

### 35.4 La causa: orden de evaluacion indefinido

En `kernel.cpp`, los opcodes de incremento con puntero estaban escritos asi:

```c
case lpti:
        pila[sp]=mem[pila[sp]]++;
    break;
```

`pila[sp]` es **a la vez** la direccion que hay que incrementar y el destino
del resultado, y entre los dos efectos secundarios no hay punto de secuencia:
el orden es indefinido. OpenWatcom incrementaba primero y luego asignaba (lo
que el autor pretendia); MSVC asigna primero, asi que el incremento acaba
cayendo en `mem[valor_antiguo]` -- la variable no cambia y, de propina, se
escribe en una posicion arbitraria de `mem[]`.

Las 12 variantes (`lipt`/`lpti`/`ldpt`/`lptd` y sus versiones `chr` y `wor`)
tenian el mismo patron. Las de pre-incremento funcionaban **por casualidad**,
segun como ordenara MSVC ese caso concreto. Se reescriben todas metiendo la
direccion en un temporal, que es exactamente la semantica pretendida:

```c
case lpti: { int dir=pila[sp]; pila[sp]=mem[dir]++; } break;
```

Queda pendiente, como riesgo latente de la misma familia, `lada`/`lsua`/... 
(`pila[sp-1]=mem[pila[sp-1]]+=pila[sp];`): hoy MSVC los resuelve bien --
`c+=1` funciona -- pero es el mismo patron sin punto de secuencia.

### 35.5 Verificacion

- `test_inc.prg` tras el arreglo: `a++`=61, `++b`=61, `c+=1`=61,
  `d=e++` deja `d`=60 y `e`=61 (semantica de post-incremento correcta).
- STEROID a 640x480, con pulsaciones inyectadas con `keybd_event`: pasa de la
  presentacion al juego, salen la nave, los 3 asteroides (`2+nivel`), los 3
  graficos de vidas y `LEVEL 1`; las flechas rotan y aceleran la nave, los
  asteroides se mueven y **no** aparece `Too many process!`. Cierre con la X:
  exit 26.
- Reconstruidos todos los targets. De paso se arreglo `div_smoke_test`, que
  no enlazaba desde §33 porque le faltaba `port_cierre.c`.

### 35.6 Ficheros

Modificados: `port/div/core/divkeybo.cpp`, `port/div/core/kernel.cpp`,
`CMakeLists.txt`.

---

## 36. `collision()` ignoraba los ids de tipo negativos (los disparos de STEROID no colisionaban)

### 36.1 El sintoma

STERoid ya era jugable tras §35: la nave rota y acelera, los asteroides se
mueven, el sonido suena... pero **los disparos atravesaban los asteroides sin
efecto**: ni puntuacion, ni subdivision, ni `s_kill`. El resto del juego
(incluida la colision nave-asteroide y `signal(TYPE ...)`/`get_id(TYPE ...)`)
funcionaba.

### 36.2 Diagnostico

Descartado lo geometrico: el disparo mide 14x5 y avanza 16 px/frame; el
asteroide mas pequeno mide 21x22 (los grandes 50x50). Con cajas de >=21 px
frente a un paso de 16 px, el tunneling es imposible. El fallo era sistematico.

La clave esta en como se numeran los tipos de proceso. El compilador emite como
id de tipo **el puntero del `struct objeto` truncado a int**
(`divc.cpp:4057`/`4123`: `g2(ltyp,(int)bloque_actual)`; el opcode `ltyp`
(lo) guarda en `mem[id+_Bloque]` al arrancar el proceso, `kernel.cpp:140`). En
x64, `(int)puntero` puede tener el bit 31 del low-32 activo, es decir, ser
**negativo** como `int`.

La `collision()` del port (`s.cpp`, escrita en el checkpoint 13) abria con:

```c
tipo=pila[sp];
if (tipo<=0) { pila[sp]=0; return; }   // <-- el bug
```

Ese guard no existe en la original (`c.cpp:157`), que solo especial-casa
`bloque==0` (collision con el raton). Con un id de tipo negativo, el port
devolvia 0 **sin mirar un solo proceso**: todas las colisiones del programa
desaparecian.

Por que colo hasta ahora: el demo del checkpoint 13 se construyo a mano con
ids de tipo 1 y 2 (positivos y pequenos), asi que el guard no se disparaba. Y
`get_id()`/`signal()` nunca tuvieron ese guard (`f.cpp:844`, `f.cpp:71`), por
eso el resto de STEROID funcionaba.

### 36.3 El cambio

`port/div/core/s.cpp`, `collision()`: `tipo<=0` -> `tipo==0` (una linea, con
comentario). El caso `==0` (raton) sigue devolviendo 0 — la colision con el
raton no esta implementada en el port, como antes.

### 36.4 Verificacion

- Test automatico (sin input): asteroide fijo en (320,240) + disparo que sale
  de (100,240) y avanza 16 px/frame; la rama `IF (collision(TYPE disparo))`
  fuerza un `load_pcm()` a un fichero inexistente, observable como
  `Error 128 (load_pcm/wav)` en stdout. Antes del fix: timeout sin error
  (rama muerta). Despues: **Error 128 inmediato** = colision detectada.
- **Confirmado en caliente por el usuario**: STEROID, los disparos rompen los
  asteroides y puntuan.

### 36.5 Riesgo latente de la misma familia

Cualquier comparacion futura que asuma "id de tipo > 0" vuelve a romper esto.
Los ids de tipo son enteros con signo arbitrario (punteros truncados); solo el
valor 0 tiene significado especial (raton). Lo mismo aplica al escanear
`_Bloque` en funciones nuevas.

---

## 37. Joystick fantasma: `inp(0x201)` devolvia 0 (MALVADO saltaba sin parar)

### 37.1 El sintoma

MALVADO arrancaba y se podia jugar, pero **el protagonista saltaba
constantemente** sin tocar ninguna tecla.

### 37.2 Diagnostico

El salto es `IF (key(_space) OR key(_control) OR joy.button1)`. Descartado el
teclado (ya verificado en §35) y `map_get_pixel` (identica a la original; las
paletas de `malvado.fpg` y `nivel1.fpg` son identicas, asi que `adaptar()` no
remapea nada), quedaba `joy.button1`.

`read_joy()` (`f.cpp:2470`) hace `n=inp(GAME_PORT)` (0x201) y luego
`if(n&16) joy->button1=0; else joy->button1=1;`. El shim del port
(`port/div/shim/i86.h`) devolvia **0** para todo puerto. Con 0:

- bit 4 = 0 -> **`joy->button1=1` permanente** (boton siempre pulsado).
- El detector de ausencia de `joy_position()` (`for(i=0;i<TIME_OUT;i++)
  if((inp(GAME_PORT)&mask)==0) break;`) salia en la primera vuelta
  (0&mask==0), asi que `joy_timeout` nunca llegaba a 6 y la proteccion de
  "joystick desconectado" de `i.cpp:813-818` (`joy_status=0`, botones a 0)
  **nunca se activaba**.

El hardware real sin joystick lee el puerto 0x201 con los bits en **alto**
(0xFF): botones sin pulsar, y los ejes (medicion RC por tiempo) agotan el
timeout. El valor correcto de "ausente" era 0xFF, no 0.

### 37.3 El cambio

`port/div/shim/i86.h`: `div_port_inp()` devuelve **0xFF** para el puerto
0x201 (game port) y 0 para el resto (las lecturas del PIT solo se usan si hay
joystick, y nunca lo hay). Con ello: botones a 0 desde el primer frame, y a
los 6 frames `joy_status` se pone a 0 solo, como en DOS sin joystick.

Nota: el IDE usa otro shim (`port_ide_pre.h`, `inportb(port) 0`); no se toca
porque no hay sintoma conocido en el IDE.

### 37.4 Verificacion

- Test automatico: `IF (joy.button1) ...Error 128... ELSE ...Error 105...`
  tras varios frames. Antes: **Error 128** (boton pegado). Despues:
  **Error 105** (boton libre).
- **Confirmado en caliente**: MALVADO ya no salta solo; el salto responde a
  espacio/control.

---

## 38. `out_region()` era un stub vacio (MALVADO no quitaba vidas ni reiniciaba al morir)

### 38.1 El sintoma

Al caer y morir, el protagonista hacia la animacion de muerte pero **no se
restaba ninguna vida y el juego no reiniciaba** al personaje.

### 38.2 Diagnostico

`muerte_jack()` (`MALVADO.PRG:687-693`) anima la muerte con
`REPEAT ... FRAME; UNTIL (out_region(id,0))` — espera a que el grafico salga
de la pantalla antes de `vidas--` y el respawn.

`out_region` estaba en `port_stubs.c` como `void out_region(void) {}`:
**no tocaba la pila**. El contrato es `reg=pila[sp--]; id=pila[sp];
pila[sp]=resultado` — el stub dejaba en `pila[sp]` el propio argumento
`region` (0 en la llamada), asi que `out_region(id,0)` devolvia **0 siempre**
("sigue dentro"): el bucle no terminaba nunca y el flujo de vidas/respawn no
se ejecutaba.

### 38.3 El cambio

Implementacion real en `port/div/core/s.cpp` (junto a `collision()`), portada
fielmente de la original (`c.cpp:30-109`):

- Caja del sprite con rotacion (`port_sp_size`, copia de `sp_size` de c.cpp,
  que no entra en el build del port), escalado (`port_sp_size_scaled`),
  pivote del grafico y flags de espejo; soporte `_XGraph`.
- `_Ctype==1` (scroll): interseccion con la ventana de cada scroll activo y
  luego con la region; `_Ctype==0` (pantalla): interseccion directa con
  `region[reg]`.
- Devuelve 1 si el grafico esta completamente fuera, 0 si la toca.
- El stub se elimina de `port_stubs.c` (queda el comentario actualizado).

### 38.4 Verificacion

- Test automatico: proceso cayendo 8 px/frame desde y=240 (sprite 32x20).
  Frame 20 (aun visible): `out_region(id,0)`=0. Frame 80 (totalmente fuera):
  `out_region(id,0)`=1. Correcto en ambos.
- **Confirmado en caliente**: MALVADO, al morir se resta una vida y el
  personaje reaparece en el punto de control.


## 39. Musica de tracker (MOD/S3M/XM) con libmikmod vendored

Antes de este hito la musica era un stub: io_load_song() devolvia -1 y todo
el bloque de canciones de divsound.cpp era codigo muerto. Ahora TOKENKAI
carga y suena su mod\tokenkai\token.s3m (Screamtracker 3.01, 8 canales, 16
posiciones, 18 patrones).

### 39.1 Ansatz

- **Vendoring** de libmikmod 3.3.14 en 3rdparty/mikmod/ como lib static:
  include/ (mikmod.h, mikmod_internals.h, mikmod_ctype.h + config.h
  propio), src/drivers/ (drv_win + drv_nos), src/loaders/ (load_mod,
  load_s3m, load_xm), src/mmio/, src/playercode/ (sin mlreg.c),
  src/posix/strcasecmp.c (el _mm_strcasecmp que mdriver.c usa).
  load_uni.c NO hace falta (load_xm no lo referencia — correccion de un
  error previo del resumen); MikMod_RegisterAllLoaders() no se usa (exige
  los 19 loaders), se registran los 3 con MikMod_RegisterLoader().
- **config.h propio** (DRV_WIN 1, NO_DEPACKERS 1, MIKMOD_STATIC,
  headers HAVE_* de MSVC) SIN HAVE_UNISTD_H ni MIKMOD_UNIX: evitan
  <unistd.h>/<pwd.h> y el bloque POSIX de mdriver.c.
- **Trampa conocida**: mikmod.h NO lee config.h (solo mikmod_internals.h
  y los .c). Cualquier TU consumidor de los headers necesita
  -DHAVE_CONFIG_H -DMIKMOD_STATIC como defines de compilacion; sin ellos
  mikmod.h declara la API como __declspec(dllimport) y el enlace falla con
  __imp_MikMod_* sin resolver.
- **TU separado** port/io/io_song.c: mikmod.h incluye <windows.h> y sus
  macros (PlaySound, CloseWindow, ShowCursor, Rectangle...) chocan
  con raylib.h en el mismo TU. La musica vive solo en ese fichero;
  io_audio.c (raylib) solo llama io_song_init()/io_song_close().
- **divsound.cpp portado**: LoadSong (io_load_song = solo valida) guarda
  el fichero en cancion[].ptr con el nuevo campo 	Cancion.Len;
  PlaySong corta lo que suene y llama io_play_song() (Player_LoadMem +
  mod->loop/mod->reppos + Player_Start). No hay global SongType en
  divsound.cpp (las funciones ya no ramifican por formato; el del IDE vive en
  port_ide_stubs.c).

### 39.2 Dos bugs de integracion descubiertos con el probe

El probe (C:\Users\Dani\AppData\Local\Temp\opencode\song_probe) revelo dos
detalles que no estaban documentados en ningun tutorial de libmikmod:

1. **Player_LoadMem(maxchan=0) no configura las voces**: mloader.c solo
   llama a MikMod_SetNumVoices_internal si maxchan>0. Con maxchan=0,
   md_sngchn queda en 0, c_softchn=0, y VC1_WriteBytes entra en su
   rama if(!vc_softchn) return VC1_SilenceBytes(...): escribe silencio y
   **jamas avanza**. Sintoma: Player_Active()==1 pero Player_GetRow()==0
   eterno. Fix: MikMod_SetNumVoices(32,0) justo despues de MikMod_Init()
   en io_song_init().
2. **El driver winmm es poll-driven, no tiene hilo propio**: la mezcla solo
   ocurre cuando la aplicacion llama MikMod_Update(). Sin esa llamada, se
   llenan los ~2 buffers iniciales de waveOut (240ms) y la musica se queda
   congelada. Fix: io_song_update() (-> MikMod_Update()) llamado UNA vez
   por frame desde rame_end() (i.cpp), declarado en port/div/shim/dos.h
   como el resto de bridges PORT.

### 39.3 Verificacion

- Probe de carga: TOKEN.S3M -> "Screamtracker 3.01", 8 canales, 16 posiciones.
- Probe de reproduccion (ciclo exacto del juego: init + SetNumVoices +
  Player_LoadMem + loop + updates): ctivo=1, fila avanza en tiempo real a
  traves del waveOut real, Player_SetPosition(5) -> orden 5 confirmado.
- Juego real: div32run_port.exe TOKENKAI.div32 corre 25s sin errores ni
  cierre, con la musica cableada.

### 39.4 Pendiente

- **Confirmacion auditiva** del S3M dentro del juego (objetivo cerrado por
  probe, falta oido humano en la maquina).
- En el IDE (div_port, bucle en src/div/div.cpp) todavia no se llama
  io_song_update(): su musica fallaria igual que el sintoma de arriba
  cuando llegue el turno del audio del IDE.

## 40. `path_find()`/`path_line()`/`path_free()` reales (el movimiento de TOKENKAI)

### 40.1 Síntoma

TOKENKAI: "el teclado no responde y el personaje no se mueve". Dos cosas
distintas en una:

1. **El malentendido del teclado**: TOKENKAI no se controla con el teclado.
   El protagonista se mueve CON EL RATÓN (clic izquierdo = ir a ese punto,
   clic derecho = disparar); el teclado solo cambia de arma (1-4), pausa
   (ESC) y avanza los títulos con `scan_code`. Confirmado leyendo la fuente
   embebida en su propio `.PRG` (la fuente viaja dentro del bytecode; se
   extrae decodificando CP850 desde "Desarrollado en su integridad con
   DIV Games Studio 2.0").
2. **El bug real**: el movimiento por ratón calcula la ruta con
   `path_find(0,0,5,6,x,y,OFFSET,SIZEOF)` y los enemigos usan
   `path_line()`/`path_free()` para la línea de visión. En el port, esas
   tres funciones eran **STUBS VACÍOS** en `port_stubs.c`
   (`void path_find(void) {}`): no consumían argumentos de `pila[]` ni
   devolvían nada. El juego recibía 0 puntos de ruta (o, peor, la pila
   desincronizada) y el personaje jamás se movía.

Nota: `posicion()` (L1381 de la fuente de TOKENKAI) es un `process` PROPIO
del juego, no un builtin — seguía funcionando. El único hueco era el módulo
de búsqueda de caminos.

### 40.2 Diagnóstico

- En el repo restaurado el módulo original vive en `src/div32run/ia.cpp`
  (no estaba en ningún target del port): `path_find` (búsqueda rápida tipo
  A* / exacta tipo Dijkstra sobre el gráfico del FPG, con `capar()`),
  `path_line` (Bresenham) y `path_free`, más los helpers (`init_find`,
  `calcula_vertices`, `puede_ir`, `expand`/`expand2`, `add`/`add2`) y la
  global `find_status` (declarada `extern` en `i.cpp`, que la resetea en su
  init).
- `f.cpp` ya despachaba los tres builtins (case 84/85/86, f.cpp:4343-4345);
  solo faltaban los cuerpos.

### 40.3 El cambio

- **`port/div/core/ia.c` (nuevo)**: port fiel de `src/div32run/ia.cpp`, con
  tres diferencias documentadas en su cabecera:
  1. Variables de estado del módulo `static` (en el original eran globales
     con linkage externo; aquí nadie más las referencia, así no chocan con
     los globals del port).
  2. La comprobación de límites del buffer destino con `capar()` se usa
     SIEMPRE: es la única que encaja con este port (f.cpp ya usa
     `capar()` en todos los builtins).
  3. `find_status` conserva linkage externo a propósito: `i.cpp` lo
     declara `extern` y lo resetea.
- **`port/div/core/port_stubs.c`**: eliminados los stubs de `path_find`,
  `path_line`, `path_free` y la definición `int find_status=0` (ahora la da
  `ia.c`); notas de sección actualizadas.
- **`CMakeLists.txt`**: `ia.c` añadido a `div_core` con `/FIport_pre.h`
  (force-include, como el resto del núcleo).

### 40.4 Verificación

- `cmake --build build --target div32run_port --config Release` → limpio,
  sin símbolos sin resolver.
- `div32run_port.exe TOKENKAI.div32` (25 s, cwd `div2\DIV2\DATA`): entra en
  partida, el protagonista se mueve con el ratón siguiendo la ruta de
  `path_find`, los enemigos patrullan con `path_find`/`path_free`; termina
  solo con exit 26 limpio (raylib cierra sin errores, stderr vacío).
- Confirmación visual del usuario: "va perfecto".

### 40.5 Notas

- Semántica del "mapa de búsqueda": es el gráfico FPG indicado; paleta 0
  (negro) = zona libre, lo demás = obstáculo. El origen es SIEMPRE la
  posición actual del proceso llamante (`mem[id+_X]`/`mem[id+_Y]`);
  `path_find` devuelve el nº de puntos de la ruta (0 si no hay) y los
  escribe en la estructura `{x,y}` pasada por offset/size.
- `init_find()` reserva tres tablas `word[254*254]` (~1,2 MB) a la primera
  búsqueda; `find_status` evita repetirlo.
- Con esto, el repertorio de builtins de un scroll real (teclado, ratón,
  caminos, sonido, música) queda completo salvo los nichos documentados
  (Modo-8, DLLs, red, FLI).
---

## 41. Letra proporcional a la resolución (IDE): `big2` base 1280 y escala sintetizada (2026-09-20)

### 41.1 Objetivo

Hasta ahora la letra tenia dos estados: normal (maquetacion de 8 px logicos) o
"big" (doble, 16 px). En una pantalla 4K la presentacion escala el framebuffer
logico, asi que la letra de 8 px quedaba diminuta (~4 px fisicos a 3840x2160).
El plan original del port duplicaba la letra de forma automatica cuando la
escala no llegaba a 0.8, pero eso solo servia para 1920: a 2560/3840 seguia
quedando pequena.

Decision (usuario): **letra proporcional real a la resolucion**, no un simple
doble:

- Formula (la que implementa de verdad `Load_Cfgbin`, `div.cpp:3847`):
  `fac = min(8, max(1, vga_an/1280))`, `base = big ? 2 : 1`,
  `big2 = max(fac, base)`, y despues `big = (big2 > 1)`.
  - ancho < 1280 -> big2 = big ? 2 : 1 (modo big clasico, sin cambios).
  - 1920x1080    -> fac=1, base=2 -> big2 = 2 (igual que el modo big clasico).
  - 2560x1440    -> fac=2, base=2 -> big2 = 2.
  - 3840x2160    -> fac=3, base=2 -> big2 = 3 (x3 sintetizado).
- `big` resulta `big2 > 1 ? 1 : 0` (el modo big clasico se enciende solo).
- Tambien se escala **la letra del editor de codigo** (`font_corner_build`),
  desviacion deliberada: el DIV original no escalaba el editor ni en modo big.

### 41.2 El problema de la tabla de caracteres (`word dir`)

`GRANDE.FON` (usada en modo big) tiene 256 glifos de 14 px de alto y la tabla
ocupa 256*4 bytes con `dir` de 16 bits. Al sintetizar un x3 los datos de todos
los glifos (256 * 21 * hasta 27 px) superan los 65535 bytes que admite un
`word`, invalidando el esquema original.

Solucion: **formato interno normalizado** de `text_font` (independiente de como
este guardado el .fon en disco):

- `text_font[0]` = altura fisica de los glifos (7 pequeno / 14 grande / 21, 28,
  33... sintetizado).
- Tabla de 256 entradas de **8 bytes** desde `text_font+1`: `byte an` (avance
  fisico) + `3 pad` + `int dir` (offset del glifo respecto a `text_font+2049`).
- Los glifos empiezan en `text_font+2049`.
- `text_font_build(src, altura, factor)` normaliza cualquier .fon (4 bytes por
  entrada, 1 byte por pixel) a ese formato y hace upscale por vecino mas
  cercano si `factor > 1`. Devuelve el buffer a `free()`.
- `divwindo.cpp`: el struct local `{byte an; word dir}` paso a
  `{byte an; int dir}`, y el codigo que apuntaba a `text_font+1025` pasa a
  `text_font+2049`. Todos los usuarios quedan confinados a `div.cpp` y
  `divwindo.cpp` (las fuentes del editor y los grph definen tamaños al vuelo,
  no usan la tabla).

### 41.3 Escalado de los grhficos del sistema (graf)

`graf_p.div` (187 registros) se reescribe con `graf_build_scaled()` cuando
`big && big2 > 2`:

- Cada registro se convierte al **formato "con mascara"** (int@+60 no nulo,
  cabecera 68 bytes, centro en word@+64/+66) para poder escribir el centro
  escalado por `factor` sin depender de como estaba guardado el original
  (173 ya eran con mascara; los 14 planos conservan su centro escalado a mano
  o (0,0) si no tenian).
- Los pixeles se escalan por vecino mas cercano (mismo aspecto que en el
  modo big original).
- El cursor del raton (indice 21) **no se dobla**: el original lo dibujaba
  siempre a 7x7; se conserva ese comportamiento.

### 41.4 Fuente del editor de codigo

`font_corner_build(src, sw, sh, factor, &o_sw, &o_sh)` escala los 256 glifos
consecutivos de `sysXXxXX.bin` por vecino mas cercano. Se aplica:

- En `inicializacion` (div.cpp), despues de leer el `.bin`, solo si `big2 > 1`;
  `font_an`/`font_al` quedan multiplicados y `char_size` se recalcula despues.
- En la recarga de fuente del dialogo de opciones (divsetup.cpp), con el mismo
  criterio.
- `put_char3()` (divedit.cpp) ahora tiene un `case default:` en el switch de
  `font_an` (glifo generico), porque las fuentes sintetizadas no tienen ancho
  6/8/9.

### 41.5 Generalizacion de los `*2` / `/2` del modo big

Toda maquetacion multiplicada/`dividida` por 2 bajo `if (big)` pasa a `big2`:
wbox/wgra/wresalta/bwput (divwindo), boton de cierre y modulos de texto
(constantes +2/+3 conservadas), wmouse (/big2), wput -45/-35 del raton de la
barra de control, mover_ventana, divdsktp, divedit (get_slide_x/y, barras),
divfont (tan/tal de la ventana de fonts, slots 86/64 del selector), divpaint
(paleta de color 8x7, vuelca_barra, outer cursors), divsetup (iconos de color
del raton en Opciones), divbin/divbrow/divcdrom/divfpg/divhandl/divpalet/divpcm
(mismos patrones `/2` de mirrors de buffer).

`big2` se calcula en `Load_Cfgbin()` ANTES de cargar los recursos, y se elimina
el auto-big `< 0.8f` de `port_ide_video.c::svmode()` que decidia por escala de
ventana (ahora lo decide la resolucion logica, no el monitor).

### 41.6 Verificacion

- `cmake --build build --target div_ide_port --config Release` limpio (solo
  warnings preexistentes C4091/C4311 del codigo heredado).
- Arranca sin crashear a 1920x1080 (la apariencia no debe variar).
- Pendiente verificacion visual del usuario a 2560x1440 y 3840x2160.

## 42. El IDE reventaba a 3840x2160: geometria de sesion mayor que el buffer (2026-09-21)

### 42.1 Sintoma

`div_ide_port` arrancaba bien a 1920x1080 y 2560x1440 pero moria con
`0xC0000005` a 3840x2160, antes de pintar el primer frame.

Matriz real (ojo: `inicializacion()` cae a 320x200 si la resolucion guardada no
esta en la lista de `detectar_vesa()`, asi que solo los modos listados prueban
algo de verdad):

| Modo | big2 | Resultado |
|---|---|---|
| 1920x1080 | 2 | OK |
| 2560x1440 | 2 | OK |
| 3840x2160 | 3 | **segfault** |

### 42.2 Lo que NO era

El factor 3 de escalado esta bien: el arnes `tools/test_scale_fact3.c` ejercita
`text_font_build`, `graf_build_scaled` y `font_corner_build` con los ficheros
reales del sistema y pasa entero (`TODO OK`).

La pista buena: borrando `system\session.dtf` el IDE arranca y pinta a 4K sin
problema. El crash estaba en **restaurar la sesion**, no en la letra grande.

### 42.3 Diagnostico sin depurador

`tools/dumpstack/crashcatch.exe` captura el minidump, pero `dumpstack.exe` no
lee bien el registro de excepcion (imprime `code=0`). Se resolvio parseando el
minidump a mano en Python:

- Stream 6 (ExceptionStream) -> `code=0xC0000005`, direccion de fallo, y en
  `ExceptionInformation[1]` la direccion leida: `0x60643100`.
- Stream 4 (ModuleList) -> la direccion de la instruccion cae en
  `VCRUNTIME140.dll` (o sea, dentro de `memcpy`).
- Contexto del hilo -> `rcx`=destino, `rdx`=origen (`0x60642CB8`), `r8`=1128.
- Barrido de la pila buscando valores dentro del rango del exe y resolucion
  contra `build/Release/div_ide_port.map` (base `0x10000000`):

```
memcpy (VCRUNTIME140)
wvolcado+0xFE         divwindo.obj
actualiza_caja+0x417  div.obj
UpLoad_Desktop+0xBEC  divdsktp.obj
```

- Stream 16 (MemoryInfoList) -> la region *committed* que contiene el buffer
  origen acaba justo en `0x60643000`; a partir de ahi esta solo reservada.
- Leyendo `ventana[]` del propio dump (simbolo `ventana` del .map, stride 1296
  = `sizeof(struct tventana)` en x64): `ventana[0]` era un menu con
  `an=1128 al=660` (744.480 bytes) y `ptr=0x605D2B60`, pero desde ese `ptr`
  solo habia ~459.936 bytes accesibles.

### 42.4 La causa

`nueva_ventana_carga()` (`divdsktp.cpp`) hace, en este orden:

1. `call(init_handler)` fija `v.an`/`v.al` en unidades logicas.
2. `if (big) { v.an*=big2; v.al*=big2; }` -> unidades fisicas.
3. `an=v.an; al=v.al;` y **`malloc(an*al)`**.
4. ...y mas abajo, si `!VidModeChanged`:
   `v.an=ventana_aux.an; v.al=ventana_aux.al;` — la geometria **guardada en
   `session.dtf`**, que sustituye a la recien calculada *despues* de haber
   reservado el buffer con ella.

`VidModeChanged` solo compara resolucion (`alto + ancho*10000 + big<<31`), no
la escala de letra. Una sesion escrita por un build con otra semantica de
`big2` — justo el caso de la sesion guardada en mitad del trabajo de §41 —
pasa ese test y mete `an`/`al` mayores que el bloque reservado. A partir de
ahi **todos** los volcados de esa ventana (`wvolcado` desde `actualiza_caja`)
leen fuera del heap.

Es un fallo latente del codigo original que en DOS no se notaba porque la
escala nunca cambiaba entre sesiones.

### 42.5 El cambio

`tools/patch_session_geom.py` (parche byte-exacto e idempotente, `src/` es
CP850) sobre `divdsktp.cpp`:

- Nueva variable `long lon_ptr` con los bytes realmente reservados.
- `malloc(lon_ptr)` y `memset(ptr,c0,lon_ptr)` usan ese mismo valor.
- La geometria de la sesion solo se acepta si cabe:

```c
if (ventana_aux.an>0 && ventana_aux.al>0 &&
    (long)ventana_aux.an*ventana_aux.al<=lon_ptr &&
    (long)ventana_aux._an*ventana_aux._al<=lon_ptr) {
  v.an=ventana_aux.an;
  v.al=ventana_aux.al;
  v._an=ventana_aux._an;
  v._al=ventana_aux._al;
}
```

`_an`/`_al` (geometria de minimizado) entran en la misma condicion porque
`div.cpp:1505` y los `swap(v.an,v._an)` las convierten en `an`/`al` al
restaurar una ventana minimizada: aceptarlas sueltas reintroduciria el fallo
mas tarde.

Cuando se rechazan, la ventana se queda con la geometria recien calculada,
que es coherente con su buffer.

### 42.6 `io_video_fit_window()` estaba escrita pero no se llamaba

Se encontro a medias de una biseccion: la funcion existia en
`port/io/io_video.c`, estaba declarada en `div_io.h`, y en `svmode()`
(`port_ide_video.c`) quedaba **el comentario que la describe sin la llamada
debajo**, mas un marcador `/* XXXBISEC: clamp de ventana desactivado */` en
`io_video.c`. Se desactivo persiguiendo este mismo crash, que resulta que no
tenia nada que ver.

Restaurada la llamada `io_video_fit_window(vga_an, vga_al);` en `svmode()` y
quitado el marcador. Sin ella la ventana se presenta casi 1:1 y el escritorio
se ve diminuto en un monitor grande, que es justo lo que §41 pretendia evitar.

### 42.7 Corrupcion CP850 de los fuentes (el incidente U+FFFD, otra vez)

Al ir a parchear `divdsktp.cpp` se vio que **14 de los 15 ficheros de
`src/div/` modificados sin commitear tenian la codificacion CP850 destruida**:
1.202 lineas con la secuencia UTF-8 `EF BF BD` (U+FFFD) donde habia un byte
CP850. En HEAD esos mismos ficheros tienen **cero**. Es el incidente de §24.3
repetido durante el trabajo de §41.

Casi todo eran comentarios (acentos y las lineas de separacion `═`), pero habia
dos literales reales danados:

| Fichero | Literal | Estado |
|---|---|---|
| `div.cpp:51` | tabla de caracteres acentuados CP850 | destruido |
| `divfont.cpp:1208` | `pletras[]="...()Ññçáéíóú"` | acentos destruidos |

Reparado con `tools/fix_cp850.py`, que trabaja **a nivel de bytes** (nunca
decodifica el fichero entero, porque algunos pares CP850 como `C4 BF` = `─┐`
formaron UTF-8 valido por casualidad y siguen intactos en disco: re-codificarlos
los destruiria). Alinea cada linea con su original en HEAD por un "esqueleto"
ASCII — todo byte >= 0x80 y toda secuencia `EF BF BD` reducidos a `` — y
devuelve a su sitio el byte original. 1.179 lineas restauradas asi; las 23
restantes son comentarios nuevos de §41 (nacieron ya rotos, sin pareja en HEAD)
y se arreglan con una tabla explicita de palabras.

Resultado: cero `EF BF BD` en el arbol, los dos literales identicos a HEAD, y
el diff de `src/div/` baja de 1727+/1347- a 350+/142-, que es el trabajo real
de §41 mas este arreglo.

**La regla de §24.3 sigue en pie y conviene comprobarla antes de cada commit**:

```bash
for f in $(git diff --name-only -- src/div/); do   python -c "print('$f', open('$f','rb').read().count(b'ï¿½'))"; done
```

### 42.8 Verificacion

- `cmake --build build --target div_ide_port --config Release` limpio.
- Con la **misma `session.dtf` que reventaba** (guardada a 3840x2160, 5
  ventanas), las tres resoluciones arrancan y vuelcan frame:

```
1920x1080 -> exit 0    2560x1440 -> exit 0    3840x2160 -> exit 0
```

- La captura de 4K muestra el escritorio restaurado entero (menus, Menu
  SISTEMA y la ventana FUTBOL.PRG) con la letra a big2=3, y los acentos
  (`Año`, `Configuración`, `Menú de edición`) bien, lo que confirma de paso la
  reparacion CP850.

### 42.9 Ficheros

- `src/div/divdsktp.cpp` (via `tools/patch_session_geom.py`)
- `port/div/ide/port_ide_video.c`, `port/io/io_video.c` (llamada + marcador)
- `src/div/*.cpp`, `src/div/global.h` (via `tools/fix_cp850.py`, solo bytes CP850)
- `tools/patch_session_geom.py`, `tools/fix_cp850.py` (nuevos)

## 43. Los 2 bugs abiertos de E6a (2026-09-21): browser arreglado del todo, editor de mapas con el crash bloqueante quitado pero aun no funcional

### 43.1 Bug abierto 1 -- el browser petaba subiendo de directorio: causa real y arreglo

El diagnostico previo (E6a, WIP `4917a42`) apuntaba a nombres largos sin alias
8.3 o a `chdir`/`getcwd`. Ninguna de las dos cosas era la causa real.

**Reproduccion controlada**: con `tools/patch_dbg_browser.py` (instrumentacion
temporal, no commiteada, revertida tras el diagnostico) se anadieron
checkpoints en `menu_mapas2`, `browser0`, la rama de navegacion de
`divbrow.cpp` y `dir_abrirbr`. Con eso se confirmo que **Release -> build ->
DIV funcionaban bien**, y que el crash aparecia mas arriba, en directorios con
ficheros **sin extension** (`LICENSE`, `makefile`, `Vagrantfile` en la raiz
del propio repo DIV).

**Causa**: `crear_un_thumb_MAP()` (`divbrow.cpp`) y otros 15 puntos del mismo
fichero repiten el patron:

```c
strcmp(strupr(strchr(l->lista+(l->lista_an*num),'.')), ".MAP")
```

`strchr(nombre,'.')` devuelve NULL si el nombre no tiene punto.
`strupr(NULL)` no crasheaba en el CRT de Watcom/DOS original, pero la UCRT de
MSVC valida el parametro de las funciones "inseguras" y aborta el proceso con
`__fastfail` (excepcion `0xC0000409`, la MISMA que un fallo de cookie /GS --
por eso el primer analisis con `crashcatch`/desensamblado parecia apuntar a
un desbordamiento de pila en `crear_un_thumb_MAP`, y en realidad era esto).

**Arreglo**: `tools/patch_browser_ext_crash.py` (parche byte-exacto,
idempotente) anade un helper:

```c
char * mayus_extension(char * nombre) {
  char * p = strchr(nombre,'.');
  return p ? strupr(p) : "";
}
```

y sustituye las 16 llamadas `strupr(strchr(l->lista+(l->lista_an*num),'.'))`
por `mayus_extension(l->lista+(l->lista_an*num))`. Mismo comportamiento para
ficheros con extension; sin extension se compara contra `""`, que nunca
coincide con ninguna extension real (se trata como tipo desconocido, igual
que antes para extensiones no reconocidas).

### 43.2 Bonus: `determina_unidades()` -- la lista de unidades <X:> era basura

Encontrado investigando el mismo bug (la hipotesis (c) del handoff, "cambiar
de unidad"): `determina_unidades()` (`div.cpp`) usaba `intdos()` para leer el
mapa de unidades logicas (INT 21h AX=440Eh). El shim `intdos()` de este port
(`port_ide_pre.h`) es un **no-op literal** (`#define intdos(inregs,outregs)
0`) que no toca `r`. El bucle original leia `r.w.cflag`/`r.h.al` **sin
inicializar** (el propio compilador ya avisaba con C4700), y en la practica
`unidades[]` terminaba SIEMPRE con una unica letra bogus (en este equipo,
`"N"`) en vez de las unidades reales -- confirmado en el browser, que
mostraba `<N:>` como unica entrada en "Unidad:".

**Arreglo**: `tools/patch_determina_unidades.py` sustituye el bucle por
`GetLogicalDrives()` (Win32), el equivalente real y determinista:

```c
void determina_unidades(void) {
  unsigned long mapa=GetLogicalDrives();
  int n,uni=0;
  for (n=0;n<26;n++) if (mapa&(1UL<<n)) unidades[uni++]=(char)('A'+n);
  unidades[uni]=0;
}
```

Verificado en caliente: el browser ahora muestra `<C:>`, `<X:>`, `<Z:>` (las
unidades reales del equipo de pruebas) en vez de `<N:>`.

### 43.3 Verificacion del bug 1 (ambos arreglos juntos)

Con `tools/dumpstack/crashcatch.exe` + inyeccion de clics via PowerShell
(`Get-Process -Id <pid>`, `SetCursorPos`/`mouse_event`) se repitio la subida
de directorio **11 veces seguidas** (`Release -> build -> DIV -> Documents ->
Dani -> Users`, cruzando decenas de dotfiles sin extension como
`.gitconfig`/`.viminfo`/`.lesshst`): el proceso siguio vivo en todos los
pasos, sin excepcion. Antes del arreglo, la misma secuencia moria de forma
determinista al llegar a un directorio con algun fichero sin extension.

### 43.4 Bug abierto 2 -- el mapa del escritorio no se abria para editar: crash bloqueante identificado y quitado, pero el editor aun no funciona

**Reproduccion**: abrir un `.MAP` real (copiado desde `div2/DIV2/DATA/MAP/...`,
que no esta versionado) crea la ventana de mapa en el escritorio sin
problema (confirma lo que ya decia el WIP de E6a). El **doble clic** sobre su
contenido no hace nada (ni crashea ni entra en modo edicion -- deteccion de
doble-clic que no dispara, hipotesis (a) del handoff, sin investigar mas a
fondo por quedar eclipsada por el hallazgo de 43.5). El menu **"Mapas ->
Editar mapa"** (que llama a `v.click_handler` directamente, sin pasar por la
deteccion de doble-clic) **si llega** a `mapa2()` (`divhandl.cpp`) -- y
**crashea** con `0xC0000005`.

### 43.5 Causa raiz del crash: dos stubs declarados como datos que el codigo llama como funciones

Resuelto con `crashcatch` + desensamblado (mismo metodo que el bug de 4K,
`12-port-progreso.md` §42.3): `RIP` al crashear coincidia EXACTO (offset 0)
con el simbolo `M3D_crear_thumbs` del `.map`. La pista: `port_ide_stubs.c`
declaraba

```c
void *M3D_crear_thumbs = NULL;   // dato, NO funcion
```

para satisfacer el enlazador (el modulo real, `divmap3d.cpp`, no esta
compilado en este port -- Modo-8 sigue siendo stub). Pero `mapa2()`
(`divhandl.cpp:1722`, dentro del flujo real de "editar mapa", al construir la
lista de texturas de pincel desde `SYSTEM\BRUSH.FPG`) **lo llama de
verdad**: `M3D_crear_thumbs(&ltexturasbr,0);`. Saltar a la direccion de un
`void*` de datos (no ejecutable) es exactamente lo que raltaba: 0xC0000005 al
intentar ejecutar ahi.

Se encontro el mismo patron exacto, ya confirmado alcanzable, en
`calculadora` (`divhandl.cpp:1319`, `case 4: calculadora(); break;` del menu
Sistema): tambien declarada como `void *calculadora = NULL;` en el mismo
fichero de stubs, tambien llamada de verdad. La implementacion real existe en
`src/div/divcalc.cpp` (337 lineas) pero ese fichero no esta en
`DIV_IDE_SOURCES` -- portarlo de verdad queda fuera de alcance de esta
sesion; se stubea como no-op para que **no crashee**, no para que calcule.

**Arreglo** (`port/div/ide/port_ide_stubs.c`, editado directo -- es fichero
del port, no de `src/`, no lleva la restriccion CP850): los dos simbolos
pasan de dato a funcion no-op con la firma exacta que esperan sus llamadores:

```c
void calculadora(void) { }
void M3D_crear_thumbs(struct t_listboxbr * l, int prog) { (void)l; (void)prog; }
```

`mixer0`/`RecSound0`/`aligned`/`m3d`/`nueva_paleta`/`muestra` se dejan como
estaban (`void*`): son del bloque de audio del IDE (`divmixer`/`divsb`),
explicitamente fuera de alcance todavia (§7 de `13-handoff.md`), y no se ha
confirmado que se lleguen a invocar como funcion desde ningun camino
alcanzable hoy (a diferencia de `calculadora`/`M3D_crear_thumbs`, verificados
con evidencia real de crash).

### 43.6 Verificacion parcial y lo que queda -- el editor de mapas NO esta cerrado

Con el fix de 43.5, la secuencia completa (abrir el `.MAP`, `Mapas -> Editar
mapa`) **ya no crashea en el momento de entrar**. Pero:

- La pantalla que se ve al entrar en modo edicion es **completamente negra**
  (un solo pixel no-negro entre 136.637 muestreados) -- el editor de pintura
  a pantalla completa (`edit_mode_*`, `divpaint.cpp`) no esta pintando el
  lienzo ni la barra de herramientas. Puede ser una inicializacion que falta,
  no necesariamente otro stub roto -- no investigado mas alla de confirmar
  que el proceso sigue vivo y respondiendo (`Get-Process ... .Responding` =
  `True`).
- Pulsar **ESC** para salir de ese modo negro **crashea** con
  `0xC0000374` (`STATUS_HEAP_CORRUPTION` -- confirmado via minidump bajo
  `crashcatch`, no es el mismo patron fastfail que 43.1/43.5: aqui el propio
  gestor de heap de Windows detecto una escritura fuera de los limites de
  alguna reserva). No se identifico el punto exacto: requeriria Page Heap /
  Application Verifier para acotarlo, que no se ha intentado en esta sesion.

**Conclusion honesta**: el crash bloqueante (43.5) esta arreglado y
verificado, y es una mejora real (antes NINGUN intento de editar un mapa
sobrevivia). Pero el editor de mapas en si (`divpaint.cpp` en modo pantalla
completa) sigue sin ser funcional -- coincide con lo que ya estimaba
`15-port-ide-assessment.md`: los editores de recursos son un bloque de
trabajo propio, no un one-liner. **No se marca el bug 2 como cerrado.**

### 43.7 Verificacion

```
cmake --build build --target div_ide_port --config Release   # limpio, 0 errores
```

- Bug 1: 11 navegaciones "subir de directorio" seguidas, Release hasta
  `C:\Users`, proceso vivo en todas. `<Unidad:>` muestra letras reales.
- Bug 2: entrar en modo edicion ya no crashea (antes: 100% reproducible con
  `crashcatch`, `0xC0000005` en `M3D_crear_thumbs`). Pantalla negra y crash
  al salir con ESC (`0xC0000374`) quedan documentados como trabajo pendiente,
  no como regresion de este cambio (nunca se llego tan lejos antes).
- `src/div/` sigue en CP850 limpio (0 `EF BF BD` en los ficheros tocados).
- Instrumentacion de diagnostico (`tools/patch_dbg_browser.py`) se aplico,
  se uso para diagnosticar, y se **revirtio** antes de aplicar los arreglos
  reales -- `src/div/divbrow.cpp`/`divhandl.cpp`/`div.cpp` no llevan ningun
  `fprintf` de depuracion en el arbol final.

### 43.8 Ficheros

- `src/div/divbrow.cpp` (via `tools/patch_browser_ext_crash.py`)
- `src/div/div.cpp` (via `tools/patch_determina_unidades.py`)
- `port/div/ide/port_ide_stubs.c` (editado directo: `calculadora`,
  `M3D_crear_thumbs`)
- `tools/patch_browser_ext_crash.py`, `tools/patch_determina_unidades.py`,
  `tools/patch_dbg_browser.py` (nuevos; el ultimo es de diagnostico, revertido
  del arbol pero se deja el script por si hace falta re-diagnosticar)

## 44. Bug 2 de E6a CERRADO (2026-09-21): pantalla negra + heap corruption al
     editar un mapa -- cinco simbolos de datos de `port_ide_stubs.c` que en
     realidad son structs/arrays completos, no punteros de 8 bytes

**Sintoma**: con el crash bloqueante de §43.5 ya arreglado, entrar en el
editor de mapas (`Mapas -> Editar mapa`) pintaba la pantalla completamente en
negro y crashear con `STATUS_HEAP_CORRUPTION` (`0xC0000374`) al salir con
ESC, sin mas pista en el Visor de eventos que un offset dentro de `ntdll.dll`
(el heap manager detectando el dano, no el punto donde se escribio).

### 44.1 Primera pista real: un warning de raylib, no el crash

Reproducido en caliente con el usuario (build normal, sin instrumentacion):
al entrar en el editor aparecian en consola tres avisos de raylib **nada mas
entrar**, antes incluso de que la pantalla se pusiera en negro:

```
WARNING: TEXTURE: [ID 3] Failed to update for current texture format (0)
WARNING: TEXTURE: Current format not supported (0)
WARNING: TEXTURE: [ID 3] Failed to update for current texture format (0)
```

La textura `[ID 3]` es la propia textura de presentacion del framebuffer del
IDE (`tex`, `port/io/io_video.c`), una variable **estatica** que solo se
recrea en `svmode()`/`io_video_close()` -- ninguna de las dos se llama desde
`mapa2()`/`divpaint.cpp`. Que apareciera con `format(0)` (invalido) durante
la edicion de un mapa, sin que nadie la tocara por ese camino, apuntaba a una
escritura fuera de limites corrompiendo memoria estatica/global cercana --
la misma familia de bug que el crash al salir, solo que aqui se veia "en
vivo" antes del crash final.

### 44.2 Intento con AddressSanitizer -- descartado (falla al iniciar en esta maquina)

Se probo compilar una build de diagnostico con `/fsanitize=address` (build
paralela `build-asan/`, sin tocar el build normal). Tras copiar
`clang_rt.asan_dynamic-x86_64.dll` junto al `.exe` (ausente por defecto, el
runtime dinamico de ASan no se copia solo), el propio ASan fallaba al
arrancar:

```
==13492==WARNING: AddressSanitizer failed to mprotect 0x008000022000 (549755953152) bytes (error code: 87)
AddressSanitizer: CHECK failed: asan_poisoning.cpp:39 "((AddrIsInMem(addr))) != (0)" (0x0, 0x0) (tid=21752)
```

Incompatibilidad conocida de ASan de MSVC con la reserva de shadow memory en
ciertas configuraciones de Windows -- no relacionado con el bug. Descartado
sin mas investigacion (no hacia falta: la alternativa de abajo funciono a la
primera).

### 44.3 CRT de depuracion (build Debug, sin admin) -- confirma la pista de la textura

Sin permisos de administrador para Page Heap (`gflags`/IFEO requieren
`HKLM`), se opto por una build en config **Debug** (`build-dbg/`, CRT de
depuracion, cero cambios de codigo). Reproducido por el usuario: los mismos
tres avisos de textura salian **nada mas entrar en el editor**, confirmando
que la corrupcion empieza en la entrada de `mapa2()`, no durante la edicion
ni al salir.

### 44.4 Causa raiz: `port_ide_stubs.c` stubeaba 5 structs/arrays reales como `void *` de 8 bytes

Revisando el camino de entrada de `mapa2()` (`divhandl.cpp`), tres llamadas
antes de que aparezca cualquier textura rota:

```c
for(n=0; n<max_texturas; n++) thumb_tex[n].ptr=NULL;   // max_texturas=1000
...
strcpy(m3d_edit.fpg_path, full);                        // M3D_info.fpg_path[256]
...
M3D_crear_thumbs(&ltexturasbr,0);                       // &ltexturasbr = struct t_listboxbr*
...
crear_mapbr_thumbs(&lthumbmapbr);                        // rellena thumb_map[] de verdad
```

`divpaint.cpp` (compilado desde E6a) declara `thumb_tex`/`ltexturasbr`/
`m3d_edit` como `extern` (structs/arrays reales, con el layout completo en
`global.h`/`divpaint.cpp`), esperando que su **definicion** viva en otro
sitio -- en el DIV original, en `divmap3d.cpp` (Modo-8), que **no esta
compilado en este port** (stub, ver `13-handoff.md` §2). La unica definicion
real que quedaba en el arbol para esos nombres era esta, en
`port_ide_stubs.c`, escrita en una sesion anterior a E6a (cuando
`divpaint.cpp` todavia no estaba en el build y estos nombres solo hacia
falta que "existieran" para enlazar):

```c
void *thumb_map = NULL;
void *thumb_tex = NULL;
...
void *ltexturasbr = NULL;
void *m3d_edit = NULL;
```

Cada uno, un puntero de 8 bytes. **`link.exe` no lo reporta como simbolo
duplicado** pese a que `divpaint.cpp` SI define `thumb_map[max_windows]`
como array real (no `extern`): MSVC fusiona la definicion "tentativa" (un
array sin inicializador, regla de "common symbol" de C) con esta, mas
pequena y con inicializador explicito -- y la definicion que "gana" en el
enlazado es la de 8 bytes. El resultado: todo el codigo de `divpaint.cpp`/
`divhandl.cpp` que trata estos nombres como structs/arrays de verdad
(`thumb_tex[n].ptr`, `m3d_edit.fpg_path`, campos de `t_listboxbr`) escribia
sistematicamente **fuera** de esos 8 bytes:

| Simbolo | Tipo real esperado | Tamano real necesario | Reservado |
|---|---|---|---|
| `thumb_tex` | `struct _thumb_tex[1000]` (40 bytes/entrada) | ~40.000 bytes | 8 bytes |
| `thumb_map` | `struct _thumb_map[96]` (40 bytes/entrada) | ~3.840 bytes | 8 bytes (pero superado por la definicion real de `divpaint.cpp`, ver abajo) |
| `ltexturasbr` | `struct t_listboxbr` | ~56 bytes | 8 bytes |
| `m3d_edit` | `M3D_info` (incluye `fpg_path[256]`) | ~530+ bytes | 8 bytes |

`thumb_map` en concreto es un caso mixto: `divpaint.cpp` SI tiene la
definicion real (`struct _thumb_map thumb_map[max_windows];`, sin `extern`),
asi que una vez retirado el stub compite sola y gana por tamano/orden de
enlazado -- confirmado porque quitar solo el stub (sin tocar nada mas del
codigo) ya bastaba para que `thumb_map` funcionara. `thumb_tex` en cambio
**no tiene ninguna definicion real en el arbol compilado** (solo `extern` en
`divhandl.cpp`/`divpaint.cpp`, la real esta en el `divmap3d.cpp` no
compilado): hubo que darle una definicion nueva en el port con el layout
exacto del struct.

**El bug 2 completo** (pantalla negra + `STATUS_HEAP_CORRUPTION` al salir)
era una escritura fuera de limites de ~40 KB (`thumb_tex`) mas otra de
~500+ bytes (`m3d_edit.fpg_path`) sobre lo que fuera que el enlazador
hubiera colocado a continuacion en el segmento de datos del `.exe` -- en la
practica, entre otras cosas, la textura estatica `tex` de `io_video.c`
(explicando los avisos de raylib de §44.1) y, mas adelante en memoria, los
buffers estaticos de teclado de `port_ide_keybo.c` (`buf[]`/`ibuf`/`fbuf`),
cuyo primer uso tras la corrupcion (la siguiente llamada a `tecla()`, ya
dentro del bucle de edicion) desencadenaba el `0xC0000005` que se vio nada
mas arreglar `thumb_tex` a medias (ver §44.5).

### 44.5 Iteracion: arreglar `thumb_tex` solo no bastaba

Primer intento: solo se le dio definicion real a `thumb_tex` (retirando
tambien el stub de `thumb_map`, que ya no hacia falta). Recompilado y
probado: **crash mas temprano** que antes, `0xC0000005` dentro de `tecla()`
(`port_ide_keybo.c`) segun el offset del Visor de eventos resuelto con el
`.map` del linker (`base 0x10000000` fija, `/FIXED`/`/DYNAMICBASE:NO`, ver
`13-handoff.md` §3 sobre por que la base es fija) -- exactamente la firma de
"la corrupcion ahora es mas pequena pero sigue ahi", ya que quedaban
`ltexturasbr`/`m3d_edit` sin arreglar, escribiendo mas alla en memoria (mas
cerca de los buffers de teclado que antes, al no estar ya cubiertos por el
desbordamiento gigante de `thumb_tex`). Arreglados tambien esos dos: el
editor de mapas paso a funcionar de punta a punta.

### 44.6 `map_save`/`map_read`/`map_saveedit`/`map_readedit`: el mismo patron, pero como funciones (no datos)

Mismo hallazgo colateral que `calculadora`/`M3D_crear_thumbs` en §43.5:
revisando el resto de simbolos declarados como `void *` en `port_ide_stubs.c`
que en realidad son cosas mas grandes/distintas, aparecieron cuatro que el
codigo real **llama como funciones** (`map_save()`/`map_read()` desde
`divhandl.cpp`, `map_saveedit()`/`map_readedit()` desde `divdsktp.cpp` al
guardar la sesion del escritorio con una ventana de mapa 3D abierta) --
saltar a la direccion de un dato no ejecutable es el mismo `0xC0000005` de
§43.5, solo que en una ruta no alcanzada por esta sesion todavia (no forma
parte del bug 2, pero es una mina real: guardar/cerrar el IDE con un mapa en
edicion la habria disparado). Arregladas con no-ops de firma exacta, mismo
patron. `map_saveedit`/`map_readedit` esperan `lptmap` (tipo real de
`divmap3d.hpp`, no compilado); como la `M3D_info` del port reduce ese campo a
un `int` placeholder (`port/div/ide/global.h`), se declararon con `void *`
en vez de reintroducir `lptmap` -- son no-ops, el tipo exacto del parametro
no importa mientras el tamano (puntero) coincida con lo que pasan los
llamadores.

**No se audito el resto de los ~30 stubs `void *` de `port_ide_stubs.c`
one a one** contra sus usos reales en `divpaint.cpp`/`divbrow.cpp`/
`divfont.cpp`/etc. -- se arreglaron los que bloqueaban el camino de esta
sesion (editor de mapas) mas los que aparecieron como llamadas reales
durante la revision de ese mismo camino. Es razonable sospechar que queden
mas casos del mismo patron en simbolos ligados a editores todavia no
ejercitados (paleta, PCM, ayuda) -- **si aparece otro
`STATUS_HEAP_CORRUPTION`/`0xC0000005` sin causa obvia al portar el siguiente
editor de recursos, sospechar primero de esto antes de re-investigar desde
cero.**

### 44.7 Verificacion

- `cmake --build build --target div_ide_port --config Release` y
  `--config Debug`: ambos compilan limpio, 0 errores.
- Reproducido en caliente por el usuario con `div2/DIV2/DATA/MAP/DARK/MAPA1.MAP`
  (fichero real, no versionado): abrir el mapa, `Mapas -> Editar mapa`,
  pintar con el raton, salir con ESC -- **funciona de punta a punta**, sin
  pantalla negra ni crash, en la build Debug primero y luego confirmado en
  la build Release normal.
- `src/div/` no se toco en absoluto en este hito: los 5 simbolos corregidos
  viven todos en `port/div/ide/port_ide_stubs.c` (fichero del port, sin
  restriccion CP850).
- Build de diagnostico `build-asan/` y `build-dbg/` se dejan en el arbol
  (ignoradas por git via `/build.*`) por si hace falta retomar este tipo de
  diagnostico en el siguiente editor de recursos.

**Pendiente, no parte de este cierre**: doble-clic sobre el mapa del
escritorio para entrar en modo edicion sigue sin hacer nada (hipotesis (a)
del handoff, §43.4) -- el camino que SI funciona es el menu explicito
`Mapas -> Editar mapa`. No investigado.

### 44.8 Ficheros

- `port/div/ide/port_ide_stubs.c` (editado directo): `thumb_map` (stub
  retirado), `thumb_tex` (definicion real anadida), `ltexturasbr`/`m3d_edit`
  (definicion real anadida), `map_save`/`map_read`/`map_saveedit`/
  `map_readedit` (no-ops reales en vez de datos)

## 45. Auditoria sistematica de `port_ide_stubs.c` + editor de paleta funcional
     (2026-09-21) — 14 simbolos mas del mismo patron (`void *` de datos que en
     realidad son structs/arrays/funciones reales)

Con el editor de mapas cerrado (§44), siguiente hito del roadmap: "resto de
editores de recursos". Antes de tocar el editor de paleta (`divpalet.cpp`,
ya compilado desde antes de E6a, nunca verificado en caliente), se hizo una
auditoria sistematica de los ~50 simbolos `void *NOMBRE = NULL;` de
`port_ide_stubs.c` contra el codigo real de todos los `.cpp` en
`DIV_IDE_SOURCES`, buscando el mismo patron exacto que causo el bug 2 de
§44: (a) una definicion real (array/struct, no `extern`) en algun fichero
compilado con el mismo nombre -- la fusion "common symbol" de MSVC hace que
el `void*` de 8 bytes gane el enlazado igualmente; o (b) el nombre usado
como llamada real `nombre(...)` en algun fichero compilado -- saltar a la
direccion de un dato no ejecutable, mismo `0xC0000005` que `calculadora`/
`M3D_crear_thumbs`/`map_save` en §43-44.

### 45.1 Metodo

Dos greps automatizados sobre los 19 ficheros de `DIV_IDE_SOURCES` (lista
completa: `div.cpp`, `divwindo.cpp`, `divhandl.cpp`, `divedit.cpp`,
`divbasic.cpp`, `divdsktp.cpp`, `mem.cpp`, `divlengu.cpp`, `divcolor.cpp`,
`divgama.cpp`, `divbin.cpp`, `divmouse.cpp`, `divpaint.cpp`, `divpalet.cpp`,
`divbrow.cpp`, `divfont.cpp`, `divfpg.cpp`, `fpgfile.cpp`, `divforma.cpp`,
`divsetup.cpp`), para cada uno de los nombres stubeados como `void *`:

1. `grep -nE "^TIPO NOMBRE(\[...\])?;" $FILES | grep -v extern` — definicion
   real de dato (array o escalar) sin `extern`.
2. `grep -n "\bNOMBRE($FILES" — uso como llamada de funcion (con parentesis).

**Limitacion del metodo 1** (confirmada al aplicarlo): el regex solo
detecta tipos "de una palabra" (`byte`, `int`, `char`) antes del nombre; no
detecta definiciones con tipos compuestos como `struct t_listboxbr NOMBRE`
o `M3D_info NOMBRE` (los tres casos de §44 -- `thumb_tex`/`ltexturasbr`/
`m3d_edit` -- se habian encontrado a mano, no con este regex). Para esos
casos hace falta revisar a mano cualquier nombre que aparezca usado con
`.campo` en el codigo real (indicio de que es un struct, no un puntero
simple).

### 45.2 Resultado: 2 casos del patron (a), 12 del patron (b)

**Patron (a) — definicion real shadowed por el stub**:

- `nueva_paleta`: `divpalet.cpp:1162` define `byte nueva_paleta[768];` de
  verdad (no `extern`) -- exactamente el caso `thumb_map` de §44 (el stub
  de 8 bytes gana el enlazado sobre el array real). **Arreglado**: retirado
  el stub, gana la definicion real de `divpalet.cpp`.
- `back`: `divpaint.cpp:29` define `int back;` -- el stub reservaba 8 bytes
  para un `int` de 4. Tipo incorrecto pero **sin riesgo de desbordamiento**
  (el slot reservado es mayor que lo necesario, no al reves) -- no se toca,
  documentado aqui por si hace falta revisar en el futuro.

De paso se revisaron `superget`/`scroll_x`/`scroll_y`/`zoom_level`, con el
mismo patron "stub mas grande de lo necesario" (`int`/`float` reales contra
`void*` de 8 bytes) -- mismo diagnostico: type-confused pero no explota
memoria ajena, no se tocan.

**Patron (b) — llamados de verdad como funcion**: `OpenSound`, `OpenSong`,
`OpenSoundFile`, `PasteNewSounds`, `SaveSound` (editor de sonido, menu de
recursos en `divhandl.cpp`), `OpenDesktopSound`, `SaveDesktopSound`,
`OpenDesktopSong` (`divdsktp.cpp`, al abrir/guardar la sesion del escritorio
con una ventana de sonido/tracker abierta), `barra_vertical`/`vuelca_help`
(`divdsktp.cpp:1048`, refresco de la ayuda del escritorio),
`mostrar_mod_meters` (`div.cpp:1008`, medidor de tracker en el escritorio),
`nuevo_mapa3d` (`divhandl.cpp:602,2871`, menu Mapas → Nuevo mapa 3D). Las
implementaciones reales viven en `divpcm.cpp`/`divhelp.cpp`/`divmap3d.cpp`,
ninguno compilado todavia. **Arregladas las 12**: no-ops con la firma exacta
de cada llamador (mismo patron que `calculadora`/`map_save` de §43-44).

**Descartados** (declarados pero sin ninguna llamada real en el codigo
compilado, por tanto inalcanzables hoy): `MapperCreator2`, `RecSound0`,
`calc0`, `calc2`, `mixer0`. `set_init_mixer` tiene una llamada real en
`div.cpp:2971` pero condicionada a `judascfg_device != DEV_NOSOUND`, y
`judascfg_device` esta fijado a `DEV_NOSOUND` en el propio
`port_ide_stubs.c` (audio del IDE sin portar) -- codigo muerto mientras eso
no cambie. Estos 6 quedan como estaban, documentados por si se vuelven
alcanzables al portar audio/calculadora/mapa 3D.

### 45.3 `cargadac_JPG` — el caso que la auditoria SI encontro pero se dejo pasar en una primera pasada

Al arreglar los 12 de §45.2, un primer intento de build parecia limpio, pero
probar "Paletas → Abrir" (cargar un `.PAL` real) crasheaba con `0xC0000005`.
Resuelto el offset del Visor de eventos contra `build/Release/div_ide_port.map`
(hubo que reconfigurar con `-DCMAKE_EXE_LINKER_FLAGS="/MAP"`, el build normal
no lo genera): caia exacto en `cargadac_JPG`, que **si** habia aparecido en
el grep de la §45.1 (`divpalet.cpp:555,599`, `divhandl.cpp:2687`,
`"try|=cargadac_JPG(PalName);"`) pero se dejo con su comentario original
("JPG sigue stub") sin reconocer que ESE comentario hablaba de no
*implementar* la decodificacion JPG, no de que fuera seguro dejarlo como
dato. **Arreglado**: `int cargadac_JPG(char *name) { return 0; }` -- replica
solo el "no es un JPG / no soportado" que ya hace la version real de
`divforma.cpp` (excluida bajo `#ifndef PORT_IDE` por el choque de jpeglib
con `windows.h`, ver `13-handoff.md`) cuando el fichero no es `.JP*` o falla
la decodificacion; no decodifica nada, solo evita saltar a un dato.

**Leccion para la proxima auditoria de este tipo**: un comentario que dice
"sigue stub" al lado de un `void *NOMBRE = NULL;` describe la FALTA de
implementacion, no la seguridad del stub en si — hay que verificar
igualmente si el nombre se llama con parentesis en algun sitio, sin fiarse
del comentario.

### 45.4 Verificacion

- `cmake --build build-dbg --target div_ide_port --config Debug` y
  `cmake --build build --target div_ide_port --config Release`: ambos
  compilan limpio, 0 errores, en dos iteraciones (la segunda tras el
  hallazgo de `cargadac_JPG`).
- Confirmado por el usuario en caliente (build Release): editor de mapas
  sigue funcionando igual que en §44 (chequeo de regresion), editor de
  paleta (`Paletas → Editar`) funciona, y **abrir/cargar una paleta**
  (`Paletas → Abrir`, ejercita `cargadac_JPG` dentro de `LoadPal()`) tras el
  fix de §45.3 funciona sin crashear.
- No se re-audito el resto de simbolos de `port_ide_stubs.c` que no son
  `void *` (funciones ya no-op, `int`/`char[]` reales, etc.) — quedan fuera
  del alcance de este metodo.

### 45.5 Ficheros

- `port/div/ide/port_ide_stubs.c` (editado directo): `nueva_paleta` (stub
  retirado), `OpenSound`/`OpenSong`/`OpenSoundFile`/`PasteNewSounds`/
  `SaveSound`/`OpenDesktopSound`/`SaveDesktopSound`/`OpenDesktopSong`/
  `barra_vertical`/`vuelca_help`/`mostrar_mod_meters`/`nuevo_mapa3d`/
  `cargadac_JPG` (no-ops reales en vez de datos)

## 46. "Información del sistema" con datos reales (2026-09-21) — DPMI 0500h
     emulado (memoria libre) + FP_SEG/FP_OFF neutralizados desde el E1

Petición del usuario tras cerrar §44/§45: la ventana **Sistema → Información
del sistema** (`MemInfo0`/`MemInfo1`, `divsetup.cpp`) ya era código real
(mapas, % de recursos, memoria del heap CRT vía `_heapwalk` — todo eso
correcto desde que `divsetup.cpp` entró al build en E6b), pero la cifra de
"memoria libre" salía sin sentido.

### 46.1 Causa 1: DPMI 0500h sin emular

`GetFreeMem()` (`divsetup.cpp`) llama a `int386x(0x031, ..., EAX=0x0500)` —
la función DPMI real de DOS "Get Free Memory Information", que escribía un
bloque de 12 `unsigned long` en `ES:EDI`. `port_int386x()`
(`port/div/ide/port_ide_dos.c`) no tenía ningún caso para `intno==0x31`:
caía al `default` de `port_int386()` (que además ignora `s`, el puntero
`SREGS` con `ES`) y dejaba `Mi_meminfo` con basura de pila sin inicializar.
**Arreglado** replicando el mecanismo que ya existía para el runtime
(`int386x` en `port/div/core/port_misc.cpp`, mismo campo 0 vía
`GlobalMemoryStatusEx`).

### 46.2 Causa 2: FP_SEG/FP_OFF del IDE son no-ops desde el E1, y con razón

El primer intento (idéntico al patrón del runtime: reconstruir el puntero
real a partir de `ES:EDI` con el truco de 32 bits `FP_SEG`/`FP_OFF`) **no
funcionó**: `FP_SEG()`/`FP_OFF()` en `port_ide_dos.c` son no-ops que
**siempre devuelven 0** (comentario original: "sin sentido en x64 plano"),
deliberado desde que se escribieron para el IDE — hasta ahora nada
necesitaba el valor real. `ES:EDI` llegaban siempre a 0/0, así que
`port_dpmi_fill_meminfo()` nunca escribía nada (`if (!flat) return;`),
dejando la cifra en basura de pila (que en la práctica salía como 0).

**Arreglado sin reintroducir el riesgo de direcciones >4 GB que el runtime
sí acepta como conocido** (habría hecho falta tocar además
`divkeybo.cpp`/`cdrom.cpp`, ninguno compilado hoy, con el mismo patrón): en
vez de codificar el puntero en 32 bits de `ES:EDI`, se usa un "handle" de
una sola entrada (`port_fp_last_ptr`, `port_ide_dos.c`) — `FP_SEG(p)`/
`FP_OFF(p)` recuerdan el puntero real tal cual se les pasa, y
`port_int386x()` lo lee directamente sin pasar por `ES:EDI`. Válido porque
las dos únicas llamadoras reales (`div.cpp:3100-3101`, `divsetup.cpp:355-356`)
hacen `FP_SEG(p); FP_OFF(p); int386x(...);` en secuencia inmediata con el
mismo `p`, sin nada intermedio que lo pise.

### 46.3 Causa 3: el struct de `div.cpp` es más pequeño que el de `divsetup.cpp`

El DPMI real escribe 12 `unsigned long` (48 bytes). `divsetup.cpp` usa
`meminfo` (12 campos, 48 bytes) pero `div.cpp` usa su propio
`struct _meminfo MemInfo` (9 campos, 36 bytes) para un debug-log interno
(`GetMemoryFree()`/`DebugFile()`, nunca visible en el uso normal del IDE) —
escribir los 12 campos siempre habría desbordado 12 bytes sobre `MemInfo`
(un global; el siguiente dato en memoria es `char MemoriaLibre[100]`, no
crítico pero real). Los dos únicos consumidores reales solo leen el primer
campo, así que `port_dpmi_fill_meminfo()` escribe únicamente ese
`unsigned long`, sin tocar el resto — ni desborda el struct pequeño, ni
hace falta saber cuál de los dos llamó.

### 46.4 Causa 4 (reportada por el usuario tras el primer fix): saturaba en ~4 GB

Con las tres causas anteriores arregladas, la ventana ya mostraba un número
real — pero fijo en ~4.194.300 KB (≈4 GB) sin importar la RAM libre real de
la máquina. El campo 0 real de DPMI 0500h es "mayor bloque libre, **en
bytes**" (no páginas de 4 KB — esa conversión sí es correcta para el
runtime, pero para *otra* fórmula distinta, `memory_free()` en `f.cpp`, no
para esta). Un `unsigned` de 32 bits en bytes satura sobre ~4 GB — límite
real de la spec DPMI de 1999, muy por debajo de la RAM de cualquier PC
moderno.

**Arreglado** cambiando la unidad a **KB** (rango realista, hasta ~4 TB) en
`port_dpmi_fill_meminfo()`, con un parche byte-exacto a la única línea de
`divsetup.cpp` que asumía bytes (`tools/patch_meminfo_units.py`, CP850,
idempotente): `mem=(Mi_meminfo.Bloque_mas_grande_disponible+mem)/1024;` →
`mem=Mi_meminfo.Bloque_mas_grande_disponible+mem/1024;` (el campo DPMI ya
viene en KB; solo `mem`, el heap libre de `_heapwalk`, sigue en bytes y hay
que convertirlo). Sigue saturando a `ULONG_MAX` KB (~4 TB) en vez de
envolver, por si algún día hace falta más — dar un número menor que el real
sería peor que saturar.

### 46.5 Verificación

- `cmake --build build --target div_ide_port --config Release`: compila
  limpio.
- Confirmado por el usuario, en tres vueltas: (1) mostraba 0 tras el primer
  intento con `ES:EDI` — diagnosticado el problema de `FP_SEG`/`FP_OFF`;
  (2) mostraba ~4 GB fijos tras el fix del handle — diagnosticada la unidad
  bytes-vs-KB; (3) **valor real de RAM libre** tras el fix de unidades.
- `src/div/divsetup.cpp`: 0 bytes `EF BF BD` tras el parche (comprobación
  obligatoria de la regla §24.3).

### 46.6 Ficheros

- `port/div/ide/port_ide_dos.c` (editado directo): `port_int386x()` (caso
  DPMI 0x31/0x0500 nuevo), `FP_SEG`/`FP_OFF` (guardan el puntero real en vez
  de devolver siempre 0), `port_dpmi_fill_meminfo()` (nueva)
- `src/div/divsetup.cpp` (vía `tools/patch_meminfo_units.py`, nuevo)

## 47. Audio del IDE (E7) — shim de compatibilidad JUDAS sobre port/io,
     divpcm.cpp compilado, editor de sonido funcional

Hito grande pedido por el usuario tras cerrar §44-46. Decisión tomada con el
usuario antes de escribir código: **shim de compatibilidad** (reimplementar
solo las funciones `judas_*` que `divpcm.cpp` llama de verdad, apoyándose en
`port/io/io_audio.c`/`io_song.c`) en vez de portar la librería JUDAS real
completa (~9.400 líneas en `3rdparty/judas/`, un motor de mezcla/tracker
propio).

### 47.1 Alcance real: `divmixer.cpp`/`divsb.cpp` son irrelevantes, `divpcm.cpp` es el verdadero obstáculo

`divmixer.cpp`/`divsb.cpp` acceden a hardware real de 1999 (registros del
mezclador Sound Blaster/GUS vía `outp`/`inp`, DMA de grabación) — igual que
`divvideo.cpp`/`det_vesa.cpp` en su momento (§9), no se portan. Lo
reutilizable ya existía y estaba probado en el runtime: `port/io/io_audio.c`
(efectos vía raylib) e `io_song.c` (tracker MOD/S3M/XM vía libmikmod). El
obstáculo real es `divpcm.cpp` (editor de sonido/PCM, nunca compilado):
llama a ~30 funciones `judas_*` (carga WAV/raw, reproducción de muestras,
tracker XM/S3M/MOD, posición/línea/canales, VU meter), no solo las 5-6 que
necesitó `divsound.cpp` del runtime.

### 47.2 `port/div/ide/port_ide_judas.c` (nuevo, ~600 líneas) — el shim

- **Muestras (WAV/RAW)**: reales. `judas_loadwav`/`judas_loadwav_mem`
  parsean un RIFF WAV de verdad (chunks `fmt `/`data`) y normalizan a mono
  16 bits, igual que el JUDAS real (consultado en `3rdparty/judas/
  judaswav.c` como referencia, no compilado). `judas_loadrawsample` asume
  8 bits sin signo mono a 11025 Hz fijo, misma convención que el JUDAS real
  (`judasraw.c`). `judas_playsample` envuelve el PCM normalizado en un WAV
  mínimo construido al vuelo (con la frecuencia pedida en cada llamada,
  igual que la API real) y lo reproduce vía `io_load_sound()`/
  `io_play_sound()`/`io_change_channel()`.
- **Tracker (XM/S3M/MOD)**: reales, vía `io_load_song()`/`io_play_song()`
  (autodetectan el formato real con libmikmod) — `judas_loadxm`/
  `judas_loads3m`/`judas_loadmod` aceptan cualquiera de los tres formatos
  por igual; `SongType` puede no coincidir con el formato real si el
  llamador prueba xm primero y acierta con un `.mod`, pero la reproducción
  es correcta igual.
- **`judas_getvumeter()`**: siempre 0 (medidor plano) — extraer el nivel
  real por canal exigiría enganchar el mixer interno de libmikmod, fuera de
  alcance del shim (degradación aceptada con el usuario).
- **Grabación** (`RecordSound()`/`PollRecord()`, vía DSP/DMA real de Sound
  Blaster): NO implementada — no hay hardware real que grabar. Sus
  dependencias (`sbinit`/`sbrec`/`spkon`/`MIX_*`/`set_mixer`/
  `timer_uninit`) quedan como no-ops reales en `port_ide_stubs.c` (mismo
  criterio que JPG en el editor de imágenes: deshabilitado, no crashea).

### 47.3 `judascfg_device`: de `DEV_NOSOUND` fijo a `DEV_SB`

`divhandl.cpp`/`divbrow.cpp`/`divpcm.cpp` comprueban
`judascfg_device==DEV_NOSOUND` antes de llamar a `OpenSound()`/`OpenSong()`/
preescuchar en el browser — con `DEV_NOSOUND` fijo (como estaba desde E3)
esos caminos quedaban bloqueados con un diálogo de error, sin llegar nunca a
las funciones reales de `divpcm.cpp`. Cambiado a **`DEV_SB`** (no
`DEV_SBPRO`/`DEV_SB16`): suficiente para desbloquear reproducción, pero el
menú "Sonido → Grabar" exige explícitamente `DEV_SBPRO`/`DEV_SB16`
(`divhandl.cpp:1190-1191`), así que con `DEV_SB` la grabación muestra un
diálogo de "no soportado" en vez de intentar grabar con hardware que no
existe — no hace falta emular nada más para eso.

### 47.4 Auditoría de stubs repetida (mismo método de §45): 2 casos más del patrón `void*`

- `aligned`: real es `unsigned char *aligned[2]` (2 punteros, divsb.cpp,
  buffers DMA de grabación); estaba stubeado como un único `void*`. Solo
  una LECTURA fuera de límites (`aligned[1]`), no escritura — arreglado
  igualmente con el array real de 2 punteros (ambos NULL, así que el `if`
  que gatea la grabación nunca es cierto, consistente con §47.3).
- `set_init_mixer`: código muerto mientras `judascfg_device` era
  `DEV_NOSOUND` (`div.cpp:2971`, `if(judascfg_device!=DEV_NOSOUND)
  set_init_mixer();`) — dejó de serlo al cambiar a `DEV_SB`, y un `void*`
  de datos ahí crasheaba con `0xC0000005` nada más arrancar el IDE.
  Arreglado con un no-op real.

Quince símbolos de `port_ide_stubs.c` (los que `divpcm.cpp` ya implementa de
verdad, `OpenSound`/`OpenSong`/`OpenSoundFile`/`PasteNewSounds`/`SaveSound`/
`OpenDesktopSound`/`SaveDesktopSound`/`OpenDesktopSong`/`mostrar_mod_meters`/
`SongType`/`SongCode`/`last_mod_clean`/`FreeMOD`/`IsWAV`/`wline`/`PCM0`/
`EditSound0`/`RecSound0`, y los `judas_*` que antes eran no-ops falsos) se
retiraron para dejar paso a las implementaciones reales, mismo patrón de
limpieza que §45.

### 47.5 Bug de heap corruption al listar miniaturas de sonido (`crear_un_thumb_PCM`)

Al abrir por primera vez un `.PCM` real (`Sonido → Abrir`), el navegador
crasheaba con `STATUS_HEAP_CORRUPTION` (0xC0000374) generando la miniatura
de la forma de onda — código de `divbrow.cpp` (E3) nunca antes ejercitado,
porque `OpenSound()`/preescucha estaban bloqueados por `judascfg_device`
hasta este mismo hito. Diagnosticado con `crashcatch`+CRT de depuración
(`Debug Assertion Failed: is_block_type_valid`/`_CrtIsValidHeapPointer`,
confirmando corrupción de heap real) y bisección con un patch de
diagnóstico temporal (`tools/patch_diag_disable_thumb_pcm.py`, revertido).

**Causa real**: en la rama "else" de `crear_un_thumb_PCM()` (ficheros
grandes, `filesize>=3*an`), el código escribe directamente
`thumb[num].ptr[x+y*an]=c_g_low` con `y=temp[p0]*al/256` — `temp` es un
`char` con signo, así que para una muestra por debajo de cero `y` sale
**negativo**, y `x+y*an` cae **antes** del buffer (`malloc(an*al)`):
escritura fuera de límites real. La rama "if" (ficheros pequeños) no tiene
este problema porque usa `wline()`/`linea_pixel()`, que sí comprueban
límites (`x>=0 && y>=0 && x<an && y<al`) — un bug real y preexistente del
DIV original de 1999, invisible en DOS por su modelo de memoria sin
protección, expuesto ahora por un heap allocator moderno más estricto.
**Arreglado** (`tools/patch_thumb_pcm_bounds.py`, CP850) clampando `y0`/`y1`
al rango válido `[0, al-1]` antes de escribir, igual que ya hace
`linea_pixel()` para la otra rama — no cambia el resultado visual de las
filas que ya caían dentro de rango.

### 47.6 Cuelgue permanente tras reproducir un sonido — `PollInputEvents()` nunca se llamaba fuera de `volcado()`

Con el crash de miniaturas arreglado, reproducir un `.PCM` funcionaba pero
el IDE se quedaba colgado (para siempre, no un bloqueo temporal) justo al
terminar. Diagnosticado con trazas por stderr (`[tick]`/`[volcado]`/prints
dentro de `judas_playsample`): la función de audio terminaba OK y devolvía
el control, pero ningún `[tick]`/`[volcado]` volvía a aparecer nunca más —
el bucle principal se paraba en seco.

**Causa real**: `PCM2()` (`divpcm.cpp`, click handler de la ventana de
sonido) hace `judas_playsample(...); while (mouse_b&1) read_mouse();` — el
idiom DOS clásico de "esperar a que se suelte el botón". En DOS esto
funcionaba porque el estado del ratón lo actualizaba una IRQ hardware
asíncrona; en el port, `read_mouse()` → `read_mouse2()` → `int386(0x33,
AX=3)` → `io_mouse_button()` → `IsMouseButtonDown()` (raylib) solo se
actualiza cuando algo llama a `PollInputEvents()` — y eso **solo ocurría
dentro de `EndDrawing()`**, a su vez solo dentro de `volcado()`
(`port/io/io_video.c:286`). Como este bucle de espera nunca llama a
`volcado()`, `PollInputEvents()` nunca se ejecuta de nuevo, y
`IsMouseButtonDown()` se queda devolviendo "pulsado" para siempre: cuelgue
real, no solo lento. Ningún otro camino del IDE había ejercitado antes este
patrón exacto (esperar el ratón sin repintar) sin pasar por `volcado()`.

**Arreglado** en `port/io/io_input.c` (fichero compartido con el runtime,
cambio seguro/aditivo): `io_poll()` ahora también llama a
`PollInputEvents()` (antes solo recalculaba la posición cacheada desde
`GetMousePosition()`), y `port_ide_dos.c` llama a `io_poll()` en el caso
`AX=3` (estado de botones) de la emulación de INT 33h, antes de leer el
estado. Redundante pero inocuo cuando `EndDrawing()` ya lo hizo ese mismo
frame; protege cualquier bucle de espera activa futuro con el mismo patrón,
no solo este.

### 47.7 Verificación

- `cmake --build build-dbg/build --target div_ide_port --config Debug/Release`:
  ambos compilan limpio, 0 errores, en varias iteraciones (auditoría de
  stubs, bug de miniaturas, bug de cuelgue).
- Confirmado por el usuario en caliente, en varias vueltas hasta agotar
  todos los problemas: **abrir y reproducir un `.PCM` real** (`LASER6.PCM`)
  funciona sin crashear ni colgarse; **listar miniaturas de una carpeta de
  sonidos real** (`div2/DIV2/DATA/PCM/ALIEN/`) funciona sin crashear;
  **abrir y reproducir una canción tracker real** (`DARK.S3M`) funciona.
  Regresión confirmada: editor de mapas y editor de paleta (§44/§45) siguen
  funcionando igual.
- `src/div/divbrow.cpp`: 0 bytes `EF BF BD` tras el parche (comprobación
  obligatoria §24.3). **Incidente evitado por poco**: un primer intento de
  revertir un parche de diagnóstico en `divpcm.cpp` con la herramienta de
  edición de texto normal (no un script Python byte-exacto) corrompió el
  fichero (955 bytes `EF BF BD`) — detectado inmediatamente por la
  comprobación de la regla §24.3 y restaurado con `git checkout` antes de
  commitear nada. Recordatorio para la próxima sesión: **ningún editor de
  texto normal en `src/`, nunca, ni para revertir "solo una línea"**.

### 47.8 Ficheros

- `port/div/ide/port_ide_judas.c` (nuevo): shim JUDAS → `io_audio.c`/
  `io_song.c`
- `port/div/ide/port_ide_stubs.c` (editado directo): `aligned` (array real),
  `judascfg_device` (`DEV_SB`), `set_init_mixer` (no-op real), 15 símbolos
  retirados (reales en `divpcm.cpp`), bloque nuevo de no-ops de grabación
  (`judascfg_port`/`DmaBuf`/`timer_uninit`/`SetCDVolume`/`MIX_*`/
  `set_mixer`/`sbinit`/`sbsettc`/`sbrec`/`spkon`/`spkoff`/`dmacount`/
  `dmastatus`)
- `port/div/ide/port_ide_dos.c` (editado directo): `port_ide_tick()` llama
  a `io_song_update()`; `port_int386()` caso `AX=3` llama a `io_poll()`
- `port/div/ide/port_ide_video.c` (editado directo): `svmode()` llama a
  `io_audio_init()` una vez; `rvmode()` llama a `io_audio_close()`
- `port/io/io_input.c` (editado directo, compartido con el runtime):
  `io_poll()` llama a `PollInputEvents()`
- `src/div/divbrow.cpp` (vía `tools/patch_thumb_pcm_bounds.py`):
  `crear_un_thumb_PCM()`, clamp de `y0`/`y1`
- `CMakeLists.txt`: `divpcm.cpp`/`port_ide_judas.c` añadidos a
  `DIV_IDE_SOURCES`; `io_audio.c`/`io_song.c`/mikmod añadidos al target
  `div_ide_port`
- `tools/patch_diag_disable_thumb_pcm.py`, `tools/patch_diag_pcm2_trace.py`
  (diagnóstico, revertidos del árbol, scripts conservados por si hace falta
  re-diagnosticar); `tools/patch_thumb_pcm_bounds.py` (el fix real, aplicado)

## 48. Ayuda del IDE (E8) — divhelp.cpp compilado, sistema de ayuda
     hipertexto funcional

Hito pedido por el usuario tras cerrar §44-47 ("vamos a por la ayuda"). A
diferencia de los hitos anteriores (mapas, paleta, audio), este resultó
sencillo: `divhelp.cpp` no depende de ninguna librería externa como JUDAS
(la ayuda es texto plano con marcado propio + imágenes de un `.fpg`, todo
leído de ficheros ya presentes en `help/`), así que el ciclo fue
prácticamente idéntico al de los hitos anteriores pero sin ningún
"descubrimiento de API externa" — solo el patrón ya conocido de declaraciones
adelantadas K&R + auditoría de stubs.

### 48.1 Declaraciones adelantadas (mismo patrón K&R de siempre)

Al añadir `divhelp.cpp` a `DIV_IDE_SOURCES`, siete funciones se llaman antes
de su definición dentro del propio fichero (`resize_help`, `help_xref`,
`arregla_linea`, `vuelca_help`, `put_image_line`, `put_chr`, `Print_Help`) —
error `C2371` ("nueva definición; tipos básicos distintos"), el mismo patrón
de `busca_packfile`/`load_pal`/etc. del runtime (ver §2.4/§6.1) y de
`port_ide_forward_decls.h` en el IDE. Añadidas las siete declaraciones a ese
fichero con el tipo real (`unsigned char *`, no `byte *`: en el punto donde
se procesa `port_ide_forward_decls.h`, `byte` todavía no existe como macro —
mismo detalle no obvio que §6.1 del runtime).

### 48.2 Auditoría de stubs (mismo método de §45/§47): 3 arrays reales más tapados por `void*`

- `helpidx[4096]` (`int`, 16 KB) — índice del hipertexto por término
  {inicio,longitud}.
- `backto[64]` (`int`, 256 bytes) — cola circular de tópicos consultados.
- `help_title[128]` (`byte`) — título del término actual.

Los tres tenían definición real (no `extern`) en `divhelp.cpp` pero estaban
tapados por un `void *NOMBRE = NULL;` de 8 bytes en `port_ide_stubs.c`
(mismo mecanismo de fusión "common symbol" de MSVC que `thumb_map`/
`nueva_paleta`, ver §44/§45) — potencial de escritura muy fuera de límites
si algún camino los hubiera llegado a rellenar. Arreglados retirando los
stubs (deja ganar la definición real). Otros 12 símbolos (`help`, `help0`,
`help2`, `help_buffer` -puntero-, `index` -puntero-, `load_index`,
`make_helpidx`, `vuelca_help`, `barra_vertical`, `tabula_help`,
`determina_help`, `help_paint`) causaban `LNK2005` (duplicados directos, ya
no `void*` sino funciones/punteros reales) — retirados de los stubs sin
más, dejando la definición real de `divhelp.cpp`.

Auditoría repetida contra **todos** los ~26 stubs `void *` restantes tras el
cierre (patrón (a) definición real no-`extern`, patrón (b) llamada real
`nombre(`): **0 casos nuevos** — los únicos hits (`MapperCreator2`, `calc0`,
`calc2`, `mixer0`) son prototipos sin llamada real, igual que en auditorías
anteriores. `divhelp.cpp` no introdujo ningún caso nuevo del patrón
"llamado como función" (el que causó `calculadora`/`M3D_crear_thumbs`/
`cargadac_JPG`/`set_init_mixer` en hitos previos).

### 48.3 Verificación

- `cmake --build build-dbg/build --target div_ide_port --config Debug/Release`:
  compila y enlaza limpio **a la primera** tras los dos pasos de arriba (sin
  ninguna iteración de diagnóstico de crash, a diferencia de mapas/paleta/
  audio).
- Confirmado por el usuario en caliente (build Release): menú **Ayuda**
  (última entrada de la barra del escritorio, `help(3)` en
  `menu_principal2()`, `divhandl.cpp:83`) abre una ventana de ayuda
  **funcional y navegable** — texto real de `help/help.idx`/`help.dat`,
  hipertexto con enlaces.

### 48.4 Pendiente (cosmético, no bloqueante) — tamaño de letra de la ayuda

El usuario reporta el texto de la ayuda "grande" — iría bien reducirlo, pero
no es bloqueante y se deja pendiente a petición explícita del usuario
("más adelante"). Hallazgo parcial durante la investigación, para no volver
a empezar de cero: `vuelca_help()` (`divhelp.cpp:1119`) calcula el offset
inicial del volcado como `di=v.ptr+(v.an*(10+16)+2)*big2;` — sospechoso de
escalar `v.an` **dos veces** por `big2` si `v.an` ya viene en píxeles reales
(escalado) en este punto, igual que hace la línea anterior
(`wbox(v.ptr,v.an/big2,v.al/big2,...)`, que sí divide `v.an` por `big2`
antes de usarlo). `divhelp.cpp` es código de 1999 nunca tocado por el
trabajo de "letra proporcional" (§41, hecho antes de que este fichero
entrara al build) — plausible que sus fórmulas de escalado no estuvieran
adaptadas a la convención `big2` que sí se aplicó al resto de `src/div/`.
No confirmado con pruebas, solo una hipótesis a verificar la próxima vez
que se retome esto.

### 48.5 Ficheros

- `port/div/ide/port_ide_forward_decls.h` (editado directo): 7 declaraciones
  nuevas para `divhelp.cpp`
- `port/div/ide/port_ide_stubs.c` (editado directo): `helpidx`/`backto`/
  `help_title` (stubs retirados, arrays reales), 12 símbolos más retirados
  (funciones/punteros reales en `divhelp.cpp`)
- `CMakeLists.txt`: `divhelp.cpp` añadido a `DIV_IDE_SOURCES`

## 49. Calculadora del IDE (E9) — divcalc.cpp compilado, funcional

Tercer hito consecutivo de este bloque (mapas/paleta/audio/ayuda/calculadora)
pedido por el usuario tras descartar el generador de sprites (`divspr.cpp` +
`src/div/visor/`, ~5.500 líneas con motor 3D propio y ensamblador — demasiado
grande para este momento del roadmap, aparcado sin tocar). `divcalc.cpp` es
pequeño (337 líneas) y sin dependencias externas (solo `global.h`) — parser/
evaluador de expresiones (xor/or/and, `<<`/`>>`, `+`/`-`, `*`//`%`, signo/`!`)
más una ventana simple. Mismo patrón que la ayuda (E8): sin ningún
descubrimiento de API externa, cerrado a la primera.

### 49.1 Declaraciones adelantadas (mismo patrón K&R de siempre)

`expres0`..`expres5` y `get_token` se llaman antes de su definición dentro
del propio fichero — error `C2371`, igual que en divhelp.cpp (§48.1) y el
runtime (§2.4/§6.1). Añadidas a `port_ide_forward_decls.h`.

### 49.2 Auditoría de stubs: 0 casos nuevos del patrón array/struct, 4 símbolos duplicados retirados

A diferencia de TODOS los hitos anteriores, esta vez la auditoría (mismo
método de §45.1) no encontró ningún array/struct real tapado por un `void*`
de 8 bytes — `divcalc.cpp` solo usa escalares (`int`/`double`) y una struct
con dos punteros (`pcalc`/`readcalc`, ya correctamente dimensionados como
`void*` en los stubs, sin cambios). Sí hubo 4 duplicados directos
(`LNK2005`) retirados de `port_ide_stubs.c`: `calc0`, `calc2`, `superget`
(escalar `int`, tamaño ya compatible con el `void*` que lo tapaba, pero
duplicado igualmente) y **`calculadora`** — esta última es la MISMA función
que se convirtió en no-op en el cierre del bug 2 de E6a (§43.5, para evitar
el 0xC0000005 de saltar a un dato al abrir la Calculadora desde el
escritorio) — ahora tiene su implementación real.

### 49.3 Verificación

- `cmake --build build-dbg/build --target div_ide_port --config Debug/Release`:
  compila y enlaza limpio a la primera tras los dos pasos de arriba, sin
  ninguna iteración de diagnóstico de crash (segundo hito consecutivo así,
  tras la ayuda).
- Confirmado por el usuario en caliente (build Release): menú **Sistema →
  Calculadora** abre una ventana funcional (escribir una expresión, ver el
  resultado).

### 49.4 Ficheros

- `port/div/ide/port_ide_forward_decls.h` (editado directo): 7 declaraciones
  nuevas para `divcalc.cpp`
- `port/div/ide/port_ide_stubs.c` (editado directo): `calc0`/`calc2`/
  `superget`/`calculadora` retirados (reales en `divcalc.cpp`)
- `CMakeLists.txt`: `divcalc.cpp` añadido a `DIV_IDE_SOURCES`

## 50. Generador de explosiones del IDE (E10) — diveffec.cpp compilado,
     funcional, con un bug real de signo de `char` corregido

Cuarto hito consecutivo de este bloque, sugerido por el propio usuario tras
preguntar si era tan complejo como el generador de sprites (no lo es).
`diveffec.cpp` es pequeño (532 líneas) y sin dependencias externas (solo
`global.h`) — generador procedural de partículas (puntos que se expanden)
más gradiente de color. Mismo patrón que ayuda/calculadora: sin
declaraciones adelantadas nuevas esta vez, solo 4 duplicados directos
(`GenExplodes`, `exp_Color0`/`exp_Color1`/`exp_Color2` — estos últimos ya
existían en `port_ide_stubs.c` para servir a `divpaint.cpp` como `extern`;
ahora `diveffec.cpp` aporta la definición real) — retirados de
`port_ide_stubs.c`, compiló y enlazó limpio a la primera.

### 50.1 Bug real encontrado en caliente: `char` con signo indexando `ExpDac[256]`

Verificado por el usuario: la explosión se generaba, pero con **píxeles de
color aleatorio** salpicados en la parte más brillante del anillo (negro
del centro y degradado gris exterior correctos). Causa: `GenExplodes()`
guarda el brillo de cada píxel (0..255) en `Buff_exp`, declarado `char *`
(con signo por defecto en MSVC) —
`Buff_exp[y*exp_ancho+x]=exp_Coloracum/n_exp;` con `exp_Coloracum/n_exp` en
el rango 0..255. Para valores >=128 (la mitad brillante del degradado), la
conversión a `signed char` da un valor **negativo** (wraparound de
complemento a dos). Ese valor negativo se usa después directamente como
índice: `v.mapa->map[x]=ExpDac[Buff_exp[x]];` — con `ExpDac[256]` una
variable **local** (en la pila de `GenExplodes()`), un índice negativo lee
memoria **antes** del array, dentro del propio stack frame: valores
prácticamente aleatorios reinterpretados como índices de la paleta DIV de
256 colores (de ahí los colores vivos, no tonos de gris) — coincide
exactamente con el síntoma visual reportado.

**Arreglado** (`tools/patch_explode_signed_char.py`, CP850) con un cast
mínimo en el único punto de lectura que importa para el color:
`ExpDac[(unsigned char)Buff_exp[x]]`. No se tocó la declaración de
`Buff_exp` ni las comparaciones con `DEEP`/`DEEP*2` del bucle de "fade"
(mismo problema de signo en potencia, pero es un efecto estético menor sin
relación con el síntoma reportado — documentado aquí por si hiciera falta
revisar en el futuro, no arreglado ahora para mantener el parche mínimo).

Es un bug real y preexistente del DIV original de 1999 (mismo patrón que
otros ya encontrados en este port: asunciones de signo/tamaño de tipo que
un compilador o una plataforma distinta expone de forma distinta), no algo
introducido por el port.

### 50.2 Verificación

- `cmake --build build-dbg/build --target div_ide_port --config Debug/Release`:
  compila y enlaza limpio a la primera (auditoría de stubs sin sorpresas),
  y de nuevo limpio tras el parche de color.
- Confirmado por el usuario en caliente (build Release): **Mapas →
  Generador de explosiones** genera una explosión visualmente correcta
  (gradiente negro→gris→blanco sin píxeles de color aleatorio).

### 50.3 Ficheros

- `port/div/ide/port_ide_stubs.c` (editado directo): `GenExplodes`/
  `exp_Color0`/`exp_Color1`/`exp_Color2` retirados (reales en
  `diveffec.cpp`)
- `src/div/diveffec.cpp` (vía `tools/patch_explode_signed_char.py`): cast a
  `unsigned char` en la lectura de `Buff_exp[x]` como índice de `ExpDac[]`
- `CMakeLists.txt`: `diveffec.cpp` añadido a `DIV_IDE_SOURCES`

---

## 51. Reproducción de vídeo FLI/FLC en el runtime (`start_fli`/`frame_fli`/`end_fli`/`reset_fli`) — 2026-09-22

Pedido explícito del usuario tras confirmar en caliente que Modo-7 ya
funcionaba con un juego real (SPEED.PRG): *"Vamos directos al FLI (que me
permitirá probar otro juego)"*. A diferencia de los hitos anteriores (todos
en el IDE), este es en el **runtime** (`div32run_port`) — `port/div/core/`.

### 51.1 El problema: TopFLC no está vendored

`src/div32run/divfli.cpp` es un wrapper fino sobre la API pública de
**TopFLC v1.0** (Johannes Lehtinen, 1996), una librería de terceros para
decodificar FLI/FLC. Del repositorio original solo sobrevivieron
`3rdparty/topflc/topflc.h` y `topflc.txt` (documentación) — el `.c`/`.lib`
real nunca se restauró (mismo hueco que JUDAS, ver 11-port-windows11-mikedx.md
§0.2). Hizo falta escribir un decodificador FLI/FLC compatible desde cero.

### 51.2 `port_topflc.c` — decodificador nuevo, API-compatible con TopFLC

Implementación de ~450 líneas contra el formato FLI/FLC de Autodesk
Animator/Animator Pro (cabecera de 128 bytes, chunks de frame de 16 bytes
con `size`/`type`=0xF1FA/`nchunks`, sub-chunks de 6 bytes con
`size`/`type`), cubriendo exactamente la superficie de API que
`divfli.cpp` llama (`TFAnimation_NewFile/Delete/GetInfo/SetLooping/
SetPaletteFunction`, `TFBuffers_Set`, `TFFrame_Decode`):

- **Paleta**: `COLOR256`(4, 8 bits/canal, escalado `>>2` a 6 bits VGA) y
  `COLOR64`(11, ya 6 bits, passthrough) — paquetes de skip/change/RGB.
- **Píxeles**: `BRUN`(15, RLE de frame completo — positivo=run, negativo=
  literal), `COPY`(16, frame crudo sin comprimir), `LC`(12, delta de líneas
  nativo de FLI — convención de signo **opuesta** a BRUN: positivo=literal,
  negativo=run), `SS2`(7, delta por palabras de FLC — flag de saltar líneas
  vía bit alto 0xC000, último píxel vía bit 0x8000).
- `BLACK`(13)/`PSTAMP`(18) también reconocidos (frame negro / miniatura,
  ignorada esta última).

`TFFrame_Decode()` re-sincroniza la posición del fichero con `fseek`
absoluto tras cada chunk y tras el frame completo (defensivo: un chunk mal
decodificado no debe desalinear los siguientes).

### 51.3 `divfli.cpp` copiado sin tocar + fontanería

`port/div/core/divfli.cpp` es copia byte a byte de
`src/div32run/divfli.cpp` (verificado con `diff`). Enganchado a
`CMakeLists.txt` (fuente de `div_core`, más `/TC` para forzar compilación
como C igual que `i.cpp`/`f.cpp`/`s.cpp`, más el include dir
`3rdparty/topflc`). `port/div/shim/mem.h` nuevo (shim trivial que solo
incluye `<string.h>`) para el `#include <mem.h>` de Watcom/Borland que
`divfli.cpp` trae. Los stubs viejos de `fli_palette_update`/`StartFLI`/
`Nextframe`/`EndFli`/`ResetFli` en `port_stubs.c` se retiraron (ahora
reales). Compiló limpio a la primera.

### 51.4 Bug real encontrado en caliente: pantalla negra (no un bug del decodificador)

Primera prueba (bytecode a mano, `build_fli_demo.py`, ver §51.5): el
usuario confirmó que el programa corría sin errores (~7s, la duración
esperada) pero **la pantalla se quedaba en negro** todo el tiempo.

`start_fli()`/`frame_fli()` (f.cpp) decodifican cada frame directamente en
`copia2` (el buffer de fondo, ver `DIV_export("background",...)` en
`i.cpp`) — la hipótesis de partida era que el pipeline normal de cada
`frame;` ya copia `copia2` a `copia` (el buffer visible) sin que el
programa tenga que hacer nada más. Cierto solo a medias: `i.cpp` tiene
**dos** mecanismos de compositado separados, y el segundo es un sistema de
"regiones sucias" (`restore()`, indexado por el array `scan[]`) que copia
solo los rectángulos que algún sprite marcó como "voy a pintar aquí" en el
frame anterior (`volcado_parcial()`, llamado desde la caja delimitadora
`_x0.._y1` de cada proceso). El bytecode de prueba no dibuja ningún
sprite, así que nunca se marca nada sucio — `copia2` se actualiza
correctamente cuadro a cuadro, pero nunca llega a `copia`.

La causa raíz no era el decodificador ni el pipeline de vídeo, sino **el
propio bytecode de prueba**: inicializaba todos los globales del sistema a
0, pero el compilador real de DIV (ver `system/ltobj.def`) inicializa
`restore_type` y `dump_type` a **1** ("volcado/restauración completos",
`const complete_dump=1`), no a 0 ("parcial"). Con `restore_type==1`, el
mismo `i.cpp` (línea ~921: `if (old_restore_type==0) restore(...) else
memcpy(copia,copia2,vga_an*vga_al);`) hace un `memcpy` completo cada
frame, sin depender de ningún sprite en escena — exactamente lo que un
programa DIV real (compilado por `divc`, que sí pone estos defaults)
obtiene siempre. `restore_type`/`dump_type` son globales de DIV
accesibles por nombre (`mem[end_struct+17]`/`mem[end_struct+18]`,
`end_struct=1529` en `inter.h`), así que el arreglo fue en el script de
prueba, no en el port: `globales[end_struct+17-long_header]=1` y
`globales[end_struct+18-long_header]=1` antes de comprimir el payload.

**Confirmado por el usuario tras el arreglo**: el vídeo de `INTRO.FLI`
(320×200, 121 frames, tipo 0xAF11, el intro real del juego ALIEN de
`div2/DIV2/DATA/FLI/`) se reproduce correctamente de principio a fin.

### 51.5 `build_fli_demo.py`

Mismo patrón que `build_sound_demo.py`/`build_sprite_demo.py`: `n =
start_fli("intro.fli", 0, 0)`, bucle de 121× `frame_fli(); frame;`,
`end_fli();`, 48 frames extra con el último cuadro en pantalla antes de
`lret`. Requiere `fli\intro.fli` en el directorio de trabajo (misma
convención de `open_file()` ya documentada en §17). El fichero deja
anotado en un comentario el motivo de fijar `restore_type`/`dump_type`
(ver §51.4) para que no se repita el mismo diagnóstico en el futuro.

### 51.6 Ficheros

- `port/div/core/port_topflc.c` (nuevo): decodificador FLI/FLC, API
  compatible con TopFLC v1.0
- `port/div/core/divfli.cpp` (nuevo, copia byte a byte de
  `src/div32run/divfli.cpp`)
- `port/div/shim/mem.h` (nuevo): shim trivial de `<mem.h>`
- `port/div/core/port_stubs.c`: stubs viejos de FLI retirados (reales en
  `divfli.cpp`)
- `CMakeLists.txt`: `divfli.cpp`/`port_topflc.c` añadidos a las fuentes de
  `div_core`, include dir `3rdparty/topflc`, `divfli.cpp` añadido a la
  lista de `/TC` (compilación forzada como C)
- `port/div/core/build_fli_demo.py` (nuevo): script de prueba manual
  (bytecode a mano, mismo patrón que `build_sound_demo.py`)

## 52. Editor de mapa 3D del IDE (Modo-8) — cerrado y funcional (2026-09-22)

`divmap3d.cpp` (4164 líneas) es el editor de mapas 3D del IDE (menú
`Mapas → Nuevo mapa 3D`) — **no** el motor de renderizado en tiempo real
que usa un juego compilado (ver §53). A pesar de ser 7-8x más grande que
los módulos anteriores (calculadora, ayuda, explosiones), resultó mucho
más tratable de lo esperado: sin ensamblador, sin dependencias de
`src/div/visor/` (eso es solo del generador de sprites, aparcado), y sin
includes externos raros — solo `global.h` y su propio `.hpp`.

### 52.1 Añadido al build y auditoría de stubs

Añadido a `DIV_IDE_SOURCES` en `CMakeLists.txt`. Compiló limpio a la
primera. El enlazador señaló solo:

- **8 símbolos duplicados** en `port_ide_stubs.c`: `M3D_crear_thumbs`,
  `MapperVisor0`, `MapperCreator2`, `MapperBrowseFPG0`, `map_save`,
  `map_read`, `map_saveedit`, `map_readedit`, `nuevo_mapa3d` — todos
  no-ops preparados de antemano en §44 (mismo patrón "dato en vez de
  función" ya documentado: el código real los llama de verdad, no los usa
  como punteros). `MapperCreator2` en particular era el mismo patrón
  exacto (`void *MapperCreator2 = NULL;`) que ya había causado un
  0xC0000005 en otros módulos. Con la implementación real de
  `divmap3d.cpp` ya en el build, estos stubs sobraban — retirados (se
  deja un comentario apuntando a dónde vive la implementación real, igual
  que con calculadora/ayuda/explosiones).
- **2 símbolos sin resolver**: `last_x`/`last_y`, dos `int` globales de
  tracking de arrastre del ratón (`MapperCreator2`, la función de
  `divmap3d.cpp`, no el símbolo de datos del stub viejo). Solo estaban
  definidos en `divspr.cpp` (generador de sprites, no compilado) —
  coincidencia de nombre entre dos módulos distintos, no relación real.
  Añadidos como `int last_x = 0, last_y = 0;` en `port_ide_stubs.c`.

Se repitió la auditoría sistemática de §45.1 (grep de cada `void
*SIMBOLO` de `port_ide_stubs.c` contra `divmap3d.cpp`) sin encontrar
ningún caso adicional del patrón "struct/array real tapado por un void*
pequeño" (el que sí afectó a `thumb_tex`/`ltexturasbr`/`m3d_edit` en su
momento, §44) — `ltexturasbr`/`m3d_edit` ya tenían su definición real
desde entonces y no hizo falta tocarlas.

También se revisó el patrón de "`char` con signo usado como índice de
brillo 0-255" (el bug real de §50/E10) — no aparece en `divmap3d.cpp`
(los `char[]` que hay son buffers de thumbnail/paths, no tablas de
brillo).

### 52.2 Verificado en caliente por el usuario

Abrir `Mapas → Nuevo mapa 3D`, pintar puntos/paredes/regiones con el
ratón y salir con ESC funciona de punta a punta — confirmado por el
usuario. El editor de mapa 3D del IDE queda **cerrado**.

## 53. Motor de renderizado Modo-8 en tiempo de ejecución (`src/vpe/`) — investigado, NO portado

Al probar un juego real que usa Modo-8 (`WSPORTX.PRG`), el nivel no
renderiza nada al entrar — comportamiento esperado: el editor del IDE
(§52) y el **motor de renderizado en tiempo real** son módulos
completamente distintos. El runtime portado (`port/div/core/f.cpp`) solo
tiene no-ops literales para los opcodes de Modo-8 (`set_point_m8`/
`get_point_m8` y compañía) — cualquier programa que use Modo-8 falla en
silencio, tal como ya documentaba la tabla de estado del handoff.

### 53.1 Localización: `src/vpe/` — el motor real, no `divmap3d.cpp`

El motor real vive en `src/vpe/` ("Virtual Presence Engine", según su
propia cabecera): **5657 líneas** en 17 ficheros `.cpp`/`.h` más **3
ficheros de ensamblador** (`draw_fa.asm`, `draw_oa.asm`, `draw_wa.asm`).
Es del mismo orden de magnitud y riesgo que el generador de sprites ya
aparcado (tamaño grande + ensamblador propio) — no se ha empezado a
portar, solo investigado a petición del usuario para evaluar si conviene
reescribirlo en vez de portar el ASM literal.

### 53.2 El motor ya es "sectores con portales" estilo Build, no un motor con BSP

El modelo de datos (`struct Point`/`Wall`/`Region`/`Object` en
`src/vpe/vpe.h`) es prácticamente idéntico al que ya edita
`divmap3d.cpp`/`divmap3d.hpp` (mismos campos: paredes con
`Front`/`Back` region, regiones con `FloorH`/`CeilH`/texturas, soporte de
"regiones 3D" apiladas vía `Below`/`Above`). No hay BSP tree: la
visibilidad se resuelve con un recorrido de portales entre regiones
(`scan.cpp`, `ScanLevel`/`ScanRegion`, con una pila de `Level`/`CurLevel`
para las regiones apiladas) — el mismo enfoque que el motor Build de Duke
Nukem 3D (sectores explícitos con portales, sin particionado binario del
espacio), no el de Doom con BSP. Esto es relevante porque el enfoque
"Build" está también muy bien documentado hoy en día.

### 53.3 Reparto del código: reutilizable vs. a sustituir

**Reutilizable casi tal cual** (~2100 líneas, modelo de datos y gestión,
sin asm ni acoplamiento DOS real más allá de un detalle trivial):

- `zone.cpp`/`load.cpp`/`mem.cpp` — carga de `.ZON`/`.WLD`, texturas,
  caché de memoria.
- `object.cpp`/`update.cpp`/`vpe.cpp`/`fixed.cpp`/`globals.cpp` — física
  de objetos, movimiento, matemática de punto fijo, API de alto nivel
  (`VPE_Init`/`VPE_Update`/etc.).
- `gfx.cpp` tiene solo 2 líneas de acoplamiento real a hardware DOS
  (`outp(0x3c8,...)`/`outp(0x3c9,...)`, escritura directa del DAC de
  paleta VGA) — mismo patrón ya resuelto en otros módulos del port
  (`port/io/`), trivial de sustituir.
- `hard.h`/`hard.cpp` son solo typedefs y una constante DPMI, sin
  acoplamiento real.

**A sustituir por un renderizador nuevo** (~1850 líneas de C + los 3
`.asm`):

- `scan.cpp` (421 líneas) — el algoritmo central de recorrido de
  portales/visibilidad.
- `view.cpp` (411 líneas) — orquestación del frame, lista de `VDraw`
  (qué se dibuja y en qué orden).
- `draw_sw.cpp`/`draw_cw.cpp`/`draw_o.cpp`/`draw_f.cpp` (1022 líneas) —
  preparan cada columna/span (textura, delta, distancia) y llaman al
  núcleo en ensamblador.
- `draw_wa.asm`/`draw_fa.asm`/`draw_oa.asm` — **confirmado**: son el
  único sitio donde existen `DrawWSpan`/`DrawFSpan`/`DrawOSpan`
  (`internal.h:134-140`) — no hay versión en C, ni siquiera como fallback
  lento (grep en los `draw_*.cpp` solo encuentra las *llamadas* a estas
  funciones, nunca su definición). Son los bucles internos de pintado de
  píxeles con mapeo de textura, desenrollados a mano con tablas de saltos
  (`WLoopOffset`/`FLOOP_LEN`/etc., `.inc` files) — técnica clásica de
  optimización de 1999, sin fallback en C que se pueda copiar tal cual.

### 53.4 El formato `.WLD` no cambia — un renderizador nuevo sería agnóstico del contenido

Verificado con un `.WLD` real (`div2/DIV2/DATA/WLD/WSPORTX/ZEENWS3.WLD`,
el mapa que usa `WSPORTX.PRG`): cabecera `"wld\x1a\x0d\x0a\x01"` seguida
de un bloque de "cabecera de edición" y luego los mismos structs
`ZF_Header`/`ZF_Point`/`ZF_Region`/`ZF_Wall` que ya lee/escribe
`divmap3d.cpp` (§52). `LoadZone()` (`src/vpe/load.cpp:26`) es quien
parsea ese formato, y **queda en el bloque "reutilizable"** (§53.3) — un
renderizador nuevo solo sustituiría lo que pasa *después* de cargar los
datos (`ScanLevel`/`DrawView`/los `draw_*`), que consume las tablas
`Points`/`Regions`/`Walls`/`Objects` ya en memoria sin saber nada del
juego que las generó. Conclusión: cualquier `.WLD` compilado por la
herramienta original debería cargar y renderizar igual con un
renderizador nuevo, sin recompilar el juego ni tocar sus recursos —
misma garantía de compatibilidad que ya se tiene con Modo-7/FLI (se
sustituye la implementación interna, no el contrato con el bytecode/
recursos DIV).

### 53.5 Decisión pendiente

No se ha decidido si escribir un renderizador nuevo (estilo Build, en C
limpio contra el framebuffer de `port/io`) o portar el ASM literal.
**Investigado y documentado a petición del usuario, sin empezar
implementación.** Si se retoma: diseñar primero `scan.cpp`+`view.cpp`+los
4 `draw_*.cpp` nuevos (el bloque "a sustituir" de §53.3), reutilizando
tal cual el bloque de carga/gestión de datos y el formato `.WLD` (§53.4).

## 54. Motor de renderizado Modo-8 en tiempo de ejecución — PORTADO

Decisión tomada (§53.5): renderizador nuevo en C limpio, no portar el ASM
literal. Al releer `src/vpe/` de cabo a rabo se descubrió que la
estimación de §53.3 era pesimista en dos sitios importantes:

- **`src/vpe/vpedll.cpp` (887 líneas) ya era la implementación real y
  completa** de todos los opcodes Modo-8 (`load_wld`/`start_mode8`/
  `stop_mode8`/`loop_mode8`/`set_fog`/`set_sector_texture`/
  `get_sector_texture`/`set_wall_texture`/`get_wall_texture`/
  `set_env_color`/`set_point_m8`/`get_point_m8`/`go_to_flag`/
  `set_sector_height`/`get_sector_height`) y del sistema de "objetos" de
  Modo-8 (`create_object`/`_object_data_input`/`_object_data_output`/
  `_object_destroy`/`_object_avance`) — los que hasta ahora eran no-op en
  `port_stubs.c`. Ya hacía pop/push correcto de `pila[]`/`sp` y usaba
  globals del runtime DIV que el port ya tenía portados (`mem[]`,
  `pila[]`, `sp`, `id`, `region`/`t_region`, `m8[]`, `copia`,
  `vga_an`/`vga_al`, `g[]` FPG pool, `e()`, `open_file`,
  `elimina_proceso`). No hizo falta diseñar la integración con el
  dispatcher de opcodes, solo portarla (mismo patrón que `divfli.cpp` en
  el hito de vídeo FLI: "copiado sin tocar" salvo adaptar includes DOS).
- De los 3 `.asm`, **solo 7 rutinas de pintado de span** necesitaban
  reemplazo (`DrawWSpan`/`DrawMaskWSpan`/`DrawTransWSpan` en
  `draw_wa.asm`, `DrawFSpan` en `draw_fa.asm`, `DrawOSpan`/
  `DrawMaskOSpan`/`DrawTransOSpan` en `draw_oa.asm`) — todo lo demás
  (`scan.cpp`/`view.cpp`/`draw_sw.cpp`/`draw_cw.cpp`/`draw_f.cpp`/
  `draw_o.cpp`) ya era C portable que solo *llamaba* a esas 7 rutinas.

### 54.1 Árbol nuevo: `port/vpe/`

Todo el motor VPE portado vive en `port/vpe/` (18 ficheros `.c`/`.h`,
compilados dentro de `div_core`/`div32run_port` vía `CMakeLists.txt`).
Resumen por fichero (todos `.c`, aunque el origen era `.cpp` — es C puro
sin ninguna construcción real de C++, igual que el resto del núcleo):

- **Copias casi literales** (`fixed.c`, `mem.c`, `globals.c`, `zone.c`,
  `object.c`, `update.c`, `vpe.c`, `scan.c`, `view.c`, `draw_sw.c`,
  `draw_cw.c`, `draw_f.c`, `draw_o.c`, `vpedll.c`): solo cambia
  `#include "div32run/inter.h"` → `#include "inter.h"` (ruta real del
  port) y se quitan 2-3 includes DOS muertos (`<dos.h>`/`<i86.h>` en
  `object.c`, nunca usados de verdad).
- **`hard.c`** (nuevo): `MemAlloc`/`MemRealloc`/`MemZAlloc`/`MemFreeAll`
  copiados tal cual; `FatalError` pierde las llamadas DOS
  (`rvmode`/`kbdReset`, conmutación de modo de vídeo BIOS que este port
  no hace); `FixMul`/`FixDiv` pasan de `#pragma aux` (ensamblador Watcom,
  que MSVC ignora en silencio, mismo patrón que `mul_24`/`mul_16` en
  `port_stubs.c`) a una implementación real en C de 64 bits.
- **`gfx.c`** (nuevo, reemplaza `gfx.cpp`): `InitGraph`/`ShutGraph`/
  `GetGraphWidth`/`GetGraphHeight` triviales sin cambios de fondo.
  `SetPalette` (el único `outp()` real al DAC VGA) no se llama nunca en
  el flujo real (confirmado por grep) — no-op documentado, el port ya
  gestiona la paleta vía `port/io` (`io_palette_set`, alimentada de
  `paleta[]` de DIV). `DrawBuffer`/`draw_buffer` (el otro acoplamiento
  DOS real, `#pragma aux` en ensamblador) se reemplaza por un memcpy
  rectangular en C plano.
- **`draw_span.c`** (nuevo): las 7 rutinas que reemplazan los 3 `.asm`.
  Son bucles de stepping en punto fijo 16.16 con lookup de paleta
  (sombreado) o de tabla de traslucencia 256×256 (`Pal.Trans`, que
  apunta a `ghost[]`, la misma tabla de traslucencia que ya usa el resto
  del runtime DIV) — sin preservar los trucos de registros del ASM
  original (optimizaciones de un 386, irrelevantes hoy). El índice de
  traslucencia usa el mismo orden que el resto del código ya portado
  (`ghost[(texel<<8)+dest]`, ver `s.cpp`). El suelo/techo (`DrawFSpan`)
  cambió de un `Coord`/`Delta` empaquetado en un DWORD (truco de
  registros `shld`/`shld` del ASM para extraer índice 2D de un solo
  golpe) a dos pares `U,V`/`dU,dV` `FIXED` explícitos en `struct FLine`
  (`internal.h`) — mismo resultado, sin necesidad de replicar el
  bit-twiddling original.

### 54.2 `port_stubs.c` y `CMakeLists.txt`

Se borró el bloque de no-ops Modo-8 de `port_stubs.c` (los opcodes y el
sistema de objetos, ver §54 arriba) — las implementaciones reales viven
ahora en `port/vpe/vpedll.c`. `CMakeLists.txt`: nuevo `VPE_SOURCES` con
los 18 ficheros de `port/vpe/`, añadido a `div_core` (por tanto disponible
en `div32run_port`), con el mismo force-include `/FIport_pre.h` que
`port_stubs.c`/`ia.c` (necesario porque `load.c`/`object.c`/`hard.c`/
`vpedll.c` incluyen `inter.h`, que choca con `<time.h>` si no se procesa
en el orden correcto — ver el comentario grande en `port_pre.h`).

### 54.3 Bugs encontrados y arreglados (todos con ASan, `build-asan/`)

Un juego real (`WSPORTX.PRG`, compilado con `divc_port.exe` a
`.div32` y ejecutado con `div32run_port.exe` desde `div2/DIV2/DATA`)
crasheaba con `STATUS_HEAP_CORRUPTION` (`0xC0000374`) al cargar el nivel.
El build normal (Debug/Release) no da ningún diagnóstico útil para este
tipo de crash — hizo falta el build con AddressSanitizer ya configurado
en `build-asan/` (`/fsanitize=address`) para localizar cada bug con pila
de llamadas exacta. Se añadieron temporalmente `fprintf(stderr,...)` en
puntos clave (`load_wld`/`_vpe_inicio`/`loop_mode8`) para bisectar antes
de tener ASan compilando, luego se quitaron.

Los 5 bugs encontrados, todos variantes de **patrones ya vistos en el
port** (ver `[[project_div_port_status]]`/memoria) más uno nuevo:

1. **`load.c` (`LoadZone`): `WallPtrs=CacheAlloc(Walls.Number*2*4)`** —
   asumía puntero de 4 bytes (DOS 32-bit); en x64 un `struct Wall*` mide
   8, así que el buffer se quedaba a la mitad y `WallPtrs[NumWallPtrs]=
   wall` escribía fuera (heap-buffer-overflow, primer crash encontrado).
   Arreglado con `sizeof(struct Wall *)`.
2. **`view.c` (`SetActiveView`): `BufScan=malloc(pv->Height*4)`** — mismo
   bug, un array de `BYTE*` (una entrada por scanline) asumiendo 4 bytes
   por puntero. Arreglado con `sizeof(BYTE *)`.
3. **`load.c` (`LoadPalette`): `Pal.Tables[0]=(BYTE*)(((DWORD)Pal.MemTables
   +255)&0xFFFFFF00)`** — casteaba el puntero a `DWORD` (32 bits) para
   alinearlo a 256 bytes, truncando la mitad alta en x64
   (access-violation al escribir a través de `Pal.Tables[0]` en
   `set_fog_table`). Arreglado con aritmética de `uintptr_t`.
4. **`load.c` (`set_fog_table`): `char *tabla_color`** — mismo patrón que
   el bug de E10 (`diveffec.cpp`, ya documentado): valores de índice de
   paleta 0-255 guardados en un `char` **con signo** (default MSVC); para
   valores ≥128 el wraparound a negativo, multiplicado por 256 al indexar
   `ghost[]`, produce un offset enorme fuera de rango
   (heap-use-after-free reportado por ASan al leer muy lejos de
   `ghost`). Arreglado cambiando el tipo a `BYTE *`.
5. **`vpedll.c` (`start_mode8`/`loop_mode8`): `(t_region*)((int)region+
   sizeof(t_region)*num_region)`** — el patrón clásico de puntero
   truncado a `int` de 32 bits, encontrado por inspección (warning
   C4311) antes de que llegara a manifestarse como crash. Arreglado con
   `&region[num_region]` (misma aritmética, sin pasar por `int`).
6. **`draw_sw.c` (`DrawSimpleWall`): `CurLevel->Clip[PickVX]` leído sin
   comprobar `PickVFlag`** — con el pick desactivado (`Engine.PickX=-1`,
   el estado por defecto) esto leía `Clip[-1]`, un byte antes del array.
   En DOS real-mode esa lectura fuera de rango era inofensiva (el valor
   ni se usaba, ya que su único uso más abajo SÍ está protegido por `if
   (PickVFlag)`); ASan lo marca como heap-buffer-overflow real. Arreglado
   envolviendo también la lectura inicial en `if (PickVFlag)`.
7. **`vpedll.c` (`_object_destroy`): sin comprobar `num_object<0`** —
   encontrado *después* de dar el hito por cerrado, al probar
   `WSPORTX.PRG` de verdad vía el IDE (el usuario reportó "el juego
   crashea al empezar una partida, aunque el Modo-8 del menú principal sí
   funciona"). `elimina_proceso()` (`i.cpp:1187`) llama
   `_object_destroy(mem[id+_M8_Object])` para **cualquier** proceso que
   se destruye, tenga o no objeto Modo-8 asociado — `_M8_Object` vale -1
   por defecto si nunca se creó uno, exactamente el mismo caso que ya
   comprueban sus hermanas `_object_data_input`/`_object_data_output`
   (`if (mem[ide+_M8_Object]==-1) return;`) pero que `_object_destroy` no
   comprobaba. Sin el guard, `Objects.ptr[-1]` lee 8 bytes *antes* del
   array — el campo `Size` de la `Table` (`~sizeof(struct Object)`,
   ≈0x108) — como si fuera un puntero válido, y crashea al
   desreferenciarlo (`po->pp`). Confirmado con ASan
   (access-violation, dirección leída ≈0x108, coincide con el tamaño de
   `struct Object`) reproduciendo con clics/teclas sintéticos hasta
   entrar en la minipartida real de salto de esquí acuático. Arreglado
   añadiendo el mismo guard `if (num_object<0 || num_object>=
   Objects.Number) return;` al principio de la función. **Verificado**:
   la partida real (modo "Jump", `WSPORTX.PRG`) corre de punta a punta
   tras el fix — marcador de distancia subiendo con normalidad, sin
   crash.

Los bugs 1-3 y 5 son la misma familia de raíz (asumir puntero de 32 bits,
como en el bug ya documentado del generador de explosiones y otros
módulos) — **queda como lección para el siguiente módulo que se porte**:
grep sistemático de `*4`, `(DWORD)`, `(int)puntero` al portar cualquier
fichero nuevo de `src/`, no solo confiar en los warnings del compilador
(el bug 3 no genera ningún warning, `(DWORD)puntero` es una conversión
explícita). El bug 7 es de otra familia: **al portar una función que
tiene "hermanas" con una forma muy parecida (`_object_data_input`/
`_object_data_output`/`_object_destroy`, todas indexadas por
`mem[id+_M8_Object]`), comprobar que TODAS repiten la misma guarda de
sentinela (`==-1`/`<0`) antes de dar el port por cerrado — no asumir que
si una función well-formed implica que sus vecinas también lo son.

### 54.4 Verificado con un juego real

`WSPORTX.PRG` (juego real, el mismo usado para diagnosticar el problema
en §53) compilado con `divc_port.exe` y ejecutado con
`div32run_port.exe` desde `div2/DIV2/DATA`:

- **Arranca, muestra menús y pantallas de título reales** ("EXTREME
  WATER SPORTS", selección de modo, "SKI Acuático") con texturas,
  degradados de paleta y fundidos a negro correctos — confirmado con
  capturas de pantalla del proceso en ejecución.
- **`start_mode8` y `loop_mode8` se ejecutan de verdad** (confirmado con
  trazas temporales): el bucle llama a `VPE_Render()` en cada frame de
  forma continua durante 20+ segundos bajo AddressSanitizer sin ningún
  error nuevo (heap corruption, overflow, use-after-free o
  access-violation) — la ruta completa `load_wld` → `start_mode8` →
  `loop_mode8`/`VPE_Render` es estable.
- **Bug adicional encontrado al probar desde el IDE (F10/F12) con una
  partida real** (`_object_destroy` sin guard, ver bug 7 de §54.3) — con
  ese fix, **la minipartida de salto de esquí acuático ("Jump") corre de
  punta a punta**: automatizado con clics/teclas sintéticos (título →
  "START MODE" → "SKI Acuático" → "LET'S GO!" → salto en marcha), captura
  de pantalla confirma el marcador de distancia subiendo con normalidad
  (95' → 139') mientras `start_mode8`/`loop_mode8` siguen activos y
  llamando a `VPE_Render()` cada frame sin ningún error de ASan. La
  escena (agua, edificios/árboles de fondo, esquiador+barca como
  sprites/objetos) se renderiza mientras el Modo-8 está activo, aunque no
  se ha aislado con un pick manual pixel a pixel que **toda** esa
  geometría de fondo pase por `DrawSimpleWall`/`DrawComplexWall` (vs.
  fondo Modo-7 + Modo-8 sólo para algo más) — confirmación visual
  razonablemente sólida, no matemáticamente exhaustiva.

### 54.5 Pendiente / siguientes pasos

- Renderizado de objetos/sprites Modo-8 (`DrawObject`, `_object_avance`
  desde un programa DIV real) — el código está portado (`draw_o.c`,
  `DrawOSpan`/`DrawMaskOSpan`/`DrawTransOSpan` en `draw_span.c`) y la
  partida de "Jump" ya lo ejercita en la práctica (el esquiador/la barca
  se ven con normalidad), pero no se ha inspeccionado a fondo si alguno
  de esos elementos usa el camino Modo-8 real (`Object`/`TexCon`) o son
  sprites 2D normales del motor DIV.
- Posible detalle cosmético sin confirmar: la orientación del suelo/techo
  en `DrawFSpan` (`draw_span.c`) usa indexado fila-mayor (`V*Width+U`);
  como las texturas se cargan rotadas 90° internamente (`LoadPic`, mismo
  camino que usan las paredes), es posible que el suelo salga rotado
  respecto al original -- no se ha detectado a simple vista en las
  capturas de la partida de "Jump", pero tampoco se ha revisado a
  conciencia con un nivel con suelo texturizado visible en primer plano.
- Explorar otros modos de `WSPORTX` (o el resto de los "water sports")
  para confirmar un nivel Modo-8 más "clásico" (interior con
  paredes/pasillos), si el usuario quiere seguir verificando el motor.

## 55. Generador de sprites (GENSPR) — PORTADO Y VERIFICADO (2026-09-22)

Aparcado explícitamente unas horas antes en esta misma jornada (ver
`16-port-plan-pendiente.md` §3) por su tamaño aparente. El usuario pidió
una investigación de solo lectura para valorar si "reescribirlo" era
razonable, y con el resultado en la mano pidió retomarlo en la misma
sesión.

### 55.1 Qué había que portar de verdad

`divspr.cpp`+`divsprit.cpp` (integración GUI, diálogos `GenSpr0..3`) y
`src/div/visor/` (motor 3D propio: `animated`/`complex`/`fileanim`/
`global`/`hlrender`/`llrender`/`resource`/`sprite3d`/`visor`, ~5.200
líneas). De `src/div/visor/main.cpp` (driver de demo standalone con
VESA/`<conio.h>`/`<i86.h>`) no hacía falta nada: huérfano, no lo usa
`divspr.cpp` (que entra directo por `visor.hpp`).

Único ensamblador real: `t.asm` (788 líneas — en realidad solo 2
rutinas, con/sin máscara, desenrolladas a mano 6 veces por tamaño de
textura de potencia de dos) + **6 rutinas `#pragma aux` de Watcom más**,
repartidas entre `visor.cpp` (`lfset`, relleno de palabras) y
`llrender.cpp` (`inicio_division`/`fin_division`/`asignacion_u_v`,
encadenamiento de una `fdiv` x87 para solapar su latencia con otro
trabajo — truco de Pentium de 1999 invisible a nivel de C; y
`nucleo1`/`nucleo2`, en realidad una sola rutina de blending RGB565
partida en dos con `pushad` en la primera y `popad` al final de la
segunda, sin restaurar registros entre medias — siempre se llaman
seguidas, `nucleo1(); nucleo2();`). Estas 6 no se habían detectado en la
investigación inicial (que solo miró `t.asm`); aparecieron como errores
de enlazado (`LNK2019`) al añadir `llrender.cpp`/`visor.cpp` al build.

### 55.2 Traducción de `t.asm` — núcleo de mapeado de texturas

Nuevo fichero `port/div/ide/port_ide_genspr_t.c`. Las 12 rutinas del
`.asm` (`nucleo8_8..256` y `mask_nucleo8_8..256`) son la misma rutina
parametrizada por el número de bits de ancho de textura (3..8): un DDA
de punto fijo que empaqueta `(u,v)` en un único par de registros
`edx:ecx` para avanzar ambas coordenadas con un solo `ADD`/`ADC`
(equivalente a sumar un entero de 64 bits), y usa `AND`+`ROL` para
extraer el índice de textura sin multiplicar. Traducción bit a bit,
verificada a mano contra las 12 variantes del `.asm` (no una reescritura
"limpia" del algoritmo): un único núcleo en C con `uint64_t` para el
acumulador de 64 bits y 12 wrappers finos con los nombres originales
exactos, así `llrender.cpp` no necesita ningún cambio en el `switch` que
los despacha.

**Bug real encontrado de paso** (mismo patrón que los 6 de Modo-8,
§54.3): `direccion_textura` (el puntero a la textura activa) estaba
declarado `int` en `t.h` y se le asignaba un puntero real
(`llrender.cpp:1161`, `direccion_textura=(long)mat->textura->Levels[...]`)
— válido en el DOS de 1999 (puntero de 32 bits), pero trunca el puntero
real en x64. Arreglado ensanchando la declaración a `intptr_t` (mismo
tamaño en DOS de 32 bits, correcto en Windows de 64 — no rompe el build
MS-DOS). Como `t.asm` ya no se compila en este port, `direccion_textura`
tampoco tenía ya dónde vivir como variable real (antes vivía en el
`.DATA` del propio `.asm`) — se definió en `port_ide_genspr_t.c`.

### 55.3 Las otras 6 `#pragma aux`

Traducidas in-place dentro de `visor.cpp`/`llrender.cpp` (no en un
fichero de port aparte, porque dependen de globales propios de cada
fichero). `lfset` es un relleno de palabras de 16 bits (`rep stosw`),
trivial. `inicio_division`/`fin_division`/`asignacion_u_v` se
simplificaron a cálculo directo (`inversa = 1.0f/(*x)`, luego
`u_0=lrintf(inversa*fu)` etc. — `lrintf` para igualar el redondeo de
`FISTP`). `nucleo1`/`nucleo2` se tradujeron literal, registro a
registro, con un `uint32_t` de estado compartido a nivel de fichero
(`nucleo_eax/ebx/edx`) en vez de registros reales, ya que ambas
funciones solo se invocan siempre seguidas.

### 55.4 Otros hallazgos al integrar en el build

- `divsprit.cpp` usa `sp_normal_mask`/`sp_scan`/`sp_scanc`/
  `sp_scan_mask`/`sp_scanc_mask` antes de definirlas — mismo patrón ya
  visto en E8/E9 (declaración implícita vs. definición real,
  incompatible al compilar como C estricto). Arreglado con
  declaraciones adelantadas al principio del fichero.
- `ERROR` (variable global `char *ERROR` del módulo, usada como
  "último error fatal") choca con la macro `ERROR` de `wingdi.h`
  (definida como `0`, entra vía el force-include de `windows.h`).
  Añadido `#undef ERROR` a `port_ide_pre.h`, mismo patrón que los
  `#undef CreateFont*` ya existentes.
- Auditoría de stubs (método §45.1): `port_ide_stubs.c` tenía 7 stubs
  duplicados de símbolos ahora reales (`t64`, `generador_sprites`,
  `sp_normal`, `sp_rotado`, `sp_size`, `invierte_hor`, `invierte_ver`)
  — todos simples duplicados de función/variable (`LNK2005`), no el
  patrón peligroso de `void*` tapando un struct real; se retiraron sin
  incidencias.
- Faltaba la *junction* de recursos `genspr/` en `build/Release` (el
  resto de recursos —`system`/`help`/`resource`— ya la tenían, ver §27).
  Sin ella, `ParseAnimFile()`/`CargarTextura()` (que resuelven contra el
  directorio de trabajo del proceso) no encontraban nada y el generador
  fallaba en silencio. Creada a mano con `New-Item -ItemType Junction`
  (mismo mecanismo que las otras, no automatizado por CMake).

### 55.5 Verificación en caliente

`Mapas → Generador de sprites` abre el diálogo (`GenSpr0`), muestra la
lista de animaciones (`GOLPE`/`ANDAR`/`CORRER`/...), y tanto "Aceptar"
como "Cancelar" cierran limpio sin crash — probado con
`tools/drive_ide.ps1` (clics sintéticos) + `DIV_IDE_SHOT_EVERY`
(capturas), recompilando `build/Release` (no solo `build-dbg`) antes de
cada prueba.

**Limitación heredada, no del port**: el `genspr\textura.pcx` que trae
el repo (las 3 copias: raíz, `div2/DIV2/DATA`, `div2/DIV2/DATA2`) es en
realidad un JPEG (cabecera `FF D8 FF E0`) con extensión `.pcx` — y JPG
sigue stub en este port (decisión de E6a, `divforma.cpp`, conflicto
`jpeglib`/`rpcndr.h`). Con el contenido tal cual viene, `CargarTextura()`
recorre `es_MAP`/`es_PCX`/`es_BMP`/`es_JPG`, ninguno reconoce el
fichero, y sale "No se reconoce el tipo de fichero" — comportamiento
correcto del código, dato de entrada incorrecto. **Verificado que el
resto del pipeline funciona de verdad**: sustituyendo temporalmente
`genspr/textura.pcx` por un PCX real (`help/help.pcx`, mismo repo) el
generador renderiza el modelo 3D con textura y sombreado Gouraud
correctos (visto en las capturas: logo de DIV Games Studio en la
miniatura, muñeco sombreado en la vista 3D). Esto confirma que la
traducción de `t.asm` (§55.2) y de `nucleo1`/`nucleo2` (§55.3) es
correcta. Si se quiere corregir el problema de raíz haría falta decodificar
JPEG de verdad (fuera de alcance actual, mismo motivo que en E6a) o
reemplazar el asset por un PCX genuino.

### 55.6 Pendiente / siguientes pasos

- Nada bloqueante. El único hueco es el JPG de `textura.pcx` (arriba),
  ya documentado y fuera de alcance.
- No se ha probado a fondo el flujo completo de "Aceptar" → escribir un
  sprite nuevo en un `.fpg` real (`CreaSpriteFPG()`) — solo se probó
  con la ruta de error de textura y con "Cancelar". Si se quiere cerrar
  del todo, sería el siguiente paso natural (con un PCX real puesto
  temporalmente, como en la verificación de arriba).

### 55.7 Crash real al clicar la vista 3D — doble `free()`, CERRADO (2026-09-23)

La verificación de §55.5 solo había probado "Cancelar" y la ruta de
error de textura. Al probar con un PCX real puesto y hacer clic sobre
el muñeco 3D, el IDE crasheaba con `STATUS_HEAP_CORRUPTION`
(`0xC0000374`), confirmado en el Visor de sucesos de Windows.

**Causa** (bug real del original de 1999, no introducido por el port):
en `CargarSprite()` (`src/div/divspr.cpp`), cuando el thumbnail no
necesita reescalarse (`man<=96*big2 && mal<=64*big2`), el código hace
`ThumbSprite=temp2` con `temp2==mapa==MapaSprite` — es decir,
`ThumbSprite` y `MapaSprite` acaban siendo **el mismo puntero**. Pero
tanto al principio de la propia `CargarSprite()` como en
`FinalizaGenerador()`, la limpieza los libera como si fueran
allocaciones independientes (`free(MapaSprite); free(ThumbSprite);`).
La primera apertura del diálogo toma la rama de alias; el manejador de
rotación (`GenSpr2`) llama a `CargarSprite()` de nuevo en cuanto hay
clic (incluso sin arrastre real), y esa segunda pasada libera el mismo
bloque dos veces.

**Diagnóstico**: se descartó `GenSpr1` (dibujo de la cruceta) y
`reescalar_sprite3d()` (recorte tras rotar) instrumentando ambos con
`fprintf(stderr,...)` — valores siempre dentro de límites. La pista
reveladora: un solo print de `GenSpr1` pero DOS de
`reescalar_sprite3d()` antes del crash, situando el fallo justo en la
segunda llamada a `CargarSprite()`.

**Fix**: en `CargarSprite()` y en `FinalizaGenerador()`, liberar
`ThumbSprite` solo si es distinto de `MapaSprite` (comprobación hecha
**antes** de poner `MapaSprite` a `NULL`):

```c
if(ThumbSprite!=NULL && ThumbSprite!=MapaSprite)
{
  free(ThumbSprite);
}
ThumbSprite=NULL;

if(MapaSprite!=NULL)
{
  free(MapaSprite);
  MapaSprite=NULL;
}
```

Aplicado con un script Python byte-exacto (`'rb'`/`'wb'`, `data.find()`
sobre los bytes crudos, sin decodificar) — `divspr.cpp` es CP850.
Comentado con `/* PORT: ... */` explicando el aliasing.

**Bonus encontrado al recompilar**: apareció un `LNK2019` nuevo,
`lfset` sin resolver — un séptimo `#pragma aux` de Watcom en
`visor.cpp` (relleno de palabras de 16 bits, `rep stosw`) que la
investigación de §55.3 no había detectado porque no está en
`llrender.cpp` (donde se buscó `#pragma aux` la primera vez). Traducido
a C llano:

```c
void lfset(void *buf, long c, long d)
{
  unsigned short *p = (unsigned short *)buf;
  unsigned short val = (unsigned short)c;
  long n = d;
  while (n-- > 0) *p++ = val;
}
```

(El `memset()` que dejaba comentado el propio original de 1999 como
alternativa NO era equivalente — `memset` rellena bytes, `rep stosw`
rellena palabras de 16 bits; por eso nunca se activó esa vía.)

**Verificación en caliente**: recompilado `build/Release`, reproducido
con `tools/drive_ide.ps1` (secuencia de clics de
`17-genspr-crash-en-curso.md`) — sobrevive un clic simple y una
secuencia de 10 clics seguidos sobre la vista 3D (`salio_solo=False`
en ambos casos). **Confirmado además por el usuario probándolo
manualmente en el IDE real.**

**Lección reutilizable**: al buscar `#pragma aux` en un módulo nuevo
(lección ya anotada en §55.3), grepear **todos** los ficheros que se
añaden al build, no solo el que ya dio problemas de enlazado la
primera vez — aquí `llrender.cpp` ya se había revisado, pero
`visor.cpp` (mismo directorio, mismo build) tenía uno más que solo
salió a la luz al ejercitar la ruta de código que lo llama
(`visor_loop()`, alcanzada solo al clicar/rotar, no al simple abrir el
diálogo).

Con esto, GENSPR queda cerrado de verdad: abre, renderiza, y aguanta
clics/arrastres repetidos en la vista 3D sin crash.
