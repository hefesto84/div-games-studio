# Build system

## Panorama general

Todo el build de DIV se resuelve con **`wmake`** (el `make` de Watcom/OpenWatcom). No hay CMake, autotools, ni ningún generador de proyectos: el `makefile` de la raíz y una decena de submakefiles repartidos por `src/`, `dll/`, `tools/`, `3rdparty/` y `pmwlite/` son el build system completo. Es un diseño "recursivo": el makefile raíz no compila nada directamente, sino que hace `cd` a cada subdirectorio y relanza `wmake` con un `MAKE_OPTIONS` fijo (`CONFIG`, `ASM`, `ROOT`, y opcionalmente `TASM_EXE`/`PACK`).

Puntos clave que se detallan abajo:

- El **toolchain obligatorio es OpenWatcom 1.9** (no 2.x): produce ejecutables de 16 y 32 bits para DOS y también compila algunas herramientas nativas del host (Linux/Windows) usadas durante el propio build.
- El build produce dos árboles de salida paralelos: `build.dos/` (todo lo que termina siendo un binario DOS: `D.EXE`, `DIV32RUN`, las DLL de ejemplo, el instalador...) y `build.<sys>/` (herramientas que corren en la máquina que compila: `bin2h`, `testdll`, `pmwlite`, `unpak`, `wld`), donde `<sys>` es `win`, `lin` o `unx` según el host.
- La variable `SESSION` decide si `src/div32run/makefile` genera el intérprete de depuración embebido en el IDE (`session.div`/`session.386`) o el runtime final que se distribuye con los juegos (`div32run.ins`/`div32run.386`).
- El ejecutable final de 32 bits para DOS (`div32run.ins`, `install.ovl`) no lleva el extensor DOS/32 estándar de Watcom: en `CONFIG=release` y `PACK=1` se "empaqueta" (bind) con **PMODE/W** usando una herramienta propia, `pmwlite`, que se compila para el host como parte del propio build.

## Toolchain

- **Compilador/assembler**: OpenWatcom 1.9. El propio `README`/comentarios de los makefiles advierten que hace falta esta versión concreta (OW2 no es compatible con este árbol de fuentes).
- **`wcc`**: compilador C de 16 bits DOS, usado solo en `src/wstub` (el stub `.exe`).
- **`wcc386`**: compilador C/C++ de 32 bits, usado para prácticamente todo el resto (IDE, runtime, instalador, herramientas de host).
- **`wasm`**: ensamblador por defecto (`ASM=WASM`). Es la opción recomendada por el fork porque es libre; el ensamblador original de DIV era **Turbo Assembler** (`ASM=TASM`, requiere `tasm32.exe`), que sigue siendo necesario únicamente si se quieren **recompilar** las librerías de terceros (JUDAS en particular sólo sabe compilar con TASM, ver más abajo) — no para compilar el propio DIV, ya que las libs de terceros se distribuen precompiladas en `3rdparty/lib/`.
- **`wlink`**: linker, invocado explícitamente desde cada makefile (no hay `.lnk` de respuesta activos, ver la sección "Casos especiales").
- **Host toolchain**: además de los compiladores DOS (`-bt=dos`), OpenWatcom también compila binarios nativos del sistema donde se ejecuta `wmake` (Windows/Linux) para las herramientas usadas durante el propio proceso de build: `bin2h`, `pmwlite`, `testdll`, `unpak`, `wld`.

## `os.mif`: detección de host y de sistema destino

`os.mif` es el primer include del makefile raíz y de prácticamente todos los submakefiles. Define dos conceptos distintos:

- **`SYS`** = sistema operativo **host** (donde corre `wmake`), detectado por las macros predefinidas del compilador Watcom: `__UNIX__` → `unx`, `__LINUX__` → `lin`, `__MSDOS__` → `dos`, `__NT__` → `win`. También fija `%OS` (macro de entorno) al nombre largo correspondiente (`UNIX`/`LINUX`/`DOS`/`WINDOWS`).
- **`TARGET_SYS`** = sistema **destino** de la compilación; por defecto es igual a `SYS` (compilación nativa), pero puede sobreescribirse en la línea de comandos (usado por ejemplo en `tools/unpak` y `tools/wld`, que se compilan "para el host" pero permiten forzar otro target).

A partir de `TARGET_SYS`/`SYS` se derivan tablas de macros indexadas (`$(X_$(TARGET_SYS))`, un patrón repetido en todo el árbol):

- `WATCOM_BT_LIN/WIN/DOS` → argumento de `-bt=` (`linux`, `nt`, `dos`).
- `WLINK_TARGET_SYS_LIN/WIN/DOS` → cláusula `system ...` completa para `wlink` (`system linux option noextension`, `system nt`, `system dos4g`).
- `EXE_SUFFIX_LIN/WIN/DOS` → sufijo de ejecutable (`""`, `.exe`, `.exe`); además `EXE_SUFFIX_HOST` fija el sufijo del **host** independientemente del target.

`os.mif` también define, condicionado a Unix vs Windows: `COPY` (`cp` / `copy /Y`), `DELETE` (`rm` / `del`), `SEP` (`/` o `\`), y en Windows resuelve `%HOME` a partir de `%USERPROFILE`. Todo el resto de los makefiles usa `$(SEP)` en vez de rutas literales para ser portable entre host Windows y Linux/WSL — algo confirmado leyendo cualquier submakefile (`cd ..$(SEP)..`, etc).

`os.mif` usa un guard (`OS_MIF`) para no re-ejecutarse si ya fue incluido, como es habitual en los `.mif` de este árbol.

## Árbol de submakefiles

Makefiles reales encontrados en el repo (vía `**/makefile`):

```
makefile                                   # raíz — orquesta todo
os.mif                                     # detección de SO, sólo macros
common.mif                                 # targets savedir/loaddir (poco usados)
3rdparty.mif                               # agrega los .mif de cada lib de terceros

src/div/makefile                           # IDE: D.EXE (CPU=586) / D.386 (CPU=386)
src/div32run/makefile                      # runtime: session.* (SESSION=1) / div32run.* (SESSION=0)
src/wstub/makefile                         # wstub.exe, el stub de 16 bits embebido en D.EXE
src/div_stub/makefile                      # generador de div_stub.h (actualmente NO conectado al build)
src/install/makefile                       # install.ovl (instalador, versión activa)
src/install2/makefile                      # variante alternativa de install.ovl (NO invocada desde la raíz)
src/netlib/makefile                        # makefile suelto, no usa os.mif (ver gotchas)

dll/makefile                               # DLLs de ejemplo para desarrolladores de terceros (agua/hboy/ss1)

tools/bin2h/makefile                       # bin2h: convierte binarios a .h (usado por div_stub)
tools/testdll/makefile                     # testdll: harness de pruebas para las DLL de usuario
tools/unpak/makefile                       # unpak: extractor de paquetes .pak/.fpg etc.
tools/wld/makefile                         # wld2zon/wlddbg (NO está en el target "tools" de la raíz)

pmwlite/src/pmwlite/makefile               # pmwlite: empaquetador PMODE/W, herramienta de host

3rdparty/jpeglib/makefile                  # libjpeg (makefile propio de IJG, adaptado)
3rdparty/judas/makefile                    # JUDAS (audio), requiere TASM sí o sí
3rdparty/scitech/src/pmode/makefile        # PMODE/W (parte de SciTech)
3rdparty/scitech/src/svgakit/makefile      # SVGAKit (parte de SciTech)
3rdparty/scitech/src/ztimer/makefile       # ZTimer (parte de SciTech, no referenciado por 3rdparty.mif)
```

Los `.mif` (`os.mif`, `common.mif`, `3rdparty.mif`, `3rdparty/*.mif`) no son makefiles ejecutables por sí solos: son fragmentos `!include`-ados que sólo definen macros y/o targets reutilizables, con guards (`!ifndef ... !endif`) para evitar doble inclusión.

### Cómo se invoca cada submakefile desde la raíz

El makefile raíz no usa una lista genérica de subdirectorios: cada target de alto nivel (`d.exe`, `d.386`, `session.div`, `session.386`, `div32run.ins`, `div32run.386`, `dlls`, `install.ovl`, `bin2h`, `testdll`, `unpak`) es un target `.SYMBOLIC` que hace `cd <dir>` y relanza `wmake` con `$(MAKE_OPTIONS)` más las variables específicas de ese target (típicamente `CPU=` y `SESSION=`):

```
d.exe: .SYMBOLIC
	cd src\div
	$(MAKE) $(MAKE_OPTIONS) CPU=586 d.exe
	cd ..\..

session.div: .SYMBOLIC
	cd src\div32run
	$(MAKE) $(MAKE_OPTIONS) CPU=586 SESSION=1 session.div
	cd ..\..
```

`MAKE = $+$(MAKE) -h$-` reutiliza el propio `wmake` invocante (con `-h`, que hace que herede el entorno/handles en vez de relanzar un proceso "limpio"). `MAKE_OPTIONS` propaga `CONFIG`, `ASM`, `ROOT` (y `TASM_EXE`/`PACK` si aplica) a cada subllamada — es el mecanismo por el cual una única invocación `wmake CONFIG=debug` en la raíz se propaga a los ~10 submakefiles.

El target `all` es simplemente la unión de todos esos productos:

```
all: d.exe d.386 session.div session.386 div32run.ins div32run.386 dlls install.ovl .SYMBOLIC
```

Es decir, un build completo compila **seis** variantes distintas del runtime/IDE (2 CPU × [D.EXE, session, div32run]) más las DLL de ejemplo y el instalador, cada una en su propia carpeta de salida.

## Variables de build y flags

De línea de comandos / configurables en el makefile raíz:

| Variable | Valores | Efecto |
|---|---|---|
| `CONFIG` | `release` (default) / `debug` | Activa `-d2 -D_DEBUG` (debug) o `-oneatx/-oneatr -d0` (release, con optimizador "one-pass"). También decide si se hace `debug all` en `wlink` y si se activa el empaquetado PMODE/W. |
| `CPU` | `586` (default en la mayoría) / `386` | Selecciona `-5r -fp5` (Pentium, FPU obligatoria) vs `-3r -fpc` (386, FPU emulable). Determina también qué `.lib` de terceros se linkea (`3rdparty/lib/586` vs `.../386`) y el nombre del ejecutable resultante (`d.exe`/`d.386`, `session.div`/`session.386`, `div32run.ins`/`div32run.386`). |
| `ASM` | `WASM` (default) / `TASM` | Ensamblador para los `.asm` propios de DIV. `TASM` requiere fijar también `TASM_EXE`. |
| `TASM_EXE` | `tasm32.exe` (default, comentado `tasm.exe` como alternativa) | Ruta/nombre del ejecutable de Turbo Assembler cuando `ASM=TASM`. |
| `SESSION` | `1` / `0` | Sólo en `src/div32run/makefile`. `1` = intérprete con depurador embebido (`session.*`, `system dos4g`, añade `d.cpp`); `0` = runtime redistribuible (`div32run.*`, `system pmodew`, candidato a empaquetado con PMW). |
| `INSTALL_DIR` | ruta | Destino de `wmake install`/`update`. Por defecto `$(%HOME)\dosbox\DIV`, pensado para apuntar a una carpeta compartida con DOSBox. |
| `PACK` | `1` (default en `src/div32run` e `src/install`) / `0` | Si está a 1 (y `CONFIG=release`), tras linkear se invoca `pmwlite` para "bindear" el stub PMODE/W al ejecutable. |
| `TARGET_SYS` | `dos`/`win`/`lin`/`unx` | Sólo relevante en `os.mif`/herramientas de host (`unpak`, `wld`); por defecto igual al host. |
| `DOSBOX` | `dosbox-x` (+ sufijo de exe) | Emulador usado por los targets `test_*` para ejecutar binarios DOS 32-bit fuera de DOS real. |

Flags de compilación comunes que aparecen repetidos en los `wcc386` de `src/div` y `src/div32run` (los "core" del proyecto):

- `-bt=dos` — build target DOS.
- `-wx` — todos los warnings como advertencias máximas (nivel más estricto).
- `-mf` — modelo de memoria *flat* (32 bits).
- `-q` — modo silencioso (quiet).
- `-i=judas -i=netlib -i=vbe` — directorios de include adicionales relativos al *include path* de Watcom.
- CPU-dependientes: `-5r -fp5` (586) o `-3r -fpc` (386); en debug se agrega `-d2 -D_DEBUG`, en release `-oneatx`/`-oneatr -d0` (el optimizador "one-pass", variante `x` para ejecutables y `r` para el runtime).

`src/wstub/makefile` usa flags distintos porque compila **16 bits** DOS: `-bt=dos -wx -ms -q` (`-ms` = modelo de memoria *small*), reflejando que el stub es un programa DOS "clásico" muy pequeño.

## Librerías de terceros: `3rdparty.mif` y las macros `*_LIB`

`3rdparty.mif` centraliza el include de un `.mif` por cada librería externa (`jpeglib.mif`, `judas.mif`, `scitech.mif` —que cubre tanto SVGAKit como PMODE/W—, `topflc.mif`, `zlib.mif`), controlable individualmente con `NO_JPEG`, `NO_JUDAS`, `NO_SCITECH`, `NO_TOPFLC`, `NO_ZLIB` (usado por ejemplo en `tools/unpak/makefile`, que sólo necesita zlib y desactiva las demás con `!inject 1 NO_JPEG NO_JUDAS NO_SCITECH NO_TOPFLC`).

Cada `*.mif` de terceros sigue el mismo patrón, con dos modos de inclusión controlados por `INC_NOVARS` / `INC_NOTARGETS`:

- **Definición de macros** (bloque `!ifndef INC_NOVARS`): si `CPU` está definido, expone una macro `<LIB>_LIB` apuntando a `$(ROOT)/3rdparty/lib/$(CPU)/<archivo>.lib` — por ejemplo `JUDAS_LIB`, `JPEG_LIB` (→ `libjpeg.lib`), `ZLIB_LIB` (→ `zlib_f.lib`), `SVGA_LIB`/`PMODE_LIB` (ambas viven en `scitech.mif`, apuntando a `svga.lib`/`pmode.lib`), `TFLC_LIB` (→ `tflc_w32.lib`, TopFLC). También agrega su carpeta de include a `INCLUDE`.
- **Targets** (bloque `!ifndef INC_NOTARGETS`): una regla que sabe **recompilar** la librería desde su código fuente en `3rdparty/<nombre>/` si hiciera falta (`cd 3rdparty/judas && wmake judas.lib CPU=...`) y copiarla a `3rdparty/lib/$(CPU)/`, más un target `clean_<lib>` agregado a la lista acumulada `CLEAN_LIB3P_DEPS` (consumida por `clean_lib3p` en `3rdparty.mif`, y en última instancia por `libclean` en la raíz).

En la práctica, sin embargo, **las librerías vienen precompiladas**: existe `3rdparty/lib/386/` y `3rdparty/lib/586/`, cada una con `judas.lib`, `libjpeg.lib`, `pmode.lib`, `svga.lib`, `tflc_w32.lib`, `zlib_f.lib` ya generados. Esto es intencional: `judas/makefile` sólo sabe ensamblar `judasasm.asm` con **TASM** (si `ASM` no es `TASM` aborta con `@%abort`), así que un desarrollador sin licencia de Turbo Assembler no podría regenerar JUDAS — de ahí que el target `libclean` de la raíz imprima una advertencia explícita ("Esto eliminará el contenido de 3rdparty/lib... Tendrás que recompilar las librerías (necesitarás TASM)... No se recomienda!!") y haga una pausa (`%stop`) antes de borrar.

Las carpetas `3rdparty/lib/386` y `.../586` son la materialización física del switch de CPU: `src/div/makefile` y `src/div32run/makefile` seleccionan una u otra según `CPU=386|586` sin ningún `if` adicional, simplemente porque la macro `$(CPU)` forma parte de la ruta.

`3rdparty.mif` también expone el target `lib3p_dir` (crea `3rdparty/lib`, `.../386`, `.../586` si no existen) del que dependen todas las reglas de compilación de libs de terceros, y termina con `!include $(ROOT)/common.mif` (que sólo aporta los targets `savedir`/`loaddir`, aparentemente sin usar en el resto del árbol explorado).

Nota: `3rdparty/scitech/src/ztimer/makefile` existe en el árbol de fuentes de SciTech pero **no** está referenciado por `scitech.mif` — ZTimer no forma parte de los `*_LIB` consumidos por DIV.

## Proceso de build paso a paso (`wmake` / `wmake all`)

1. Se incluye `os.mif` → se fijan `SYS`, `TARGET_SYS`, `WATCOM_BT`, `WLINK_TARGET_SYS`, `EXE_SUFFIX`, `COPY`/`DELETE`/`SEP`.
2. Se fijan las variables configurables (`CONFIG`, `INSTALL_DIR`, `ASM`, `TASM_EXE`, `DOSBOX`), y se calculan `ROOT` (cwd absoluto), `OUTDIR_BASE` = `build.dos`, `OUTDIR_BASE_SYS` = `build.$(SYS)`.
3. El target `.BEFORE` imprime el host detectado y las rutas de salida, y crea `build.dos/` si no existe.
4. `all` dispara, en este orden de dependencias (wmake resuelve el grafo, pero el orden declarado es):
   - `d.exe` → `cd src/div; wmake CPU=586 d.exe` — compila el IDE de 32 bits "Pentium" y lo linkea con `system dos4g`, embebiendo `wstub.exe` como *DOS stub* (ver sección Linking).
   - `d.386` → igual pero `CPU=386`, produce `D.386` (variante 386 del IDE, sin visor de sprites 3D — ver `SOURCES` condicionales).
   - `session.div` → `cd src/div32run; wmake CPU=586 SESSION=1 session.div` — build del intérprete **con** depurador (incluye `d.cpp`), enlazado `system dos4g`.
   - `session.386` → igual, `CPU=386`.
   - `div32run.ins` → `cd src/div32run; wmake CPU=586 SESSION=0 div32run.ins` — build del runtime **redistribuible**, `system pmodew`, candidato a empaquetado PMW.
   - `div32run.386` → igual, `CPU=386`.
   - `dlls` → `cd dll; wmake` — compila las DLL de ejemplo (`agua.dll`, `hboy.dll`, `ss1.dll`) que documentan la API de extensión en C/C++ para desarrolladores de terceros.
   - `install.ovl` → `cd src/install; wmake install.ovl` — compila el generador de instaladores, un `.ovl` (overlay) en `system pmodew`, también candidato a empaquetado PMW.
5. `wmake tools` compila por separado `bin2h`, `testdll`, `unpak` (pero **no** `wld`, ver gotchas) — son herramientas usadas por el propio desarrollo/pruebas, no parte del producto final.
6. `wmake update` copia todo lo generado (más recursos estáticos: `genspr/`, `help/`, `install/`, `setup/`, `system/`, y subconjuntos filtrados de `resource/`) a `$(INSTALL_DIR)`, recreando el árbol de directorios esperado por DIV en tiempo de ejecución (`system/`, `dll/`, `install/`, etc.).
7. `wmake install` = `update` + limpieza de artefactos de sesión previos (`setup.bin`, `session.dtf`, `user.nfo`, `exec.*`, `sound.cfg`) para dejar una instalación "de fábrica".
8. `wmake test` = `test_dll` + `test_div32run_386` + `test_div32run_586`: compila y ejecuta pruebas automatizadas, usando **DOSBox-X** cuando el host no es DOS (ver `src/div32run/test.mif`).
9. `wmake clean` limpia recursivamente `src/div` (ambas CPU), `src/div32run` (ambas CPU × ambos `SESSION`), `src/install`, `tools/bin2h`, `tools/testdll`, `dll`, más `clean_lib3p` (limpieza opcional de libs de terceros) y `clean_tools`.

## `SESSION`: `session.*` vs `div32run.*`

`src/div32run/makefile` compila el **mismo código fuente base** (intérprete DIV32RUN: `i.cpp`, `f.cpp`, `c.cpp`, `s.cpp`, `v.cpp`, el motor VPE en `vpe/`, la capa de red en `netlib/`, etc.) en dos configuraciones mutuamente excluyentes, seleccionadas por `SESSION`:

- **`SESSION=1`** → añade `d.cpp` (el depurador) a `SOURCES`, define `-DDEBUG`, enlaza con `system dos4g` (extensor DOS estándar), sale como `session.div` (CPU 586) o `session.386` (CPU 386), en `build.dos/session/<config>.<cpu>/`. Este es el intérprete que el IDE (`D.EXE`) lanza internamente cuando el usuario pulsa "ejecutar" — de ahí el nombre "session": es una sesión de depuración dentro del propio entorno de autor.
- **`SESSION=0`** (default) → sin `d.cpp`, enlaza con `system pmodew` (el extensor ligero PMODE/W en vez de DOS/4G), sale como `div32run.ins` (586) o `div32run.386` (386), en `build.dos/div32run/<config>.<cpu>/`. Es el runtime que se empaqueta con los juegos terminados. Sólo en esta rama, y sólo si `CONFIG=release` y `PACK=1`, se invoca `pmwlite` tras el link (ver más abajo). El nombre `.ins` sugiere su rol: se copia a la carpeta `install/` del árbol instalado, para ser incorporado por el generador de instaladores al paquete final del juego.

Ambas variantes comparten el mismo `.BEFORE`, las mismas reglas de compilación `.c.obj`/`.cpp.obj`/`.asm.obj`, y el mismo bloque final de `3rdparty.mif` — la única diferencia real de contenido de `SOURCES` es `d.cpp`.

## `wstub.exe` y `D.EXE`

`src/wstub/makefile` compila un ejecutable DOS de **16 bits** (`-bt=dos -wx -ms -q`, modelo *small*) a partir de `wstub.c` y `cpuid.asm`. Este binario (`build.dos/wstub/<config>/wstub.exe`) actúa como **DOS stub**: el fragmento de código de 16 bits que un ejecutable protegido (`system dos4g`) ejecuta primero bajo DOS real antes de cargar el extensor de 32 bits — normalmente detecta el hardware disponible (de ahí `cpuid.asm`) y arranca DOS/4GW.

`src/div/makefile` lo integra así: la macro `STUB` apunta a `build.dos/wstub/<config>/wstub.exe`. Si `CPU=586`, el bloque `!ifndef STUB ... option nostub ... !else ... option stub=$(STUB) ... !endif` decide si `wlink` recibe `option stub=$(STUB)` (siempre que `STUB` esté definido, cosa que sucede por defecto) o `option nostub`. El target `$(EXE)` de `D.EXE` depende explícitamente de `wstub` (`.SYMBOLIC`), cuyo cuerpo hace `cd ../wstub && wmake ...` **sólo si `STUB` está definido** — construyendo así el stub como dependencia previa al link de `D.EXE`. Para `CPU=386` (`D.386`) el makefile fuerza siempre `OP_STUB = option nostub`: **`D.386` no lleva stub personalizado**, sólo `D.EXE` (la variante 586/Pentium) lo usa.

## `install.ovl` y el empaquetado del instalador

`src/install/makefile` compila el generador de instaladores (`i.cpp`, `divkeybo.cpp`, más `.asm`) como `install.ovl`, enlazado `system pmodew`. El nombre `.ovl` (overlay) y el hecho de que dependa de `PMW_DEPS`/pueda pasar por `pmwlite` sugieren que es un módulo cargado dinámicamente por el ejecutable final del instalador de cada juego, más que un `.exe` independiente — pero **esto no pudo confirmarse leyendo sólo los makefiles**; haría falta revisar el código fuente de `src/install` o `src/setup`/`system/` para saber quién carga ese `.ovl` en tiempo de ejecución.

El target `test` de `src/install/makefile` (y de `install2`) muestra cómo se arma un ejecutable de prueba a partir del overlay: concatena en binario el `.ovl` con un stub de datos, usando

```
copy /b $(OUTDIR)\install.ovl + test\install.dat $(OUTDIR)\test\install.exe
```

en Windows, o `cat $(OUTDIR)/install.ovl test/install.dat > $(OUTDIR)/test/install.exe` en Unix — es decir, un `.exe` "cosido a mano" concatenando dos binarios, técnica típica de instaladores DOS de la época (el `.ovl` como código, `install.dat` como los datos/payload empaquetados del instalador de prueba). Es razonable pensar que el instalador real de un juego terminado se arma con la misma técnica (overlay + datos del juego), pero de nuevo esto excede lo que documentan los makefiles y no se verificó en el código fuente.

`src/install2/makefile` es una **variante casi idéntica** de `src/install/makefile` (mismo esquema de `wlink`/PMW, misma lógica de `test`) pero con un conjunto de fuentes distinto (`main.c video.c vesa.asm fnt.c fpg.c mouse.c time.c keyboard.c fases.c timer.asm rect.c`, sin `divkeybo.cpp` ni dependencia de `../div`) y **no está enlazada al makefile raíz** — ningún target de `makefile` hace `cd src/install2`. Es, aparentemente, una implementación alternativa/anterior del instalador que sólo se compila manualmente.

## `pmwlite` y el empaquetado `PACK=1`

`pmwlite/src/pmwlite/makefile` compila `pmwlite` (`pmwlite.c` + `encode.c`) como herramienta del **host**: usa `OUTDIR_BASE = $(ROOT)/build.$(SYS)` (no `build.dos`) y no depende de `CPU`. En DOS (`__MSDOS__`) se linkea con `system pmodew` (bootstrap: el propio pmwlite corriendo bajo PMODE/W), en cualquier otro host no se fija `system` explícito (se linkea nativo para ese host).

`pmwlite` es una reimplementación/herramienta compatible con el "binder" oficial de PMODE/W (SciTech): según su propio `usage()` acepta `-Cx` (nivel de compresión 0–4) y `-S<archivo>` (stub alternativo a `PMODEW.EXE`). Tanto `src/div32run/makefile` como `src/install/makefile` la invocan igual, **después** del link de `wlink` y sólo bajo esta condición triple:

```
!ifeqi CONFIG release
!ifeq SESSION 0        # (o no aplica, en src/install)
!ifeq PACK 1
	$(OUTDIR_BASE_SYS)/pmwlite/$(CONFIG)/pmwlite -C4 -S$(ROOT)/pmwlite/pmodew.exe $^@
!endif
!endif
!endif
```

Es decir: `-C4` (compresión máxima) y `-S$(ROOT)/pmwlite/pmodew.exe` (usa el `pmodew.exe` versionado en el repo, en `pmwlite/`, como stub PMODE/W) se "cosen" al final del ejecutable recién linkeado (`$^@`), reemplazándolo en el sitio. El resultado es un único `.exe`/`.ins`/`.ovl` autocontenido que no depende de tener `PMODEW.EXE` como archivo externo junto al ejecutable — la razón de ser de PACK=1 en el pipeline release.

Ambos makefiles que usan `pmwlite` declaran un target `pmwlite: .SYMBOLIC` que hace `cd $(ROOT)/pmwlite/src/pmwlite && wmake` como dependencia de compilación (`PMW_DEPS = pmwlite` sólo si `PACK=1` y `CONFIG=release`) — así que la primera vez que se hace un build release con empaquetado, `wmake` construye primero la herramienta `pmwlite` para el host antes de poder terminar de linkear `div32run.ins`/`install.ovl`.

En `CONFIG=debug`, o con `PACK=0`, este paso se omite por completo y el ejecutable queda "crudo" (requiere `PMODEW.EXE` al lado, o se ejecuta bajo DOS/4G en el caso de `session.*`).

## Linking: `wlink` embebido vs. los `.lnk` heredados

**Importante**: pese a que hay varios ficheros `.lnk` en el repo (`c.lnk`, `cdbg.lnk` en la raíz; `src/div32run/i.lnk`, `386ins.lnk`, `386dbg.lnk`, `586ins.lnk`, `586dbg.lnk`; `src/div32run/wstub/wstub.lnk`; `src/div_stub/*.lnk`; `dll/div_dll.lnk`; `src/netlib/div_dll.lnk`; `pmwlite/pmodew.lnk`), **ninguno de los makefiles activos del árbol de build actual los invoca con `@archivo.lnk`**. Se comprobó con una búsqueda de texto completa: sólo aparecen mencionados en la documentación (`ARCHITECTURE.md`), no en ningún `makefile`.

Los `wlink` reales que ejecuta el build están **embebidos directamente en cada makefile**, como comandos multilínea con continuación `&`. Por ejemplo, el de `D.EXE` (`src/div/makefile`):

```
*wlink &
	option quiet &
	system dos4g &
	name $^@ &
	debug all &          # sólo si CONFIG=debug
	option stub=$(STUB) &  # o "option nostub"
	option map=$^* &
	path $(OUTDIR) &
	file { $(OBJS) } &
	libfile { $(LIBS) }
```

Comparando `c.lnk`/`cdbg.lnk` con este patrón, sus contenidos (`system div4g`, `name divc.exe`, `file divc.obj,cdll1.obj,cdll2.obj`, `lib judas\judas.lib,source\zlib.lib`) no corresponden a ningún target del build actual: `divc.exe`/`cdll1`/`cdll2` no existen como productos en ningún makefile leído, y `system div4g` no es una de las cláusulas generadas por `os.mif`. Todo indica que **son artefactos heredados** de un esquema de build anterior (probablemente basado en scripts `.bat` + `wlink @archivo.lnk`, el estilo típico de proyectos Watcom de los 90) que quedaron en el repo sin usarse tras la migración a `wmake`. Lo mismo aplica a los `.lnk` de `src/div32run/` (`i.lnk`, `586ins.lnk`, etc.): su contenido es reconocible como la versión "manual" de lo que hoy generan `src/div32run/makefile` (mismos `.obj`, mismas libs), pero con nombres de sistema (`int4g`) y organización de módulos ya reemplazados por el `wlink` inline (`system dos4g`/`system pmodew`). Estos ficheros no se pudieron atribuir a un target activo; se documentan aquí como legado, no como parte del pipeline vigente.

Diferencias reales **release vs debug** en el `wlink` inline (constantes en todos los makefiles que compilan para DOS):

- `debug all` se agrega a la línea de `wlink` **sólo** si `CONFIG=debug` (en todos: `src/div`, `src/div32run`, `src/wstub`, `src/install`, `src/install2`, `pmwlite`).
- `src/wstub/makefile` además añade `option symfile=$^*` en debug (archivo de símbolos separado), algo que no hacen los demás makefiles.
- El resto de la línea de `wlink` (target `system`, stub, mapa, paths, objetos, libs) es **idéntica** en ambas configuraciones; el cambio real de "debug-ness" ocurre sobre todo en el paso de compilación (`-d2 -D_DEBUG` vs `-oneatx/-oneatr -d0`), no en el link.
- El empaquetado PMW (`pmwlite -C4 -S...`) sólo corre en `CONFIG=release` — en debug los ejecutables 32-bit quedan sin empaquetar (más fáciles de depurar/tracear con un depurador externo, ya que no están comprimidos/reubicados por pmwlite).

## Herramientas (`tools/`)

- **`bin2h`** — convierte un binario en un `.h` de C (usado por `src/div_stub` para embeber `div_stub.exe` como array de bytes; también podría usarse para otros recursos, no verificado más allá).
- **`testdll`** — ejecutable de host que carga/valida las DLL de ejemplo (`dll/*.dll`), invocado por `dll/makefile` en su target `test`.
- **`unpak`** — extractor de los formatos de paquete de DIV (usa sólo `ZLIB_LIB` de terceros, desactivando expresamente JPEG/JUDAS/SciTech/TopFLC vía `!inject 1 NO_JPEG NO_JUDAS NO_SCITECH NO_TOPFLC`).
- **`wld`** (`wld2zon`, `wlddbg`) — herramientas relacionadas con el formato `.wld` (mundos/mapas 3D del "Mapworld"/VPE); tiene su propio makefile completo (con bloques de `3rdparty.mif` comentados, como si en algún momento hubiera dependido de zlib y ya no), pero **el target `tools` del makefile raíz no lo incluye** (`tools: bin2h testdll unpak`) — hay que compilarlo manualmente con `cd tools/wld && wmake`. Coincide con el commit reciente del historial (`0b859df makefile para wld tools`), que aparentemente añadió el makefile sin cablearlo al target agregador `tools`.

Todas las herramientas de `tools/` compilan hacia `build.$(TARGET_SYS)/<tool>/`, no hacia `build.dos/`, reforzando que son binarios de host, no productos DOS.

## `src/div_stub` y `src/netlib`: makefiles presentes pero no integrados / atípicos

- **`src/div_stub/makefile`** genera `div_stub.exe` (un stub en ensamblador puro, `asm/exec.asm`) y luego lo convierte a `div_stub.h` con `bin2h`. Su propio comentario inicial explica por qué no se usa activamente: *"así se compilaría el div_stub pero de momento no podemos hacerlo ya que si cambia su tamaño en un solo byte se rompe la compatibilidad con la DIV32RUN.DLL de DIV2"*. Es decir, el `div_stub` real usado en producción es un binario congelado (probablemente embebido directamente en el código fuente de `src/div32run` u otro lugar como bytes literales), y este makefile documenta cómo se generaría si se decidiera romper esa compatibilidad binaria. Coherente con que `src/div/makefile` tenga la línea `DIV_STUB_DIR` comentada con la nota `# No usar de momento!`.
- **`src/netlib/makefile`** es distinto a todos los demás: no incluye `os.mif`, no usa `$(SEP)`/`$(COPY)` ni ninguna macro del resto del árbol, y llama a `wcc386` directamente con flags fijos (`-zp1 -5r -mf`) sin pasar por `OPTIONS`. Da la impresión de ser un makefile independiente/histórico para compilar la librería de red de forma aislada; el código de `netlib/` que realmente se compila en el build normal se integra como fuente directa dentro de `src/div32run/makefile` (`SOURCES += netlib/red.cpp netlib/net.c ...`), no a través de este makefile suelto.

## CI: `.travis.yml`

El pipeline de Travis CI (`language: minimal`, `dist: focal`) define una matriz de **dos jobs**, uno por SO:

- **`os: windows`** — variables de entorno apuntando a un OpenWatcom instalado en `$HOME/watcom`: `PATH` incluye `$WATCOM/binnt:$WATCOM/binw`, `INCLUDE=$WATCOM/h`, `EDPATH=$WATCOM/eddat`, `WIPFC=$WATCOM/wipfc`, `LIB` con `lib286`, `lib286/dos`, `lib386`, `lib386/dos`.
- **`os: linux`** — mismas variables salvo `PATH`, que usa `$WATCOM/binl:$WATCOM/binw` (binarios Linux de Watcom en vez de NT).

Pasos:

1. `before_install`: ejecuta `.travis/install_watcom.sh` (script no incluido en el árbol leído — **no se encontró en el repo actual**; puede haberse perdido o vivir en un branch/versión de Travis distinta; no se puede documentar su contenido sin ese archivo).
2. `script`:
   - `wmake` — build completo por defecto (`CONFIG=release` implícito, target `all`).
   - `wmake tools` — compila `bin2h`, `testdll`, `unpak`.
   - `wmake test_dll` — compila y ejecuta las pruebas de las DLL de ejemplo (`dll/makefile: test`), que a su vez depende de `testdll`.
3. `cache: directories: ~/watcom` — cachea la instalación de OpenWatcom entre builds de CI para no tener que reinstalarlo en cada corrida.

Nótese que el CI **no** ejecuta `wmake test_div32run_386`/`_586` (que sí están definidos en el makefile raíz y requieren DOSBox-X) ni `wmake install`/`update` — el pipeline de Travis valida sólo que el build compile y que las DLL de ejemplo carguen correctamente, no un ciclo completo de instalación ni las pruebas del runtime completo.

## Vagrant

`Vagrantfile` (Vagrant config version 2) define una VM basada en `ubuntu/bionic64`, pensada como entorno reproducible de compilación en Linux:

- Comparte la raíz del repo con el guest en `/home/vagrant/DIV` (`synced_folder`).
- Si la variable de entorno `INSTALL_DIR` está definida en el host, también comparte esa carpeta como `/home/vagrant/install` — pensado para que `wmake install INSTALL_DIR=...` deposite el build directamente en una carpeta visible desde fuera de la VM (p. ej. compartida con una instalación de DOSBox en el host).
- El aprovisionamiento delega en `vagrant/provision.sh` (`config.vm.provision "shell", path: "vagrant/provision.sh"`).

`vagrant/provision.sh` automatiza, dentro de la VM:

1. `apt update && apt upgrade`, instala `dosbox` y `unzip`.
2. Crea `/usr/bin/watcom` (con permisos para el usuario `vagrant`).
3. Descarga **OpenWatcom 1.9** (build Linux) desde SourceForge y lo descomprime ahí — hay un bloque comentado equivalente para OpenWatcom 2.0 (`open-watcom-v2` en GitHub), explícitamente deshabilitado, reforzando que **1.9 es la versión soportada**.
4. Genera `/usr/bin/watcom/owsetenv.sh`, que exporta `PATH` (incluye `binl`/`binw`), `INCLUDE`, `WATCOM`, `EDPATH`, `WIPFC` — el mismo conjunto de variables que usa `.travis.yml` para Linux.
5. Da permisos de ejecución a los binarios (`chmod +x .../binl/*`), con una línea comentada para `binl64` indicando que sólo aplicaría a OW2.
6. Añade `source .../owsetenv.sh` a `.bashrc` si no está ya (idempotente, vía `fgrep -q`).
7. Define y exporta `INSTALL_DIR=$HOME/install` (también añadido a `.bashrc`), y crea esa carpeta si no existe.

En conjunto, Vagrant + `provision.sh` son la forma "documentada como código" de reproducir exactamente el mismo entorno que usa el job Linux de Travis, pero de forma persistente/interactiva en una VM local — útil para desarrollar sin tener que instalar Watcom directamente en la máquina del desarrollador.

## Casos especiales / gotchas

- **`tools/wld` no está en el target `tools`** de la raíz: hay que compilarlo aparte (`cd tools/wld && wmake`).
- **`src/install2` no está conectado** al makefile raíz: es una variante alternativa del instalador que sólo se puede compilar manualmente entrando a su directorio.
- **`src/div_stub` está deliberadamente desconectado**: generar el stub de nuevo rompería la compatibilidad binaria con `DIV32RUN.DLL` de DIV2, según el comentario del propio makefile; el include correspondiente en `src/div/makefile` está comentado con la nota "No usar de momento!".
- **`c.lnk`, `cdbg.lnk`, y varios `.lnk` en `src/div32run/` y `src/div_stub/` son legado**: ningún makefile activo los referencia; su contenido (`divc.exe`, `system div4g`, `system int4g`) corresponde a un esquema de build previo al `wmake` actual. No editarlos esperando que afecten al build real.
- **`src/netlib/makefile` es atípico**: no sigue las convenciones del resto (no incluye `os.mif`, hardcodea flags), y el código de `netlib/` que sí se compila en el build normal entra como fuente directa de `src/div32run/makefile`, no a través de este makefile.
- **JUDAS sólo se puede recompilar con TASM** (`3rdparty/judas/makefile` aborta explícitamente si `ASM` no es `TASM`); por eso el árbol trae `3rdparty/lib/{386,586}/*.lib` precompilados y `wmake libclean` advierte fuertemente antes de borrarlos.
- **El empaquetado PMW (`PACK=1`) sólo ocurre en `CONFIG=release`**, y sólo para `div32run.ins`/`div32run.386` (SESSION=0, no para `session.*`) y para `install.ovl`; `D.EXE`/`D.386` nunca pasan por `pmwlite` (usan `wstub`/`nostub` en su lugar, no PMODE/W).
- **`D.386` no lleva stub personalizado** (`option nostub` forzado); sólo `D.EXE` (CPU=586) usa `wstub.exe` vía `option stub=`.
- **El script `.travis/install_watcom.sh` referenciado por `.travis.yml` no existe en el árbol actual del repo** — no se pudo determinar cómo instala Watcom exactamente en CI sin ese archivo.
- **La semántica exacta de `install.ovl`** (quién lo carga, cómo se combina con los datos del juego para producir el instalador final distribuible) no puede confirmarse sólo con los makefiles; el target `test` sugiere una concatenación binaria `overlay + datos`, pero el mecanismo de producción real (fuera de pruebas) requeriría leer el código fuente de `src/install`/`system`/`setup`.
- **`libfile` vs `file` en las líneas de `wlink`**: todos los ejecutables 32-bit del proyecto (D.EXE, session.*, div32run.*, install.ovl) enlazan sus objetos propios con `file { $(OBJS) }` y las librerías de terceros con `libfile { $(LIBS) }` — separación que vale la pena mantener si se agregan nuevas librerías de terceros.

## Archivos leídos

- `makefile`
- `os.mif`
- `common.mif`
- `3rdparty.mif`
- `c.lnk`
- `cdbg.lnk`
- `.travis.yml`
- `Vagrantfile`
- `vagrant/provision.sh`
- `src/div/makefile`
- `src/div32run/makefile`
- `src/div32run/test.mif`
- `src/div32run/i.lnk`
- `src/div32run/586ins.lnk`
- `src/div32run/wstub/wstub.lnk`
- `src/wstub/makefile`
- `src/install/makefile`
- `src/install2/makefile`
- `src/div_stub/makefile`
- `src/netlib/makefile`
- `dll/makefile`
- `pmwlite/src/pmwlite/makefile`
- `pmwlite/src/pmwlite/pmwlite.c` (sólo la función `usage()`, para confirmar el significado de `-C`/`-S`)
- `tools/bin2h/makefile`
- `tools/testdll/makefile`
- `tools/unpak/makefile`
- `tools/wld/makefile`
- `3rdparty/judas.mif`
- `3rdparty/scitech.mif`
- `3rdparty/zlib.mif`
- `3rdparty/jpeglib.mif`
- `3rdparty/topflc.mif`
- `3rdparty/judas/makefile`
- `3rdparty/jpeglib/makefile`
- `docs/architecture/01-vision-general.md` (sólo como referencia de estilo/formato)
