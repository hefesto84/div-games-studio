#ifndef DIV_PORT_COMPILER_PRE_H
#define DIV_PORT_COMPILER_PRE_H

/* ==========================================================================
 *  Port del compilador DIV (divc.cpp) -> Windows nativo (hito C1).
 *
 *  Force-include (/FI) para todas las unidades del compilador portado.
 *  Mismo patron que port_pre.h del nucleo (ver docs/architecture/
 *  12-port-progreso.md): adelantar las cabeceras CRT ANTES de que las
 *  macros del codigo original puedan colisionar con ellas, y centralizar
 *  los ajustes del port en un solo sitio.
 * ========================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <direct.h>
#include <conio.h>
#include <setjmp.h>

/* PORT: math.h NO se incluye (divc.cpp no usa ninguna funcion matematica)
 * porque su parser de expresiones define su propia exp2(), que colisiona
 * con el exp2(double) de C99. */

/* PORT: MSVC predefine _DLL=1 al compilar con /MD (CRT dinamico). divc.cpp
 * declara "typedef struct _DLL {...} DLL;" (linea 3069) para la tabla de
 * funciones importadas; sin este #undef el nombre se sustituye por la
 * constante y toda la seccion DLL cascada con errores de sintaxis. Las
 * cabeceras CRT ya han consumido _DLL a estas alturas (include guards),
 * asi que quitarlo aqui es inocuo para ellas. */
#ifdef _DLL
#undef _DLL
#endif

/* PORT: MSVC no define PATH_MAX (su equivalente es _MAX_PATH, que global.h
 * ya usa en otros sitios). Mismo valor que en port_pre.h del nucleo. */
#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#include "port_compiler_forward_decls.h"

#endif /* DIV_PORT_COMPILER_PRE_H */
