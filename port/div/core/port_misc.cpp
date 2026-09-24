/*
 *  port_misc.cpp -- implementaciones reales de los shims declarados en
 *  port/div/shim/{dos,bios}.h. Ver docs/architecture/12-port-progreso.md
 *  para el detalle de cada punto de uso original en i.cpp/f.cpp/kernel.cpp
 *  y las limitaciones conocidas de cada aproximacion.
 *
 *  Unidad de compilacion AISLADA a proposito: NO incluye port_pre.h ni los
 *  headers del nucleo DIV (inter.h/divsound.h/...), asi que puede incluir
 *  <windows.h>/<io.h> sin arriesgar colisiones de macros con el resto del
 *  core (far/near/word/byte, etc. definidos por las shims para ese lado).
 */
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

#include "../shim/dos.h"
#include "../shim/bios.h"

/* ==========================================================================
 *  _bios_timeofday -- unico uso real: i.cpp:241, sembrar el RNG con el
 *  reloj del sistema al arrancar. No necesita ser el reloj PIT real, solo
 *  un valor que cambie entre ejecuciones.
 * ========================================================================== */
extern "C" int _bios_timeofday(int cmd, long *timerticks)
{
    if (cmd != _TIME_GETCLOCK)
        return 0;
    if (timerticks)
        *timerticks = (long)GetTickCount64();
    return 0;
}

/* ==========================================================================
 *  _dos_findfirst / _dos_findnext / _dos_setfileattr
 *  -- usados por get_dirinfo/get_fileinfo y varias sentencias de fichero
 *  del lenguaje DIV (f.cpp, i.cpp). Reimplementados sobre _findfirst64/
 *  _findnext64 (<io.h>) traduciendo atributos y empaquetando fecha/hora al
 *  formato DOS de 16 bits que ya interpreta f.cpp (macros YEAR/MONTH/...).
 * ========================================================================== */

static void pack_dos_datetime(time_t t, unsigned short *wr_date, unsigned short *wr_time)
{
    struct tm tmv;
    if (t == 0 || localtime_s(&tmv, &t) != 0) {
        *wr_date = 0;
        *wr_time = 0;
        return;
    }
    int year = tmv.tm_year + 1900;
    if (year < 1980) year = 1980; /* el formato DOS no representa fechas anteriores */
    *wr_date = (unsigned short)(((year - 1980) << 9) | ((tmv.tm_mon + 1) << 5) | tmv.tm_mday);
    *wr_time = (unsigned short)((tmv.tm_hour << 11) | (tmv.tm_min << 5) | (tmv.tm_sec / 2));
}

static struct __finddata64_t g_finddata; /* handle _dos_find* solo soporta una busqueda activa a la vez,
                                             igual que el DTA unico del DOS original */
static intptr_t g_findhandle = -1;

static void fill_find_t(struct find_t *out)
{
    memset(out, 0, sizeof(*out));
    out->attrib = (unsigned char)g_finddata.attrib;
    out->size = (unsigned long)g_finddata.size;
    pack_dos_datetime(g_finddata.time_write, &out->wr_date, &out->wr_time);
    strncpy(out->name, g_finddata.name, sizeof(out->name) - 1);
}

extern "C" unsigned _dos_findfirst(const char *path, unsigned attrib, struct find_t *fileinfo)
{
    (void)attrib; /* _findfirst64 no filtra por atributos como el DOS original;
                     el codigo DIV ya vuelve a comprobar attrib si le importa */
    if (g_findhandle != -1) {
        _findclose(g_findhandle);
        g_findhandle = -1;
    }
    g_findhandle = _findfirst64(path, &g_finddata);
    if (g_findhandle == -1)
        return (unsigned)errno;
    fill_find_t(fileinfo);
    return 0;
}

extern "C" unsigned _dos_findnext(struct find_t *fileinfo)
{
    if (g_findhandle == -1)
        return (unsigned)ENOENT;
    if (_findnext64(g_findhandle, &g_finddata) != 0)
        return (unsigned)errno;
    fill_find_t(fileinfo);
    return 0;
}

extern "C" unsigned _dos_setfileattr(const char *path, unsigned attrib)
{
    DWORD winattr = FILE_ATTRIBUTE_NORMAL;
    if (attrib & _A_RDONLY) winattr |= FILE_ATTRIBUTE_READONLY;
    if (attrib & _A_HIDDEN) winattr |= FILE_ATTRIBUTE_HIDDEN;
    if (attrib & _A_SYSTEM) winattr |= FILE_ATTRIBUTE_SYSTEM;
    if (winattr == FILE_ATTRIBUTE_NORMAL && attrib != _A_NORMAL)
        winattr = FILE_ATTRIBUTE_NORMAL;
    return SetFileAttributesA(path, winattr) ? 0 : (unsigned)GetLastError();
}

/* ==========================================================================
 *  _dos_getdrive / _dos_setdrive / _dos_getdiskfree
 *  -- sobre las equivalentes modernas _getdrive/_chdrive/_getdiskfree.
 *  Numeracion identica en ambos mundos (1=A, 2=B, 3=C, ...).
 * ========================================================================== */

extern "C" unsigned _dos_getdrive(unsigned *pdrive)
{
    if (pdrive)
        *pdrive = (unsigned)_getdrive();
    return 0;
}

extern "C" unsigned _dos_setdrive(unsigned drive, unsigned *pnumdrives)
{
    _chdrive((int)drive);
    if (pnumdrives) {
        DWORD mask = GetLogicalDrives();
        unsigned count = 0;
        while (mask) { count += (mask & 1); mask >>= 1; }
        *pnumdrives = count;
    }
    return 0;
}

extern "C" unsigned _dos_getdiskfree(unsigned drive, struct diskfree_t *diskspace)
{
    if (!diskspace)
        return (unsigned)-1;
    return _getdiskfree(drive, (struct _diskfree_t *)diskspace);
}

/* ==========================================================================
 *  _setvideomode -- v.cpp/vesa.asm original; el port sustituye toda la
 *  capa de modo de video por port/io (raylib). Aqui solo evita el error de
 *  enlazado: siempre "exito", el video real ya lo gestiona port/io.
 * ========================================================================== */
extern "C" int _setvideomode(int mode)
{
    (void)mode;
    return 1;
}

/* ==========================================================================
 *  int386 / int386x -- unicos dos puntos de uso reales en f.cpp (ver
 *  docs/architecture/12-port-progreso.md, tabla de llamadas DOS/BIOS):
 *
 *   1) int386(0x21, ...) con AX=0x4409 dentro de disk_free(): consulta
 *      IOCTL "es un dispositivo de bloques que soporta E/S generica" para
 *      decidir si continuar con _dos_getdiskfree. Devolvemos "sin error,
 *      IOCTL soportado" para que siempre continue por esa rama.
 *
 *   2) int386x(0x31, ...) con EAX=0x0500 dentro de GetFreeMem()/
 *      memory_free(): funcion DPMI "Get Free Memory Information", que
 *      escribe un bloque de 12 unsigned long (48 bytes) en ES:EDI. Se
 *      reconstruye el puntero plano desde FP_SEG(ES)/FP_OFF(EDI) (ver
 *      limitacion mas abajo) y se rellena con datos reales de
 *      GlobalMemoryStatusEx, en unidades de pagina de 4 KB para que las
 *      formulas de memory_free() (que dividen por 1024 asumiendo Kb)
 *      den un resultado del orden de magnitud correcto.
 *
 *  LIMITACION CONOCIDA (no resuelta en este commit): FP_SEG/FP_OFF (ver
 *  port/div/shim/i86.h) empaquetan un puntero partiendolo en dos mitades
 *  de 16 bits (esquema real-mode de 20 bits utiles). Sobre un proceso
 *  x64, esto solo reconstruye la direccion original correctamente si esa
 *  direccion cabe en 32 bits, cosa que el sistema operativo NO garantiza
 *  para una variable de pila corriente. Si el puntero real de `meminfo`
 *  cae por encima de 4 GB, GetFreeMem() escribira en una direccion
 *  incorrecta. Mitigacion pendiente: la via correcta es tocar f.cpp para
 *  que memory_free() no pase por int386x en el port (ver TODO en el
 *  documento de progreso); por ahora queda documentado como riesgo, no
 *  como bug oculto.
 * ========================================================================== */

static void fill_dpmi_meminfo(void *flat)
{
    if (!flat)
        return;
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    unsigned long *m = (unsigned long *)flat; /* 12 x unsigned long, layout identico
                                                  al "meminfo" privado de f.cpp */
    unsigned long free_pages = (unsigned long)(ms.ullAvailPhys / 4096);
    unsigned long total_pages = (unsigned long)(ms.ullTotalPhys / 4096);
    unsigned long avail_linear = (unsigned long)(ms.ullAvailVirtual / 4096);

    m[0] = free_pages;    /* Bloque_mas_grande_disponible (aprox.) */
    m[1] = free_pages;    /* Maximo_de_paginas_desbloqueadas */
    m[2] = free_pages;    /* Pagina_bloqueable_mas_grande */
    m[3] = avail_linear;  /* Espacio_de_direccionamiento_lineal */
    m[4] = free_pages;    /* Numero_de_paginas_libres_disponibles */
    m[5] = free_pages;    /* Numero_de_paginas_fisicas_libres */
    m[6] = total_pages;   /* Total_de_paginas_fisicas */
    m[7] = avail_linear;  /* Espacio_de_direccionamiento_lineal_libre */
    m[8] = 0;             /* Tamano_del_fichero_de_paginas (no aplicable) */
    m[9] = m[10] = m[11] = 0; /* reservado[3] */
}

extern "C" int int386(int intno, union REGS *in, union REGS *out)
{
    if (out && out != in)
        *out = *in;

    if (intno == 0x21 && in->w.ax == 0x4409) {
        out->w.cflag = 0;      /* sin error */
        out->w.dx = (1 << 9);  /* bit 9: "soporta E/S generica" (ver disk_free en f.cpp) */
        return 0;
    }

    if (out) out->w.cflag = 1; /* interrupcion no emulada: fallar cerrado, no en silencio */
    return -1;
}

extern "C" int int386x(int intno, union REGS *in, union REGS *out, struct SREGS *segregs)
{
    if (out && out != in)
        *out = *in;

    if (intno == 0x31 && in->x.eax == 0x0500 && segregs) {
        void *flat = (void *)(((uintptr_t)segregs->es << 16) | (uint16_t)in->x.edi);
        fill_dpmi_meminfo(flat);
        if (out) out->x.cflag = 0;
        return 0;
    }

    if (out) out->x.cflag = 1;
    return -1;
}

/* ==========================================================================
 *  port_get_reloj -- ver comentario junto a su declaracion en
 *  port/div/shim/dos.h. Calcula un "reloj" equivalente al contador de
 *  ~100Hz que la interrupcion de temporizador real de DOS mantenia en el
 *  original (variable global 'reloj' en i.cpp/f.cpp), a partir de
 *  QueryPerformanceCounter -- mismo mecanismo que port/io/io_timer.c
 *  (io_time_now), reimplementado aqui en vez de llamar a port/io
 *  directamente para no crear una dependencia nueva de i.cpp/f.cpp/kernel.cpp
 *  hacia esa capa (que hasta ahora solo tocan v.cpp/mouse.cpp/divkeybo.cpp/
 *  divsound.cpp, los ficheros designados como "bridge").
 * ========================================================================== */
extern "C" int port_get_reloj(void)
{
    static LARGE_INTEGER freq = {0};
    static LARGE_INTEGER start = {0};
    LARGE_INTEGER now;

    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
    QueryPerformanceCounter(&now);
    return (int)(((now.QuadPart - start.QuadPart) * 100) / freq.QuadPart);
}

/* ==========================================================================
 *  port_frame_yield -- ver comentario junto a su declaracion en
 *  port/div/shim/dos.h.
 * ========================================================================== */
extern "C" void port_frame_yield(void)
{
    Sleep(1);
}

/* ==========================================================================
 *  chain_intr / _chain_intr / _int86_intr / _harderr* -- declarados en
 *  dos.h pero NO referenciados por ningun fichero portado en este commit
 *  (ver grep en docs/architecture/12-port-progreso.md). _harderr* ya son
 *  macros no-op en el propio shim; chain_intr/_int86_intr se dejan solo
 *  como declaraciones (sin cuerpo) hasta que algo los use de verdad --
 *  si el enlazado llegase a fallar por esto, es la senal de que ya hace
 *  falta portar el modulo que los invoca.
 * ========================================================================== */
