# Handoff — port de DIV Games Studio 2 a Windows 11 nativo

> **Para quien retome esto** (otra sesión, otro modelo de IA, u otra herramienta): este documento es el punto de entrada único. No hace falta leer `12-port-progreso.md` entero (es una bitácora cronológica de 10 checkpoints, muy detallada pero larga) salvo que necesites el razonamiento completo de un hallazgo concreto — este handoff resume todo lo accionable y enlaza a la sección exacta cuando hace falta más detalle. Está escrito para ser autosuficiente: no asume que hayas visto la conversación donde se hizo este trabajo.
>
> **Para "¿qué queda y en qué orden?"** ver [16-port-plan-pendiente.md](16-port-plan-pendiente.md) — es el plan priorizado post tag `1.1.0+mode8`, más corto y directo que el backlog histórico de la §8 de este documento.

Última actualización: 2026-09-22. **Portado el motor de renderizado Modo-8 en tiempo real** (§54 de `12-port-progreso.md`): `port/vpe/` (18 ficheros nuevos) — resultó mucho más tratable de lo que estimaba §53, porque `src/vpe/vpedll.cpp` ya era la implementación real de todos los opcodes Modo-8 (solo hacía falta portarla, no diseñarla) y de los 3 `.asm` solo 7 rutinas de pintado de span necesitaban reemplazo en C. Se encontraron y arreglaron **7 bugs reales** con AddressSanitizer (`build-asan/`) — 6 en la primera pasada (casi todos "puntero truncado a 32 bits", ver §54.3) y un 7º encontrado *después*, al probar desde el IDE con una partida real (`_object_destroy` sin comprobar objeto -1, crasheaba `elimina_proceso()` al destruir CUALQUIER proceso, no solo los de Modo-8 — reportado por el usuario: "el juego crashea al empezar una partida, aunque el Modo-8 del menú sí funciona"). **Verificado con un juego real** (`WSPORTX.PRG`, vía IDE y vía `div32run_port.exe` directo): arranca, navega menús, y con el 7º fix **la partida real de "Jump" (salto de esquí acuático) corre de punta a punta** — marcador de distancia subiendo con normalidad, sin crash, con `start_mode8`/`loop_mode8` activos todo el rato. Importante: al recompilar recordar que `div32run_port.exe`/`divc_port.exe` que usa el IDE son los de `build/Release` (busca sus herramientas junto al propio exe del IDE) — no basta con recompilar `build-dbg`/`build-asan`, que son directorios de pruebas aparte.

Antes, en la misma sesión larga: cerrado el **editor de mapa 3D del IDE (Modo-8)** (§52 de `12-port-progreso.md`): `divmap3d.cpp` (4164 líneas) añadido al build, resultó mucho más tratable de lo que su tamaño hacía pensar (sin ensamblador, sin dependencias del generador de sprites) — solo 8 stubs duplicados que retirar y 2 globales (`last_x`/`last_y`) que añadir. **`Mapas → Nuevo mapa 3D` funciona**, verificado en caliente por el usuario. Esto era solo el **editor** — el motor de renderizado en tiempo real (arriba) es un módulo totalmente distinto.

Antes, en la misma sesión larga: cerrado el hito **"generador de explosiones del IDE" (E10)** (§50 de `12-port-progreso.md`): `diveffec.cpp` (532 líneas, sin dependencias externas) compilado, y un **bug real de signo de `char`** encontrado y arreglado — `Buff_exp` (brillo 0..255 de cada píxel) es `char` con signo; para valores ≥128 el wraparound a negativo, usado luego como índice de `ExpDac[256]` (variable local, en la pila), producía una lectura fuera de límites antes del array: píxeles de color aleatorio en la parte brillante de la explosión. Arreglado con un cast mínimo a `unsigned char` en el único punto de lectura relevante. **Menú Mapas → Generador de explosiones ya funciona**, verificado en caliente. Antes: **"calculadora del IDE" (E9)** (§49) y **"ayuda del IDE" (E8)** (§48), ambas compiladas a la primera sin sorpresas. Pendiente cosmético (no bloqueante, aplazado): el texto de la ayuda se ve grande — hipótesis sin confirmar en §48.4. Se investigó también el **generador de sprites** (`divspr.cpp` + `src/div/visor/`, ~5.500 líneas con motor 3D propio + ensamblador) y se **aparcó explícitamente** por su tamaño, a petición del usuario.

Antes de eso, en la misma sesión larga: cerrado el hito **"audio del IDE" (E7)** (§47) — `divpcm.cpp` compilado con un **shim de compatibilidad JUDAS nuevo** (`port/div/ide/port_ide_judas.c`) sobre `port/io/io_audio.c`/`io_song.c`, sin portar la librería JUDAS real; dos bugs reales cerrados por el camino (heap corruption en miniaturas de sonido del navegador, cuelgue permanente por `PollInputEvents()` nunca re-ejecutándose fuera de `volcado()` — fix en `io_input.c`, compartido con el runtime). Antes: **"Sistema → Información del sistema"** con memoria libre real (§46). Antes: **auditoría sistemática de `port_ide_stubs.c`** (§45) que cerró el editor de paleta. Antes: cerrado del todo el **bug 2 de E6a** (editor de mapas, §44). Antes: bug 1 de E6a (browser) y el crash bloqueante original del bug 2 (§43). Antes: crash del IDE a 3840×2160 y corrupción CP850 (§41-42). Historial completo, con todo el razonamiento y los números de línea exactos, en `12-port-progreso.md`.

---

## 1. Qué es este proyecto

**DIV Games Studio 2** es un entorno de desarrollo de juegos para MS-DOS (Hammer Technologies, 1998-99): editor + compilador de un lenguaje propio ("DIV") + intérprete de bytecode. Este repositorio es una restauración GPLv3 del código fuente original (fork de [DIVGAMES/DIV-Games-Studio](https://github.com/DIVGAMES/DIV-Games-Studio)).

**Objetivo de este trabajo concreto**: portar el proyecto para que funcione de forma nativa en Windows 11 — tanto el runtime (`DIV32RUN`, el intérprete de bytecode) como, más adelante y con **prioridad explícita del usuario**, el editor/IDE. La decisión de arquitectura (ya tomada, no se vuelve a discutir): usar **raylib** como capa de vídeo/audio/input, sin soporte de red, con el editor como prioridad por encima del empaquetado de juegos distribuibles. El razonamiento completo de esa decisión está en [`11-port-windows11-mikedx.md`](11-port-windows11-mikedx.md) (incluye la evaluación del fork de MikeDX y por qué se descartó reutilizar su capa de vídeo).

## 2. Estado actual en una frase

**El runtime (`DIV32RUN`) está portado y verificado; el compilador (`divc.cpp`) también. Modo-7 y vídeo FLI/FLC confirmados con contenido real (un juego y un vídeo reales). El editor/IDE ya cierra el ciclo completo de trabajo: arranca y pinta su escritorio (E2.1), responde al ratón (E2.2) y al teclado (E2.3), abre/guarda ficheros con el browser real y edita código con fuentes reales (E3), y **compila (F11) y prueba (F10/F12) el programa sin salir del IDE** (E4), **resolviendo sus recursos** como lo haría una instalación real de DIV (E5). El editor FPG, el editor de mapas, el editor de paleta, el editor de sonido/PCM, la ayuda, la calculadora, el generador de explosiones, el editor de mapa 3D y **el generador de sprites (GENSPR)** ya funcionan (E6a/E7/E8/E9/E10/Modo-8-editor/GENSPR, ver §55), con audio real (shim JUDAS sobre raylib/libmikmod). El motor de renderizado Modo-8 en tiempo real también está portado y verificado (§54). No queda ningún hueco funcional grande aparcado en el IDE.**

| Pieza | Estado |
|---|---|
| Toolchain (MSVC + raylib), capa `port/io/` | Hecho |
| Núcleo del intérprete portado (`i.cpp`/`kernel.cpp`/`f.cpp`/`s.cpp`/`divlengu.cpp`) | Compila limpio, enlaza, ejecuta |
| Vídeo (`v.cpp`) | Reescrito contra `port/io`, verificado en caliente |
| Ratón/teclado (`mouse.cpp`/`divkeybo.cpp`) | Reescrito contra `port/io`, verificado en caliente y **con bytecode** (`key()` + flechas + ESC, checkpoint 12). `ascii`/`scan_code` **ya funcionan** desde la cola de eventos de `io_keyboard.c` (§35 de `12-port-progreso.md`) |
| Sonido de efectos (`divsound.cpp`) | Reescrito contra `port/io`, **verificado auditivamente con bytecode** (`load_pcm`+`sound`, checkpoint 11) |
| Colisiones (`collision()`, función 008) | Portada de verdad en `s.cpp` (AABB de la caja del gráfico con `_Size`/pivot/`_Ctype`/`_Resolution`), **verificada con bytecode** (checkpoint 13): dos procesos tipo 1 y 2 (`main` + hijo creado con `lcal`), el programa se auto-cierra al chocar (~3 s). Se eliminó el stub |
| Builtins de juego (`set_mode`/`load_fnt`/`write`/`write_int`/`delete_text`/`put_pixel`/`random`/`fade`+`fading`/`let_me_alone`/`get_id`/`get_distx`/`get_disty`) | **Verificadas con bytecode** (checkpoint 14): `build_api_demo.py` — 130 frames de estrellas + textos con fuente real + hijo trazando un anillo, luego fade/`let_me_alone`/`get_id`/`delete_text` y auto-cierre (exit 26 en ~7 s, estable 5/5). Incluye ruta de rotación de sprites (`sp_rotado`) y clipping fuera de pantalla, robustas |
| Música de tracker (MOD/S3M/XM) | **Hecho** con libmikmod vendored (`3rdparty/mikmod`, ver §39 de `12-port-progreso.md`): `io_song.c` (TU separado, sin raylib), `divsound.cpp` con `tCancion.Len`, `MikMod_SetNumVoices(32,0)` tras init y `io_song_update()` por frame en `frame_end()`. TOKENKAI carga y suena su `token.s3m` |
| Búsqueda de caminos (`path_find`/`path_line`/`path_free`) | **Hecho** — port fiel del módulo original `src/div32run/ia.cpp` en `port/div/core/ia.c` (A* rápido / Dijkstra exacto sobre el gráfico del scroll + Bresenham). Eran stubs vacíos: TOKENKAI no movía al protagonista (ver §40 de `12-port-progreso.md`) |
| Modo-8 (motor 3D) | **Portado** (§54 de `12-port-progreso.md`): `port/vpe/`, opcodes reales, verificado estable con `WSPORTX.PRG` (arranca, menús, `start_mode8`/`loop_mode8` corren sin errores). Falta confirmar visualmente un nivel 3D concreto (paredes/suelo/techo con textura) |
| Modo-7 (`pinta_modo7`, `mul_24`/`mul_16`) | **Confirmado con un juego real** (2026-09-22, SPEED.PRG) — ya funcionaba desde el checkpoint 8 (`mul_24`/`mul_16` son implementación real, no stub), sin trabajo adicional |
| Vídeo FLI/FLC (`start_fli`/`frame_fli`/`end_fli`/`reset_fli`) | **Hecho y confirmado visualmente 2026-09-22** (§51 de `12-port-progreso.md`): decodificador nuevo `port_topflc.c` (API compatible con TopFLC v1.0, no vendored) + `divfli.cpp` copiado sin tocar. Probado con `INTRO.FLI` real (juego ALIEN, 121 frames) |
| DLLs/plugins | Pool de símbolos real, pero sin *loader* PE — `DIV_LoadDll` siempre falla limpio |
| CD-audio/red | Stubs inertes, fuera de alcance actual |
| Limitador de fotogramas | Arreglado (checkpoint 9) — ver §6 |
| Compilador (`divc.cpp`) | **Portado y funcional vía CLI** (`divc_port`): compila el corpus tutorial 11/11, tag `0.0.10-compiler+corpus` |
| Editor/IDE — núcleo UI (`div.cpp`/`divwindo.cpp`/`divhandl.cpp`/`divedit.cpp` + primitivas) | **E1 hecho** (`div_ide_port` compila y enlaza) — ver §8 |
| Editor/IDE — vídeo | **E2.1 hecho** — el escritorio de DIV 2.01 se dibuja sobre `port/io`/raylib (tapiz, barra "menú", "DIV 2.01", cursor) a 640×480. Ver §27 de `12-port-progreso.md` |
| Editor/IDE — ratón | **E2.2 hecho** — `divmouse.cpp` portado, INT 33h emulado en modo **absoluto** (el cursor del IDE converge sobre el puntero real), cursor del SO oculto. Menús clicables y navegables. Ver §28 de `12-port-progreso.md` |
| Editor/IDE — teclado y reloj | **E2.3 hecho** — `divkeybo.cpp` **sustituido** por `port/div/ide/port_ide_keybo.c` (el original es puro DOS: IRQ 9 + puerto 0x60 + INT 16h). Cola propia de eventos, emparejado ascii↔scancode, `kbdFLAGS` por flancos, filtro de ALT GR y `reloj` de 100 Hz por fin vivo. Verificado con `ALT+X`. Ver §29 de `12-port-progreso.md` |
| Editor/IDE — diálogos de fichero (`divbrow.cpp`) y texto (`divfont.cpp`) | **E3 hecho** — ambos en `DIV_IDE_SOURCES`; `Programas → Nuevo/Abrir` abre el browser real y el editor pinta texto con fuentes reales. Ver §30 de `12-port-progreso.md` |
| Editor/IDE — Compilar (F11) y Probar (F10/F12) | **E4 hecho** — `port/div/ide/port_ide_compila.c` invoca `divc_port.exe` y `div32run_port.exe` con `CreateProcessA`, parsea `Error N (linea X, columna Y)` y **el IDE ya no se cierra al probar**. Ver §31 de `12-port-progreso.md` |
| Editor/IDE — recursos al Probar (FPG/PCM/FNT) | **E5 hecho** — `open_file()` usa la cadena de búsqueda permisiva del build `SESSION` original (respeta subcarpetas tipo `tutorial\tutor0.fpg`) y admite raíces extra vía `DIV_RES_PATH`; el runner se lanza con cwd = raíz de DIV. Ver §32 de `12-port-progreso.md` |
| Editor/IDE — editor FPG + importación de imágenes | **E6a WIP commiteado (`4917a42`)** — `divfpg.cpp`/`fpgfile.cpp`/`divforma.cpp` en el build; FPG nuevo/abrir con thumbnails y arrastrar gráfico al escritorio funcionan. Bug del browser (subir de directorio) **cerrado 2026-09-21**. **Editor de mapas (`Mapas → Editar mapa`) cerrado y funcional 2026-09-21** (§44 de `12-port-progreso.md`): abrir, pintar con el ratón y salir con ESC funcionan de punta a punta. Pendiente aparte: doble-clic sobre el mapa del escritorio sigue sin abrir el editor (hay que usar el menú) |
| Editor/IDE — editor de paleta | **Cerrado y funcional 2026-09-21** (§45 de `12-port-progreso.md`): editar y abrir/cargar una paleta (`Paletas → Editar`/`Abrir`) funcionan. `divpalet.cpp` ya estaba en el build desde antes de E6a, solo hacía falta la auditoría de `port_ide_stubs.c` que cerró este hito |
| Editor/IDE — audio (E7) | **Cerrado y funcional 2026-09-22** (§47 de `12-port-progreso.md`): abrir/reproducir un `.pcm`/`.wav`, miniaturas de forma de onda en el navegador, abrir/reproducir tracker MOD/S3M/XM. Shim de compatibilidad JUDAS (`port_ide_judas.c`), no la librería real. VU meters planos, grabación deshabilitada |
| Editor/IDE — ayuda (E8) | **Cerrado y funcional 2026-09-22** (§48 de `12-port-progreso.md`): menú Ayuda navegable con datos reales. Pendiente cosmético: tamaño de letra grande (§48.4, aplazado) |
| Editor/IDE — calculadora (E9) | **Cerrado y funcional 2026-09-22** (§49 de `12-port-progreso.md`): Sistema → Calculadora funciona |
| Editor/IDE — generador de explosiones (E10) | **Cerrado y funcional 2026-09-22** (§50 de `12-port-progreso.md`): Mapas → Generador de explosiones funciona. Bug real de signo de `char` encontrado y arreglado (`Buff_exp` indexando `ExpDac[]`) |
| Editor/IDE — editor de mapa 3D (Modo-8) | **Cerrado y funcional 2026-09-22** (§52 de `12-port-progreso.md`): `Mapas → Nuevo mapa 3D` funciona (pintar puntos/paredes/regiones, salir con ESC). Esto es solo el **editor** — el motor de renderizado en tiempo real es un módulo distinto, ver fila siguiente |
| Runtime — motor de renderizado Modo-8 (`src/vpe/` → `port/vpe/`) | **Portado y verificado con una partida real** (§54 de `12-port-progreso.md`): renderizador nuevo en C limpio para las 7 rutinas de pintado que eran ASM (`draw_span.c`), resto portado casi literal (`vpedll.c` ya era la implementación real de los opcodes). 7 bugs encontrados y arreglados con ASan (6 de puntero truncado a 32 bits + 1 de guard que faltaba en `_object_destroy`, este último solo se manifestaba al jugar de verdad, no en el menú). `WSPORTX.PRG` corre la minipartida de "Jump" de punta a punta sin crash |
| Editor/IDE — generador de sprites (GENSPR) | **Portado y cerrado 2026-09-23** (§55 de `12-port-progreso.md`): `Mapas → Generador de sprites` renderiza el modelo 3D (textura+Gouraud) y aguanta clics/arrastres repetidos en la vista 3D. Doble `free()` real del original de 1999 encontrado y arreglado (§55.7), confirmado por el usuario en el IDE real. Limitación heredada, no del port: el `textura.pcx` que trae el repo es en realidad un JPEG (JPG sigue stub, decisión de E6a) — verificado que el resto del pipeline funciona sustituyendo la textura por un PCX real |
| Runtime — cierre de la ventana del juego (X / ALT+F4) | **Hecho** — `port_cierre.c`, llamado desde `volcado()` (`v.cpp`); apagado ordenado estilo `_exit_dos()` y `exit(26)`. Antes un juego con bucle infinito era inmortal. Ver §33 de `12-port-progreso.md` |
| Runtime — `set_mode()` en caliente (cambio de resolución) | **Hecho** — `io_video_resize()` en `port/io/io_video.c`, llamado desde `svmode()`. Antes la 2ª llamada a `set_mode()` se ignoraba y la imagen salía desdoblada/desplazada (stride desfasado). Ver §34 de `12-port-progreso.md` |
| Runtime — `scan_code`/`ascii` y operadores `++`/`--` | **Hecho** — `tecla()` alimenta `scan_code`/`ascii` desde la cola de `io_keyboard.c`, y los 12 opcodes de inc/dec con puntero de `kernel.cpp` se reescriben con la dirección en un temporal (orden de evaluación indefinido: MSVC asignaba antes de incrementar, así que `x++` **no hacía nada**). Ver §35 de `12-port-progreso.md` |
| Juego completo real (STEROID) | **Jugable** a 640×480: presentación, teclas, nave, asteroides, vidas, nivel y cierre con la X (exit 26) |

## 2.1 Punto de retomada (última sesión: 2026-09-20, path builtins)

**Lo último cerrado y commiteado**: input real de un juego completo (`9d6ed06`, §35), `collision()` con ids de tipo negativos (`e3190fe`, §36 — los disparos de STEROID no colisionaban), joystick fantasma + `out_region()` real (`269ea05`, §37-38 — MALVADO saltaba sin parar y no restaba vidas al morir), **E6a commiteado como WIP** (`4917a42`, editor FPG + importación de imágenes, con los 2 bugs abiertos de abajo) y la **música de tracker** (libmikmod, `b6d1fb9`, §39). Tags hasta `0.0.19-runtime+input`. MALVADO y STEROID son **100% jugables**.

## 2.2 Punto de retomada (2026-09-21): §41 (letra proporcional) + §42 (crash 4K y CP850) — verificado, PENDIENTE de commit

**Nada de esto está commiteado todavía.** El árbol compila limpio y las tres
resoluciones arrancan y vuelcan frame con la misma `session.dtf` que antes
reventaba:

```
1920x1080 -> exit 0     2560x1440 -> exit 0     3840x2160 -> exit 0
```

Contenido sin commitear:

- **§41 letra proporcional** (`big2` base 1280): `src/div/*.cpp` + `global.h`
  (fuentes y gráficos del sistema sintetizados en `divwindo.cpp`, fuente del
  editor escalada, todos los `*2`/`/2` del modo big pasados a `big2`).
- **§42 crash a 3840x2160**: `divdsktp.cpp` vía `tools/patch_session_geom.py`.
  `nueva_ventana_carga()` reservaba el buffer con la geometría recién
  calculada y luego la sustituía por la guardada en `session.dtf`; si la
  sesión venía de un build con otra escala, `wvolcado()` leía fuera del heap.
  Ahora la geometría de la sesión solo se acepta si cabe en `lon_ptr`.
- **§42 `io_video_fit_window()`**: estaba escrita y declarada pero **sin
  llamar** (quedó a medias de una bisección, con un marcador `XXXBISEC` en
  `io_video.c`). Restaurada la llamada en `svmode()` y quitado el marcador.
- **§42 reparación CP850**: 14 de los 15 ficheros de `src/div/` modificados
  tenían 1.202 líneas con `EF BF BD` (U+FFFD) donde había bytes CP850 —
  incluidos dos literales reales (`div.cpp:51`, tabla de acentos, y
  `divfont.cpp:1208`, `pletras[]`). Reparado con `tools/fix_cp850.py`.
- **E6b empezado** (sin documentar aún en §41/§42): `src/div/divsetup.cpp`
  añadido al build (Sistema → Modo de vídeo / Configuración / Memoria /
  Fondo), ~30 stubs retirados, `MAX_YRES` 2048→4096 y 8 modos nuevos en
  `detectar_vesa()` (1280x800, 1366x768, 1600x900, 1600x1200, 1920x1080,
  1920x1200, 2560x1440, 3840x2160).

**Comprobación obligatoria antes de cada commit que toque `src/`** (regla
§24.3, que ya se ha roto dos veces):

```bash
for f in $(git diff --name-only -- src/div/); do \
  python -c "print('$f', open('$f','rb').read().count(b'\xef\xbf\xbd'))"; done
```

**Ojo con los artefactos de ejecución**: `system/SETUP.BIN` se reescribe al
correr el IDE (`git checkout -- system/SETUP.BIN` antes de commitear) y
`system/session.dtf` guarda la resolución y la geometría de las ventanas.
`div2/`, `tools/dumpstack/` y `tools/test_scale_fact3.c` están sin versionar.

**Pendiente**: verificación visual tuya a 2560x1440 y 3840x2160 en caliente
(las capturas automáticas ya salen bien), y luego commit. Sugerencia de
trocearlo en dos: `IDE: letra proporcional (big2 base 1280)` y
`IDE: arreglado el crash al restaurar sesion a 4K + reparada la codificacion CP850`.

**E6a: bug 1 y bug 2 CERRADOS (2026-09-21).** El editor FPG, el browser y el
editor de mapas (`Mapas → Editar mapa`) funcionan de punta a punta — ver el
detalle abajo. Pendiente para cerrar E6a del todo: doble-clic sobre el mapa
del escritorio (hay que usar el menú), y el resto de editores de recursos
(paleta, PCM, ayuda, 3D) siguen stub.

### Qué se hizo en E6a

- **Editor FPG portado**: `divfpg.cpp` (ventanas FPG: `FPG0N`/`FPG0A`/`FPG1`/`FPG2`/`FPG3`, `nuevo_fichero`, `abrir_fichero`, `SaveFPG`, thumbnails...) + `fpgfile.cpp` (E/S de FPG: `Abrir_FPG`, `Crear_FPG`, `Anadir_FPG`, `Borrar_FPG`, `Sort`...).
- **Importación de imágenes**: `divforma.cpp` (PCX/BMP/MAP reales: `es_*`/`descomprime_*`/`graba_*`/`cargadac_*`). **JPG queda stub** (devuelve error controlado): jpeglib 6.x define `boolean` como enum(int) y choca con el `unsigned char boolean` de `rpcndr.h` (windows.h entra por force-include), y `3rdparty/jpeglib/jconfig.h` es el de Watcom/DOS. Se aborda aparte si hace falta JPG.
- Parche byte-exacto de `divforma.cpp` (script `tools/patch_e6a_divforma.py`, idempotente): bajo `#ifndef PORT_IDE` excluye los includes de jpeglib, las typedefs `RGBQUAD`/`BITMAPFILEHEADER`/`BITMAPINFOHEADER` (las de `wingdi.h` son idénticas campo a campo y divforma las usa solo leyendo campos sueltos o con `memcpy` de 40 bytes) y las funciones JPG.
- ~30 stubs eliminados de `port_ide_stubs.c`.
- **Verificado en caliente**: FPG Nuevo/Abrir abre ventanas con la lista/thumbnails; arrastrar un gráfico al escritorio pide confirmación y crea la ventana del mapa en el escritorio.

### Bug 1 CERRADO (2026-09-21) — el browser petaba navegando "más atrás" de `build\Release`

La causa real **no era** `chdir`/`getcwd` ni el fix de alias 8.3 (ese fix sigue siendo necesario, pero no era la causa de esto). Era `strupr(NULL)`: `crear_un_thumb_MAP()` y otros 15 puntos de `divbrow.cpp` hacen `strcmp(strupr(strchr(nombre,'.')),".EXT")`, y `strchr` devuelve NULL en ficheros **sin extensión** (`LICENSE`, `makefile`, `Vagrantfile` — la raíz del propio repo DIV es un caso real). La UCRT de MSVC aborta el proceso (`__fastfail`, excepción `0xC0000409` — la misma que un fallo de cookie /GS, así que el primer análisis con `crashcatch` apuntaba a un desbordamiento de pila y en realidad era esto) en vez de devolver NULL como hacía el CRT de Watcom/DOS original.

**Arreglado** con `tools/patch_browser_ext_crash.py` (helper `mayus_extension()` null-safe, sustituye las 16 llamadas). **Bonus encontrado investigando esto**: `determina_unidades()` (`div.cpp`) leía `unidades[]` (la lista `<X:>` del browser) con `intdos()`, que en este port es un no-op literal — el bucle corría con memoria de pila sin inicializar y `unidades[]` quedaba con una única letra bogus (`"N"` en este equipo, no las unidades reales). Arreglado con `tools/patch_determina_unidades.py` (sustituye por `GetLogicalDrives()`).

**Verificado**: 11 navegaciones "subir de directorio" seguidas (`Release → build → DIV → Documents → Dani → Users`, atravesando decenas de dotfiles sin extensión) sin ningún crash; `<Unidad:>` muestra letras reales. Detalle completo, incluido cómo se diagnosticó: §43.1-43.3 de `12-port-progreso.md`.

### Bug 2 CERRADO (2026-09-21) — el mapa del escritorio no se abría para editarlo

- **Doble-clic sobre el mapa**: sigue sin hacer nada (no crashea, tampoco entra en modo edición). No investigado — usar el menú (siguiente punto) en su lugar.
- **Menú `Mapas → Editar mapa`** (bypassa la detección de doble-clic, llama a `v.click_handler` directo): **llegaba a `mapa2()`, y crasheaba con `0xC0000005`** — RIP caía exacto (offset 0) en el símbolo `M3D_crear_thumbs`, que en `port_ide_stubs.c` era un `void*` de **datos** (`= NULL`), no una función; pero `mapa2()` (`divhandl.cpp:1722`, construyendo la lista de texturas de pincel desde `SYSTEM\BRUSH.FPG`) lo **llama de verdad**. Saltar a la dirección de un dato no ejecutable = 0xC0000005. Mismo patrón exacto y también confirmado alcanzable en `calculadora` (menú Sistema → Calculadora). **Arreglado**: ambos pasan de dato a función no-op con la firma real.
- **Tras quitar ese crash, el editor seguía sin ser funcional**: la pantalla de edición salía **completamente negra**, y pulsar **ESC** para salir crasheaba con `0xC0000374` (`STATUS_HEAP_CORRUPTION`). **Causa real** (§44 de `12-port-progreso.md`): `port_ide_stubs.c` stubeaba `thumb_tex`/`ltexturasbr`/`m3d_edit` como punteros `void*` de 8 bytes, cuando `divpaint.cpp`/`divhandl.cpp` (ya en el build desde E6a) los tratan como structs/arrays completos (`thumb_tex[1000]`, `struct t_listboxbr`, `M3D_info` con `fpg_path[256]`) — escrituras de hasta ~40 KB fuera de esos 8 bytes, corrompiendo memoria estática vecina (incluida la textura de framebuffer del IDE — de ahí la pantalla negra — y los buffers de teclado, de ahí el crash al salir). **Arreglado** dándoles su definición real en `port_ide_stubs.c` (`thumb_map` ya la tenía en `divpaint.cpp`, solo sobraba el stub). De paso, mismo patrón "dato en vez de función" encontrado en `map_save`/`map_read`/`map_saveedit`/`map_readedit` (llamados desde `divhandl.cpp`/`divdsktp.cpp`, ruta no alcanzada todavía pero real) — arreglados igual, con no-ops.
- **Verificado en caliente por el usuario**: abrir un `.MAP` real, `Mapas → Editar mapa`, pintar con el ratón y salir con ESC — funciona de punta a punta, sin pantalla negra ni crash. Detalle completo del diagnóstico (incluye por qué AddressSanitizer de MSVC no sirvió en esta máquina y cómo se usó una build Debug + resolución manual del offset del Visor de eventos contra el `.map` del linker): §44 de `12-port-progreso.md`.

### Reglas vigentes (sin cambios)

- Los ficheros de `src/` son **CP850**: editarlos **solo** con parches byte-exactos en Python (incidente U+FFFD, §24.3). Excepción: `port/div/core/s.cpp` ya está en UTF-8 (comentarios con FFFD heredados) → editable normal. `port/div/ide/global.h` es CP850 → byte-exact.
- Preferencia del usuario: **probar en caliente primero, commitear después**.
- `div2/` sin versionar; `system/SETUP.BIN` se reescribe solo al correr el IDE → `git checkout -- system/SETUP.BIN` tras probar (no commitear).
- Truco de test sin input: forzar errores visibles en stdout del runner para observar ramas (`load_pcm` inexistente → `Error 128`, `load_fpg` → `Error 105`) — usado para verificar `collision()`, `joy.button1`, `out_region()` desde shell.

**Qué NO funciona y por qué** (importante, para no perseguir fantasmas):

1. **Editores de recursos**: FPG, editor de mapas y editor de paleta funcionan. Doble-clic sobre el mapa del escritorio no entra en modo edición (usar el menú `Mapas → Editar mapa`). PCM/ayuda/3D siguen stub o sin verificar (necesitan audio del IDE y/o compilar módulos todavía fuera del build: `divpcm.cpp`, `divhelp.cpp`, `divmap3d.cpp`, `divcalc.cpp`). **Auditoría de `port_ide_stubs.c` ya hecha una vez** (§45 de `12-port-progreso.md`): de ~50 símbolos `void *`, se encontraron y arreglaron 14 con este patrón (2 structs/arrays reales shadowed por el stub, 12 llamados de verdad como función). Quedan ~36 sin llamada real detectada hoy — **al portar el siguiente módulo (`divpcm.cpp`/`divhelp.cpp`/`divcalc.cpp`), repetir el mismo método** (dos greps: definición real no-`extern` del mismo nombre, y uso `nombre(` en los ficheros que se vayan a compilar) antes de asumir que un crash nuevo es otra cosa — ver §45.1 de `12-port-progreso.md` para el método exacto y su limitación conocida (no detecta structs con tipo compuesto, como pasó con `thumb_tex`/`ltexturasbr`/`m3d_edit` en su momento).
2. **Audio del IDE cerrado (2026-09-22)**: editor de sonido/PCM (`divpcm.cpp`) funciona — abrir, reproducir, miniaturas de forma de onda, tracker MOD/S3M/XM. Shim de compatibilidad (`port/div/ide/port_ide_judas.c`), no JUDAS real — ver §47 de `12-port-progreso.md`. VU meters siempre planos (degradación aceptada) y grabación deshabilitada (sin hardware real que grabar). `divmixer.cpp`/`divsb.cpp` siguen sin portar (irrelevantes, acceso a hardware ISA real).
3. **Música de tracker** (MOD/S3M/XM) implementada tanto en el runtime (libmikmod, §39) como en el IDE (§47, vía el mismo `io_song.c`).
4. **Un juego que se cuelgue sin volcar ningún frame** sigue necesitando matar el proceso: el chequeo de cierre de ventana vive en `volcado()` (§33 de `12-port-progreso.md`). Además, mientras el juego corre el IDE queda bloqueado esperándolo.
5. **Riesgo latente de la misma familia que el fallo de `++`**: los opcodes `lada`/`lsua`/`lmua`/... de `kernel.cpp` siguen escritos como `pila[sp-1]=mem[pila[sp-1]]+=pila[sp];`, sin punto de secuencia entre los dos efectos. Hoy MSVC los resuelve bien (`c+=1` funciona), pero es el mismo patrón de orden de evaluación indefinido. Ver §35.4.

**Siguiente hito: confirmar visualmente un nivel Modo-8 real (paredes/suelo/techo con textura). El IDE ya no tiene ningún hueco funcional grande pendiente**

Cerrado E4/E5, el ciclo de trabajo del programador (escribir → compilar → probar con recursos) está completo. E6a/E7/E8/E9/E10, el editor de mapa 3D y **GENSPR** meten el editor FPG, el editor de mapas, el editor de paleta, el editor de sonido/PCM, la ayuda, la calculadora, el generador de explosiones, el editor de mapa 3D y el generador de sprites — los nueve ya funcionales. El **motor de renderizado Modo-8 en tiempo real** (`port/vpe/`, distinto del editor del IDE) ya está **portado y verificado estable** (§54 de `12-port-progreso.md`) — lo que queda es la confirmación visual final (ver resumen justo debajo). El **generador de sprites** (`divspr.cpp` + `src/div/visor/`) se investigó y se portó en la misma sesión (§55 de `12-port-progreso.md`) — resumen justo debajo.

**Generador de sprites (GENSPR) — resumen para quien retome** (detalle completo en §55 de `12-port-progreso.md`): `divspr.cpp`+`divsprit.cpp` + `src/div/visor/` (motor 3D propio) portados. El único ensamblador real era `t.asm` (núcleo de mapeado de texturas, reescrito en `port/div/ide/port_ide_genspr_t.c`) más 7 rutinas `#pragma aux` de Watcom más en `visor.cpp`/`llrender.cpp` que no se habían detectado en la investigación previa (aparecieron como errores de enlazado al añadir esos ficheros al build, la última — `lfset` — solo al ejercitar la ruta de rotación) — todas traducidas, ver §55.2-55.3 y §55.7. Bug real de puntero truncado a 32 bits encontrado y arreglado de paso (`direccion_textura`, mismo patrón que los 6 de Modo-8, §54.3). Y un **doble `free()` real del original de 1999** (`ThumbSprite`/`MapaSprite` aliasing en `CargarSprite()`/`FinalizaGenerador()`) que crasheaba al clicar la vista 3D — diagnosticado y arreglado en §55.7, **confirmado por el usuario en el IDE real (2026-09-23)**. **`Mapas → Generador de sprites` funciona de punta a punta**: abre, renderiza el modelo 3D con textura y sombreado Gouraud correctos, y aguanta clics/arrastres repetidos sin crash. Limitación heredada (no del port): `genspr\textura.pcx` es en realidad un JPEG con extensión `.pcx`, y JPG sigue stub (E6a) — así que con el contenido tal cual viene del repo, el generador siempre da "No se reconoce el tipo de fichero" al cargar la textura. Pendiente si se quiere seguir: probar el flujo de "Aceptar" (escribir un sprite en un `.fpg` real vía `CreaSpriteFPG()`), no solo "Cancelar".

**Motor de renderizado Modo-8 (`port/vpe/`) — resumen para quien retome** (detalle completo en §54 de `12-port-progreso.md`): portado en esta sesión. `src/vpe/vpedll.cpp` ya era la implementación real de todos los opcodes Modo-8 (solo hacía falta portarla, ver §54.1); de los 3 `.asm` (`draw_fa.asm`/`draw_oa.asm`/`draw_wa.asm`) solo 7 rutinas de pintado de span necesitaban reemplazo, ahora en C limpio (`port/vpe/draw_span.c`). Se encontraron y arreglaron 6 bugs con AddressSanitizer (`build-asan/`), la mayoría variantes de "puntero truncado a 32 bits" (§54.3) — lección para el siguiente módulo que se porte: grep sistemático de `*4`/`(DWORD)`/`(int)puntero`, no solo confiar en warnings. Verificado con `WSPORTX.PRG`: arranca, navega menús, pinta texturas/paleta/fundidos correctamente, y `start_mode8`/`loop_mode8` corren 20+ segundos seguidos bajo ASan sin ningún error nuevo. **No se llegó a confirmar visualmente el visor 3D de un nivel Modo-8 concreto** porque el juego pide navegar sus menús de selección de deporte antes de entrar al modo que lo usa, y no se automatizó esa navegación — siguiente paso: identificar qué modo de `WSPORTX` (o qué otro juego real) entra en un nivel Modo-8 propiamente dicho y confirmar visualmente paredes/suelo/techo con textura (posible detalle cosmético a revisar de paso: orientación del suelo/techo en `DrawFSpan`, ver §54.5).

**Generador de explosiones del IDE (E10) — resumen para quien retome** (detalle completo en §50 de `12-port-progreso.md`): `diveffec.cpp` (532 líneas, sin dependencias externas) — mismo cierre directo que ayuda/calculadora, pero con un **bug real de signo de `char`** de propina: `Buff_exp` (brillo 0..255) es `char` con signo, así que valores ≥128 se guardan negativos y, usados como índice de `ExpDac[256]` (variable local), leían memoria de pila ajena — píxeles de color aleatorio en la parte brillante de la explosión. Arreglado con un cast a `unsigned char` en el único punto de lectura relevante (`tools/patch_explode_signed_char.py`). **Sospechar de este mismo patrón** (`char` con signo usado como índice/brillo 0-255) si aparece un bug visual similar ("colores aleatorios") al portar otro módulo.

**Calculadora del IDE (E9) — resumen para quien retome** (detalle completo en §49 de `12-port-progreso.md`): `divcalc.cpp` (337 líneas, sin dependencias externas) — declaraciones adelantadas + auditoría de stubs (esta vez SIN ningún array/struct real tapado, solo duplicados directos) — compiló limpio a la primera.

**Ayuda del IDE (E8) — resumen para quien retome** (detalle completo en §48 de `12-port-progreso.md`): `divhelp.cpp` no tenía dependencias externas, así que el cierre fue directo (declaraciones adelantadas + auditoría de stubs, compiló limpio a la primera). Pendiente cosmético sin confirmar: el texto se ve grande, posible doble escalado por `big2` en `vuelca_help()` (`divhelp.cpp:1126`, `(v.an*(10+16)+2)*big2` — comparar con la línea anterior, que sí divide `v.an` por `big2` antes de usarlo) — `divhelp.cpp` es código de 1999 que nunca pasó por el trabajo de "letra proporcional" (§41) porque no estaba en el build entonces.

**Audio del IDE (E7) — resumen para quien retome** (detalle completo en §47 de `12-port-progreso.md`):

- `divmixer.cpp`/`divsb.cpp` son irrelevantes para el port (hardware ISA real de 1999); no se portan.
- El shim vive en `port/div/ide/port_ide_judas.c` (nuevo, ~600 líneas), apoyado en `port/io/io_audio.c`/`io_song.c` (compartidos con el runtime, sin cambios de comportamiento ahí salvo `io_poll()`, ver más abajo).
- `judascfg_device` es `DEV_SB` en el IDE (antes `DEV_NOSOUND` fijo) — desbloquea reproducción/preescucha, bloquea grabación (requiere `DEV_SBPRO`/`DEV_SB16` explícitamente).
- **Si se porta `divmap3d.cpp`/sprites y aparece un crash nuevo**: repetir la auditoría de stubs de `port_ide_stubs.c` (método de §45.1) contra los símbolos que esos ficheros necesiten — el patrón `void *NOMBRE = NULL;` tapando algo real ha aparecido en mapas/paleta/audio/ayuda (no en calculadora, que salió limpia — no es una garantía, solo una tendencia).
- `port/io/io_input.c` (`io_poll()`) ahora también llama a `PollInputEvents()` — si un bucle de espera activa nuevo (patrón DOS `while(mouse_b&1) ...;`) se cuelga igual que el de `PCM2()` (§47.6), sospechar primero de esto antes de re-investigar desde cero.

**Verificación de E2.3 que quedó pendiente**: la ruta está probada con teclas sintéticas (letras, mayúsculas, F1, flechas, Enter, `CTRL+L`, `ALT+X`), pero **no** con acentos, `ñ` ni `ALT GR` físico en un teclado español real. El filtro de ALT GR está implementado (ver §29.6) pero sin confirmar en caliente.

**Ojo con los artefactos de ejecución**: al salir, el IDE reescribe `system/SETUP.BIN` con el layout de MSVC (3560 bytes, frente a los 2052 del original de Watcom que hay versionado) y crea `system/session.dtf`. Tras probar el IDE hay que hacer `git checkout -- system/SETUP.BIN` para no commitear ese cambio; `session.dtf` está en `.gitignore`. El original de 2052 bytes se conserva a propósito: `port_setup_bin_valido()` lo rechaza por tamaño y fuerza los defaults de primera ejecución (640×480 y la paleta de `div.cpp:3763`), que es justo lo que hace que el escritorio se vea bien. Ver §27 de `12-port-progreso.md`.

**Herramientas de verificación disponibles** (no hay depurador en la máquina):

- `DIV_IDE_SHOT=f.png` [+ `DIV_IDE_SHOT_FRAME=N`] → vuelca un frame, imprime un resumen por stderr y sale con 0.
- `DIV_IDE_SHOT_EVERY=N` → modo ráfaga: `f_NNN.png` cada N **volcados** (no cada N ms; si nada cambia en pantalla, no hay volcado) sin terminar el proceso.
- `DIV_IDE_KEYLOG=1` → vuelca por stderr cada evento de teclado entregado (`[key] ascii/scan/shift/reloj`) y cada transición de `kbdFLAGS` (`[flg]`). Es la forma de verificar el teclado sin depurador.
- `tools/drive_ide.ps1`: lanza el IDE e inyecta **clics y teclas** sintéticos. Ejemplos:
  `powershell -File tools\drive_ide.ps1 -Clicks "25,472;40,30"`
  `powershell -File tools\drive_ide.ps1 -Keys "alt+x"`
  `powershell -File tools\drive_ide.ps1 -Clicks "25,472" -Keys "down;down;enter"`
  Convierte coordenadas con la escala que saca de `GetClientRect`/`ClientToScreen`.
- **Gotcha 1**: si el usuario está usando la máquina, sus movimientos y clics reales contaminan la prueba. Comprobar siempre `GetCursorPos` después de `SetCursorPos`.
- **Gotcha 2**: `CTRL+ESC` (`div.cpp:1235`) **no sirve** como prueba sintética de salida: Windows lo intercepta para abrir el menú Inicio y la tecla nunca llega a la aplicación. Usar `ALT+X`.
- Crash sin depurador: Visor de eventos → offset; recompilar con `/MAP`; resolver sobre `build/Release/div_ide_port.map` sumando la base `0x10000000`; `dumpbin /disasm:nobytes /section:.text`.

**Reglas del usuario que siguen vigentes**:

- Los ficheros de `src/` son **CP850**: editarlos **solo** con parches byte-exactos en Python, nunca con herramientas de edición de texto (incidente U+FFFD, §24.3).
- No tocar `i.cpp`/`f.cpp`/`kernel.cpp`/`inter.h` salvo que sea imprescindible, y siempre con comentario `/* PORT: ... */`.
- Ficheros del port pequeños y modulares, nunca "mega ficheros".
- Documentar cada subhito en los `.md` y cerrar con commit + tag, para poder cambiar de modelo/herramienta sin perder contexto.

## 3. Cómo construir y ejecutar (comandos verificados)

```powershell
# Configurar (una vez; RAYLIB_PATH tiene un default en CMakeLists.txt, pasar -DRAYLIB_PATH=... si no coincide)
cmake -B build .

# Compilar el runtime completo
cmake --build build --target div32run_port --config Release

# El binario queda en build/Release/div32run_port.exe (raylib.dll se copia automaticamente al lado)
```

Ejecutar sin argumentos imprime el error real de DIV32RUN y sale (comportamiento original, no un bug):
```
build/Release/div32run_port.exe
```

Para ejecutar un bytecode de prueba (ver §5.5 sobre por qué hace falta una carpeta con estructura `fpg\`/`pcm\`):
```powershell
cd build/test_game        # carpeta de prueba, NO versionada (vive dentro de build/, ignorado por git)
../Release/div32run_port.exe test_sprite_demo.div32
```

Otros targets de CMake:
- `div_core` — biblioteca `OBJECT` con todo el núcleo portado (no enlazable por sí sola).
- `div32run_port` — el runtime portado.
- `divc_port` — compilador CLI portado.
- `div_ide_port` — **IDE portado (E2.3: arranca, pinta el escritorio y responde a ratón y teclado)**. Necesita `system\`, `help\` y `resource\` junto al `.exe` (junctions en `build/Release/`). Con `DIV_IDE_SHOT=fichero.png` (y opcional `DIV_IDE_SHOT_FRAME=N`) vuelca un frame a disco y termina — útil para verificar el render sin mirar la pantalla. Con `DIV_IDE_SHOT_EVERY=N` pasa a modo ráfaga: escribe `fichero_NNN.png` cada N volcados y **no** termina, para seguir una secuencia de interacción completa.
- `div_smoke_test` — ejecutable de diagnóstico que ejercita vídeo/ratón/teclado directamente, sin intérprete de bytecode.
- `div_port` — esqueleto mínimo de la Fase 1 original (capa `port/io` sola, sin el núcleo DIV).

## 4. Scripts para generar bytecode DIV de prueba (no hay compilador todavía)

No existe compilador funcional en este port (`divc.cpp` no está portado), así que cualquier `.div32` de prueba se construye a mano con Python, replicando el formato EML byte a byte. Tres scripts existentes, cada uno más rico que el anterior, todos en `port/div/core/`:

| Script | Qué genera | Para qué sirve |
|---|---|---|
| `build_test_prg.py` | Un proceso con un único opcode `lret` | Prueba mínima del *scheduler* — termina en el primer frame |
| `build_frame_demo.py` | 180× `lfrm` (FRAME) + `lret` | Confirma que la ventana se queda abierta y se cierra sola (~7.5s a 24fps) |
| `build_sprite_demo.py` | `load_fpg()` + asigna `_Graph`/`_X`/`_Y` del proceso + 240× `lfrm` + `lret` | Confirma carga de recursos reales y render de sprite — primero verificado visualmente |
| `build_sound_demo.py` | `load_fpg` + `load_pcm` + `sound()` + 240× `lfrm` + `lret` | Confirma audio real (PCM bruto sin RIFF, con fallback) — verificado auditivamente; incluye sintetizador del `.pcm` |
| `build_input_demo.py` | `load_fpg` + `key()` (ESC y flechas) + `clear_screen()` + bucle con `ljpf`/`ljmp`/`lada`/`lsua` | Confirma input desde bytecode (sprite que sigue a las flechas, ESC cierra) — primer bytecode con ramas y aritmética reales |
| `build_collision_demo.py` | `load_fpg` + proceso hijo creado con `lcal` (parámetros por `lcar2`/`lcbp`/`lcaraidcpa`, tipo con `ltyp`) + `collision()` + `signal()` | Confirma colisiones desde bytecode: la nave (tipo 1) avanza lenta hacia el fantasma (tipo 2); al solaparse las cajas el programa se auto-cierra (~3 s) |
| `build_api_demo.py` | `set_mode` + `load_fpg` + `load_fnt` + `write`/`write_int`/`delete_text` + `put_pixel`/`random` + `fade`/`fading` + `let_me_alone`/`get_id` + hijo con `get_distx`/`get_disty` | Confirma el repertorio de builtins de un juego real (tipo STEROID): estrellas, textos con fuente real, anillo trigonométrico, fade y auto-cierre (~7 s). La cabecera del script trae el programa DIV equivalente |

Además existen reproducers de depuración no listados aquí (`build_min_debug.py`, `build_min2_debug.py`, `build_min3a/b/c_debug.py`) que se usaron para biseccionar un crash `0xC0000005` que resultó no reproducible — ver §22 de `12-port-progreso.md`.

Todos escriben el `.div32` en el directorio desde el que se ejecutan (ruta relativa hardcodeada en cada script). **Si vas a escribir un script nuevo**, la sección §5 de este documento tiene todo lo necesario sin tener que releer código fuente de cero.

## 5. Conocimiento técnico crítico (para no re-investigar)

Esta sección es el resumen destilado de investigación real hecha leyendo `divc.cpp`, `i.cpp`, `kernel.cpp`, `f.cpp` e `inter.h` — no está en ningún sitio más condensado que aquí.

### 5.1 Formato del fichero `.div32` (bytecode compilado)

```
[602 bytes: stub original, todo ceros para pruebas manuales]
[40 bytes: cabecera de 10 enteros LE — mem[0..8] + n]
[4+ bytes: payload comprimido con zlib (compress()/uncompress())]
```

La cabecera de 9 campos (`mem[0..8]`), más el 10º campo `n` (tamaño en bytes del payload **descomprimido**):

| Campo | Nombre | Significado |
|---|---|---|
| `mem[0]` | `program_type` | 0 = normal |
| `mem[1]` | entry point | `_IP` inicial del proceso "main" — índice de `mem[]` del primer opcode a ejecutar |
| `mem[2]` | `iloc` | offset del "molde" de 44 campos públicos (ver §5.4) — **no** es la ubicación del proceso en ejecución |
| `mem[3]` | `max_process` | 0 = heurística automática del runtime |
| `mem[4]` | — | sin uso confirmado en la carga |
| `mem[5]` | `iloc_priv` (nº de campos "privados") | 0 si no hay `PRIVATE` |
| `mem[6]` | `iloc_pub_len` (nº de campos "públicos") | mínimo 44 (ver §5.4) |
| `mem[7]` | — | sin uso confirmado en la carga |
| `mem[8]` | `imem` final | "longitud del cmp" — código+locales+textos; de aquí arranca la asignación dinámica de nuevos procesos en tiempo de ejecución |
| `n` (10º campo, 40º-44º byte) | tamaño descomprimido | bytes exactos del payload tras `uncompress()` |

Payload descomprimido = `mem[9 .. mem[8]-1]` concatenado (código + datos + molde), en ese orden de bytes.

### 5.2 Layout de `mem[]`: por qué el código de usuario NO empieza en `mem[9]`

`long_header = 9`. Justo después de la cabecera, **antes** de cualquier dato/código de usuario, el compilador real (`precarga_obj()` en `divc.cpp`, que parsea `system\ltobj.def`) reserva un bloque fijo de **1588 palabras** para las "globales de sistema" que expone el lenguaje DIV (`mouse`, `scroll`, `m7`, `joy`, `setup`, `net`, `m8`, `dirinfo`, `fileinfo`, `video_modes`, `timer[]`, `text_z`, `fading`, `shift_status`, `ascii`, `scan_code`, `joy_filter`, `joy_status`, `restore_type`, `dump_type`, `max_process_time`, `fps`, `argc`, `argv[]`, `channel[]`, `vsync`, `draw_z`, `num_video_modes`, `unit_size`). Confirmado exactamente en `inter.h`:

```c
#define end_struct long_header+14+10*10+10*7+8+11+9+10*4+1026+146+32*3   // = long_header + 1520
#define unit_size mem[end_struct+67]   // el ultimo campo, 68 palabras despues de end_struct
```

**El primer hueco libre real para datos/código de usuario es `mem[9 + 1588] = mem[1597]`, no `mem[9]`.** Escribir código de usuario en `mem[9]` corrompe silenciosamente el struct `mouse` (que `inicializacion()` apunta ahí) y provoca un crash dentro del *scheduler*, no un error obvio de carga.

### 5.3 El "molde" (`iloc`, `mem[2]`) — un bloque de valores por defecto, no la posición del proceso

`inicializacion()` (`i.cpp` líneas ~213-229) copia con `memcpy` los `iloc_pub_len` (`mem[6]`) campos desde `mem[iloc]` hacia el nuevo proceso, y **después** fija explícitamente `_Id`, `_IdScan`, `_Status`, `_IP` (usando `mem[1]` como `_IP` inicial de "main"). Cualquier otro campo se queda con el valor que tuviera en el molde.

### 5.4 Los 44 campos de proceso — offsets e importantísimo: valores por defecto NO nulos

`inter.h` define los offsets (`_Campo`) 0 a 43 (44 campos en total, confirmado por conteo exacto):

```
0  _Id            10 _Painted       20 _Father        30 _Flags         40 _M8_Wall
1  _IdScan        11 _Dist1/_M8_Object  21 _Son        31 _Size          41 _M8_Sector
2  _Bloque        12 _Dist2/_Old_Ctype  22 _SmallBro   32 _Angle         42 _M8_NextSector
3  _BlScan        13 _Frame        23 _BigBro         33 _Region        43 _M8_Step
4  _Status        14 _x0           24 _Priority       34 _File
5  _NumPar        15 _y0           25 _Ctype          35 _XGraph
6  _Param         16 _x1           26 _X              36 _Height
7  _IP            17 _y1           27 _Y              37 _Cnumber
8  _SP            18 _FCount       28 _Z              38 _Resolution
9  _Executed      19 _Caller       29 _Graph          39 _Radius
```

`system\ltobj.def` (sección `local`) declara **valores por defecto no nulos** para varios de estos campos. Si construyes un molde a mano, tiene que llevarlos o el comportamiento será incorrecto de formas no obvias (p.ej. un sprite invisible sin ningún error):

```
offset 4  (_Status)         = 2   (vivo) -- irrelevante si inicializacion() lo fija explicito, pero ponlo igual
offset 11 (_M8_Object)      = -1
offset 31 (_Size)           = 100  -- "% de tamaño del gráfico"; en 0 el sprite es INVISIBLE sin error
offset 36 (_Height)         = 1
offset 40 (_M8_Wall)        = -1
offset 41 (_M8_Sector)      = -1
offset 42 (_M8_NextSector)  = -1
offset 43 (_M8_Step)        = 32
```

Todos los demás campos por defecto son 0. Ver `build_sprite_demo.py` para una implementación de referencia.

### 5.5 Recursos (`.fpg`/`.pcm`/etc.): la ruta que pasas al builtin se IGNORA

`open_file()` en `f.cpp` (versión de producción, sin `#ifdef DEBUG`) hace `_splitpath()` sobre la ruta que le des y **se queda solo con nombre+extensión**, reconstruyendo la ruta real como `<extensión_sin_punto>\<nombre>.<extensión>`, relativa al directorio de trabajo del proceso. Esto es el mecanismo **real y original** con el que DIV organiza los recursos de un juego compilado (subcarpetas `fpg\`, `pcm\`, `map\`, `fnt\` junto al ejecutable) — no un bug del port.

**Consecuencia práctica**: para que `load_fpg("archivo.fpg")` funcione, tiene que existir `<directorio_de_trabajo>\fpg\archivo.fpg`.

**PORT (E5)**: el port **no** usa esa variante, sino la del build `DEBUG` (`SESSION.386`, el binario que el IDE de DOS lanzaba al "Probar"), activada con `PORT_OPEN_FILE_SEARCH` en `port_pre.h`. Es un superconjunto: prueba la **ruta literal** primero, luego `<ext>\<ruta>`, luego `<nombre>.<ext>` y por último `<ext>\<nombre>.<ext>`. Gracias a eso `load_fpg("tutorial\tutor0.fpg")` resuelve `fpg\tutorial\tutor0.fpg`, que es como están organizados los recursos de una instalación real de DIV. Además, si toda la cadena falla se reintenta sobre las raíces de la variable de entorno **`DIV_RES_PATH`** (lista separada por `;`, implementada en `port/div/core/port_res_path.c`); el IDE la rellena con la carpeta del `.div32`, la raíz del repo y `<raíz>\resource`. Ver §32 de `12-port-progreso.md`.

### 5.6 ISA del intérprete (opcodes) — tabla completa de los usados hasta ahora

El opcode ocupa el **byte bajo** de una palabra de 32 bits en `mem[]` (`switch((byte)mem[ip++])` en `nucleo_exec()`, `i.cpp`). Cada `case` de `kernel.cpp` documenta si consume palabras de operando adicionales.

| Opcode | Valor | Operandos | Efecto |
|---|---|---|---|
| `lnop` | 0 | — | no-op |
| `lcar` | 1 | 1 palabra (valor) | `pila[++sp]=valor` |
| `lasi` | 2 | — | pop addr (en `sp-1`) y valor (en `sp`, pusheado después): `mem[addr]=valor`; deja `valor` en la pila |
| `laid` | 20 | — | `pila[sp]+=id` (convierte un offset de campo en una dirección real, sumando el id del proceso actual) |
| `lcid` | 21 | — | `pila[++sp]=id` |
| `ljmp` | 23 | 1 palabra (dirección absoluta) | `ip=mem[ip]` — salto incondicional |
| `ljpf` | 24 | 1 palabra (dirección absoluta) | si el valor en `pila[sp--]` es falso (bit 0 a 0), salta; si no, `ip++` |
| `lfun` | 25 | 1 palabra (código de función) | invoca `function()` (`f.cpp`), que lee el código con su propio `mem[ip++]` |
| `lret` | 27 | — | auto-elimina el proceso actual (fin de "main" = fin del programa si no hay más procesos) |
| `lasp` | 28 | — | `sp--` (descarta el valor superior de la pila) |
| `lfrm` | 29 | — | **FRAME**: cede la ejecución del proceso hasta el próximo fotograma |
| `ladd`/`lsub` | 12/13 | — | `pila[sp-1]=pila[sp-1]±pila[sp]; sp--` — binaria, deja el resultado |
| `lptr` | 18 | — | `pila[sp]=mem[pila[sp]]` — lee el contenido de una dirección |
| `lada`/`lsua` | 42/43 | — | `pila[sp-1]=mem[pila[sp-1]]+=pila[sp]` (o `-=`); empí­jese la dirección primero |
| `lcal` | 26 | 1 palabra (dirección **absoluta** del código del proceso) | crea un proceso: `mem[_IP]=ip+1`, `ip=mem[ip]` (el operando ES el código, no un puntero intermedio), copia el molde `iloc` al hueco (si `mem[id+_Status]` en `plantilla[4]` no es 2, el proceso nace muerto), el hijo corre hasta su primer `lfrm` y devuelve el control al padre dejando su id en la pila (el padre hace `lasp`) |
| `lcbp` | 30 | 1 palabra (num_par) | `mem[_NumPar]=x; mem[_Param]=sp-x+1` — fija la pila de parámetros del proceso (imprescindible para que su `lfrm` restituya bien la pila) |
| `ltyp` | 32 | 1 palabra (tipo) | `mem[_Bloque]=tipo` — el identificador de proceso que usa `collision()` |
| `lcar2` | 60 | 2 palabras (dos constantes) | `pila[++sp]=a; pila[++sp]=b` — empuja dos constantes (aquí: los argumentos del proceso que crea `lcal`) |
| `lcaraidcpa` | 68 | 1 palabra (offset de campo) | `mem[campo+id]=pila[_Param++]` — leer el parámetro n-ésimo directamente en un campo (aquí: `x=arg1`, `y=arg2`) |

Tabla completa (127 opcodes, incluidas variantes fusionadas por el optimizador del compilador) en `inter.h` líneas ~154-293 si hace falta algo no listado aquí (aritmética, cadenas, `switch`, bytes/words, etc.).

### 5.7 Cadenas de texto en bytecode

Un "string" es simplemente un **entero = índice de `mem[]`**; su contenido, reinterpretado como bytes (`(char*)&mem[offset]`), es una cadena C terminada en NUL. No hace falta ninguna marca especial para que builtins como `load_fpg`/`load_pcm` la lean (la marca `0xDAD0...` que usa `nullstring[]` solo la necesitan los opcodes de manipulación de cadenas — `lstrcpy`/`lstrcat`/etc. — no la carga de ficheros). Basta con colocar los bytes ASCII+NUL en cualquier hueco libre de `mem[]` (word-alineado) y pasar su índice con un `lcar` normal.

### 5.8 Tabla de funciones internas (`lfun <código>`) — las usadas hasta ahora, y dónde ver el resto

La tabla completa de ~165 builtins está en `system/ltobj.def` (líneas `function NNN tipo nombre(...)`) y se implementa en el switch gigante de `function()` en `f.cpp` (~línea 4245). Convención de paso de argumentos: se empujan con `lcar` en **orden izquierda a derecha**, y la implementación los desapila en orden inverso (`pila[sp--]` para el último argumento primero); el primer argumento queda en `pila[sp]` al final como "slot de retorno" (la función suele sobreescribirlo con el valor de retorno).

| Código | Nombre | Firma | Usado en |
|---|---|---|---|
| 0 | `signal` | `(tipo, código)` — `s_kill=0` pone `_Status=1` (muerto) a todos los procesos del tipo | `build_collision_demo.py` |
| 1 | `key` | `(scan_code)` → 1 mientras esté pulsada | `build_input_demo.py` |
| 3 | `load_fpg` | `(fichero)` → id de fpg | `build_*_demo.py` |
| 8 | `collision` | `(tipo)` → id del proceso de ese tipo con el que se solapa la caja (0 si ninguno) | `build_collision_demo.py` |
| 9 | `get_id` | `(tipo)` → id del primer proceso vivo del tipo (0 si ninguno; ojo: devuelve dirección par, testear con `ligu`/`ldis`, no con `ljpf` directo) | `build_api_demo.py` |
| 10/11 | `get_distx`/`get_disty` | `(ángulo, distancia)` → incremento en X/Y (ángulo en unidades DIV: vuelta = 360000) | `build_api_demo.py` |
| 14 | `fade` | `(r,g,b,speed)` — la global `fading` (`mem[1540]`) vale 1 mientras dure | `build_api_demo.py` |
| 15 | `load_fnt` | `(fichero)` → id de fuente (slot libre desde 1); exige `fnt\<nombre>` bajo el cwd (§5.5) | `build_api_demo.py` |
| 16/17 | `write`/`write_int` | `(font,x,y,centro,ptr)` → id de texto; `ptr` = índice de `mem[]` de la cadena o del entero | `build_api_demo.py` |
| 18 | `delete_text` | `(id)` — 0 = todos | `build_api_demo.py` |
| 21 | `random` | `(min,max)` → entero en rango (el `rand` del lenguaje) | `build_api_demo.py` |
| 24 | `put` | `(file,graf,x,y)` → id de gráfico | no usado directamente — se prefiere el render automático de proceso (§5.9) |
| 28 | `put_pixel` | `(x,y,color)` — escribe en el buffer `copia2` | `build_api_demo.py` |
| 33 | `clear_screen` | `()` → 0 (¡sin argumentos!) | `build_input_demo.py` |
| 36 | `set_mode` | `(código_modo)` — reasigna `copia`/`copia2` **y redimensiona la ventana en caliente** (§34); modos no en `video_modes[]` caen al fallback 640×480 | `build_api_demo.py` |
| 37 | `load_pcm`/`load_wav` | `(fichero,loop)` → id de sonido | `build_sound_demo.py` |
| 39 | `sound` | `(id_sonido,volumen 0..256,frecuencia 0..256)` → id de canal | `build_sound_demo.py` |
| 66 | `let_me_alone` | `()` — mata a todos los procesos excepto el llamante | `build_api_demo.py` |

**Ojo al verificar demos automatizados**: `e()` (errores no críticos) imprime `Error NNN ...` y hace `exit(26)` — el mismo código de salida que la terminación normal. Hay que mirar stdout o medir la duración para distinguirlos (ver §22.3 de `12-port-progreso.md`).

### 5.9 Render automático de proceso — la forma "normal" de mostrar un gráfico

Un proceso con `_Ctype==0` (por defecto) y `_Graph>0` se pinta **solo**, cada fotograma, sin que el bytecode llame a ningún builtin de dibujo — confirmado leyendo `frame_end()` (`i.cpp` ~975-1059): hay un bucle que ordena por `_Z` todo lo pintable (procesos, scrolls, modo-7, textos, ratón, *drawings*) y llama a `pinta_sprite()` (`s.cpp`) para el ganador en cada iteración. Para mostrar algo, basta con fijar `_File`/`_Graph`/`_X`/`_Y`/`_Z` del proceso (vía `lcar <campo>; laid; lcar <valor>; lasi; lasp;`, ver `build_sprite_demo.py`) — así es como funciona un juego DIV real (los procesos SON los sprites).

## 6. Bugs reales encontrados y arreglados durante el port

| Bug | Causa | Fix | Detalle |
|---|---|---|---|
| Truncamiento de punteros a 32 bits en x64 (`FILE*`, pilas guardadas, `mem[]`, `divmalloc`) | El original en DOS/32-bit truncaba punteros a `int` a propósito (mismo tamaño) | `intptr_t`/`uintptr_t` donde corresponde, o tablas de *handles* de 256 entradas donde el valor tiene que caber en una celda de `mem[]` (`port_native_handles.h`) | `12-port-progreso.md` §7 |
| El limitador de fotogramas reiniciaba el audio en cada frame | `reloj` (el "tick" de temporizador) nunca se incrementaba — la IRQ de temporizador real de DOS no está en este repo restaurado; además, el umbral de 60000 iteraciones "¿temporizador congelado?" estaba calibrado para CPUs de 1998 | `port_get_reloj()` (reloj real, `QueryPerformanceCounter`) + `port_frame_yield()` (`Sleep(1)` por vuelta del spin) | `12-port-progreso.md` §16 (checkpoint 9) |
| Sprite invisible aunque todo lo demás funcionase | Molde de 44 campos a cero — `_Size=0` = "0% de tamaño" | Molde con los defaults reales de `ltobj.def` (ver §5.4 de este documento) | `12-port-progreso.md` §17.3 (checkpoint 10) |
| `Error 105 (load_fpg)` con una ruta de fichero que sí existe | `open_file()` en producción ignora la ruta pasada (ver §5.5) | No es un bug — pasar solo el nombre de fichero y ejecutar con el directorio de trabajo correcto | `12-port-progreso.md` §17.4 |
| **Audio mudo aunque la pila del bridge funcionase** | Los nombres de la API de audio del runtime DIV (`LoadSound`/`PlaySound`/`StopSound`/`UnloadSound`) coinciden con los de raylib; sin dllimport, las llamadas de `io_audio.c` se enlazaban contra `divsound.obj` (la struct `Sound` se descomponía en ints basura) | Doble fix: `USE_LIBTYPE_SHARED` en los targets raylib (su API pasa a resolver por `__imp_*` contra raylib.dll) + macros de renombrado de los 4 símbolos en `port_pre.h` | `12-port-progreso.md` §19 (checkpoint 11) |
| Los disparos de STEROID no colisionaban | Los ids de tipo de proceso son `(int)(struct objeto*)` del compilador (`divc.cpp:4057`); en x64 pueden ser **negativos**, y la `collision()` del port abría con `if (tipo<=0) return 0` (la original solo especial-casa `==0`=ratón). El demo del checkpoint 13 coló porque usaba ids 1/2 hechos a mano | `tipo<=0` → `tipo==0` en `s.cpp` | `12-port-progreso.md` §36 |
| MALVADO: el personaje saltaba sin parar | El shim `inp(0x201)` (joystick) devolvía 0; el hardware real sin joystick lee **0xFF** (bits en alto). Con 0, `read_joy()` veía `joy.button1=1` permanente y el detector de ausencia de `joy_position()` salía al instante, así que el joystick fantasma nunca se desactivaba | `div_port_inp(0x201)` devuelve 0xFF en `port/div/shim/i86.h` → botones a 0 y autodesactivación a los 6 frames | `12-port-progreso.md` §37 |
| MALVADO: al morir no restaba vidas ni reiniciaba | `out_region()` era un stub vacío que no tocaba la pila: devolvía el argumento `región` (0) y el `REPEAT..UNTIL` de `muerte_jack()` no terminaba nunca | Implementación real en `s.cpp`, portada de `c.cpp:30-109` (caja con rotación/escalado/pivote/flags, scroll y pantalla, `_XGraph`) | `12-port-progreso.md` §38 |
| IDE: el browser petaba navegando directorios con nombres largos | `dir_abrirbr` (`divbrow.cpp:1808/1816`) hace `strcpy` a slots de **13 bytes** (nombres 8.3 de la era DOS); el shim devolvía nombres largos de Windows → overflow | El shim `_dos_findfirst/_dos_findnext` (`port_ide_dos.c`) devuelve nombres estilo DOS: alias 8.3 real en mayúsculas; los no representables se omiten. **OJO: queda abierta la petada al subir hacia la raíz** (ver §2.1, bug abierto 1) | E6a (WIP `4917a42`) |
| TOKENKAI: el personaje no se movía (y "el teclado no respondía") | `path_find`/`path_line`/`path_free` eran stubs vacíos en `port_stubs.c`: no consumían `pila[]` ni devolvían nada → 0 puntos de ruta y pila desincronizada (además TOKENKAI se mueve con el ratón, no con el teclado — el malentendido del síntoma) | Port fiel del módulo original en `port/div/core/ia.c` (base `src/div32run/ia.cpp`) | `12-port-progreso.md` §40 |

## 7. Riesgos y limitaciones conocidas sin resolver

- **`divmalloc`/`memory_free()` en x64** (`f.cpp`): la resta de punteros para simular "offset dentro de `mem[]`" no tiene garantía matemática en x64 si el heap del proceso crece mucho. Riesgo documentado, no bug activo confirmado. Ver `12-port-progreso.md` §7.4.
- **`ascii`/`scan_code` en el RUNTIME** (`div32run_port`, macros sobre `mem[]`) se quedan siempre a 0: `src/div/mouse.cpp` usa *polling* por estado (`io_key_down`), sin cola de eventos. No afecta a `key()`/`KEY()` (lo más común), sí a la entrada de texto desde bytecode. **En el IDE esto ya está resuelto** (E2.3, §29): `port_ide_keybo.c` tiene cola de eventos real. Si algún día hace falta en el runtime, la pieza a reutilizar es `port/io/io_keyboard.c`. Ver `12-port-progreso.md` §11.2.
- **Warning `LNK4098`** (conflicto de biblioteca de ejecución `LIBCMT`) al enlazar `div32run_port` — inocuo en la práctica (el binario corre bien), no investigado a fondo.

## 8. Backlog priorizado

1. ~~**(Bajo esfuerzo) Extender el bytecode de prueba para llamar a `load_pcm`+`sound` y confirmar audio real**~~ — **HECHO (checkpoint 11)**: `build_sound_demo.py` + confirmación auditiva.
2. ~~**(Bajo esfuerzo) Confirmar input desde bytecode**~~ — **HECHO (checkpoint 12)**: `build_input_demo.py` (`key()` + flechas + ESC), confirmado por el usuario. **El círculo gráficos + input + sonido del runtime queda cerrado y verificado.**
3. ~~**(Bajo esfuerzo) Portar la pieza viva de "gestión de objetos": `collision()`**~~ — **HECHO (checkpoint 13)**: `build_collision_demo.py` (dos procesos, hijo por `lcal`, tipos 1 y 2, `signal` para cerrar), el programa se auto-cierra al chocar (~3 s). Ver §21 de `12-port-progreso.md`.
4. ~~**(Bajo esfuerzo) Ejercitar las builtins de un juego real**~~ — **HECHO (checkpoint 14)**: `build_api_demo.py` (`set_mode`/`load_fnt`/`write`/`write_int`/`delete_text`/`put_pixel`/`random`/`fade`+`fading`/`let_me_alone`/`get_id`/`get_distx`/`get_disty`), exit 26 en ~7 s, estable 5/5. El crash `0xC0000005` heredado de la sesión anterior resultó **no reproducible** (era casi seguro un estado intermedio del bytecode del script en edición — ver §22 de `12-port-progreso.md`). Pulido opcional restante: `put`/`xput` (024), mover el ratón con `mouse` desde bytecode; nada bloquea lo siguiente.
5. ~~**(Bajo esfuerzo) Confirmar Modo-7 con un juego real y añadir reproducción de vídeo FLI**~~ — **HECHO 2026-09-22**: Modo-7 ya funcionaba (SPEED.PRG, sin trabajo adicional); FLI nuevo (`port_topflc.c` + `divfli.cpp`), confirmado con `INTRO.FLI` real. Ver §51 de `12-port-progreso.md`.
6. **(Medio)** Revisar los warnings `C4028` pendientes en `v.cpp` (inconsistencia `char*`/`byte*` preexistente en el original, documentada como inocua pero no verificada a fondo) y el riesgo de `divmalloc`/`memory_free()` en x64 (§7). También verificar que los builds con `-DDEBUG` siguen enlazando tras el define `USE_LIBTYPE_SHARED` (checkpoint 11).
6. **(Grande, no dimensionado)** Motor Modo-8 — solo si se decide que vale la pena antes del editor; dudoso valor si el objetivo final es el IDE, no juegos 3D.
7. ~~**(Grande)** Portar el compilador (`divc.cpp`, ~6700 líneas, vive en `src/div`)~~ — **HECHO (checkpoints 15-18)**: spec EML en [`14-formato-div32-eml.md`](14-formato-div32-eml.md); `divc_port` (driver CLI en `port/div/compiler/`) compila el corpus completo de tutoriales (11/11) y el bytecode ejecuta en el runtime. Detalle y bugs x64 encontrados en §23-25 de `12-port-progreso.md`. Pendiente opcional de endurecimiento: `PRIVATE`/`STRUCT`/punteros/strings-dato extensivos, programas grandes reales (STEROID), `IMPORT` (stub con error 63 deliberado — DLLs fuera de alcance). **Regla tras el incidente U+FFFD (§24.3): los ficheros derivados de `src/` (CP850/latin-1) se editan SOLO con parches byte-exactos en Python, nunca con la herramienta `edit`.**
8. ~~**(Grande)** Portar el editor/IDE (`div.cpp`/`divwindo.cpp`/`divedit.cpp`/`divpaint.cpp`)~~ — **E1 HECHO** (checkpoint 19), **E2.1** (vídeo, §27), **E2.2** (ratón, §28), **E2.3** (teclado + reloj, §29), **E3 HECHO** (browser de ficheros + editor de texto/fuentes, §30), **E4 HECHO** (Compilar F11 / Probar F10-F12 sin salir del IDE, §31), **E5 HECHO** (recursos al Probar: cadena permisiva de `open_file()` + `DIV_RES_PATH`, §32). **El ciclo de trabajo del programador está cerrado.** **E6a WIP commiteado (`4917a42`)**: editor FPG + importación de imágenes (PCX/BMP/MAP). Bug del browser al subir de directorio **cerrado 2026-09-21** (`strupr(NULL)` en ficheros sin extensión); el editor de mapas ya no crashea al entrar (`M3D_crear_thumbs`/`calculadora` eran datos, no funciones) pero sigue sin ser funcional (pantalla negra, crash al salir con ESC) — diagnóstico completo en §43 de `12-port-progreso.md`. Después: resto de editores de recursos (paleta/paint ya compilan, PCM necesita audio del IDE, ayuda, sprites) y audio del IDE (`divmixer`/`divsb`).

## 9. Mapa de ficheros relevantes

```
port/
├── io/                          # Capa de I/O sobre raylib (Fase 1, no tocada en checkpoints 8-10)
│   ├── div_io.h                 # Contrato de la API (io_video_*, io_audio_*, io_input_*, io_timer_*)
│   ├── io_video.c / io_audio.c / io_input.c / io_timer.c
│   ├── io_keyboard.c            # Cola de eventos de teclado estilo INT 16h (E2.3)
│   ├── io_keymap.h / io_keymap.c # raylib <-> scancode set 1, y Unicode -> CP850 (E2.3)
├── div/
│   ├── core/                    # El núcleo DIV32RUN portado
│   │   ├── i.cpp                # Incluye kernel.cpp; tiene el main() real de DIV32RUN
│   │   ├── kernel.cpp           # Los opcodes de la VM (no compilable solo, se #include-a)
│   │   ├── f.cpp                # Los builtins del lenguaje (function())
│   │   ├── s.cpp                # Render de sprites/scroll
│   │   ├── v.cpp / det_vesa.cpp # Bridge de vídeo (REESCRITO contra port/io)
│   │   ├── mouse.cpp / divkeybo.cpp   # Bridge de input (REESCRITO contra port/io)
│   │   ├── divsound.cpp         # Bridge de sonido (REESCRITO contra port/io)
│   │   ├── divlengu.cpp         # Textos/idioma
│   │   ├── divfli.cpp           # Vídeo FLI/FLC (copia byte a byte de src/div32run/divfli.cpp)
│   │   ├── port_topflc.c        # Decodificador FLI/FLC nuevo, API compatible con TopFLC v1.0
│   │   ├── port_stubs.c         # Todo lo NO portado (Modo-8, DLLs, CD, red, objetos)
│   │   ├── port_res_path.c      # Raices extra de busqueda de recursos, DIV_RES_PATH (E5)
│   │   ├── port_cierre.c        # Cierre de la ventana del juego (X / ALT+F4)
│   │   ├── port_misc.cpp        # Bridges nativos de Windows aislados (dos.h/bios.h, reloj, etc.)
│   │   ├── port_forward_decls.h # Declaraciones adelantadas (patron K&R que MSVC no acepta)
│   │   ├── port_native_handles.h# Tabla de handles para punteros que deben caber en mem[]
│   │   ├── port_pre.h           # Force-include (/FI) para todo el núcleo
│   │   ├── inter.h              # Copia de src/div32run/inter.h (offsets _Campo, opcodes, macros)
│   │   ├── smoke_test.c         # Diagnóstico de vídeo/input sin intérprete completo
│   │   ├── build_test_prg.py / build_frame_demo.py / build_sprite_demo.py / build_sound_demo.py / build_input_demo.py / build_fli_demo.py  # Generadores de bytecode de prueba
│   ├── compiler/                # Compilador DIV portado (hito C1-C4)
│   │   ├── divc.cpp / global.h / divlengu.cpp   # Copias byte-exactas de src/div con parches "PORT x64"
│   │   ├── divc_main.c          # Driver CLI
│   │   ├── port_compiler_stubs.c# Stubs del IDE + globals para el compilador
│   │   ├── port_compiler_pre.h / port_compiler_forward_decls.h
│   └── ide/                     # Editor/IDE portado (hito E1 en adelante)
│       ├── div.cpp / divwindo.cpp / divhandl.cpp / divedit.cpp / divbasic.cpp / divdsktp.cpp ...  # Nucleo UI (compilado desde src/div)
│       ├── port_ide_pre.h / port_ide_forward_decls.h / port_ide_stubs.c
│       ├── port_ide_video.c         # Sustituye a divvideo.cpp (E2.1)
│       ├── port_ide_keybo.c         # Sustituye a divkeybo.cpp (E2.3)
│       ├── port_ide_compila.c       # Compilar (divc_port) y Probar (div32run_port) (E4)
│       ├── port_ide_protos.h        # Prototipos generados con tools/gen_protos.py
│       ├── port_ide_dos.c           # INT 33h emulado + reloj de 100 Hz (port_ide_tick)
│       ├── global.h / div.h / svga.h / i86.h / bios.h / dos.h / graph.h  # Headers adaptados/stub
│   └── shim/                    # dos.h/bios.h/i86.h/graph.h/mem.h — reemplazos de headers DOS (nucleo)
├── main.c                       # Esqueleto Fase 1 (target div_port, no el runtime real)
CMakeLists.txt                   # Targets: div_port, div_core, div_smoke_test, div32run_port, divc_port, div_ide_port
docs/architecture/
├── 11-port-windows11-mikedx.md  # Decisión de arquitectura + plan por fases + evaluación MikeDX
├── 12-port-progreso.md          # Bitácora cronológica completa (todo el detalle)
├── 13-handoff.md                # Este documento
└── 14-formato-div32-eml.md      # Especificación de referencia del bytecode .div32/EML
```

## 10. Cómo seguir trabajando (reglas que ha pedido el usuario, explícitas)

- **Cambios pequeños e incrementales, documentados en `.md` tras cada mejora** — el usuario puede alternar entre esta sesión y otras herramientas/modelos, así que nada debe depender de memoria de conversación.
- **No tocar `i.cpp`/`f.cpp`/`kernel.cpp`/`inter.h` salvo que sea imprescindible**, y siempre con un comentario `/* PORT: ... */` explicando qué y por qué (patrón seguido en todos los checkpoints — ver ejemplos reales en cualquiera de los ficheros citados).
- **Antes de portar un fichero nuevo, comprobar si de verdad hace falta reescribirlo** — varios "huecos" resultaron ser opcionales con fallback nativo ya presente (ver `12-port-progreso.md` §8.2, el hallazgo sobre el backend de vídeo).
- **Compilar como C (`/TC`), no C++**, todo lo que viene de `src/div32run/*.cpp` salvo `port_misc.cpp` (que sí es C++ real) — decisión ya tomada y verificada, no reabrir esta discusión (`12-port-progreso.md` §3).
- Cuando algo no compile por una función usada antes de definirse en el mismo fichero (patrón K&R que Watcom aceptaba y MSVC no), añadir la declaración a `port_forward_decls.h`, no dispersar declaraciones sueltas.
