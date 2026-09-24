#ifndef DIV_PORT_PRE_H
#define DIV_PORT_PRE_H

/* ==========================================================================
 *  Port de DIV Games Studio 2 -> Windows nativo (raylib).
 *
 *  Este cabecera se fuerza a cada unidad de compilacion del nucleo
 *  (i.cpp, f.cpp, kernel.cpp, divlengu.cpp y los modulos port/div):
 *     - Comando MSVC equivalente al original:
 *         CC = g++ ... (Todas las fuentes del core se compilan tal cual,
 *         con las shims de dos.h/bios.h/i86.h/graph.h/<direct.h>).
 *     - `// A-uto-load ... ` -> solo prototipos.
 *
 *  Sustituye la I/O del DOS (v.cpp/vesa.asm, divsound/timer.asm, divkeybo,
 *  divmouse, PIT 100 Hz) por la capa `port/io` (div_io.h) con backends
 *  raylib. La VM (i.cpp/f.cpp/kernel.cpp) se porta CASI SIN CAMBIOS; los
 *  unicos puntos tocados son los que dependen del hardware DOS/IRQ.
 * ========================================================================== */

/* IMPORTANTE: todas las cabeceras estandar/CRT que i.cpp/f.cpp/kernel.cpp
 * acaban incluyendo se fuerzan aqui, ANTES de que "inter.h" tenga ocasion
 * de definir sus constantes de campo internas (#define _Id 0, _Status 4,
 * _Size 31, _Height 36, ...). Motivo real encontrado compilando: inter.h
 * define `#define _Size 31` (offset de campo del proceso, ver linea 466)
 * y ese mismo nombre `_Size` lo usa <time.h>/corecrt.h de MSVC como
 * parametro de plantilla interno
 * (__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1: "template <size_t _Size>").
 * Si inter.h se procesa primero, ese `#define` sustituye textualmente
 * "_Size" por "31" dentro de la cabecera de <time.h>, generando
 * "template <size_t 31>" y una cascada de errores de sintaxis.
 * Incluyendo aqui <time.h> (y el resto de cabeceras CRT usadas) antes de
 * "inter.h", su contenido ya queda parseado y protegido por sus propios
 * include-guards, así que la re-inclusion posterior desde i.cpp es un
 * no-op inofensivo. Ver docs/architecture/12-port-progreso.md. */
#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include <direct.h>
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#define chdir _chdir
#define getcwd _getcwd

/* PORT (E5): activa en open_file() (f.cpp) la cadena de busqueda permisiva
 * que el original reservaba al build DEBUG (SESSION.386, el que el IDE de DOS
 * lanzaba al "Probar"). Es la unica que respeta subcarpetas de recursos
 * ("tutorial\tutor0.fpg" -> "fpg\tutorial\tutor0.fpg"), imprescindible para
 * ejecutar los programas del entorno tal cual los escribio el usuario.
 * Ver docs/architecture/12-port-progreso.md, hito E5. */
#define PORT_OPEN_FILE_SEARCH 1

#include "shim/bios.h"
#include "shim/dos.h"
#include "shim/i86.h"
#include "shim/graph.h"
#include "math_.h"
#include "port_forward_decls.h"
#include "port_native_handles.h"

/* PORT (checkpoint 11): RENOMBRADO de 4 funciones del bridge de sonido
 * para eliminar la colision de simbolos con la API de raylib (usada por
 * port/io/io_audio.c). raylib exporta LoadSound/PlaySound/StopSound/
 * UnloadSound con la MISMA nombre que divsound.h; mientras la capa io se
 * llamaba con el nombre plano (sin __declspec(dllimport)), esas llamadas
 * se enlazaban contra divsound.obj y el audio de raylib NUNCA se oia
 * (PlaySound(snd) acababa llamando a io_play_sound(int,int,int) con el
 * contenido de la struct `Sound` descompuesto en ints). Ver
 * docs/architecture/13-handoff.md. Con USE_LIBTYPE_SHARED las llamadas
 * raylib van por __imp_<name> (raylib.dll), y con estos renames los
 * simbolos del nucleo dejan de chocar con los thunks del import lib.
 * Los llamadores (f.cpp/i.cpp) no se tocan: el macro los reescribe a
 * todos hacia la misma definicion renombrada. */
#define LoadSound    divLoadSound
#define PlaySound    divPlaySound
#define StopSound    divStopSound
#define UnloadSound  divUnloadSound

#endif /* DIV_PORT_PRE_H */
