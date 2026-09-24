# Evaluación de un port a Windows 11 nativo + comparación con el fork de MikeDX

> **Propósito de este documento**: registrar, con el mayor detalle posible, la investigación hecha para evaluar la viabilidad de (a) ejecutar DIV Games Studio 2 de forma nativa en Windows 11 y (b) compilar desde el IDE juegos que corran como `.exe` nativos de Windows 11 — sin DOS ni DOSBox. Incluye el assessment inicial (hecho leyendo solo este repo) y la evaluación posterior del fork multiplataforma de MikeDX (`DIVGAMES/DIV-Games-Studio`), que ya intentó resolver el mismo problema. Está escrito para que un modelo de IA (u otra persona) pueda retomar el trabajo sin tener que re-investigar desde cero. Fecha de la investigación: 2026-09-17/18.

---

## 0. Decisiones ya tomadas en esta conversación

Estas decisiones fueron acordadas explícitamente con el usuario y deben tratarse como punto de partida, no como opciones abiertas a reconsiderar sin motivo:

1. **Gráficos/audio/input: se usará [raylib](https://www.raylib.com/)**, no SDL2, pese a que el fork de MikeDX usa SDL. Razones (detalladas en §5): build más simple en Windows/MSVC, `SetSoundPitch()` nativo (evita el hack de `Mix_RegisterEffect` que tuvo que hacer MikeDX), API de input más directa, licencia zlib (compatible con GPLv3).
2. **Networking: NO es una prioridad por ahora.** El multijugador (antes IPX) se deja fuera del alcance inicial del port. Si se retoma más adelante, ver §4.4 y §6 sobre qué tan floja es la solución de MikeDX (no usable tal cual).
3. **El editor/IDE SÍ importa** — no es un "nice to have" descartable; el usuario confirmó que portar el IDE (no solo el runtime del juego) es un objetivo real del proyecto. Ver §4.7: ya está confirmado como viable por precedente directo del fork de MikeDX.
4. **El editor es la prioridad #1 del port, por delante del empaquetado del `.exe` distribuible para jugadores.** Razón explícita del usuario: "la gracia del editor es precisamente eso, el editor" — DIV Games Studio es, ante todo, una herramienta de autor; que un juego terminado se pueda empaquetar como `.exe` nativo es importante pero secundario a que el propio entorno de desarrollo funcione de punta a punta en Windows 11. Esto reordena el plan de fases de §7 respecto a la versión anterior de este documento (que priorizaba primero el runtime/empaquetado). Matiz importante que no cambia por esta decisión: el editor **no es separable de tener un intérprete/VM funcionando**, porque el propio IDE ejecuta y depura programas desde dentro (equivalente a F9/F10 lanzando `session.div`, ver `07-modelo-ejecucion.md`) — así que la base de vídeo/audio/input sobre raylib sigue siendo el primer trabajo compartido; lo que se reordena es que el *empaquetado final* de un `.exe` standalone para distribuir a jugadores (la Fase de `pack()`/trailer `"DX"` de §4.6) pasa a depender de que el editor ya sea usable, no al revés.

---

## 1. Objetivo del port (recordatorio del alcance)

Dos entregables distintos, con esfuerzo distinto:

- **(A) El IDE (`D.EXE` en el original) corriendo nativo en Windows 11.**
- **(B) Los juegos compilados desde ese IDE corriendo como `.exe` nativos de Windows 11** (sin necesidad de DOSBox).

Ambos comparten el mismo problema de fondo: el código original asume que es el único software corriendo en la máquina (programa el PIT/PIC directamente, escribe registros VGA, hace DMA a la Sound Blaster, simula interrupciones reales vía DPMI). Nada de eso existe en Windows moderno.

Este documento asume como contexto previo todo lo ya documentado en `03-componentes-principales.md`, `04-build-system.md`, `07-modelo-ejecucion.md` y `08-aspectos-tecnicos.md` de esta misma carpeta — no se repite aquí el detalle de cómo funciona nuestro propio repo, solo se referencia.

---

## 2. Assessment inicial (propio, sin mirar aún el fork de MikeDX)

Este es el resumen del primer análisis, hecho releyendo `03`, `04`, `07` y `08` de nuestra propia documentación, **antes** de clonar el fork de MikeDX. Se conserva aquí porque sigue siendo válido como mapa de qué hay que tocar.

### 2.1 Lo que se puede reaprovechar casi tal cual (de nuestro propio código)

| Módulo | Estado | Por qué |
|---|---|---|
| `kernel.cpp` (el switch de opcodes, VM) | Reaprovechable | 0 llamadas a hardware/DOS. Puro cálculo sobre `mem[]`. |
| `i.cpp` — el dispatcher (`nucleo_exec`, `exec_process`, scheduler) | Reaprovechable | Solo 11 coincidencias de DOS en todo el archivo, todas en localización de rutas/unidad, ninguna en el bucle de opcodes. |
| `s.cpp` (blitting de sprites/scroll/modo-7) | Reaprovechable | 0 llamadas a hardware. Rasterización de software pura sobre buffers en memoria. |
| `divc.cpp` (el compilador del lenguaje DIV) | Reaprovechable | C++ portable, no toca hardware en absoluto. |
| El modelo de bytecode/procesos (`mem[]`, `_Status/_Frame/...`, opcodes `lfrm`/`lcal`) | Reaprovechable como diseño | Es una VM de bytecode; nada intrínsecamente DOS. |
| `f.cpp` | Reaprovechable ~90% | Cientos de funciones puras (matemáticas, strings, control); solo ~20-30 funciones (joystick por puerto, `_dos_findfirst`, disco) están mezcladas ahí y hay que extraerlas una por una. |

### 2.2 Lo que hay que sustituir por completo

| Capa actual | Qué hace hoy | Reemplazo previsto |
|---|---|---|
| **Vídeo** — `vesa.asm` (BIOS `int 10h`), `v.cpp` (escribe directo a `CRTC_INDEX 0x3d4`, `SC_INDEX 0x3c4`, framebuffer en `0xA0000`) | Modos VGA/VESA personalizados, 256 colores indexados | Ventana + backend gráfico moderno que reciba el buffer de 8-bit y una LUT de paleta a RGB. |
| **Audio** — `divsb.cpp` (Sound Blaster: I/O de puertos, DMA 8237 a mano) | Driver bare-metal | Backend de audio moderno reimplementando el mezclador de JUDAS. |
| **Timer** — `timer.asm`+`divtimer.cpp` (PIT 8253 + IRQ0 del PIC 8259) | Interrupción de hardware redirigida | Temporizador de alta resolución del SO. |
| **Input** — joystick por puerto de juego (RC timing), ratón por interrupción DOS | Lectura analógica por hardware | API de input moderna. |
| **Red** — `ipxlib.c` + `dpmi_net.c` (IPX vía DPMI) | Protocolo obsoleto | Fuera de alcance por ahora (decisión §0.2). |
| **Carga de "DLLs"** — `pe_load.c` (loader PE casero, no procesa importaciones, exige PE32 exacto de Watcom) | Loader a medida para DOS | Loader nativo del SO (`LoadLibrary`/`GetProcAddress`), conservando la idea del pool `DIV_export`/`DIV_import`. |
| **Toda la arquitectura 16/32-bit + DPMI** | Frontera modo real ↔ protegido | Deja de existir por completo en un `.exe` nativo de Windows. |

### 2.3 Los dos problemas estructurales identificados (antes de ver MikeDX)

1. El stub de 602 bytes que antecede a cada juego compilado (`div_stub.h`) es un binario ya ensamblado y embebido; el código fuente que debería haberlo generado (`src/div32run/wstub/wstub.c`) está vacío en nuestro repo (`main(void){}`). **Esto ya no es un problema real** — ver §4.6, MikeDX resuelve la generación del `.exe` nativo sin necesitar ese stub en absoluto.
2. El loader PE de `pe_load.c` es específico del ABI de Watcom de los 90 (exige `SizeOfOptionalHeader==224` exacto, no procesa importaciones). **Resuelto conceptualmente** — ver §4.5, MikeDX simplemente lo tira y usa el loader nativo del SO.

---

## 3. Metodología de la evaluación del fork de MikeDX

- **Repositorio evaluado**: [`DIVGAMES/DIV-Games-Studio`](https://github.com/DIVGAMES/DIV-Games-Studio) (el fork de MikeDX mencionado en el `README.md` de nuestro propio repo).
- **Commit exacto evaluado**: `dc8b7e5a77ba9c2cdb36fc1f1e43110ccb0207d5` (2016-11-27). Clonado con `git clone --depth 1` — es un snapshot superficial, **no** se investigó el historial completo ni commits posteriores a este. Si se retoma esta investigación, comprobar primero si hay actividad más reciente en el repo real (este clon es de 2016, podría haber evolucionado desde entonces).
- **Cómo reproducir el clon**: `git clone --depth 1 https://github.com/DIVGAMES/DIV-Games-Studio.git` (se clonó a un directorio temporal de scratchpad de la sesión, **no** forma parte de este repositorio ni se commiteó nada de su código aquí).
- **Licencia del fork**: GPL v3 (confirmado leyendo `LICENCE.txt`), **la misma que nuestro repo** — esto significa que el código de MikeDX puede reutilizarse literalmente (con atribución) en nuestro port, no solo como referencia conceptual.
- **Método**: se lanzaron 5 investigaciones en paralelo (una por subsistema: vídeo, audio, input+red, carga de DLLs, build+empaquetado), cada una leyendo código fuente real del clon (no solo el README), más una investigación adicional propia sobre la viabilidad de portar el editor/IDE.

---

## 4. Evaluación del fork de MikeDX por subsistema

### 4.1 Vídeo

**Arquitectura general**: mantienen el mismo nombre/forma del módulo (`src/divvideo.c` es el sucesor directo de nuestro `v.cpp`), pero vaciaron el cuerpo de las funciones que tocaban hardware real. Las constantes (`CRTC_INDEX 0x3d4`, `SC_INDEX 0x3c4`, tablas `modox[]` de registros CRTC) siguen *declaradas* en el archivo, pero todo ese código está envuelto en `#ifdef NOTYET` — código muerto. El camino real ejecutado es 100% vía una capa `OSDEP_*`.

**Capa OSDEP** (`src/shared/osdep.c` + `src/shared/osdep/osd_sdl12.c`, y una versión más moderna en `src/shared/osdep/osd_sdl2.c`, 369 líneas):

- `OSDEP_SetVideoMode()` crea una superficie **de 8 bits indexados** (`SDL_CreateRGBSurface(0,w,h,8,0,0,0,0)`) — **conservan el modelo de paleta de 8-bit en todo el pipeline lógico**, igual que el original. La conversión a color real ocurre solo en el "present" final.
- `OSDEP_SetPalette()` es un wrapper directo de `SDL_SetPalette(surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256)` — mapeo 1:1 de lo que antes era programar el DAC de la VGA.
- El "page flip" es `SDL_Flip(OSDEP_screen)`, con escalado vía `zoomSurface` de SDL_gfx si la ventana no coincide con la resolución lógica.
- `divvideo.c:volcadosdl()` tiene además una ruta de conversión manual paleta→RGB (`switch(vga->format->BitsPerPixel)`, traduciendo `colors[*p]` píxel a píxel para 32/24/16/8 bpp) para cuando el destino no es una superficie de 8bpp.

**Modo-8/VPE**: sí lo portaron. Mantienen los `.asm` x86 originales (`src/runtime/vpe/draw_fa.asm`, `draw_oa.asm`, `draw_wa.asm`, mismos nombres que en nuestro repo) **y además** escribieron equivalentes en C puro en el mismo directorio (`draw_f.c`, `draw_o.c`, `draw_sw.c`) — casi seguro seleccionados condicionalmente según arquitectura de destino (x86 usa ASM por rendimiento, otras arquitecturas usan el fallback en C). El mecanismo exacto de selección en CMake no se confirmó en detalle.

**Ya existe un backend SDL2 completo** (no solo SDL 1.2): `src/shared/osdep/osd_sdl2.c` (369 líneas) + `osd_sdl2.h` (23 líneas), activado en `CMakeLists.txt` con `-DSDL2=${HAS_SDL}` cuando se usa `FIND_PACKAGE(SDL2 REQUIRED sdl2)` y se linkea `SDL2_net`/`SDL2_mixer`/`SDL2_ttf` vía pkg-config (líneas 153-197 del `CMakeLists.txt` raíz).

**Windows específico**: no hay ruta de vídeo distinta para Windows (nada de DirectX/GDI en `src/win/osdepwin.c`) — Windows usa la misma ruta SDL que el resto de plataformas de escritorio. `osdepwin.c` (el único archivo en `src/win/`) solo reimplementa utilidades de sistema de ficheros (`_dos_findfirst`/`_dos_findnext` vía `_findfirst`/`_findnext` reales del CRT de Windows), no gráficos.

**Qué aprovechar**: la estrategia (buffer lógico de 8-bit indexado + interfaz de funciones estable + capa delgada de presentación) es el patrón correcto y minimiza el churn en el resto del motor (sprites/paletas no se enteran del cambio). El patrón "ASM x86 + fallback C" para VPE es reutilizable si migramos los `.asm`.

**Qué NO aprovechar**: SDL 1.2 es una librería sin mantenimiento desde 2012, sin alto DPI, sin backends modernos de GPU, con API de paleta deprecada en SDL2/3. Nosotros vamos con **raylib** (decisión §0.1), no con SDL2 — pero la arquitectura de "buffer indexado + LUT de conversión en el present" sigue siendo exactamente el patrón a seguir con raylib (subir un buffer RGBA convertido por LUT a una `Texture2D` cada frame vía `UpdateTexture`).

### 4.2 Audio

- `src/div/divsb.cpp` (el driver bare-metal de Sound Blaster) fue movido a `src/other/divsb.c` — carpeta fuera del árbol activo de build, código legado archivado. `src/judas/` (JUDAS completo con `judasdma.c`, `judasio.c`, ensamblador de timer/teclado) se conserva íntegro como fuente pero solo se usa en la ruta `#ifdef DOS` — **no fue portado ni recompilado de forma portable**, queda inerte en builds SDL.
- **La solución real está en `src/shared/run/divsound.c`**: un único archivo con **dos implementaciones completas en paralelo** seleccionadas por macro (`#ifdef DOS` vs `#ifdef MIXER`, activado por `-DMIXER` cuando `HAS_SDLMIXER=1`). Cada función pública (`LoadSound`, `DivPlaySound`, `StopSound`, `ChangeSound`, `ChangeChannel`, `LoadSong`, `PlaySong`, `StopSong`, `IsPlayingSound`) tiene bloque DOS (llama a `judas_*`) y bloque MIXER (llama a `Mix_*`) **con la misma firma e igual semántica hacia el lenguaje DIV** — el bytecode de un `.PRG` existente no necesita cambiar.
- **Integración SDL_mixer**: `InitSound()` (línea 43) hace `Mix_Init(MIX_INIT_FLAC|MOD|MP3|OGG)` + `Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512)`. Sonidos WAV/OGG cargados con `Mix_LoadWAV_RW`; si el recurso es PCM crudo sin cabecera (formato propio de DIV), le envuelven un header WAV sintético en memoria (`memcpy` de `"RIFF"`/`"WAVEfmt "`/`"data"`) antes de pasarlo a `Mix_LoadWAV_RW` — truco simple y reutilizable independientemente del backend de audio que usemos. Música (MOD/S3M/XM) vía `Mix_LoadMUS_RW`/`Mix_PlayMusic`.
- **Hallazgo más valioso — pitch-bending sobre un mixer que no lo soporta nativamente**: DIV expone `ChangeSound(canal, volumen, frecuencia 0..256)` para variar el tono en tiempo real. SDL_mixer no lo soporta de fábrica; lo resuelven con `Mix_RegisterEffect(canal, freqEffect, doneEffect, NULL)`, un callback de post-proceso por canal (`freqEffect`, línea 426) que resamplea manualmente el buffer ya decodificado según `channels[chan].freq`, antes de que SDL lo mezcle. **Con raylib esto no hace falta**: `SetSoundPitch()` es nativo (vía miniaudio, confirmado con búsqueda web — ver conversación previa), así que evitamos replicar este hack.
- No hay capa "osdep" separada de audio (a diferencia de vídeo); toda la lógica vive en `divsound.c` con macros.
- **No confirmado**: si `divmixer.c`/`divmixer.hpp` (control de volumen maestro) tiene equivalente MIXER o queda completamente muerto fuera de DOS — solo se investigó `src/runtime/divmixer.c`, que resultó ser 100% `#ifdef DOS`.

**Qué aprovechar**: el patrón "misma función C, dos cuerpos por macro" es más simple de migrar incrementalmente que una abstracción con vtable — aplicable igual con raylib (macro `DOS` vs `RAYLIB` en vez de `DOS` vs `MIXER`). El truco de envolver PCM crudo en un WAV sintético en memoria es reutilizable tal cual.

### 4.3 Input

- `src/divmouse.c` (`read_mouse2`) y `src/shared/run/divkeybo.c` (`tecla()`) son los puntos de entrada reales en plataformas modernas: ambos hacen `SDL_PollEvent(&event)` y traducen `SDL_KEYDOWN/UP`, `SDL_MOUSEMOTION`, `SDL_MOUSEBUTTONDOWN/UP` al **mismo estado interno que consumía el código DOS original** (`kbdFLAGS[]`, `mouse->x/y/left/right`, `scan_code`, `shift_status`), usando una tabla de traducción `OSDEP_key[]` de `SDLK_*` a scancodes estilo BIOS. **Esto significa que no tuvieron que tocar la VM/kernel para nada de input** — la capa de traducción vive enteramente en el borde.
- El código DOS original (interrupción de teclado, `int 0x16`/`int 0x21`, lectura RC de joystick en `src/runtime/joy.cpp` y `src/div1run/joy.c`) sigue en el árbol pero envuelto en `#ifdef DOS` — coexisten dos implementaciones seleccionadas por macro, sin borrar la vieja.
- El joystick en plataformas modernas se resuelve por `SDL_JOYAXISMOTION`/`SDL_JOYBUTTONDOWN`, pero **mapeado como si moviera el ratón** (`joymx`/`joymy` sumados a `m_x`/`m_y`) o como teclas emuladas (bloques `#ifdef GCW` para el mando de la consola GCW-Zero) — no hay una API de joystick nativa con ejes/botones propios expuesta al lenguaje DIV, es una capa de compatibilidad hacia atrás, no una ampliación real.

**Qué aprovechar**: el patrón de "envolver DOS con `#ifdef DOS` y añadir una rama nueva con el mismo contrato interno de estado, sin tocar la VM" es directamente aplicable a nuestro `f.cpp`, con raylib (`IsKeyDown`/`GetMousePosition`/`GetGamepadAxisMovement`) en vez de eventos SDL.

### 4.4 Red / networking

*(Fuera de alcance para el port actual — decisión §0.2 — pero documentado por completitud en caso de retomarse.)*

- `src/runtime/net.c` reemplaza IPX por SDL_net (TCP): `net_init_internet()` llama a `SDLNet_Init()`, `net_create_game()`/`_net_get_games()` usan `SDLNet_TCP_Open`/`SDLNet_TCP_Accept`/`SDLNet_TCP_Recv`. **Conservan los mismos nombres de función que expone el lenguaje DIV** (`net_join_game`, `net_get_games`, `_net_loop`) — buena señal de compatibilidad de API si algún día se retoma.
- Pero la implementación es de calidad **prototipo/experimental**: todo el código IPX/serie/módem original queda muerto en bloques `#ifdef NOTYET` (nunca se activa); `_net_get_games` resuelve el host **hardcodeado a `"localhost"` puerto `9999`** (sin descubrimiento real de partidas en red); hay variables sin inicializar usadas (`num_games` en `_net_join_game`); comentarios como `// FER_AQUI` sugieren trabajo inconcluso. **No hay UDP/broadcast** (IPX original sí lo tenía) — todo es TCP punto a punto con un único servidor fijo.
- **Conclusión de fidelidad**: funciona en el caso más simple (dos procesos en la misma máquina/LAN con IP conocida), no es una reimplementación robusta de la semántica de sesión/broadcast de IPX.

**Si se retoma en el futuro**: usar como punto de partida conceptual (mismos nombres de función DIV, sobre Winsock en vez de SDL_net) pero no copiar la implementación — habría que rehacer descubrimiento de host y considerar UDP para paridad funcional con IPX.

### 4.5 Carga de DLLs / plugins (el hallazgo más crítico)

**Qué hicieron**: tiraron el loader PE propio por completo y delegan la carga en el mecanismo nativo de cada plataforma. Confirmado en `src/shared/divdll.h` (líneas 13-32):

```c
#ifndef __WIN32
#include <dlfcn.h>          // Linux/OSX: dlopen/dlsym/dlclose POSIX nativos
#else
#include <windows.h>
#define dlopen(a,b)  LoadLibrary(a)      // Windows: shim directo al loader nativo de Win32
#define dlsym(a,b)   (dlfunc)GetProcAddress(a,b)
#define dlclose(a)   FreeLibrary(a)
#endif
```

En Windows, `dlopen("plugin.dll")` es literalmente `LoadLibrary()`. **`pe_load.c` (nuestro parser PE casero de 224 bytes de optional header, sin tabla de importación) desapareció por completo del árbol de este fork** — ya no hace falta, el SO hace el trabajo.

Lo único "propio" que sobrevive es la búsqueda de símbolos por variantes de name-mangling en `divdlsym()` (`mikedll.c` líneas 46-83): prueba `nombre`, `nombre_`, `_nombre`, `W?nombre` con `dlsym`/`GetProcAddress` — el mismo truco de compatibilidad con el mangling de Watcom que ya teníamos, pero sobre símbolos exportados de forma estándar (`__declspec(dllexport)`, confirmado en `dll/div.h:23`) en vez de sobre una tabla de exportación PE parseada a mano. El mecanismo `divmain`/`divlibrary`/`divend` y el pool `DIV_export`/`DIV_import` (`mikedll.c` líneas 259-321) se mantiene **idéntico conceptualmente** al de nuestro repo — es la capa de contrato entre host y plugin que vale la pena conservar tal cual.

**La compatibilidad es de CÓDIGO FUENTE, no binaria — y en Windows ni siquiera está activada**:

1. Como ahora se usa `LoadLibrary`/`dlopen` real, cualquier plugin tiene que ser un `.dll`/`.so` nativo válido para esa plataforma. Los `.DLL` de DIV2 originales (PE32 de 32-bit DOS compilado con Watcom para el ABI del extensor) **no son `.dll` de Windows válidos** — `LoadLibrary` los rechazaría. La carpeta `dll/` de este fork confirma esto: contiene los mismos plugins de ejemplo que en nuestro repo (`Agua.cpp`, `Hboy.cpp`, `Ss1.cpp`, `demo0/1/2`) pero como **código fuente para recompilar** con el SDK de este fork, más plugins nuevos exclusivos (`sdlgfx/`, `spectrum/`, `modea.c/.cpp`, `phys.c`).
2. **Hallazgo no evidente — en Windows los plugins están desactivados en su propia config de build**: `tools/windows.cmake:21` tiene `SET(HAS_DLL 0)`, con `-DDIVDLL` comentado. Comparando con otras plataformas: `HAS_DLL=1` en Linux, OSX, Pandora y Raspberry Pi; `HAS_DLL=0` en Windows, Atari ST, GCW-Zero y GP2X. **El propio toolchain de cross-compilación a Windows de este fork deshabilita explícitamente el soporte de plugins.** La afirmación "100% compatible... plugin dlls" del README es cierta para Linux/OSX/Pi tal como está configurado el repo, pero **no para Windows** sin antes activar `HAS_DLL=1`/`DIVDLL` a mano y validar que compile/funcione en MinGW — no hay evidencia en el repo de que se haya probado.

**Trampa a evitar en nuestra propia reescritura**: el macro `__WIN32` (doble guion bajo) en `divdll.h` **no es el macro estándar** que define MinGW/MSVC (`_WIN32`, un guion bajo, o `WIN32`); si nadie lo define explícitamente, la rama `#include <dlfcn.h>` (no-Windows) se colaría en un build de Windows y rompería la compilación. Usar `_WIN32` estándar en nuestra implementación.

**Qué aprovechar**: la estrategia entera es correcta y aplicable tal cual — sustituir el loader PE casero por `LoadLibrary`/`GetProcAddress` nativos, conservar el pool `DIV_export`/`DIV_import` y el contrato `divmain`/`divlibrary`/`divend`, probar 3-4 variantes de name-mangling por `GetProcAddress`.

**Qué asumir como trabajo pendiente, no resuelto**: no hay compatibilidad binaria posible con plugins DOS existentes (había que recompilarlos de todos modos); y el camino de plugins en Windows específicamente está apagado y sin validar en este fork — para nosotros es trabajo genuino, no algo "ya probado por MikeDX".

### 4.6 Build system y empaquetado del ejecutable final

**Build system**: CMake 2.8+, detección de plataforma vía `TARGETOS` e inclusión de `tools/<plataforma>.cmake` (hay uno por plataforma: windows, linux, osx, pi, psp, ps2, gcw, pandora, gp2x, amiga, atarist). `tools/windows.cmake` cross-compila con **MinGW-w64** desde Linux (`i686-w64-mingw32-gcc`/`x86_64-w64-mingw32-gcc`, sin mención de MSVC), define `HAS_SDL=1`, `HAS_SDLMIXER=1`, `HAS_ZLIB=1`, `HAS_MODE8=1`, `HAS_DLL=0` (ver §4.5), y linkea librerías Win32 estándar (`gdi32`, `winmm`, `dinput8`, `dxguid`, `ole32`, `shell32`, etc.) más `mingw32`/`SDLmain` (patrón `-Dmain=SDL_main`). **Abandonaron OpenWatcom/DOS4GW/PMODE-W por completo** para el target Windows — build nativo normal con un toolchain GCC estándar, sin triple-target DOS16/DOS32/host.

**Formato de bytecode: se mantuvo intacto**. Confirmado en `src/divc.c` (función `save_exec_bin`, líneas ~1070-1170): el compilador sigue generando **exactamente el mismo formato binario** que nuestro repo: 602 bytes de stub DOS + 36 bytes de cabecera (9 enteros) + 4 bytes de tamaño descomprimido + payload zlib. Cero cambios — la promesa de compatibilidad con código DIV1/DIV2 existente se apoya en no tocar este formato en absoluto.

**Empaquetado del ejecutable final — la pieza que resuelve nuestro "misterio del stub"**: `src/divpack.c`, función `pack(runtime, exefile, datafile, outfile)`, hace: concatena bytes de `runtime` (el binario nativo `divrun-<platform>` recién compilado, un `.exe` de Windows real) + bytes de `exefile` (el `EXEC.EXE` con el stub DOS de 602 bytes + bytecode zlib, formato de arriba, ahora **vestigial/inerte** en plataformas no-DOS) + bytes de `datafile` (un `.zip` de recursos) + un trailer de 10 bytes: `exesize`(4) + `datsize`(4) + magia `"DX"`(2).

Confirmado el lado de lectura en `src/runtime/i.c` (~línea 2014-2160): al arrancar, el runtime abre `argv[0]` (su propio ejecutable), busca los últimos 2 bytes = `"DX"`; si coincide, lee los 10 bytes finales para obtener `exesize`/`datsize` y calcula `exestart = len - exesize - datsize - 10` para localizar dentro de sí mismo el bloque de bytecode y el de datos.

**Esto es el truco clásico de "self-extracting PE"**: como los loaders de PE ignoran los bytes que quedan después del final de las secciones declaradas, el resultado de `pack()` es un `.exe` de Windows válido y ejecutable con doble clic, sin ficheros externos si no se usa el `.zip` de datos separado. Constantes vistas: `winstubsize=11984` y `div1stubsize=8819` (tamaños del runtime nativo precompilado para Windows/DIV1) — confirma que el "stub" real para plataformas modernas es **el binario nativo completo**, no el de 602 bytes (ese sigue viviendo solo dentro del blob de bytecode, inerte fuera de DOS).

**Distribución final en Windows**: `src/divinsta.c`, función `create_zip`, arma un segundo `.zip` (con la librería vendored `src/shared/lib/zip`, basada en miniz) que contiene el `.exe` ya empaquetado con `pack()` más `SDL.dll`, `SDL_mixer.dll`, `libmikmod-2.dll` y un `readme.txt` — eso es lo que se distribuye a los jugadores.

**Qué aprovechar**: (1) el patrón `pack()`/trailer `"DX"` es **directamente adoptable tal cual** — resuelve exactamente el problema de diseño que teníamos pendiente sobre "cómo generar un `.exe` nativo sin el stub de 602 bytes perdido"; (2) no hace falta tocar el formato de salida del compilador (`divc.cpp`/`save_exec_bin`), solo escribir el equivalente de `pack()` y el lector de trailer en el runtime nuevo; (3) CMake + toolchain está probado para Windows, aunque para nuestro caso evaluamos MSVC en vez de MinGW (mejor integración con Visual Studio/debugging nativo — a decidir en fase de implementación, no bloqueante).

### 4.7 El editor/IDE — viabilidad confirmada por precedente directo

Este punto se investigó como pregunta de seguimiento del usuario ("¿es viable portar el editor?"), leyendo directamente `CMakeLists.txt` (no un agente, investigación propia).

**Hay dos binarios distintos que no hay que confundir**:

- **`${TARGET}` (`div-<platform>`) — el IDE completo de verdad.** Se construye con `FILE(GLOB DIV_SOURCES "src/*.c")` (línea 299 de `CMakeLists.txt`), es decir, **literalmente todos los `.c` de la raíz de `src/`**: `div.c` (5176 líneas, núcleo), `divwindo.c` (974 líneas, GUI de ventanas/botones), `divedit.c` (3430 líneas, editor de código), `divpaint.c` (5149 líneas, programa de dibujo), más `divbrow.c`, `divcalc.c`, `divhelp.c`, `divinsta.c`, `divsetup.c`, `grabador.c`, `ifs.c`, `diveffec.c`, `divc.c` (el compilador), `divmap3d.c`, `divfont.c`, `divforma.c`, `divpack.c` — todo compilado como **un único ejecutable** sobre la misma capa `OSDEP`/SDL que el runtime. Definido en `ADD_EXECUTABLE(${TARGET} src/global.h ${VISOR_SOURCES} ${DIV_SOURCES} ${JUDAS_SOURCES} ${ZIP_SOURCES} ${OSDEP} ${DLLSRC})` (líneas 381-390).
- **`${RUNNER}` (`d.exe` en Windows, `d-<platform>` en el resto) — NO es el editor.** Es un lanzador mínimo, `src/runner/r.c` (~127 líneas completo, leído íntegro): hace `system("system/div INIT ...")`, interpreta el código de retorno (`1`/`2`/`256`/`512`) y relanza el runtime de depuración (`system(dbg " system/EXEC.EXE")`) o el propio IDE en modo test (`system(ide " TEST")`). Es el equivalente funcional casi exacto de nuestro `src/wstub/wstub.c` (el supervisor 16-bit que relanza `D.EXE`/`SESSION.DIV`), portado a C portable con `system()` en vez de DOS EXEC/`spawnvp`.

**Confirmación de que compila de verdad, no solo en teoría**: el CI del propio repo (`bitbucket-pipelines.yml`, imagen `gcc:6.1`) instala `libsdl1.2-dev libsdl-net1.2-dev libsdl-mixer1.2-dev` y ejecuta `cmake . && make` — esto construye `${TARGET}` (el IDE completo) en Linux, y el badge de build de Travis en el README pasa. **Es prueba real, no teórica, de que el árbol completo del editor (GUI + editor de código + pintura + compilador) compila y enlaza contra SDL sin OpenWatcom ni DOS.**

**Dos matices importantes, no confirmados**:

1. **El cross-compile específico a Windows del IDE completo NO está verificado por CI.** `tools/windows.cmake` lo configura (MinGW-w64, `HAS_SDL=1`), pero el pipeline de CI solo prueba el build nativo de Linux. Que "compila en Linux con SDL" no garantiza que compile sin fricción con MinGW en Windows — hay que probarlo cuando llegue el momento, no asumirlo.
2. **`tools/windows.cmake` tiene `HAS_DLL=0`** (igual que el runtime, ver §4.5) — si el propio IDE necesita cargar DLLs de plugin en algún flujo interno (p. ej. para inspeccionar/probar una extensión desde el editor), esa vía estaría apagada en su configuración de Windows.

**Conclusión práctica**: portar el editor no es el proyecto "imposible" que sugería la primera intuición — la GUI inmediata dibujada a mano (`divwindo.c`) sobrevive intacta sobre una capa de presentación moderna sin reescritura conceptual, tal como demuestra este fork con SDL. El trabajo real es: (a) las mismas capas de I/O que ya se manejarían para el runtime, aplicadas también al binario del IDE, con raylib en vez de SDL; (b) validar en Windows algo que MikeDX dejó configurado pero nunca probó en CI.

---

## 5. raylib vs SDL2 — decisión y razones (registrado para no reabrir la discusión sin motivo)

Se evaluó explícitamente reemplazar SDL (usado por MikeDX) por **raylib** para nuestro propio port. Conclusión: **raylib es la elección**, por:

- **Audio con pitch-bend nativo**: MikeDX tuvo que hackear `Mix_RegisterEffect` para resamplear el pitch a mano (ver §4.2) porque SDL_mixer no lo soporta. Raylib expone `SetSoundPitch(Sound, float pitch)` de fábrica, implementado sobre miniaudio vía `ma_data_converter_set_rate()` (confirmado por búsqueda web, código fuente de `raudio.c`/`raudio.h` en `raysan5/raylib` y `raysan5/raudio`) — nos ahorra ese hack por completo.
- **Build en Windows más simple**: raylib es una sola librería con muy buen soporte oficial MSVC/CMake, contra SDL2 + SDL2_mixer + SDL2_net + SDL2_image + SDL2_ttf como piezas sueltas (como hace MikeDX).
- **Input más directo**: polling (`IsKeyDown`, `GetGamepadAxisMovement`) mapea más simple al estado interno que ya espera `f.cpp`/`kernel.cpp` que el modelo de cola de eventos de SDL usado por MikeDX.
- **Licencia**: zlib, sin conflicto con GPLv3 (igual de permisiva que la licencia de SDL2, así que no es un factor diferencial, pero se confirma que no hay bloqueo).

**Tradeoff aceptado conscientemente**: raylib no tiene el concepto de "superficie con paleta" que SDL2 ofrece (aunque deprecado en SDL2 también). Hay que mantener el buffer de 8-bit indexado nosotros mismos y convertir a RGBA con una LUT de 256 entradas antes de subir la textura cada frame (`UpdateTexture`). **No es trabajo nuevo**: es el mismo camino que ya escribió MikeDX a mano en `divvideo.c:volcadosdl()` para los casos en que el destino no era de 8bpp (ver §4.1) — con raylib sería el único camino, no una rama alternativa.

**Networking**: raylib no trae módulo de red — irrelevante para la decisión porque tampoco lo resolvía bien SDL2/SDL_net (ver §4.4), y de todos modos está fuera de alcance por ahora (§0.2).

---

## 6. Síntesis: qué es directamente reutilizable y qué hay que rehacer

### 6.1 Reutilizable de nuestro propio repo casi sin cambios
- `kernel.cpp`, el dispatcher de `i.cpp`, `s.cpp`, `divc.cpp`, el modelo de bytecode/procesos completo (ver §2.1).
- El formato de fichero del bytecode (stub 602 + header 36 bytes + tamaño + zlib) — **no hace falta rediseñarlo**, MikeDX demuestra que se puede dejar intacto y resolver la ejecución nativa por fuera de él (ver §6.2).

### 6.2 Reutilizable como *patrón de diseño* del fork de MikeDX (y legalmente, como código literal con atribución, por ser GPLv3 igual que nuestra licencia)
- El patrón `pack()` + trailer de 10 bytes `"DX"` para generar el `.exe` nativo final (§4.6) — **la pieza más valiosa de toda esta investigación**, resuelve un problema de diseño que teníamos abierto.
- El patrón de doble implementación por macro (`#ifdef DOS` vs nueva rama) para audio/input sin tocar la VM (§4.2, §4.3).
- El truco de envolver PCM crudo en un header WAV sintético en memoria antes de pasarlo a un backend de audio (§4.2).
- El diseño de carga de DLLs vía loader nativo (`LoadLibrary`/`dlopen`) + pool `DIV_export`/`DIV_import` + prueba de variantes de name-mangling (§4.5) — sustituye por completo a `pe_load.c`.
- La arquitectura de "buffer indexado 8-bit + capa de presentación delgada" para vídeo (§4.1), adaptada a raylib en vez de SDL.
- La confirmación de que la GUI inmediata del IDE (`divwindo.c` y equivalentes) puede compilarse sobre una capa de presentación moderna sin reescritura conceptual (§4.7).

### 6.3 Hay que rehacer desde cero (MikeDX no lo resolvió bien, o no aplica a nuestra elección de librería)
- Todo el backend de audio/vídeo/input concreto: en vez de SDL, sobre **raylib** (§5) — la arquitectura se copia, el código de MikeDX (atado a la API de SDL) no se reutiliza literalmente aquí.
- Networking, si se retoma en el futuro (§4.4) — la solución de MikeDX es de calidad prototipo, no producción.
- Validar y resolver la carga de DLLs en Windows específicamente (§4.5) — MikeDX la dejó apagada (`HAS_DLL=0`) y sin probar en su propio `windows.cmake`.
- Validar el build del IDE completo con MinGW o MSVC en Windows (§4.7) — MikeDX solo lo prueba en CI para Linux.

---

## 7. Plan de fases propuesto (para discutir/ajustar antes de empezar a implementar)

> Este plan es una propuesta de orden de trabajo, no una decisión cerrada — pendiente de acuerdo explícito con el usuario antes de empezar a escribir código. **Reordenado el 2026-09-18** a pedido explícito del usuario: el editor es la prioridad, no el empaquetado del `.exe` distribuible (ver §0.4). La versión anterior de este plan priorizaba primero el runtime/empaquetado (entregable B) y dejaba el editor para la Fase 2; ahora es al revés: el editor se vuelve usable lo antes posible, y el empaquetado final para distribuir juegos a jugadores se deja para después.

**Fase 0 — Toolchain y esqueleto de proyecto**
- Elegir MSVC vs MinGW para el build de Windows nativo (MikeDX usa MinGW; a evaluar si conviene MSVC por mejor integración con Visual Studio).
- Proyecto mínimo: CMake + raylib enlazado, ventana vacía abriendo en Windows 11.

**Fase 1 — Capa de I/O compartida sobre raylib (base para editor y runtime, mínima)**
- Escribir la capa de vídeo nueva sobre raylib: buffer 8-bit indexado + LUT de paleta + `UpdateTexture` (arquitectura de §4.1/§5). Esta es la pieza que consume tanto el editor (`divwindo.c`/`divpaint.c` dibujan sobre este mismo framebuffer) como el runtime.
- Escribir la capa de audio nueva sobre raylib (`LoadSound`/`PlaySound`/`SetSoundPitch`, patrón de doble macro de §4.2, sin necesitar el hack de `Mix_RegisterEffect`).
- Escribir la capa de input nueva sobre raylib (`IsKeyDown`/ratón, patrón de §4.3).
- Timer: `QueryPerformanceCounter` reemplazando PIT/PIC.
- Portar `i.cpp`/`kernel.cpp`/`f.cpp`/`s.cpp` casi sin cambios (extrayendo las ~20-30 funciones de `f.cpp` que tocan hardware/DOS, ver §2.1) — se necesita un intérprete funcional porque el propio editor lo usa internamente para ejecutar/depurar programas (equivalente a `session.div`/F9-F10, ver §0.4 y `07-modelo-ejecucion.md`), no como entregable final todavía.
- **Sin empaquetado de `.exe` distribuible, sin DLLs de plugin, sin red** en esta fase — el objetivo es únicamente tener la base de I/O y el intérprete lo bastante funcionales como para que el editor pueda apoyarse en ellos.

**Fase 2 — El editor/IDE funcionando de punta a punta (entregable A, ahora la prioridad real del proyecto)**
- Portar `div.cpp`/`divwindo.cpp`/`divedit.cpp`/`divpaint.cpp` y el resto de módulos del IDE sobre la capa de I/O de la Fase 1 (confirmado viable, §4.7).
- Reutilizar el compilador (`divc.cpp`) sin cambios de formato de salida.
- Objetivo de "hecho" de esta fase: poder abrir el editor en Windows 11, escribir/editar un programa DIV, compilarlo y ejecutarlo/depurarlo desde dentro del propio IDE (el flujo F9/F10 original) — **sin necesidad todavía de generar un `.exe` standalone para repartir a un tercero**.
- Validar específicamente en Windows algo que MikeDX nunca probó en CI (§4.7, riesgo abierto).

**Fase 3 — Empaquetado del `.exe` distribuible (entregable B, ahora pospuesto)**
- Implementar el equivalente de `pack()`/trailer `"DX"` (§4.6) para producir, desde el propio editor, un `.exe` nativo de Windows a partir de: runtime compilado + bytecode ya generado por `divc.cpp` (sin tocar ese formato) + recursos.
- Este paso depende de que la Fase 2 ya esté sólida (el editor es quien dispara este empaquetado, con el botón/menú equivalente a "Crear instalación"/"Generar ejecutable" de `divinsta.cpp`).

**Fase 4 — Carga de DLLs/plugins**
- Sustituir `pe_load.c` por `LoadLibrary`/`GetProcAddress` + pool `DIV_export`/`DIV_import` (patrón de §4.5).
- Decidir si se recompilan los plugins de ejemplo existentes (`agua`, `hboy`, `ss1`) contra el nuevo SDK para validar el mecanismo end-to-end.
- Cuidado con el macro `_WIN32` estándar (no `__WIN32`, trampa documentada en §4.5).

**Fase 5 — Networking (opcional, solo si se retoma más adelante)**
- Rehacer sobre Winsock, no copiar la solución de MikeDX (§4.4/§6.3).

---

## 8. Riesgos y preguntas abiertas explícitas

- El clon de MikeDX evaluado es de 2016 (`dc8b7e5`, ver §3) — no se revisó si el proyecto real tiene actividad/commits posteriores más maduros. Antes de dar por buena cualquier conclusión de este documento como "estado actual de MikeDX", convendría comprobar el estado real del repositorio en GitHub.
- No se investigó en detalle el mecanismo exacto de selección ASM-vs-C para VPE/Modo-8 en el CMake de MikeDX (§4.1) — quedaría por confirmar si se quiere reusar esa estrategia de fallback por arquitectura.
- No se confirmó si `divmixer.c` (control de volumen maestro) tiene sentido/equivalente fuera de DOS (§4.2).
- El build de Windows del IDE completo y de los plugins de MikeDX **no está probado ni por ellos ni por nosotros** — es la mayor incertidumbre práctica de todo el plan (§4.7, §6.3).
- No se ha decidido MSVC vs MinGW para nuestro propio toolchain de Windows (Fase 0).

---

## 9. Estado de la implementación

El trabajo de implementación real (Fase 0 y Fase 1 del plan de §7) se registra, paso a paso y de forma mucho más granular que este documento, en [`12-port-progreso.md`](12-port-progreso.md) — esa es la bitácora a consultar para saber exactamente qué compila hoy, qué se corrigió y por qué, y cuál es el siguiente paso concreto.

## 10. Cómo retomar/reproducir esta investigación

```bash
git clone --depth 1 https://github.com/DIVGAMES/DIV-Games-Studio.git
# commit evaluado en esta investigación: dc8b7e5a77ba9c2cdb36fc1f1e43110ccb0207d5 (2016-11-27)
```

Archivos concretos del fork de MikeDX citados en este documento (rutas relativas dentro de ese clon, no de nuestro repo):

- `CMakeLists.txt` (raíz) — build system completo, definición de targets `${TARGET}` (IDE), `${RUNTIME}`, `${D1RUNTIME}`, `${DEBUG}`, `${RUNNER}`.
- `tools/windows.cmake` — toolchain MinGW-w64, flags/libs de Windows, `HAS_DLL=0`.
- `bitbucket-pipelines.yml` — CI real (Linux, gcc:6.1, SDL 1.2).
- `LICENCE.txt` — GPL v3.
- `src/divvideo.c`, `src/shared/osdep.c`, `src/shared/osdep/osd_sdl12.c`, `src/shared/osdep/osd_sdl2.c`/`.h` — vídeo.
- `src/shared/run/divsound.c`, `src/other/divsb.c`, `src/judas/` — audio.
- `src/divmouse.c`, `src/shared/run/divkeybo.c`, `src/runtime/joy.cpp`, `src/div1run/joy.c` — input.
- `src/runtime/net.c` — red.
- `src/shared/mikedll.c`, `src/shared/divdll.h`, `dll/div.h`, `dll/*.c` — carga de DLLs/plugins.
- `src/divc.c` (función `save_exec_bin`), `src/divpack.c` (función `pack`), `src/runtime/i.c` (lectura del trailer `"DX"`), `src/divinsta.c` (función `create_zip`) — empaquetado.
- `src/runner/r.c` — el lanzador `d.exe`/`${RUNNER}`.
- `src/div.c`, `src/divwindo.c`, `src/divedit.c`, `src/divpaint.c` (y el resto de `src/*.c`) — el IDE completo.

Documentos hermanos de nuestro propio repo, necesarios como contexto previo para entender de qué partimos: [`03-componentes-principales.md`](03-componentes-principales.md), [`04-build-system.md`](04-build-system.md), [`07-modelo-ejecucion.md`](07-modelo-ejecucion.md), [`08-aspectos-tecnicos.md`](08-aspectos-tecnicos.md).
