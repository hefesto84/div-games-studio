#ifndef PORT_DIV_SHIM_DOS_H
#define PORT_DIV_SHIM_DOS_H
/*
 *  Shims de <dos.h> + <sys\limp.h> (Watcom/DOS) para el port (raylib).
 *
 *  Sustituye los drivers MS-DOS del nucleo DIV por equivalents modernos:
 *   - El reloj PIT de 100 Hz -> port/io (io_timer, monotono).
 *   - Las IRQ de teclado/raton -> evento de port/io (io_poll).
 *   - El raton de /dev o driver -> dx/dy del backend.
 *   - int386/int386x (DPMI int 21h/ports) -> io_* (raylib no usa DOS).
 *
 *  Declaraciones SOLO (implementaciones en port_misc.cpp), con tipos DOS
 *  compatibles (find_t, REGS...). Se puede incluir fuera de i.cpp (otras
 *  unidades del port).
 */
#include "i86.h"
#include <stdint.h>

typedef uintptr_t _intptr;
#define _A_ER            2
/* Guardados con #ifndef: port_misc.cpp incluye ademas <io.h> real (para
 * _findfirst/_finddata_t), que ya define estos mismos _A_* con los mismos
 * valores numericos -- evita choque de redefinicion en esa unidad. */
#ifndef _A_NORMAL
#define _A_SUBDIR        0x10
#define _A_NORMAL        0
#define _A_RDONLY        1
#define _A_HIDDEN        2
#define _A_SYSTEM        4
#define _A_VOLID          8
#define _A_ARCH           0x20
#define _A_ALL           ( 0xFF )
#endif

/* Estructura DOS classica de busqueda de ficheros (_dos_findfirst/next).
 * El UCRT moderno la elimino por completo (solo queda _finddata_t via
 * <io.h>); la reimplementamos aqui con el mismo layout que usaba Watcom,
 * y el cuerpo de _dos_findfirst/_dos_findnext (port_misc.cpp) la rellena
 * traduciendo desde _findfirst/_findnext + attrib/date-time DOS empaquetado.
 * name[]: 260 en vez de los 13 originales (8.3) para no desbordar con
 * nombres largos de Windows; el codigo DIV que la consume (f.cpp) solo
 * hace strcpy/lectura de campos, nunca asume sizeof(find_t) fijo. */
struct find_t {
    unsigned char reserved[21];
    unsigned char attrib;
    unsigned short wr_time;
    unsigned short wr_date;
    unsigned long size;
    char name[260];
};

/* union REGS / struct SREGS: layout clasico Watcom/Borland para
 * int86/int386. Se usan ambos accesos (.h byte, .w word, .x dword) segun
 * el sitio de la llamada (ver f.cpp: regs.w.* para int386 "IOCTL"/disco,
 * regs.x.* para int386x DPMI 0x31). */
struct WORDREGS  { unsigned short ax, bx, cx, dx, si, di, cflag, flags; };
struct BYTEREGS  { unsigned char al, ah, bl, bh, cl, ch, dl, dh; };
struct DWORDREGS { unsigned long eax, ebx, ecx, edx, esi, edi, cflag, eflags; };

union REGS {
    struct WORDREGS w;
    struct BYTEREGS h;
    struct DWORDREGS x;
};

struct SREGS { unsigned short es, cs, ss, ds, fs, gs; };

/* int386/int386x y la familia _dos_* de mas abajo: declaradas aqui,
 * IMPLEMENTADAS en port_misc.cpp -- ese fichero SI es C++ real (no pasa
 * por /TC) y las define como `extern "C"` para que el nucleo (compilado
 * como C, ver docs/architecture/12-port-progreso.md §3) pueda enlazarlas
 * por nombre sin decorar. Hay que declararlas tambien con enlace C aqui
 * (bajo #ifdef __cplusplus) para que, cuando ESTE mismo header se incluye
 * desde port_misc.cpp (C++), declaracion y definicion concuerden -- si no,
 * MSVC da error C2732 "la especificacion de vinculacion se contradice".
 * En una unidad compilada como C (i.cpp/f.cpp/...) __cplusplus no existe,
 * so el #ifdef no aporta ni quita nada: ya es enlace C por defecto. */
#ifdef __cplusplus
extern "C" {
#endif

/* int386/int386x: en el original son llamadas DPMI reales (int 21h/31h
 * simuladas). Aqui se implementan en port_misc.cpp con soporte SOLO para
 * los dos puntos de uso reales localizados en f.cpp (ver
 * docs/architecture/12-port-progreso.md): la consulta IOCTL de unidad
 * (disk_free) y la consulta DPMI 0x0500 de memoria libre (GetFreeMem).
 * Cualquier otra interrupcion no reconocida devuelve "error" (carry=1)
 * en vez de fallar silenciosamente con datos basura. */
extern int int386(int intno, union REGS *in, union REGS *out);
extern int int386x(int intno, union REGS *in, union REGS *out, struct SREGS *segregs);

/* Funciones DOS clasicas eliminadas del UCRT moderno; reimplementadas en
 * port_misc.cpp sobre las equivalentes de Windows (_findfirst/_findnext,
 * _chdrive/_getdrive, SetFileAttributesA, _getdiskfree). */
extern unsigned _dos_findfirst(const char *path, unsigned attrib, struct find_t *fileinfo);
extern unsigned _dos_findnext(struct find_t *fileinfo);
extern unsigned _dos_setfileattr(const char *path, unsigned attrib);
extern unsigned _dos_getdrive(unsigned *pdrive);
extern unsigned _dos_setdrive(unsigned drive, unsigned *pnumdrives);
extern unsigned _dos_getdiskfree(unsigned drive, struct diskfree_t *diskspace);

/* PORT: 'reloj' (i.cpp/f.cpp) se incrementaba originalmente via una
 * interrupcion de temporizador real de DOS (IRQ0/IRQ8 reprogramada a
 * ~100Hz) que no forma parte de este repositorio restaurado -- mismo tipo
 * de hueco que el stub de 602 bytes (ver
 * docs/architecture/12-port-progreso.md checkpoint 8/9). port_get_reloj()
 * calcula el equivalente a partir de un reloj real de Windows, en las
 * mismas unidades que el resto del codigo espera (100 "ticks" por
 * segundo, ver freloj=ireloj=100.0/24.0 en inicializacion()). Usada por
 * get_reloj() en f.cpp -- sin esto, el limitador de FPS de frame_start()
 * entraba siempre en su rama de "reloj congelado" (pensada para un fallo
 * real de hardware) y reiniciaba el dispositivo de sonido en cada frame.
 */
extern int port_get_reloj(void);

/* PORT: el bucle de espera de frame_start() (i.cpp) decide que el
 * temporizador esta "congelado" (fallo real de hardware, en el DOS
 * original) si 'reloj' no avanza en 60000 iteraciones de puro spin. Ese
 * umbral se calibro para CPUs de ~1998 (386/486): en ese hardware, 60000
 * iteraciones vacias tardaban varias decenas de milisegundos, tiempo de
 * sobra para que el temporizador real avanzara al menos un tick. En una
 * CPU moderna, 60000 iteraciones vacias tardan microsegundos -- muchisimo
 * menos que la resolucion de 'reloj' (~10ms) -- asi que esa rama de
 * "hardware roto" se disparaba en practicamente todos los frames (ver
 * docs/architecture/12-port-progreso.md checkpoint 9). port_frame_yield()
 * cede el resto del quantum de CPU en cada vuelta del bucle (Sleep(1))
 * para que cada iteracion represente una cantidad de tiempo real
 * comparable a la de 1998, sin tener que retocar el umbral ni la logica
 * original de frame_start(). */
extern void port_frame_yield(void);

/* PORT: el driver de musica de libmikmod (winmm) no tiene hilo propio --
 * es poll-driven: MikMod_Update() hay que llamarlo una vez por frame
 * (frame_end() en i.cpp) para que el mezclador rellene los buffers de
 * waveOut. Sin esta llamada la musica se congela a los ~2 buffers
 * iniciales (240ms). Implementado en port/io/io_song.c. */
extern void io_song_update(void);

#ifdef __cplusplus
}
#endif

struct _dos_ftime { unsigned short time; };

extern void chain_intr(void(*handler)(void));
extern void _chain_intr(void(*handler)(void));
extern void (*_int86_intr)(void);

#define _dos_getftime(fd, ft)   0
#define _dos_setftime(fd, ft)   0
#define _getdta() 0
#define _setdta(dt) ((void)0)

#define _harderr(h) ((void)(h))
#define _hardresume(rt) ((void)(rt))
#define _hardretn(ret) ((void)(ret))

/* Valores de retorno clasicos del handler de _harderr (INT 24h AL bits 0-1).
 * Usado en i.cpp: critical_error() devuelve _HARDERR_IGNORE. */
#define _HARDERR_IGNORE 0
#define _HARDERR_RETRY  1
#define _HARDERR_ABORT  2
#define _HARDERR_FAIL   3

#define LOOKUPDLLS 1
#define SALTARN 0

/* Bit de carry (CF) del registro de flags x86 tras una int386, tal como lo
 * usa f.cpp (disk_free): if (!(regs.w.cflag & INTR_CF)) ... */
#define INTR_CF 1

#endif /* PORT_DIV_SHIM_DOS_H */
