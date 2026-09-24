# Dependencias de terceros

Este documento amplía, con lectura directa del código, cómo se integran en DIV Games Studio 2 las
librerías de terceros que viven en `3rdparty/` y el extensor DOS de 32 bits que vive en `pmwlite/`.
Todas ellas se compilan como librerías estáticas para Watcom (`.lib`) y se enlazan contra los
binarios de DIV (`D.EXE`/`D.386` del IDE, `DIV32RUN.DLL`/`.ins` del intérprete, `install.ovl` del
instalador). El repositorio las trae ya compiladas en `3rdparty/lib/{386,586}/*.lib`; el código
fuente se conserva para poder reconstruirlas (requiere Turbo Assembler para las partes en ensamblador).

Estructura de control del build: el archivo raíz `3rdparty.mif` incluye, según banderas
`NO_JPEG`/`NO_JUDAS`/`NO_SCITECH`/`NO_TOPFLC`/`NO_ZLIB`, los `.mif` individuales
(`3rdparty/judas.mif`, `3rdparty/scitech.mif`, `3rdparty/jpeglib.mif`, `3rdparty/topflc.mif`,
`3rdparty/zlib.mif`). Cada uno de estos define, cuando existe la macro `CPU` (`386` o `586`):

- La ruta a la `.lib` de salida en `3rdparty/lib/$(CPU)/...`.
- La ruta al código fuente de la librería (`*_DIR`).
- Un `INCLUDE +=` con el directorio de cabeceras correspondiente.
- Una regla que compila la librería invocando su propio `makefile`/`makefile.wat`/`makefile.w32`
  con `$(MAKE) ... CPU=$(CPU)`, y copia el resultado a `3rdparty/lib/$(CPU)/`.
- Un target `clean_<lib>` agregado a `CLEAN_LIB3P_DEPS` para el `clean` global.

Esto confirma el motivo de tener libs separadas por CPU: cada `makefile` de módulo (`src/div/makefile`,
`src/div32run/makefile`, etc.) pasa `-3r -fpc` para `CPU=386` o `-5r -fp5` para `CPU=586` tanto al
compilador como al ensamblador (`WASM_OPTIONS`), por lo que hace falta una `.lib` compilada con cada
modelo de coma flotante/instrucciones.

---

## JUDAS (audio)

**Qué es.** JUDAS Apocalyptic Softwaremixing Sound System, una librería de mezcla de sonido por
software en tiempo real para DOS (Sound Blaster/SB Pro/SB16/GUS, timers de PC), escrita en C +
ensamblador x86. El `README.md` del repo enlaza el origen como
`https://github.com/volkertb/JUDAS`. El código vive en `3rdparty/judas/` (`judas.c`, `judasasm.asm`,
`judasdma.c`, `judasgus.*`, `judasmod.c`/`judass3m.c`/`judassmp.c`/`judasxm.c` para los formatos de
módulo MOD/S3M/XM, `judaswav.c` para WAV, `judastbl.c` para tablas de mezcla, `kbd.c`/`kbd.h` y
`timer.c`/`timer.h` con sus contrapartes en ensamblador para teclado e IRQ0/timer).

**Cómo se integra.** `3rdparty/judas.mif` genera `JUDAS_LIB` = `3rdparty/lib/$(CPU)/judas.lib`
compilando `3rdparty/judas` con su propio `makefile` (recibe `ASM`, `TASM_EXE`, `CPU`). Esta
variable `JUDAS_LIB` se agrega a la lista `LIBS` tanto en `src/div/makefile` como en
`src/div32run/makefile` (el propio `.mif` no expone macro de includes activada — la línea
`#INCLUDE += -I$(JUDAS_DIR)` está comentada — así que los módulos de DIV que usan JUDAS incluyen la
ruta manualmente vía opciones de compilación, p.ej. `-i=judas` en `src/div32run/makefile`).

**Dónde se usa realmente.** No se incluye directamente `judas.h` en el código de DIV mediante
`#include`, se declaran/usan las funciones de la API en minúscula (el prefijo real de la API C es
`judas_*`, no `Judas_*` como sugería la hipótesis inicial). Llamadas encontradas:

- `src/div/divsound.cpp`, `src/div32run/divsound.cpp`: `judas_config()`, `judas_init()`,
  `judas_setmusicmastervolume()`, `judas_uninit()` — inicialización/cierre del subsistema de sonido
  tanto en el IDE como en el intérprete.
- `src/div/divpcm.cpp` (gestión de canales PCM/música del IDE) y `src/div32run/divsound.cpp`
  (versión runtime): `judas_loadwav`, `judas_loadwav_mem`, `judas_loadrawsample[_mem]`,
  `judas_allocsample`, `judas_freesample`, `judas_playsample`, `judas_stopsample`,
  `judas_ipcorrect`, `judas_loadxm[_mem]`/`judas_loadmod[_mem]`, `judas_playxm`/`judas_playmod`,
  `judas_freexm`/`judas_freemod`, `judas_getxmpos`/`judas_getmodpos`,
  `judas_getxmline`/`judas_getmodline`, `judas_getxmchannels`/`judas_getmodchannels`,
  `judas_set_xm_pos`/`judas_set_mod_pos`, `judas_songisplaying`, `judas_getvumeter`
  (vúmetro visual del IDE), `judas_channel[]` (array de canales expuesto por la librería).
- `src/div/divbrow.cpp` (previsualizador de sonidos del explorador de archivos del IDE): mismas
  funciones de carga/reproducción de muestras y de MOD/XM.
- `src/div/divhandl.cpp`: `judas_stopsample()` en manejadores de eventos.
- `src/div/divtimer.cpp` y `src/div32run/divtimer.cpp`: `judas_memlock`/`judas_memunlock` para
  bloquear en memoria física el código que corre dentro de la ISR del timer (imprescindible bajo un
  extensor DOS, para que el manejador de IRQ no resida en páginas paginables).

En resumen: JUDAS es el motor de audio compartido por el IDE (reproducción de muestras/canciones
en el editor, monitor de VU) y por el runtime del lenguaje DIV (comandos de sonido del propio
lenguaje, ver `divsound.cpp` en `div32run`).

---

## SciTech SuperVGA Kit (SVGA + PMODE)

**Qué es.** No es MGL, como se sospechaba inicialmente, sino el **"SuperVGA Kit" (UniVBE Software
Development Kit)** de SciTech Software, Inc. (Chico, California), una librería más pequeña que MGL
que después fue absorbida por él. Confirmado en el propio `3rdparty/scitech/README.md` (escrito por
el mantenedor del repo) y en la cabecera de `3rdparty/scitech/include/svga.h`:

```
The SuperVGA Kit - UniVBE Software Development Kit
Copyright (C) 1996 SciTech Software. All rights reserved.
Version: $Revision: 1.8 $   Date: 29 Mar 1996
```

DIV originalmente usaba una versión de 1995; el mantenedor sólo pudo recuperar la 6.0 (1996), que
introducía cambios incompatibles a nivel de código y tuvo que adaptar manualmente. La compilación
oficial usaba DMAKE con varios compiladores; el repo sustituye eso por `makefile.wat` (Watcom)
propios. SciTech Software también negoció la apertura de Watcom, dando origen a OpenWatcom; hoy la
compañía es de Alt Richmond Inc. y ya no distribuye SDKs al público.

**Contenido de la carpeta.** `3rdparty/scitech/include/` trae `svga.h`, `vesavbe.h` (API VBE de
bajo nivel: `VBE_getModeInfo`, `VBE_setVideoMode`, `VBE_modeInfo`), `vbeaf.h` (VESA BIOS Extension /
Accelerator Functions), `pmode.h`/`pmpro.h` (soporte de modo protegido propio de SciTech, distinto
de PMODE/W), `ztimer.h` (temporizador de alta precisión), `wdirect.h`, `avidirec.h`, `debug.h`,
`model.mac`. `3rdparty/scitech/src/` trae el código fuente de `svgakit/`, `pmode/`, `util/` y
`ztimer/`.

**Cómo se integra.** `3rdparty/scitech.mif` genera **dos** librerías distintas, ambas construidas
desde este mismo árbol de fuente con `makefile.wat`:

- `SVGA_LIB` = `3rdparty/lib/$(CPU)/svga.lib`, compilada desde `3rdparty/scitech/src/svgakit`.
- `PMODE_LIB` = `3rdparty/lib/$(CPU)/pmode.lib`, compilada desde `3rdparty/scitech/src/pmode`.

Ambas quedan disponibles como macros `SVGA_LIB`/`PMODE_LIB` para quien incluya `scitech.mif`, y se
usan juntas en `LIBS` de `src/div/makefile`, `src/div32run/makefile` e `src/install/makefile`. El
`.mif` añade `-I3rdparty/scitech/include` al `INCLUDE` global.

**Nota de nomenclatura importante:** hay dos cosas llamadas "PMODE" en el repo que no deben
confundirse: `pmode.lib`/`3rdparty/scitech/src/pmode` es un módulo interno del SuperVGA Kit de
SciTech (helpers de modo protegido para acceder a VESA/VBE), mientras que **PMODE/W** (carpeta
`pmwlite/` en la raíz) es el extensor DOS de 32 bits completo, un producto totalmente distinto (ver
sección propia más abajo). Ambos coexisten en el mismo build.

**Dónde se usa realmente.** El código de DIV incluye `<svga.h>` (no `pmode.h` directamente desde
`src/`) y llama a la API VBE de alto nivel:

- `src/div32run/v.cpp` (funciones de vídeo de bajo nivel del intérprete) y `src/div/divvideo.cpp`
  (vídeo del IDE): `#include <svga.h>` y `VBE_setVideoMode(mode)` para cambiar a modos VESA/SVGA.
- `src/lfbprof.c`, `src/install/i.cpp`, `src/install2/main.c` también referencian svga/pmode
  (confirmado por búsqueda de símbolos, aunque no se leyó el detalle línea por línea de cada uno).

En la práctica: `svga.lib` resuelve el acceso a los modos gráficos VESA/SVGA (detección de modos,
cambio de modo, info de modo) usado tanto en el IDE como en el runtime; `pmode.lib` es una
dependencia interna que usan las rutinas de `svga.lib` para operar en modo protegido y se enlaza
siempre junto a ella (aparece en la misma lista `LIBS` en todos los makefiles que usan `SVGA_LIB`).

---

## libjpeg (IJG)

**Qué es.** El código en `3rdparty/jpeglib/` es una copia completa de la distribución oficial de la
Independent JPEG Group. El `README` identifica la versión exacta:

```
README for release 9c of 14-Jan-2018
```

Es decir, **libjpeg 9c** (2018-01-14), con su árbol de fuentes íntegro (autoconf, makefiles para
decenas de compiladores/plataformas, utilidades `cjpeg`/`djpeg`/`jpegtran`, imágenes de test, etc.),
de las cuales sólo hace falta compilar el núcleo de la librería para DIV.

**Cómo se integra.** `3rdparty/jpeglib.mif` genera `JPEG_LIB` = `3rdparty/lib/$(CPU)/libjpeg.lib`
compilando `3rdparty/jpeglib` con su `makefile` (recibe sólo `CPU=$(CPU)`), y añade
`-I3rdparty/jpeglib` al `INCLUDE` global (a diferencia de JUDAS, aquí sí se expone el include path).

**Dónde se usa realmente.** Un único punto de integración en todo `src/`:

- `src/div/divforma.cpp` (formatos de imagen del IDE): incluye
  `"jpeglib/jpeglib.h"` y `"jpeglib/cdjpeg.h"`, y usa la API clásica de decodificación de libjpeg —
  `jpeg_std_error`, `jpeg_create_decompress`, `jpeg_mem_src`, `jpeg_read_header`,
  `jpeg_start_decompress`, `jpeg_read_scanlines`, `jpeg_finish_decompress`,
  `jpeg_destroy_decompress` — en al menos tres funciones distintas de ese archivo (líneas ~973,
  ~997 y ~1430), aparentemente variantes para decodificar cabecera/miniatura/imagen completa (o
  para distintos casos de uso: preview vs. carga real), incluyendo manejo de `JCS_GRAYSCALE`.

En resumen, libjpeg sólo se usa para **cargar imágenes JPEG dentro del IDE** (import de imágenes en
`divforma.cpp`, el módulo de formatos gráficos de DIV); no se encontró uso de libjpeg en
`div32run` (el runtime del lenguaje DIV no decodifica JPEG en tiempo de ejecución — las imágenes de
un juego DIV se guardan como FPG en su formato propio, no como JPEG suelto).

---

## zlib

**Qué es.** Copia oficial de zlib. El `README` de `3rdparty/zlib/` confirma la versión:

```
zlib 1.2.11 is a general purpose data compression library.
(C) 1995-2017 Jean-loup Gailly and Mark Adler
```

Es decir **zlib 1.2.11**, con su árbol completo (incluye `contrib/`, `doc/`, `test/`, makefiles para
decenas de entornos, y ya trae un `watcom/` propio: `watcom_f.mak`/`watcom_l.mak` para Watcom en
modelo "flat"/"large").

**Cómo se integra.** `3rdparty/zlib.mif` genera `ZLIB_LIB` = `3rdparty/lib/$(CPU)/zlib_f.lib`
(nombre "_f" de "flat", el modelo de memoria plano de 32 bits) compilando
`3rdparty/zlib` con `watcom/watcom_f.mak CPU=$(CPU)`, y añade `-I3rdparty/zlib` al `INCLUDE` global.

**Dónde se usa realmente.** Se usa en varios componentes, siempre mediante la API simple
`compress()`/`uncompress()` (no el streaming `deflate`/`inflate` de bajo nivel):

- `src/div32run/f.cpp` (funciones del lenguaje DIV accesibles desde scripts): implementa
  `_compress(int encode)` que llama a `compress()`/`uncompress()` — este es el backend de los
  opcodes `case 161`/`case 162` (comandos `COMPRESS`/`UNCOMPRESS` del lenguaje DIV, a confirmar el
  nombre exacto en la documentación del lenguaje). También hay una llamada a `uncompress()` en la
  ruta de lectura de recursos empaquetados (línea ~249, sobre `packdir[n]`, es decir, al leer
  ficheros de un PAK con datos comprimidos).
- `src/div/divc.cpp` (compilador del lenguaje DIV dentro del IDE): usa `compress()` — probablemente
  para comprimir el bytecode o recursos embebidos al generar el ejecutable final del juego.
- `src/div/divfrm.cpp`: usa `compress()` dos veces (contexto no explorado en detalle, pero
  consistente con compresión de recursos del formato de DIV, p. ej. formularios/FRM).
- `src/install/i.cpp`: usa `uncompress()` — el instalador generado por DIV descomprime datos del
  paquete de instalación.
- `src/install2/main.c`: usa `uncompress()` — instalador alternativo en C puro, mismo propósito.

En conjunto, zlib es la librería de compresión usada tanto para el formato interno de recursos de
DIV (PAK/FRM, leídos desde `div32run`) como para los propios ejecutables/instaladores que DIV genera
para los juegos terminados. No se llegó a confirmar con exactitud el propósito de cada llamada a
`compress()` en `divc.cpp`/`divfrm.cpp` sin leer más contexto alrededor — se indica explícitamente
como pendiente de profundizar si hiciera falta más precisión.

---

## TopFLC (reproducción FLI/FLC)

**Qué es.** TopFLC library v1.0, de Johannes Lehtinen ("Snowman of Abacus"), 1996, freeware con
licencia restrictiva propia (no GPL): permite compilar/enlazar/modificar la librería siempre que el
producto resultante no sea comercial, se acredite "TopFLC library v1.0 by ABACUS" en la
documentación/ejecutable, y el autor del producto se registre (gratis) como usuario de TopFLC. Uso
comercial requeriría contactar al autor — esto es relevante porque DIV es GPLv3 pero esta
dependencia concreta trae su propia licencia, más restrictiva, sólo para su propio código (confirmado
leyendo `3rdparty/topflc/license.txt` y `topflc.txt`).

Es una librería pequeña, sin dependencias de pantalla/sistema, sólo para decodificar animaciones
FLI/FLC frame a frame (no las dibuja, sólo produce buffers de píxeles indexados + paleta).

**Cómo se integra.** `3rdparty/topflc.mif` genera `TFLC_LIB` = `3rdparty/lib/$(CPU)/tflc_w32.lib`
compilando `3rdparty/topflc` con `makefile.w32` (el makefile de 32-bit Watcom C que trae la propia
librería), y añade `-I3rdparty/topflc` al `INCLUDE`. El nombre `tflc_w32.lib` coincide literalmente
con el nombre de salida del `makefile.w32` original de TopFLC (frente a `tflc_bc.lib` para Borland o
`tflc_w16.lib` para Watcom de 16 bits, variantes no usadas por DIV).

**Dónde se usa realmente.** Un único punto de integración, en el intérprete/runtime:

- `src/div32run/divfli.cpp`: incluye `"topflc.h"` y usa toda la API pública de TopFLC —
  `TFErrorHandler_Set`, `TFAnimation_NewFile`, `TFAnimation_SetLooping`,
  `TFAnimation_SetPaletteFunction`, `TFAnimation_GetInfo`, `TFBuffers_Set`, `TFFrame_Decode`,
  `TFAnimation_Delete` — implementando las funciones `StartFLI`, `Nextframe`, `EndFli`, `ResetFli`
  que exponen la reproducción de FLI/FLC al lenguaje DIV (comando de animación FLI del lenguaje DIV,
  consumido por `div32run/f.cpp`/`kernel.cpp`, no verificado línea por línea en este pase). No se
  encontró uso de TopFLC dentro de `src/div` (el IDE), por lo que la reproducción de FLI/FLC parece
  ser una funcionalidad exclusiva del runtime de los juegos, no del propio editor.

---

## PMODE/W (extensor DOS de 32 bits)

**Qué es.** PMODE/W es un extensor de modo protegido DOS de 32 bits (DPMI host), alternativa a
DOS/4GW, ampliamente compatible con los binarios generados por Watcom C/C++ (documentado en
`pmwlite/src/docs/pmodew.faq`: "PMODE/W is compatible with nearly all Watcom C/C++ functions...").
La versión exacta usada está fijada en `pmwlite/src/pmwver.h`:

```c
#define PMODEW_MAJOR_VERSION 1
#define PMODEW_MINOR_VERSION 34
#define PMODEW_TEXT_VERSION "1.34"
```

Es decir, **PMODE/W 1.34**. Ventajas frente a DOS/4GW mencionadas en su propia FAQ: mucho menor
consumo de memoria/disco al no implementar extensiones que DOS/4GW sí trae, y soporte de
compresión del ejecutable final (algo que DOS/4GW no permite).

**Estructura de `pmwlite/`.**

- `pmwlite/pmodew.exe`: el propio extensor/stub que se "pega" a los ejecutables (binario ya
  compilado, se distribuye listo para usar).
- `pmwlite/pmwlite.exe` / `pmwlite/pmwbind.exe`: herramientas host precompiladas para "encolar" el
  extensor a un `.exe` LE de 32 bits ya enlazado — `pmwlite` comprime el resultado, `pmwbind` lo
  deja sin comprimir (ver más abajo).
- `pmwlite/pmwsetup.exe` / `pmwlite/pmwver.com`: utilidades de configuración/versión de PMODE/W.
- `pmwlite/src/`: código fuente completo de las herramientas, en C (Watcom) + ensamblador:
  - `src/pmodewe.asm`, `src/pmodewk.asm`: el propio extensor/kernel PMODE/W en ensamblador.
  - `src/pmwlite/pmwlite.c` (+ `encode.c`/`encode.h`, `pmw1.h`): la herramienta que ata el stub
    `pmodew.exe` al ejecutable final **y lo comprime** (formato propio "PMW1"; usa un motor de
    compresión propio, `ENCODE_PROBESMIN/FEW/NORM/MANY/MAX` como niveles de "profundidad de
    búsqueda" de coincidencias).
  - `src/pmwbind/pmwbind.c`: variante equivalente a `pmwlite.c` pero que **no comprime**, sólo
    enlaza el stub al ejecutable LE.
  - `src/pmwver/pmwver.asm`: utilidad que informa la versión de PMODE/W embebida en un ejecutable.
  - `src/pmwsetup/`: configurador (activa/desactiva mensajes de arranque, etc., mencionado en la
    FAQ: "You can disable the startup message using PMWSETUP").
  - `src/docs/`: documentación original de PMODE/W (`pmodew.doc`, `pmodew.faq`, `pmw1fmt.txt` con
    el formato del ejecutable comprimido PMW1, `updates.doc`, `utils.doc`).

**Cómo se compila y cómo se integra en el build de DIV (mecanismo real).** PMODE/W no se vincula
como una `.lib` más: es una **herramienta de host que se compila y ejecuta durante el propio proceso
de build**, en dos fases:

1. **Fase de enlazado (linker):** los makefiles de los componentes que deben correr bajo PMODE/W en
   vez de DOS/4GW pasan `system pmodew` (o `system dos4g` para la sesión de depuración) al linker de
   Watcom (`wlink`). Esto se ve en:
   - `src/div32run/makefile`: macro `SYSTEM = pmodew` cuando `SESSION=0` (build de la
     `DIV32RUN.DLL`/`.ins` "de producción", la que se distribuye con el instalador de un juego);
     cuando `SESSION=1` (el intérprete con trazador que usa el IDE para depurar) usa en cambio
     `SYSTEM = dos4g`.
   - `src/install/makefile`: siempre usa `system pmodew` para `install.ovl` (el instalador que DIV
     genera para los juegos terminados).
   - Los ficheros `.lnk` en `src/div32run/` (`586ins.lnk`, `386ins.lnk`: `system pmodew`; `586dbg.lnk`,
     `386dbg.lnk`: `#system pmodew` comentado, usan DOS/4G para depuración) confirman el mismo patrón
     a nivel de scripts de enlazado directos.
   - Ambos makefiles también enlazan `PMODE_LIB` (`$(SVGA_LIB) $(PMODE_LIB) ...`), que es la lib
     `pmode.lib` de SciTech mencionada arriba — es decir, el binario final depende a la vez del
     extensor PMODE/W (en tiempo de enlace/carga) y de las rutinas de modo protegido de SciTech (en
     tiempo de compilación/enlace de código propio), dos cosas distintas con nombre parecido.

2. **Fase de "bindeo"/compresión posterior al enlace:** tras generar el `.exe`/`.ins`/`.ovl` LE de
   32 bits, si `PACK=1` y `CONFIG=release`, el makefile ejecuta como *post-build step*:
   ```
   $(OUTDIR_BASE_SYS)/pmwlite/$(CONFIG)/pmwlite -C4 -S<ROOT>/pmwlite/pmodew.exe <ejecutable_generado>
   ```
   Es decir, invoca el **`pmwlite` recién compilado para la plataforma anfitriona** (no el `.exe`
   DOS precompilado que trae el repo) con `-C4` (nivel de compresión 4) y `-S` apuntando al stub
   `pmwlite/pmodew.exe`, para "atar" ese stub al binario y comprimirlo in-place. Esto explica por
   qué el `README.md` insiste en que hace falta instalar los compiladores de Watcom **también para
   la plataforma desde la que se compila** (Linux/Windows/DOS): `pmwlite` (la herramienta) se
   recompila como programa nativo del host antes de poder usarla para bindear los ejecutables DOS.
   El target correspondiente en ambos makefiles es:
   ```
   pmwlite: .SYMBOLIC
       cd <ROOT>/pmwlite/src/pmwlite
       $(MAKE)
       cd ...
   ```
   que compila `pmwlite/src/pmwlite/makefile` para producir el binario de host en
   `build.<SYS>/pmwlite/<CONFIG>/pmwlite`. Esto es justo lo que declara `PMW_DEPS = pmwlite` como
   dependencia del target de enlace cuando `PACK=1`.

**Dónde se usa realmente en DIV.** Confirmado por grep en todo el árbol:
- `src/div32run/makefile` (línea 39: `SYSTEM = pmodew`; línea 129: invocación de `pmwlite -C4 -S...`)
  y sus `.lnk` asociados — usado para `DIV32RUN.DLL`/`.ins` de producción, **no** para la sesión de
  depuración del IDE (que usa DOS/4GW normal, sin comprimir, probablemente para simplificar la
  depuración con símbolos).
- `src/install/makefile` (línea 59: `system pmodew`; línea 70: invocación de `pmwlite`) — usado para
  `install.ovl`, el instalador que el propio DIV genera para distribuir los juegos terminados.
- No se encontró uso de PMODE/W en `src/div/makefile` (el propio D.EXE/D.386 del IDE) ni rastro de
  `pmwlite`/`pmodew` en él — el IDE en sí parece enlazarse de otra forma (a confirmar revisando ese
  makefile con más detalle si hiciera falta; no se investigó a fondo en este pase porque no apareció
  en los resultados de búsqueda de `pmwbind|pmodew|pmwlite` sobre `src/div/makefile`).

En síntesis, PMODE/W cumple dos roles en DIV: (1) extensor DOS que carga y ejecuta el runtime del
lenguaje DIV y el instalador generado, como alternativa más ligera y comprimible a DOS/4GW; (2) su
propia toolchain (`pmwlite`) se compila como parte del build de DIV y se ejecuta automáticamente
como paso final para comprimir/bindear esos mismos ejecutables.

---

## Resumen de mapeo librería -> consumidor

| Librería | `.lib` generada | Usada en (código DIV) | Rol |
|---|---|---|---|
| JUDAS | `judas.lib` | `src/div/{divsound,divpcm,divbrow,divhandl,divtimer}.cpp`, `src/div32run/{divsound,divtimer}.cpp` | Motor de audio del IDE y del runtime |
| SciTech SuperVGA Kit | `svga.lib` + `pmode.lib` | `src/div/divvideo.cpp`, `src/div32run/v.cpp` (y referencias en `lfbprof.c`, `install`, `install2`) | Modos de vídeo VESA/SVGA |
| libjpeg | `libjpeg.lib` | `src/div/divforma.cpp` | Carga de imágenes JPEG en el IDE |
| zlib | `zlib_f.lib` | `src/div32run/f.cpp`, `src/div/{divc,divfrm}.cpp`, `src/install/i.cpp`, `src/install2/main.c` | Compresión de recursos/PAK e instaladores |
| TopFLC | `tflc_w32.lib` | `src/div32run/divfli.cpp` | Reproducción de FLI/FLC en el runtime |
| PMODE/W | (stub `pmodew.exe`, no `.lib`) | `src/div32run/makefile`, `src/install/makefile` (linker `system pmodew` + post-build `pmwlite`) | Extensor DOS 32-bit para `DIV32RUN` de producción e `install.ovl` |

---

## Cosas no verificadas / pendientes de profundizar

- El detalle exacto de qué codifican los opcodes 161/162 del lenguaje DIV (`_compress`/`_uncompress`
  en `src/div32run/f.cpp`) — se infiere que son los comandos `COMPRESS`/`UNCOMPRESS` del lenguaje,
  pero no se confirmó contra la documentación del lenguaje DIV.
- El uso concreto de `compress()` en `src/div/divc.cpp` y `src/div/divfrm.cpp` (para qué estructura
  de datos exactamente) no se rastreó más allá de confirmar la llamada.
- No se determinó cómo se enlaza `D.EXE`/`D.386` (el IDE) en cuanto a extensor DOS, ya que no
  aparece referencia a `pmodew`/`pmwlite` en `src/div/makefile`; podría usar DOS/4GW directamente o
  un mecanismo distinto — requeriría leer ese makefile completo.
- No se leyeron en detalle los ficheros de `3rdparty/scitech/doc/` (documentación original de
  SciTech) ni `3rdparty/scitech/src/util`/`src/ztimer` (utilidades auxiliares no referenciadas desde
  `src/`).
- `pmwlite/src/pmwsetup` y `pmwlite/src/pmwver` no tienen rastro de uso desde los makefiles de DIV
  (parecen herramientas auxiliares de PMODE/W no invocadas por el build, sólo distribuidas por si el
  desarrollador quiere usarlas manualmente).

---

## Archivos concretos leídos para este documento

- `3rdparty.mif`
- `3rdparty/judas.mif`, `3rdparty/scitech.mif`, `3rdparty/jpeglib.mif`, `3rdparty/topflc.mif`, `3rdparty/zlib.mif`
- `3rdparty/scitech/README.md`
- `3rdparty/scitech/include/svga.h` (cabecera)
- `3rdparty/topflc/license.txt`, `3rdparty/topflc/topflc.txt`
- `3rdparty/zlib/README`
- `3rdparty/jpeglib/README` (cabecera)
- `3rdparty/judas/judas.h` (fragmento)
- `pmwlite/src/docs/pmodew.faq` (fragmento)
- `pmwlite/src/pmwver.h`
- `pmwlite/src/pmwlite/pmwlite.c` (fragmento)
- `pmwlite/src/pmwbind/pmwbind.c` (fragmento)
- `src/div/divforma.cpp` (uso de libjpeg)
- `src/div/divsound.cpp`, `src/div32run/divsound.cpp` (uso de JUDAS)
- `src/div/divpcm.cpp`, `src/div/divbrow.cpp`, `src/div/divhandl.cpp`, `src/div/divtimer.cpp`, `src/div32run/divtimer.cpp` (uso de JUDAS)
- `src/div32run/divfli.cpp` (completo, uso de TopFLC)
- `src/install/i.cpp`, `src/install2/main.c`, `src/div32run/f.cpp`, `src/div/divc.cpp`, `src/div/divfrm.cpp` (uso de zlib)
- `src/div32run/v.cpp`, `src/div/divvideo.cpp` (uso de SciTech SVGA)
- `src/div32run/makefile` (completo)
- `src/install/makefile` (completo)
- `README.md` (raíz del repo)
- `docs/architecture/02-estructura-repositorio.md` (para estilo/consistencia)
- Búsquedas por patrón (`grep`) sobre todo `src/` y sobre el árbol completo para localizar
  `#include`, símbolos de API y referencias a `pmwbind`/`pmodew`/`pmwlite`.
