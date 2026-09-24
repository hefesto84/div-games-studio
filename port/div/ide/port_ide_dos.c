/*
 * port_ide_dos.c -- puentes nativos Windows para la superficie DOS/BIOS que
 * usa el nucleo UI del IDE.
 *
 * En el hito E1 estos simbolos se resolvieron como punteros de datos nulos
 * en port_ide_stubs.c (bastaba con enlazar). En cuanto el IDE arranca de
 * verdad (E2) eso dejaria de ser inocuo: llamarlos saltaria a una direccion
 * de datos. Aqui pasan a ser funciones reales, implementadas contra Win32 y
 * la CRT cuando tienen equivalente, o no-ops deliberados cuando no lo tienen.
 *
 * Ver docs/architecture/12-port-progreso.md (hito E2.1).
 */

#include "global.h"
#include <dos.h>
#include <math.h>
#include "div_io.h"
#include <windows.h>

/* ------------------------------------------------------------------ */
/*  Reloj de 100 Hz (antes: IRQ 0 del PIT reprogramado)                */
/* ------------------------------------------------------------------ */

/* PORT: `reloj` lo incrementaba el handler de la IRQ del temporizador a
 * 100 Hz. Aqui se deriva del reloj monotono de port/io y se refresca desde
 * port_ide_tick(), que llama el bucle de volcado. */
int reloj = 0;

/* div.cpp apunta `system_clock` a (void*)0x46c, el contador de ticks de la
 * BIOS en DOS. Esa direccion no existe en Windows: los desreferencios de
 * *system_clock (doble click, get_input[], parpadeo del cursor...) no solo
 * dan un valor basura, si no que fallan con 0xC0000005. Se reengancha a
 * `reloj` (100 Hz) en el primer tick, antes de cualquier interaccion. */
extern int *system_clock;

void port_ide_tick(void)
{
    system_clock = &reloj;
    reloj = (int)(io_timer_micros() / 10000ULL); /* 100 Hz */

    /* PORT (audio del IDE, E7): el driver de musica de io_song.c (winmm)
     * no tiene hilo propio -- hay que darle una vuelta por frame o la
     * cancion se congela tras llenar el buffer inicial. tecla() (que llama
     * a esto) se ejecuta en cada vuelta del bucle principal, igual que
     * frame_end() en el runtime (ver port/io/io_song.c). */
    io_song_update();
}

/* ------------------------------------------------------------------ */
/*  Emulacion de las interrupciones que usa el nucleo UI               */
/* ------------------------------------------------------------------ */

/* Ultima posicion del puntero real leida de la ventana, para detectar si el
 * raton fisico se ha movido entre dos lecturas de INT 33h AX=0Bh. */
static int mouse_last_x = -1;
static int mouse_last_y = -1;

/* Acumulador de posicion del cursor del IDE (divmouse.cpp). El original
 * sumaba ahi los incrementos que devolvia el driver DOS; aqui se usa como
 * referencia para calcular un incremento que lo deje en la posicion
 * absoluta del puntero de la ventana. */
extern float m_x, m_y;

int port_int386(int intno, union REGS *in, union REGS *out)
{
    int x = 0, y = 0;

    if (out == NULL || in == NULL)
        return 0;

    if (out != in)
        memset(out, 0, sizeof(*out));

    switch (intno) {
    case 0x33: /* Driver de raton */
        switch (in->w.ax) {
        case 0x00: /* Reset / deteccion */
            out->w.ax = 0xffff; /* siempre hay raton */
            out->w.bx = 3;      /* numero de botones */
            mouse_last_x = -1;
            mouse_last_y = -1;
            break;
        case 0x03: /* Estado de botones y posicion */
            io_poll(); /* ver comentario en io_poll() (port/io/io_input.c) */
            io_get_mouse(&x, &y);
            out->w.bx = (unsigned short)((io_mouse_button(0) ? 1 : 0) |
                                         (io_mouse_button(1) ? 2 : 0) |
                                         (io_mouse_button(2) ? 4 : 0));
            out->w.cx = (unsigned short)x;
            out->w.dx = (unsigned short)y;
            break;
        case 0x0b: /* Incremento desde la ultima lectura */
            io_get_mouse(&x, &y);
            if (mouse_last_x < 0) {
                mouse_last_x = x;
                mouse_last_y = y;
            }
            if (x != mouse_last_x || y != mouse_last_y) {
                /* PORT: en DOS el driver mantenia su propia posicion y solo
                 * devolvia incrementos relativos. En una ventana el puntero
                 * es absoluto, asi que se devuelve el incremento exacto que
                 * lleva el cursor del IDE hasta el puntero real. Eso corrige
                 * de paso cualquier desfase acumulado al clampear en los
                 * bordes. read_mouse2() divide por este mismo factor. */
                double ratio = 1.0 + (double)Setupfile.mouse_ratio / 3.0;
                out->w.cx = (unsigned short)(short)lround((x - (double)m_x) * ratio);
                out->w.dx = (unsigned short)(short)lround((y - (double)m_y) * ratio);
                mouse_last_x = x;
                mouse_last_y = y;
            }
            /* Si el puntero real no se ha movido se devuelve 0: asi el
             * desplazamiento por teclado (set_mouse) no se pisa. */
            break;
        default:
            break;
        }
        break;

    case 0x10: /* BIOS de video: sin equivalente (paleta/borde via port/io) */
    default:
        break;
    }

    return 0;
}

/* PORT: MemInfo1() (divsetup.cpp) y su gemela de div.cpp:3100 ("Sistema ->
 * Informacion del sistema", dos ventanas distintas que hacen lo mismo)
 * llaman a int386x(0x31, ..., EAX=0x0500) -- DPMI "Get Free Memory
 * Information", que en DOS escribia un bloque de 12 unsigned long en
 * ES:EDI -- para rellenar el cuadro de memoria libre. Sin caso para 0x31,
 * caia al `default` de port_int386() (que ademas ignora `s`) y dejaba el
 * meminfo local con basura de pila sin inicializar: la ventana mostraba
 * numeros sin sentido (o "0", segun lo que hubiera en la pila).
 *
 * A diferencia del runtime (int386x en port/div/core/port_misc.cpp), aqui
 * NO se reconstruye la direccion real a partir de ES:EDI con el truco
 * FP_SEG/FP_OFF de 32 bits: en este port, FP_SEG()/FP_OFF() (mas abajo en
 * este mismo fichero) son no-ops que siempre devuelven 0 -- perfectamente
 * validos para los usos DOS reales que ya no aplican (el IDE nunca tuvo
 * otra necesidad de ellos), pero eso significaba que ES:EDI llegaban aqui
 * siempre a 0/0, sin ninguna informacion real del puntero. En vez de darle
 * "sentido" a FP_SEG/FP_OFF (que tocaria ademas divkeybo.cpp/cdrom.cpp, no
 * compilados pero con el mismo patron, y reintroduciria el mismo riesgo de
 * direcciones por encima de 4 GB que el runtime acepta como conocido), se
 * usa un "handle" de una sola entrada: FP_SEG(p)/FP_OFF(p) guardan el
 * puntero real tal cual se les pasa (`port_fp_last_ptr`, ver la definicion
 * de FP_SEG/FP_OFF mas abajo en este mismo fichero) y aqui se lee
 * directamente, sin pasar por ES:EDI en absoluto. Vale porque las dos
 * llamadoras hacen FP_SEG(p); FP_OFF(p); int386x(...); en secuencia
 * inmediata con el mismo `p`, sin nada intermedio que lo pise. */
static void *port_fp_last_ptr = NULL; /* ver FP_SEG/FP_OFF mas abajo */

/* PORT: el DPMI real escribe 12 unsigned long (48 bytes); el struct de
 * div.cpp (`struct _meminfo MemInfo`, 9 campos = 36 bytes) es mas pequeno
 * que el de divsetup.cpp (`meminfo`, 12 campos = 48 bytes) -- escribir los
 * 12 campos siempre desbordaria 12 bytes sobre `MemInfo` (en `div.cpp`, un
 * global; el siguiente dato en memoria es `char MemoriaLibre[100]`, no algo
 * critico, pero desbordar no es el objetivo aqui). Los dos unicos
 * llamadores reales (`GetMemoryFree()` en div.cpp, `MemInfo1()` en
 * divsetup.cpp) solo leen el primer campo ("Bloque_mas_grande_disponible" /
 * "data1"), asi que basta con escribir ese unico `unsigned long` -- ni el
 * struct mas pequeno se desborda, ni hace falta saber cual de los dos
 * llamo. */
static void port_dpmi_fill_meminfo(void *flat)
{
    MEMORYSTATUSEX ms;
    unsigned long long avail_kb;

    if (!flat)
        return;

    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    /* PORT: el campo 0 real de DPMI 0500h es "mayor bloque libre disponible,
     * en bytes", pero cabe en un `unsigned` de 32 bits -- en bytes eso
     * satura sobre ~4 GB, muy por debajo de la RAM libre real en un PC
     * moderno (dato reportado por el usuario: "Informacion del sistema"
     * mostraba justo ~4 GB fijos sin importar la RAM real de la maquina).
     * La spec DPMI original nunca preveia mas de eso en 1999. Se devuelve
     * en KB en vez de bytes (rango realista, hasta ~4 TB) -- el unico
     * consumidor real de este campo, MemInfo1() en divsetup.cpp, se ajusto
     * a juego (tools/patch_meminfo_units.py) para no re-dividir por 1024
     * asumiendo bytes. Satura a ULONG_MAX KB en vez de envolver si algun
     * dia hay mas de eso (dar un numero mas pequeno que el real seria peor
     * que saturar). */
    avail_kb = ms.ullAvailPhys / 1024;
    *(unsigned long *)flat = (avail_kb > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (unsigned long)avail_kb;
}

int port_int386x(int intno, union REGS *in, union REGS *out, struct SREGS *s)
{
    if (intno == 0x31 && in->x.eax == 0x0500 && s) {
        port_dpmi_fill_meminfo(port_fp_last_ptr);
        if (out) {
            if (out != in) *out = *in;
            out->x.cflag = 0;
        }
        return 0;
    }
    return port_int386(intno, in, out);
}

void segread(struct SREGS *s)
{
    if (s)
        memset(s, 0, sizeof(*s));
}

/* PORT: punteros seg:off de 16 bits; sin sentido en x64 plano. El unico uso
 * real en el codigo compilado (div.cpp:3100-3101, divsetup.cpp:355-356) es
 * FP_SEG(p); FP_OFF(p); int386x(0x31,...); en secuencia inmediata para
 * pasarle un puntero a la llamada DPMI "Get Free Memory Information" (ver
 * port_dpmi_fill_meminfo() mas arriba) -- el valor de retorno en si (los
 * "0000:0000" que devolvian antes) nunca se usa para otra cosa, asi que
 * basta con recordar el puntero real para que port_int386x() lo recupere
 * directamente, sin fingir un segmento/offset que no tiene sentido en x64. */
unsigned short FP_SEG(void *p)
{
    port_fp_last_ptr = p;
    return 0;
}

unsigned int FP_OFF(void *p)
{
    port_fp_last_ptr = p;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Servicios DOS de ficheros                                          */
/* ------------------------------------------------------------------ */

/* find_t.reserved (21 bytes) guarda el HANDLE de busqueda de Win32 y el
 * filtro de atributos, igual que el original guardaba ahi el DTA de DOS. */
struct port_find_state {
    HANDLE handle;
    unsigned attr;
};

static void port_fill_find(struct find_t *info, const WIN32_FIND_DATAA *fd)
{
    info->attrib = 0;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) info->attrib |= _A_SUBDIR;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_READONLY)  info->attrib |= _A_RDONLY;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)    info->attrib |= _A_HIDDEN;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)    info->attrib |= _A_SYSTEM;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)   info->attrib |= _A_ARCH;

    info->size = (unsigned long)fd->nFileSizeLow;

    {
        FILETIME lft;
        WORD d = 0, t = 0;
        if (FileTimeToLocalFileTime(&fd->ftLastWriteTime, &lft))
            FileTimeToDosDateTime(&lft, &d, &t);
        info->wr_date = (unsigned short)d;
        info->wr_time = (unsigned short)t;
    }

    /* Nombre estilo DOS 8.3 (en mayusculas, como devolvia el findfirst de
     * DOS). El IDE asume nombres de <=12 chars en todas partes: dir_abrirbr
     * (divbrow.cpp) hace strcpy a slots de an_archivo/an_directorio = 13, y
     * un nombre largo de Windows ("div32run_port.exe") desbordaba esos
     * arrays -> corrupcion y petada al navegar en el browser. Con alias 8.3
     * (cAlternateFileName) el nombre corto ABRE el fichero de verdad. Si un
     * nombre largo no tiene alias (8.3 desactivado en el volumen), se deja
     * vacio y port_find_accept lo descarta: no es representable en esta UI.
     */
    {
        const char *src = fd->cFileName;
        size_t len = strlen(src);
        size_t i;

        if (len > 12 && fd->cAlternateFileName[0])
            src = fd->cAlternateFileName;
        else if (len > 12)
            src = "";

        for (i = 0; src[i] && i < sizeof(info->name) - 1; i++)
            info->name[i] = (char)toupper((unsigned char)src[i]);
        info->name[i] = 0;
    }
}

/* Semantica Watcom: `attr` es el conjunto de atributos ADMITIDOS ademas de
 * los ficheros normales. El IDE solo usa _A_NORMAL (solo ficheros) y
 * _A_SUBDIR (ficheros + directorios). */
static int port_find_accept(const struct find_t *info, unsigned attr)
{
    if (info->name[0] == 0) return 0; /* nombre largo sin alias 8.3: no listar */
    if ((info->attrib & _A_SUBDIR) && !(attr & _A_SUBDIR)) return 0;
    if ((info->attrib & _A_HIDDEN) && !(attr & _A_HIDDEN)) return 0;
    if ((info->attrib & _A_SYSTEM) && !(attr & _A_SYSTEM)) return 0;
    return 1;
}

unsigned _dos_findfirst(const char *name, unsigned attr, struct find_t *info)
{
    struct port_find_state st;
    WIN32_FIND_DATAA fd;

    if (info == NULL || name == NULL)
        return 1;

    memset(info, 0, sizeof(*info));

    st.handle = FindFirstFileA(name, &fd);
    st.attr = attr;
    if (st.handle == INVALID_HANDLE_VALUE)
        return 1;

    for (;;) {
        port_fill_find(info, &fd);
        if (port_find_accept(info, attr)) {
            memcpy(info->reserved, &st, sizeof(st));
            return 0;
        }
        if (!FindNextFileA(st.handle, &fd)) {
            FindClose(st.handle);
            return 1;
        }
    }
}

unsigned _dos_findnext(struct find_t *info)
{
    struct port_find_state st;
    WIN32_FIND_DATAA fd;

    if (info == NULL)
        return 1;

    memcpy(&st, info->reserved, sizeof(st));
    if (st.handle == NULL || st.handle == INVALID_HANDLE_VALUE)
        return 1;

    while (FindNextFileA(st.handle, &fd)) {
        port_fill_find(info, &fd);
        if (port_find_accept(info, st.attr)) {
            memcpy(info->reserved, &st, sizeof(st));
            return 0;
        }
    }

    FindClose(st.handle);
    memset(info->reserved, 0, sizeof(info->reserved));
    return 1;
}

unsigned _dos_setfileattr(const char *path, unsigned attr)
{
    DWORD w = FILE_ATTRIBUTE_NORMAL;

    if (path == NULL)
        return 1;

    if (attr & _A_RDONLY) w |= FILE_ATTRIBUTE_READONLY;
    if (attr & _A_HIDDEN) w |= FILE_ATTRIBUTE_HIDDEN;
    if (attr & _A_SYSTEM) w |= FILE_ATTRIBUTE_SYSTEM;
    if (attr & _A_ARCH)   w |= FILE_ATTRIBUTE_ARCHIVE;
    if (w != FILE_ATTRIBUTE_NORMAL)
        w &= ~(DWORD)FILE_ATTRIBUTE_NORMAL;

    return SetFileAttributesA(path, w) ? 0 : 1;
}

/* PORT: 1 = A:, 2 = B:, 3 = C: ... `total` recibe el numero de unidades. */
unsigned _dos_setdrive(unsigned drive, unsigned *total)
{
    char path[4];

    if (total)
        *total = 26;

    if (drive < 1 || drive > 26)
        return 0;

    path[0] = (char)('A' + drive - 1);
    path[1] = ':';
    path[2] = '\\';
    path[3] = 0;
    _chdrive((int)drive);
    (void)path;

    return _getdrive();
}

int getdisk(void)
{
    return _getdrive() - 1;
}

int setdisk(int drive)
{
    _chdrive(drive + 1);
    return 26;
}

/* PORT: GetFreeUnid() informaba del espacio libre de la unidad para el aviso
 * "no hay sitio en disco" de check_free(). */
unsigned int GetFreeUnid(char unidad)
{
    char root[4];
    ULARGE_INTEGER freeb;

    root[0] = (char)('A' + (unidad - 1));
    root[1] = ':';
    root[2] = '\\';
    root[3] = 0;

    if (!GetDiskFreeSpaceExA(root, &freeb, NULL, NULL))
        return 0x7fffffffu;

    if (freeb.QuadPart > 0x7fffffffULL)
        return 0x7fffffffu;

    return (unsigned int)freeb.QuadPart;
}

/* PORT: sondeos de memoria DPMI/DOS para el informe de memoria libre. En
 * Win64 no hay equivalente ni utilidad; devolver 0 corta los bucles que los
 * llaman (`for (n=0; DOSalloc4k(); n++)`). */
int DOSalloc4k(void)
{
    return 0;
}

int DPMIalloc4k(void)
{
    return 0;
}

int _heapshrink(void)
{
    return 0;
}

/* PORT: manejador de errores criticos de DOS (int 24h). Sin equivalente. */
void _harderr(void *handler)
{
    (void)handler;
}

/* ------------------------------------------------------------------ */
/*  Modo texto de arranque (solo se usa con el argumento INIT)         */
/* ------------------------------------------------------------------ */

int _setvideomode(int mode)
{
    (void)mode;
    return 0;
}

long _setbkcolor(long color)
{
    (void)color;
    return 0;
}

short _settextcolor(short color)
{
    (void)color;
    return 0;
}

void _outtext(const char *text)
{
    if (text)
        fputs(text, stdout);
}

/* ------------------------------------------------------------------ */
/*  Audio / CD (pendiente de E4)                                       */
/* ------------------------------------------------------------------ */

void InitSound(void)
{
}

void EndSound(void)
{
}

int CDinit(void)
{
    return 0;
}

int get_cd_error(void)
{
    return 0;
}

void *sbmalloc(int size)
{
    return malloc((size_t)(size > 0 ? size : 1));
}

void sbfree(void *p)
{
    free(p);
}
