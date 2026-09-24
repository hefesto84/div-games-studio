/*
 * port_ide_video.c -- capa de video del IDE de DIV portada a port/io (raylib).
 *
 * Reemplaza a src/div/divvideo.cpp y src/div/det_vesa.cpp, que son los dos
 * unicos modulos del nucleo UI que tocaban hardware de video real:
 *
 *   - divvideo.cpp: modos VESA/SVGA por el kit de SciTech (SV_init/SV_setMode/
 *     SV_setBank), modos-X de VGA pura (registros de secuenciador/CRTC),
 *     escritura del DAC por los puertos 0x3c8/0x3c9 y espera de retrazo por
 *     0x3da, con el framebuffer fisico en 0xA0000.
 *   - det_vesa.cpp: enumeracion de modos VESA via vbeInit()/vbeGetModeInfo().
 *
 * Modelo del original que SI se conserva (es justo el que encaja con raylib):
 * el IDE pinta siempre sobre su propio buffer indexado de 256 colores
 * (`copia`, de vga_an*vga_al bytes) y luego hace un "volcado" a la pantalla,
 * completo o por segmentos de scanline (scan[]). Aqui `vga` pasa a ser el
 * framebuffer indexado de port/io y el volcado termina en io_present().
 *
 * Las partes puras del original (init_volcado/volcado_parcial, el algoritmo
 * de fusion de hasta 2 segmentos por scanline) se copian tal cual, quitando
 * solo la rama de modo-X.
 *
 * Ver docs/architecture/12-port-progreso.md (hito E2.1).
 */

#include "global.h"
#include "div_io.h"

/* PORT: antes `byte * vga = (byte *) 0xA0000;` (segmento fisico de la VGA).
 * Ahora es el framebuffer de 8 bits indexados de port/io. NULL mientras no
 * se haya llamado a svmode(). */
byte *vga = NULL;

/* PORT: el original usaba estas dos para decidir entre VESA lineal, VESA
 * bancado o modo-X. Se conservan porque otros modulos del IDE las leen. */
int LinealMode = 1;
int modovesa = 1;

#define MAX_YRES 4096

static long port_ide_src_nz = -1;  /* diagnostico de DIV_IDE_SHOT */

/* PORT (E2.1): autoverificacion del render sin mirar la pantalla. Con
 * DIV_IDE_SHOT=fichero.png el IDE vuelca el frame DIV_IDE_SHOT_FRAME (60 por
 * defecto) a disco, escribe un resumen del estado por stderr y termina. */
static int port_ide_shot_on(void)
{
    static int on = -1;
    if (on < 0)
        on = getenv("DIV_IDE_SHOT") != NULL;
    return on;
}

static void port_ide_check_shot(void)
{
    static long frame = 0;
    static long target = -1;
    static long every = -1;
    static long serie = 0;

    if (!port_ide_shot_on())
        return;
    if (target < 0) {
        const char *n = getenv("DIV_IDE_SHOT_FRAME");
        const char *e = getenv("DIV_IDE_SHOT_EVERY");
        target = (n && *n) ? atol(n) : 60;
        every = (e && *e) ? atol(e) : 0;
    }
    ++frame;

    /* Modo rafaga: vuelca <base>_NNN.png cada `every` frames y NO termina,
     * para poder seguir una secuencia de interaccion completa. */
    if (every > 0) {
        if (frame >= target && ((frame - target) % every) == 0) {
            char path[1024];
            const char *base = getenv("DIV_IDE_SHOT");
            const char *dot = strrchr(base, '.');
            int stem = dot ? (int)(dot - base) : (int)strlen(base);
            snprintf(path, sizeof(path), "%.*s_%03ld%s", stem, base,
                     serie++, dot ? dot : ".png");
            io_screenshot(path);
        }
        return;
    }

    if (frame >= target) {
        long i, n = (long)vga_an * vga_al, nz = 0, dacsum = 0;
        for (i = 0; i < n; i++) if (vga[i]) nz++;
        for (i = 0; i < 768; i++) dacsum += dac[i];
        fprintf(stderr, "[shot] %dx%d px_no_cero=%ld suma_dac=%ld src_no_cero=%ld"
                " c0..c4=%d,%d,%d,%d,%d tapiz=%p volcados=%ld\n",
                vga_an, vga_al, nz, dacsum, port_ide_src_nz,
                c0, c1, c2, c3, c4, (void *)tapiz, frame);
        io_screenshot(getenv("DIV_IDE_SHOT"));
        exit(0);
    }
}

/* Por scan [x,an,x,an] se definen hasta 2 segmentos a volcar (igual que el
 * original). */
short scan[MAX_YRES * 4];

/* Escala de la ventana sobre la resolucion logica del IDE. A 640x480 una
 * ventana 1:1 es diminuta en un monitor moderno. */
static int port_window_scale(int w)
{
    if (w >= 1024)
        return 1;
    if (w >= 640)
        return 2;
    return 3;
}

/* ------------------------------------------------------------------ */
/*  Deteccion de "modos de video" disponibles                          */
/* ------------------------------------------------------------------ */

/* PORT: no hay BIOS VESA que interrogar. Se declara una lista fija de modos
 * que la ventana puede adoptar (la resolucion logica del framebuffer). Los 6
 * primeros son los que el original ponia siempre a mano (VGA/modo-X); el
 * resto son las resoluciones SVGA tipicas que el IDE ofrece en su dialogo de
 * configuracion de video. inicializacion() (div.cpp) cae a 320x200 si la
 * resolucion guardada en setup.bin no aparece en esta lista, asi que aqui
 * tiene que estar al menos la de por defecto de primera ejecucion (640x480).
 */
void detectar_vesa(void)
{
    static const short lista[][2] = {
        { 320, 200 }, { 320, 240 }, { 320, 400 }, { 360, 240 },
        { 360, 360 }, { 376, 282 }, { 640, 400 }, { 640, 480 },
        { 800, 600 }, { 1024, 768 }, { 1280, 1024 },
        { 1280, 800 }, { 1366, 768 }, { 1600, 900 }, { 1600, 1200 },
        { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 }, { 3840, 2160 },
    };
    int n;

    num_modos = (int)(sizeof(lista) / sizeof(lista[0]));
    if (num_modos > 32)
        num_modos = 32;

    for (n = 0; n < num_modos; n++) {
        modos[n].ancho = lista[n][0];
        modos[n].alto = lista[n][1];
        /* `modo` era el numero de modo VESA; aqui solo hace falta que sea
         * distinto de 0 para los modos "no VGA", porque svmode() del
         * original lo usaba para decidir la rama VESA. */
        modos[n].modo = (short)(n >= 6 ? 0x100 + n : 0);
    }

    VersionVesa = 0x300;
    strcpy(marcavga, "raylib framebuffer (port Windows 11)");
}

/* ------------------------------------------------------------------ */
/*  Paleta                                                             */
/* ------------------------------------------------------------------ */

/* PORT: el original esperaba el retrazo vertical leyendo el puerto 0x3da.
 * No aplica con presentacion por GPU; no-op. Se conserva la funcion porque
 * el resto del IDE la llama. */
void retrazo(void)
{
}

/* PORT: escribia las 256 entradas del DAC por 0x3c8/0x3c9 (6 bits por canal,
 * 0..63) y fijaba el color de borde con una llamada BIOS int 10h AX=1001h.
 * Ahora se convierte a RGB de 8 bits (*4, la misma conversion que usa el
 * propio IDE para dac4[]) y se sube a la LUT de port/io. El color de borde
 * no tiene equivalente en una ventana moderna. */
void set_dac(byte *_dac)
{
    uint8_t rgb[768];
    int n;

    if (_dac == NULL)
        return;

    for (n = 0; n < 768; n++) {
        byte c = _dac[n];
        if (c > 63)
            c = 63;
        rgb[n] = (uint8_t)(c * 4);
    }

    io_palette_set(rgb);
}

/* ------------------------------------------------------------------ */
/*  Modo de video                                                      */
/* ------------------------------------------------------------------ */

/* PORT: crea (o recrea, si cambia la resolucion logica) la ventana raylib y
 * apunta `vga` a su framebuffer indexado. Sustituye a toda la maquinaria
 * VESA/modo-X del original. */
void svmode(void)
{
    if (vga != NULL && io_width() == vga_an && io_height() == vga_al) {
        vga = io_framebuffer();
        return;
    }

    if (io_window_ready())
        io_video_close();

    vga = io_video_init(vga_an, vga_al, port_window_scale(vga_an),
                        "DIV Games Studio 2 (port Windows 11)");

    if (vga == NULL) {
        /* Igual que el original cuando fallaba el modo pedido: cae a VGA. */
        vga_an = 320;
        vga_al = 200;
        vga = io_video_init(vga_an, vga_al, port_window_scale(vga_an),
                            "DIV Games Studio 2 (port Windows 11)");
    }

    /* PORT: la ventana crece hasta llenar casi todo el monitor (la mayor
     * escala entera que quepa), igual que el original ocupaba la pantalla
     * completa. Sin esto, subir la resolucion interna (Vid_Setup, menu
     * Sistema -> Modo de video) encogeria tambien el texto.
     * (La letra proporcional se decide en Load_Cfgbin via big/big2, antes de
     * cargar las fuentes, segun la resolucion logica.) */
    io_video_fit_window(vga_an, vga_al);

    io_timer_init();

    /* PORT (audio del IDE, E7): una sola vez, igual que io_timer_init() --
     * svmode() se puede volver a llamar al cambiar de resolucion (Sistema
     * -> Modo de video), pero el dispositivo de audio no depende de la
     * resolucion y no hay que reabrirlo cada vez. */
    {
        static int audio_inited = 0;
        if (!audio_inited) {
            io_audio_init();
            audio_inited = 1;
        }
    }

    /* El IDE compone su propio cursor dentro de `copia` (volcado_copia()),
     * asi que el del sistema sobra. */
    io_show_cursor(false);
}

/* PORT: SV_restoreMode()+_setvideomode(3) -> cerrar la ventana. */
void rvmode(void)
{
    io_audio_close();
    if (io_window_ready()) {
        io_video_close();
        vga = NULL;
    }
}

/* ------------------------------------------------------------------ */
/*  Volcado del buffer del IDE (`copia`) a la pantalla                 */
/* ------------------------------------------------------------------ */

/* PORT: el original tenia una funcion de volcado por cada modo de video
 * (320x200, modo-X a 4 planos, SVGA lineal, SVGA bancado). Con un unico
 * framebuffer lineal indexado queda una sola, que respeta el mismo contrato:
 * si volcado_completo esta a 1 se copia todo, si no solo los segmentos
 * marcados en scan[] por volcado_parcial(). */
void volcado(char *pc)
{
    byte *p = (byte *)pc;
    int y, n;
    byte *q;

    if (vga == NULL || p == NULL) {
        init_volcado();
        return;
    }

    if (port_ide_shot_on()) {
        long i, t = (long)vga_an * vga_al;
        port_ide_src_nz = 0;
        for (i = 0; i < t; i++) if (p[i]) port_ide_src_nz++;
    }

    if (volcado_completo) {
        memcpy(vga, p, (size_t)vga_an * vga_al);
    } else {
        q = vga;
        for (y = 0; y < vga_al; y++) {
            n = y * 4;
            if (scan[n + 1])
                memcpy(q + scan[n], p + scan[n], (size_t)scan[n + 1]);
            if (scan[n + 3])
                memcpy(q + scan[n + 2], p + scan[n + 2], (size_t)scan[n + 3]);
            q += vga_an;
            p += vga_an;
        }
    }

    init_volcado();

    io_present();
    io_poll();

    /* PORT (E2.1): autoverificacion. Con DIV_IDE_SHOT=fichero.png se vuelca
     * el frame numero DIV_IDE_SHOT_FRAME (por defecto 60) y se sale. */
    port_ide_check_shot();
}

/* PORT: guardaba la pantalla como PCX usando graba_PCX() (divforma.cpp, no
 * portado todavia). No-op hasta E4. */
void snapshot(byte *p)
{
    (void)p;
}

/* ------------------------------------------------------------------ */
/*  Seleccion de la zona a volcar (codigo puro del original)           */
/* ------------------------------------------------------------------ */

void init_volcado(void)
{
    memset(&scan[0], 0, MAX_YRES * 8);
    volcado_completo = 0;
}

void volcado_parcial(int x, int y, int an, int al)
{
    int ymax, xmax, n, d1, d2, x2;

    if (an == vga_an && al == vga_al && x == 0 && y == 0) {
        volcado_completo = 1;
        return;
    }

    if (an > 0 && al > 0 && x < vga_an && y < vga_al) {
        if (x < 0) { an += x; x = 0; }
        if (y < 0) { al += y; y = 0; }
        if (x + an > vga_an) an = vga_an - x;
        if (y + al > vga_al) al = vga_al - y;
        if (an <= 0 || al <= 0)
            return;
        xmax = x + an - 1; ymax = y + al - 1;

        /* PORT: aqui el original ajustaba x/an dividiendo por 4 cuando el
         * modo era un modo-X (4 planos). Eliminado: el framebuffer del port
         * siempre es lineal. */

        while (y <= ymax) {
            n = y * 4;
            if (scan[n + 1] == 0) {         /* Caso 1, el scan estaba vacio */
                scan[n] = (short)x; scan[n + 1] = (short)an;
            } else if (scan[n + 3] == 0) {  /* Caso 2, ya hay un scan */
                if (x > scan[n] + scan[n + 1] || x + an < scan[n]) {
                    if (x > scan[n]) {
                        scan[n + 2] = (short)x; scan[n + 3] = (short)an;
                    } else {
                        scan[n + 2] = scan[n]; scan[n + 3] = scan[n + 1];
                        scan[n] = (short)x; scan[n + 1] = (short)an;
                    }
                } else {
                    if (x < (x2 = scan[n])) scan[n] = (short)x;
                    if (x + an > x2 + scan[n + 1]) scan[n + 1] = (short)(x + an - scan[n]);
                    else scan[n + 1] = (short)(x2 + scan[n + 1] - scan[n]);
                }
            } else {                        /* Caso 3, hay 2 scanes */
                if (x <= scan[n] + scan[n + 1] && x + an >= scan[n + 2]) {
                    if (x < scan[n]) scan[n] = (short)x;
                    if (x + an > scan[n + 2] + scan[n + 3]) scan[n + 1] = (short)(x + an - scan[n]);
                    else scan[n + 1] = (short)(scan[n + 2] + scan[n + 3] - scan[n]);
                    scan[n + 2] = 0; scan[n + 3] = 0;
                } else {
                    if (x > scan[n] + scan[n + 1] || x + an < scan[n]) {
                        if (x > scan[n + 2] + scan[n + 3] || x + an < scan[n + 2]) {
                            if (x + an < scan[n]) d1 = scan[n] - (x + an); else d1 = x - (scan[n] + scan[n + 1]);
                            if (x + an < scan[n + 2]) d2 = scan[n + 2] - (x + an); else d2 = x - (scan[n + 2] + scan[n + 3]);
                            if (d1 <= d2) {
                                if (x < (x2 = scan[n])) scan[n] = (short)x;
                                if (x + an > x2 + scan[n + 1]) scan[n + 1] = (short)(x + an - scan[n]);
                                else scan[n + 1] = (short)(x2 + scan[n + 1] - scan[n]);
                            } else {
                                if (x < (x2 = scan[n + 2])) scan[n + 2] = (short)x;
                                if (x + an > x2 + scan[n + 3]) scan[n + 3] = (short)(x + an - scan[n + 2]);
                                else scan[n + 3] = (short)(x2 + scan[n + 3] - scan[n + 2]);
                            }
                        } else {
                            if (x < (x2 = scan[n + 2])) scan[n + 2] = (short)x;
                            if (x + an > x2 + scan[n + 3]) scan[n + 3] = (short)(x + an - scan[n + 2]);
                            else scan[n + 3] = (short)(x2 + scan[n + 3] - scan[n + 2]);
                        }
                    } else {
                        if (x < (x2 = scan[n])) scan[n] = (short)x;
                        if (x + an > x2 + scan[n + 1]) scan[n + 1] = (short)(x + an - scan[n]);
                        else scan[n + 1] = (short)(x2 + scan[n + 1] - scan[n]);
                    }
                }
            }
            y++;
        }
    }
}
