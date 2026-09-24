# Arquitectura de DIV Games Studio 2

Documento de revisión del código fuente original de **DIV Games Studio 2** (Hammer Technologies, 1998–1999), restaurado y mantenido en este repositorio (fork de [DIVGAMES/DIV-Games-Studio](https://github.com/DIVGAMES/DIV-Games-Studio), mantenido en [vii1/DIV](https://github.com/vii1/DIV)).

Este archivo es un **índice condensado**. Cada sección tiene un documento hermano en [`docs/architecture/`](docs/architecture/) con la investigación completa: lectura real del código fuente (no solo listados de archivos), estructuras de datos, flujos de control, referencias cruzadas a símbolos concretos y, donde no se pudo confirmar algo sin más esfuerzo, una nota explícita en vez de una suposición.

| # | Sección | Documento detallado |
|---|---|---|
| 1 | Visión general | [docs/architecture/01-vision-general.md](docs/architecture/01-vision-general.md) |
| 2 | Estructura del repositorio | [docs/architecture/02-estructura-repositorio.md](docs/architecture/02-estructura-repositorio.md) |
| 3 | Componentes principales | [docs/architecture/03-componentes-principales.md](docs/architecture/03-componentes-principales.md) |
| 4 | Build system | [docs/architecture/04-build-system.md](docs/architecture/04-build-system.md) |
| 5 | Dependencias de terceros | [docs/architecture/05-dependencias-terceros.md](docs/architecture/05-dependencias-terceros.md) |
| 6 | Formatos de archivo propietarios | [docs/architecture/06-formatos-archivo.md](docs/architecture/06-formatos-archivo.md) |
| 7 | Modelo de ejecución (pipeline) | [docs/architecture/07-modelo-ejecucion.md](docs/architecture/07-modelo-ejecucion.md) |
| 8 | Aspectos técnicos / desafíos de portabilidad | [docs/architecture/08-aspectos-tecnicos.md](docs/architecture/08-aspectos-tecnicos.md) |
| 9 | Estado del árbol (fotografía puntual) | [docs/architecture/09-estado-arbol.md](docs/architecture/09-estado-arbol.md) |
| 10 | Hoja de ruta | [docs/architecture/10-hoja-de-ruta.md](docs/architecture/10-hoja-de-ruta.md) |
| 11 | Evaluación de port a Windows 11 nativo + fork de MikeDX | [docs/architecture/11-port-windows11-mikedx.md](docs/architecture/11-port-windows11-mikedx.md) |
| 12 | Progreso del port (bitácora técnica, checkpoints) | [docs/architecture/12-port-progreso.md](docs/architecture/12-port-progreso.md) |
| 13 | **Handoff — cómo continuar el port** (estado, conocimiento técnico condensado, backlog) | [docs/architecture/13-handoff.md](docs/architecture/13-handoff.md) |

---

## 1. Visión general

DIV Games Studio 2 es un **entorno completo de desarrollo de videojuegos** para MS-DOS que integra editor gráfico, editor de sprites, editor de código, compilador del lenguaje propio "DIV", depurador, reproductor/intérprete y sistema de ayuda. Publicado en 1999, este repo restaura el código comercial original bajo licencia **GPL v3**.

Roadmap: **v2.01** (reproducción fiel del DIV 2 de 1999) → **v2.02** (arreglo de bugs conocidos y pulido) → **después** (mejoras, reorganización, documentación). Objetivo de plataforma: **MS-DOS** (486+/16 MB RAM/SVGA, sobre DOSBox/DOSBox-X o DOS real). Los ports modernos nativos viven en otros repos (MikeDX/DIV-Games-Studio).

Un punto de diseño central que atraviesa todo lo demás: DIV separa el **entorno de autor** (el IDE, `D.EXE`) del **entorno de ejecución** (`DIV32RUN`, la VM de bytecode) — lo que se distribuye a los jugadores es un ejecutable autocontenido sin depurador ni herramientas de autor. → Detalle: [01](docs/architecture/01-vision-general.md).

## 2. Estructura del repositorio

```
DIV/
├── makefile, os.mif, common.mif, 3rdparty.mif, c.lnk/cdbg.lnk, .clang-format, .travis.yml, Vagrantfile
├── src/                # Código fuente (C/C++ + ASM x86): div/, div32run/, install/, install2/,
│                        # wstub/, div_stub/ (deshabilitado), netlib/, vpe/, *.c/*.h/*.asm compartidos
├── dll/                # SDK de DLL para el lenguaje DIV + ejemplos
├── formats/             # Especs de formatos de archivo (Kaitai .ksy)
├── 3rdparty/            # Librerías de terceros (código + .lib precompiladas 386/586)
├── pmwlite/             # Extensor 32-bit PMODE/W (DOS) + pmodew.exe
├── genspr/, help/, install/, resource/, setup/, system/   # Datos runtime/instalador
├── docs/                # Documentación (grafo de dependencias, esta carpeta architecture/)
└── tools/               # bin2h, testdll, unpak, wld
```

Nota: `netlib/` y `vpe/` existen tanto en `src/` (raíz) como duplicados dentro de `src/div32run/` — no confundir ambas copias al editar. `src/div_stub/` está deshabilitado a propósito (ver §3). → Detalle y notas: [02](docs/architecture/02-estructura-repositorio.md).

## 3. Componentes principales

El sistema se compone de varios binarios que colaboran en el pipeline de desarrollo:

- **El IDE** (`src/div` → `D.EXE`/`D.386`): el "Sistema Operativo DIV™" — editor, compilador (`divc.cpp`), pintura, GUI, sprites/FPG, audio, input, ayuda, generador de instalaciones. Subcarpeta `visor/` genera sprites 3D (build Pentium).
- **El intérprete** (`src/div32run` → DIV32RUN): la VM del lenguaje DIV. `kernel.cpp` no es un archivo compilable por sí solo: son los `case` de un `switch`, incluidos dos veces dentro de `i.cpp` (ejecución normal y modo *trace* del depurador). Las funciones runtime del lenguaje (gráficos, sonido, etc.) viven en un **segundo** switch gigante, `function()` en `f.cpp`, al que se llega vía el opcode `lfun`. La memoria de la VM es un único array `mem[]` sin segmentación (código + datos + estado de procesos), con ~127 opcodes duplicados a mano (mismos valores numéricos) entre `divc.cpp` (compilador) e `inter.h` (intérprete), sin cabecera compartida — riesgo real de desincronización.
- **El stub tiene en realidad dos roles distintos** (el `ARCHITECTURE.md` original los mezclaba): `src/wstub/wstub.c` es un supervisor 16-bit activo que relanza `D.EXE` interpretando el código de salida como comando; el stub *embebido* en los juegos compilados es un binario de 602 bytes ya compilado, congelado, escrito literalmente al principio de cada EXE (`div_stub.h`, generado en su día desde `src/div_stub/asm` vía `bin2h`) — confirmado byte a byte contra `formats/game_exe.ksy`. `src/div_stub/` está deshabilitado porque tocarlo rompe la compatibilidad binaria con `DIV32RUN.DLL` de DIV2.
- **El instalador** (`src/install` → `install.ovl`, + `install2/` alternativa en C puro).
- **Las DLLs** (`dll/`): SDK de plugins. El loader PE propio (`pe_load.c`/`divdll.c`) es una integración de **MikDLL** (MikMak/HaRDCoDE, 1995), no desarrollo original de Hammer Technologies; parsea cabeceras/secciones/relocations a mano pero **no procesa la tabla de importación PE** — el "enlace dinámico" real de DIV es un mecanismo propio y paralelo (`DIV_export`/`DIV_import` vía `divmain`/`divlibrary`/`divend`).
- **VPE** (`src/vpe`): "Virtual Presence Engine" modificado, Modo-8 (3D estilo *Comanche*).
- **Red** (`src/netlib`): rutinas IPX.
- **Herramientas** (`tools/`): `bin2h`, `testdll`, `unpak`, `wld` (`wld2zon`/`wlddbg`).

→ Detalle completo (estructuras de datos, opcodes, riesgos, símbolos concretos): [03](docs/architecture/03-componentes-principales.md).

## 4. Build system

Sin CMake/autotools: todo es **OpenWatcom `wmake`** (OW 1.9 obligatorio, incompatible con OW2), con targets DOS16/DOS32/host. La macro `SESSION` (`-DDEBUG`) determina la variante: `session.div`/`.386` (debug, extensor `dos4g`, con depurador `d.cpp`) vs `div32run.ins`/`.386` (release, empaquetado con `pmwlite` sobre el extensor `pmodew`).

Hallazgos de la investigación: los `c.lnk`/`cdbg.lnk` de la raíz son **legado muerto** (ningún makefile activo los invoca; el linking real está embebido inline en cada makefile). `pmwlite` se compila como herramienta de host y se usa como paso post-enlace (`pmwlite -C4 -S<pmodew.exe> <exe>`) para bindear/comprimir `DIV32RUN.DLL`/`.ins` e `install.ovl` — pero **no** se usa para `D.EXE`/`D.386` (que usan `wstub.exe`). Hay tres módulos huérfanos del árbol activo de build: `tools/wld`, `src/install2` y `src/div_stub` (este último deshabilitado a propósito).

→ Detalle (toolchain, árbol completo de submakefiles, variables, CI, Vagrant, gotchas): [04](docs/architecture/04-build-system.md).

## 5. Dependencias de terceros (`3rdparty/`)

| Librería | Uso | Integración real |
|---|---|---|
| **JUDAS** | Sonido (mixing, Sound Blaster, timers) | API `judas_*` en minúscula, usada en `divsound.cpp`, `divpcm.cpp`, `divbrow.cpp`, `divtimer.cpp` (IDE y runtime) |
| **SuperVGA Kit (SciTech)** | Acceso a modos VESA/SVGA | Genera `svga.lib` **y** un `pmode.lib` interno del propio SVGA Kit — **distinto** de PMODE/W (`pmwlite/`), aunque ambos coexisten enlazados en los mismos binarios |
| **libjpeg (IJG)** | Imágenes JPEG (v9c) | Único punto de integración: `src/div/divforma.cpp` (solo IDE) |
| **zlib** (1.2.11) | Compresión | API simple `compress()`/`uncompress()` en `divc.cpp`, `divfrm.cpp`, `div32run/f.cpp` (opcodes de PAK) y ambos instaladores |
| **TopFLC** | Playback FLI/FLC | Único punto de integración: `src/div32run/divfli.cpp` (solo runtime). Licencia propia no-GPL (uso comercial requiere contactar al autor) |
| **PMODE/W** (1.34) | Extensor DOS 32-bit | Se recompila como herramienta de host y bindea binarios release vía `pmwlite`, ver §4 |

→ Detalle (versiones exactas, símbolos, tabla librería→consumidor, pendientes no verificados): [05](docs/architecture/05-dependencias-terceros.md).

## 6. Formatos de archivo propietarios (`formats/`)

Documentados como esquemas **Kaitai Struct (`.ksy`)**: `a3d`, `fnt`, `fpg`, `game_exe`, `ifs`, `install`/`install_pak`, `map`, `o3d`, `pak`, `pal`, `wld`.

Hallazgos: **WLD es dos formatos en uno** — una capa de edición completa y, a partir de `offset_vpe+12`, un bloque runtime independiente con su propia mini-cabecera, confirmado línea a línea contra `src/vpe/load.cpp`. **PAK nunca lo lee el juego compilado**: el instalador lo descomprime a ficheros sueltos en disco; el runtime solo lee ficheros individuales. **`install_pak`** es un `pak` partido en volúmenes de disco, señalizado por un byte de "volumen no final" en la cabecera de cada `.001/.002/...`. **`game_exe`** confirmado con precisión de bytes contra `src/div32run/i.cpp` (`fseek(f,602,...)`, cabecera de 10 enteros, payload zlib).

→ Detalle (estructura binaria de cada formato, referencias cruzadas al código): [06](docs/architecture/06-formatos-archivo.md).

## 7. Modelo de ejecución (pipeline DIV)

```
Programa .PRG  →  compila (divc.cpp, lexer dirigido por system\ltlex.def)  →  bytecode + cabecera de 10 enteros
      →  se concatena al stub de 602 bytes (embebido, ver §3)  →  EXE autónomo
      →  al ejecutarse invoca DIV32RUN pasándose a sí mismo  →  VM (mem[] unificado, switch de kernel.cpp)
      →  funciones runtime resueltas como DLL vía el mismo loader PE casero (pe_load.c) que las DLLs de usuario
```

El modelo de proceso del lenguaje DIV es un bloque fijo dentro de `mem[]` (`_Status/_Father/_Son/.../_Frame`), con un scheduler round-robin por prioridad en `exec_process()`; `FRAME`/`FRAME(n)` del lenguaje son literalmente los opcodes `lfrm`/`lfrf`. Dato no obvio confirmado en el makefile real: el "stub de 602 bytes" (`div_stub.h`) es un binario ya compilado embebido como array de bytes, y el código fuente que debería generarlo (`src/div32run/wstub/wstub.c`) está reducido en el repo a un `main(void){}` vacío — discrepancia real, documentada como tal (no inventada).

→ Detalle (compilador, formato de bytecode, arranque, VM, procesos, SESSION=1 vs 0): [07](docs/architecture/07-modelo-ejecucion.md).

## 8. Aspectos técnicos relevantes / desafíos de portabilidad

1. **Compilación**: OpenWatcom 1.9 con triple target (16-bit DOS, 32-bit DOS, host); no compila con OW2 tal cual.
2. **16/32 bits mezclado vía DPMI**: el stub de arranque (`src/div_stub/asm/exec.asm`) detecta CPU con el truco clásico de flags y lanza `DIV32RUN.DLL` (32-bit, cargado por PMODE/W) vía `INT 21h AH=4Bh`. Ejemplo concreto de callback DPMI real-mode en `src/vpe/hard.cpp` (`int386Extend`, `INT 31h AX=0x300`) para simular `INT 7Ah` (IPX real-mode) desde modo protegido.
3. **Assembler x86** (WASM/TASM): `a.asm`, `timer.asm`, `vesa.asm`, `t.asm`, `draw_*.asm`.
4. **Dependencias DOS hardcodeadas**: VGA/SVGA vía VESA/BIOS, DAC, Sound Blaster bare-metal (DMA por puertos), IPX, ratón por driver, PIC.
5. **Loader PE propio** (`pe_load.c`): parsea cabeceras/secciones/relocations a mano pero no procesa importaciones PE (ver §3).
6. **Aislabilidad de la VM respecto a la I/O** (relevante para un port moderno): confirmado solo parcialmente. `s.cpp` e `i.cpp` están razonablemente aislados de hardware/DOS; `v.cpp` escribe directamente a registros CRTC/Sequencer de VGA (es la propia capa de hardware); `f.cpp` mezcla ~31 llamadas a hardware/DOS entre las funciones nativas del lenguaje, sin frontera de módulo limpia.

→ Detalle (evidencia de código, TODOs/FIXMEs reales encontrados, implicaciones para un port moderno): [08](docs/architecture/08-aspectos-tecnicos.md).

## 9. Estado del árbol

Fotografía puntual — revalidar siempre con `git status`/`git branch -a` antes de confiar en ella. A la fecha de esta revisión (2026-09-17): rama `master`, **árbol de trabajo limpio** (una revisión anterior de este documento registraba 6 archivos modificados sin commitear en `src/vpe/*.inc`; ya no están presentes). OpenWatcom **sigue sin estar instalado** en este equipo (`WATCOM` no definido, `wmake`/`wcc386` fuera del `PATH`) — es el primer paso para poder compilar. Las libs de terceros ya están precompiladas (386/586), así que no hace falta TASM para un build normal.

→ Detalle y comandos de revalidación: [09](docs/architecture/09-estado-arbol.md).

## 10. Hoja de ruta

1. Instalar OpenWatcom 1.9 (DOS16 + DOS32 + host) y verificar `wmake` en el PATH (o usar el `Vagrantfile` del repo).
2. Probar `wmake tools` antes del build completo; luego `wmake` completo, documentando errores propios de OW1.9 sobre Windows moderno.
3. Ejecutar `wmake test_dll` y, con DOSBox instalado, los tests del intérprete.
4. Decidir objetivo: compilar y correr bajo DOSBox-X (alineado con el milestone 2.01), o evaluar un port moderno sustituyendo la capa de bajo nivel — viable en principio porque la VM es parcialmente separable de la I/O (ver §8), aunque `f.cpp`/`v.cpp` no tienen una frontera limpia hoy.

→ Detalle y roadmap declarado del proyecto (milestones 2.01/2.02): [10](docs/architecture/10-hoja-de-ruta.md).
