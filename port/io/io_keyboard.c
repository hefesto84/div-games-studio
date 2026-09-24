/*
 * io_keyboard.c -- cola de teclado estilo INT 16h sobre raylib.
 *
 * Problema que resuelve: raylib vacia sus colas de eventos (`GetKeyPressed`,
 * `GetCharPressed`) en cada `PollInputEvents()`, que a su vez se ejecuta
 * dentro de `EndDrawing()`. Si el consumidor no las lee justo despues, los
 * eventos se pierden. El IDE de DIV no puede garantizarlo: hace volcados de
 * pantalla en momentos arbitrarios y tiene bucles de espera activa que no
 * dibujan nada.
 *
 * Solucion: en cuanto raylib refresca sus colas se trasvasan a una cola
 * propia persistente, emparejando ademas cada pulsacion con el caracter que
 * haya generado, que es justo el formato (ascii, scancode, modificadores)
 * que devolvia la BIOS y que espera divkeybo.
 *
 * Ver docs/architecture/12-port-progreso.md (hito E2.3).
 */

#include "div_io.h"
#include "io_keymap.h"

#include <string.h>

#include "raylib.h"

/* Mismo tamano que la cola interna del original (64 eventos). */
#define EVQ_SIZE 64

static io_key_event_t evq[EVQ_SIZE];
static int evq_head = 0;    /* siguiente a extraer */
static int evq_tail = 0;    /* siguiente a escribir */

static unsigned char keys_down[128];
static unsigned char keys_made[128];

/* ------------------------------------------------------------------ */
/*  Estado de modificadores                                            */
/* ------------------------------------------------------------------ */

unsigned short io_shift_status(void)
{
    unsigned short s = 0;

    if (IsKeyDown(KEY_RIGHT_SHIFT))   s |= 0x0001; /* SS_RIGHT_SHIFT */
    if (IsKeyDown(KEY_LEFT_SHIFT))    s |= 0x0002; /* SS_LEFT_SHIFT  */
    if (IsKeyDown(KEY_LEFT_CONTROL))  s |= 0x0104; /* SS_CTRL | SS_LEFT_CTRL  */
    if (IsKeyDown(KEY_RIGHT_CONTROL)) s |= 0x0404; /* SS_CTRL | SS_RIGHT_CTRL */
    if (IsKeyDown(KEY_LEFT_ALT))      s |= 0x0208; /* SS_ALT | SS_LEFT_ALT    */
    if (IsKeyDown(KEY_RIGHT_ALT))     s |= 0x0808; /* SS_ALT | SS_RIGHT_ALT   */

    /* Los bits de bloqueo (May/Bloq Num/Bloq Despl) no los expone raylib.
     * No hacen falta: el texto llega ya resuelto por el sistema a traves de
     * GetCharPressed(), que respeta distribucion, mayusculas y AltGr. */
    return s;
}

/* ------------------------------------------------------------------ */
/*  Cola de eventos                                                    */
/* ------------------------------------------------------------------ */

static void evq_push(unsigned char ascii, unsigned char scan, unsigned short shift)
{
    int next = (evq_tail + 1) % EVQ_SIZE;

    if (ascii == 0 && scan == 0)
        return;
    if (next == evq_head)
        return; /* llena: se descarta, como el buffer de la BIOS */

    evq[evq_tail].ascii = ascii;
    evq[evq_tail].scan = scan;
    evq[evq_tail].shift = shift;
    evq_tail = next;
}

int io_key_event_pop(io_key_event_t *ev)
{
    if (evq_head == evq_tail)
        return 0;
    if (ev)
        *ev = evq[evq_head];
    evq_head = (evq_head + 1) % EVQ_SIZE;
    return 1;
}

void io_key_events_clear(void)
{
    evq_head = evq_tail = 0;
    memset(keys_made, 0, sizeof(keys_made));
}

/* ------------------------------------------------------------------ */
/*  Trasvase desde raylib                                              */
/* ------------------------------------------------------------------ */

/* Caracter de control que la BIOS devolvia junto al scancode. */
static unsigned char ascii_de_control(int rkey)
{
    switch (rkey) {
    case KEY_ENTER:
    case KEY_KP_ENTER:  return 13;
    case KEY_BACKSPACE: return 8;
    case KEY_TAB:       return 9;
    case KEY_ESCAPE:    return 27;
    default:            return 0;
    }
}

void io_input_drain(void)
{
    int chars[32];
    int nchars = 0;
    int ichar = 0;
    int c, k, i, n;
    unsigned short shift;
    const io_keymap_entry_t *map = io_keymap_entries(&n);

    /* 1. Caracteres ya resueltos por el sistema (distribucion, mayusculas,
     *    AltGr y repeticion automatica incluidas). */
    while ((c = GetCharPressed()) != 0 && nchars < (int)(sizeof(chars) / sizeof(chars[0])))
        chars[nchars++] = c;

    /* 2. Estado de las teclas fisicas. `made` acumula hasta que alguien lo
     *    lea, para no perder pulsaciones entre dos llamadas. */
    for (i = 0; i < n; i++) {
        unsigned char sc = map[i].scan;
        if (sc == 0 || sc >= 128)
            continue;
        if (IsKeyDown(map[i].rkey)) {
            keys_down[sc] = 1;
            if (IsKeyPressed(map[i].rkey) || IsKeyPressedRepeat(map[i].rkey))
                keys_made[sc] = 1;
        } else {
            keys_down[sc] = 0;
        }
    }

    shift = io_shift_status();

    /* 3. Pulsaciones en orden, emparejadas con su caracter.
     *    raylib no encola las repeticiones en GetKeyPressed(), asi que se
     *    anaden aparte con IsKeyPressedRepeat(). */
    while ((k = GetKeyPressed()) != 0) {
        unsigned char sc = io_keymap_scan(k);
        unsigned char as = ascii_de_control(k);

        /* Solo las teclas que producen texto consumen un caracter de la cola;
         * asi una flecha pulsada a la vez que se teclea no roba la letra. */
        if (as == 0 && ichar < nchars && io_keymap_is_text(k))
            as = io_keymap_cp850(chars[ichar++]);

        evq_push(as, sc, shift);
    }

    /* Repeticion automatica de teclas sin texto (flechas, borrar, ...). */
    for (i = 0; i < n; i++) {
        if (map[i].text || map[i].scan == 0)
            continue;
        if (IsKeyPressedRepeat(map[i].rkey))
            evq_push(ascii_de_control(map[i].rkey), map[i].scan, shift);
    }

    /* 4. Caracteres sobrantes: repeticiones automaticas de teclas de texto y
     *    combinaciones (teclas muertas, AltGr) que no generan un evento de
     *    pulsacion propio. Se entregan sin scancode. */
    while (ichar < nchars) {
        unsigned char as = io_keymap_cp850(chars[ichar++]);
        if (as)
            evq_push(as, 0, shift);
    }
}

void io_input_pump(void)
{
    if (!IsWindowReady())
        return;
    PollInputEvents();
    io_input_drain();
}

void io_keys_state(unsigned char down[128], unsigned char made[128])
{
    if (down)
        memcpy(down, keys_down, sizeof(keys_down));
    if (made) {
        memcpy(made, keys_made, sizeof(keys_made));
        memset(keys_made, 0, sizeof(keys_made));
    }
}
