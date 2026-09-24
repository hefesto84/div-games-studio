/*
 * config.h -- configuracion minima de libmikmod 3.3.14 para el port DIV
 * (toolchain MSVC/x64, Windows nativo).
 *
 * Derivado de config.h.cmake del upstream (sezero/mikmod), dejando
 * definido SOLO lo que usa un build Windows/MSVC:
 *   - DRV_WIN: driver winmm (waveOut), el unico dispositivo de salida
 *     junto con drv_nos (que mdreg registra incondicionalmente). Los
 *     demas DRV_* (DS/WASAPI/writers/Unix/...) quedan fuera.
 *   - NO_DEPACKERS: mloader.c referencia PP20/MMCMP/XPK/S404 bajo
 *     #ifndef NO_DEPACKERS; sin los depackers compilados hay que
 *     habilitarlo o el enlazador falla.
 *   - NO se define HAVE_UNISTD_H ni MIKMOD_UNIX: los <unistd.h> de los
 *     loaders/mdriver quedan guardados y el bloque POSIX (getuid, pwd...)
 *     de mdriver.c no se compila. MSVC no trae ni unistd.h ni pwd.h.
 *   - MIKMOD_STATIC: la API extern se declara sin dllimport/dllexport,
 *     correcto para enlazar la lib estatica (nota en mikmod.h:46-50).
 */
#ifndef _MIKMOD_PORT_CONFIG_H_
#define _MIKMOD_PORT_CONFIG_H_

/* ========== Selection of features */

/* Windows MCI/winmm driver */
#define DRV_WIN 1

/* disable module depackers support */
#define NO_DEPACKERS 1

/* ========== Build environment information */

/* Define if you want a debug version of the library */
/* #undef MIKMOD_DEBUG */

/* Unix (POSIX) support: en este port siempre 0 */
/* #undef MIKMOD_UNIX */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define if the C compiler supports the `inline' keyword. */
/* MSVC en modo C acepta `inline' desde VS2015; por si acaso el compilador
   es mas viejo, se declare __inline y se deja el mapeo de abajo. */
/* #undef HAVE_C_INLINE */
/* Define if the C compiler supports the `__inline' keyword. */
#define HAVE_C___INLINE
/* Define if the C compiler supports the `__inline__' keyword. */
/* #undef HAVE_C___INLINE__ */

#if !defined(HAVE_C_INLINE) && !defined(__cplusplus)
# ifdef HAVE_C___INLINE__
#  define inline __inline__
# elif defined(HAVE_C___INLINE)
#  define inline __inline
# else
#  define inline
# endif
#endif

/* Define if you want a static build (no dllimport/dllexport). */
#define MIKMOD_STATIC 1

#endif /* _MIKMOD_PORT_CONFIG_H_ */