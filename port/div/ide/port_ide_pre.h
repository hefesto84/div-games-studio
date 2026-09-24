// port_ide_pre.h: force-include para el port del IDE de DIV a Windows 11 nativo
#ifndef PORT_IDE_PRE_H
#define PORT_IDE_PRE_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#undef CreateFont
#undef CreateFontA
#undef CreateFontW
// GENSPR (src/div/visor/) usa "ERROR" como variable global (char *ERROR),
// que choca con el ERROR de wingdi.h (definido como 0).
#undef ERROR
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <conio.h>
#include <direct.h>
#include <signal.h>
#include <malloc.h>
#include <ctype.h>

// Tipo para manejadores de IRQ (se usan como punteros, no como ISR reales)
typedef void (*TIRQHandler)(void);

#include "port_ide_forward_decls.h"

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Stubs para palabras clave de 16 bits que MSVC x64 no soporta
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __far
#define __far
#endif
#ifndef far
#define far
#endif
#ifndef _near
#define _near
#endif
#ifndef near
#define near
#endif
#ifndef huge
#define huge
#endif
#ifndef _HARDERR_FAIL
#define _HARDERR_FAIL 0
#endif

// Tipos de registros usados por int86/int386 (copia adaptada de src/div/div.h)
struct DWORDREGS {
    unsigned int eax, ebx, ecx, edx, esi, edi, cflag;
};
struct WORDREGS {
    unsigned short ax, bx, cx, dx, si, di;
    unsigned int cflag;
};
struct BYTEREGS {
    unsigned char al, ah, bl, bh, cl, ch, dl, dh;
};
union REGS {
    struct DWORDREGS x;
    struct WORDREGS  w;
    struct BYTEREGS  h;
};
struct SREGS {
    unsigned short es, cs, ss, ds, fs, gs;
};

// Macros de DOS/BIOS que el IDE usa inline
#define peekb(seg,off) 0
#define pokeb(seg,off,val) ((void)0)
#define inportb(port) 0
#define outportb(port,val) ((void)0)
#define inportw(port) 0
#define outportw(port,val) ((void)0)
#define int86(intno,inregs,outregs) 0
#define intdos(inregs,outregs) 0

/* PORT: el nucleo UI llama a int386() para el driver de raton (INT 33h) y,
 * puntualmente, a la BIOS de video. Se redirige al emulador de
 * port_ide_dos.c en vez de descartarlo, porque check_mouse()/read_mouse()
 * dependen de los valores devueltos. */
int port_int386(int intno, union REGS *in, union REGS *out);
int port_int386x(int intno, union REGS *in, union REGS *out, struct SREGS *s);
#define int386(intno,inregs,outregs) port_int386((intno),(inregs),(outregs))
#define int386x(intno,inregs,outregs,sregs) port_int386x((intno),(inregs),(outregs),(sregs))

/* Reloj de 100 Hz emulado (antes: IRQ 0 del PIT). */
void port_ide_tick(void);

#define INTR_CF 1

extern FILE *stdprn;

/* PORT (E2.1): las reservas del IDE estan dimensionadas para punteros de 32
 * bits; en x64 los nodos con punteros ocupan el doble. Se redirigen las
 * reservas a un envoltorio que sobredimensiona (ver port_ide_mem.c). El
 * bloque va despues de <malloc.h>/<stdlib.h> para no renombrar sus
 * declaraciones. */
void *port_ide_malloc(size_t n);
void *port_ide_calloc(size_t count, size_t size);
void *port_ide_realloc(void *p, size_t n);
#define malloc  port_ide_malloc
#define calloc  port_ide_calloc
#define realloc port_ide_realloc

// Tipos que aparecen en divsb.h / divsound.h (se redefinirán en stubs)
struct _SAMPLE;
struct _tmap;

#endif // PORT_IDE_PRE_H

/* PORT (E2.1): ver port_ide_setup.c -- el setup.bin del DIV original tiene
 * layout de 32 bits y hay que descartarlo. */
int port_setup_bin_valido(void);
