/*
 * port_ide_keybo.c -- sustituye a src/div/divkeybo.cpp.
 *
 * El original no es portable: instala un handler de la IRQ 9 que lee el
 * puerto 0x60 para mantener kbdFLAGS[], y lee los caracteres con INT 16h
 * encadenando al handler de la BIOS. Aqui se reproduce el mismo contrato
 * sobre port/io:
 *
 *   - `kbdFLAGS[128]`  tabla de teclas pulsadas por scancode (set 1).
 *   - `ascii`/`scan_code`/`shift_status`  ultimo evento extraido.
 *   - `buf`/`ibuf`/`fbuf`  cola circular de 64 eventos. divedit.cpp la mira
 *     por fuera (`ibuf!=fbuf`) para saltarse volcados si el usuario escribe
 *     mas rapido de lo que se redibuja, asi que los simbolos y su semantica
 *     se conservan tal cual.
 *
 * Ver docs/architecture/12-port-progreso.md (hito E2.3).
 */

#include "global.h"
#include "div_io.h"
#include <stdio.h>
#include <stdlib.h>

extern int reloj;   /* definido en port_ide_dos.c */

/* ------------------------------------------------------------------ */
/*  Traza opcional (DIV_IDE_KEYLOG=1)                                  */
/* ------------------------------------------------------------------ */

/* No hay depurador en la maquina de desarrollo: con DIV_IDE_KEYLOG=1 se
 * vuelca por stderr cada evento entregado, que es como se verifica que el
 * emparejado ascii/scancode y los modificadores son correctos. */
static int keylog(void)
{    static int on = -1;
    if (on < 0) {
        const char *e = getenv("DIV_IDE_KEYLOG");
        on = (e && *e && *e != '0');
    }
    return on;
}

/* ------------------------------------------------------------------ */
/*  Cola circular de eventos (formato del original)                    */
/* ------------------------------------------------------------------ */

#define BUF_SIZE (64 * 4)

byte buf[BUF_SIZE];  /* {ascii, scan_code, shift_lo, shift_hi} */
int ibuf = 0;        /* inicio de la cola */
int fbuf = 0;        /* fin de la cola */

/* PORT: lo ponia a 1 el handler de la IRQ al ver Ctrl+C. Se conserva porque
 * divkeybo lo exportaba, pero en Windows la consola no interrumpe al IDE. */
int ctrl_c = 0;

/* ------------------------------------------------------------------ */
/*  kbdFLAGS: tabla de teclas pulsadas                                 */
/* ------------------------------------------------------------------ */

/* PORT: el handler de la IRQ 9 activaba kbdFLAGS[scancode] con cada codigo
 * de pulsacion (incluidas las repeticiones automaticas) y lo desactivaba con
 * el de liberacion. Es importante respetar ese modelo de flancos y NO
 * limitarse a copiar el estado actual del teclado: el IDE "consume" teclas
 * poniendo el flag a 0 a mano (p. ej. divmouse.cpp:76), y una copia directa
 * lo resucitaria en el acto mientras la tecla siguiera hundida. */
static void refresca_kbdflags(void)
{
    unsigned char down[128], made[128];
    int n;

    io_keys_state(down, made);

    for (n = 0; n < 128; n++) {
        byte antes = kbdFLAGS[n];

        if (made[n])
            kbdFLAGS[n] = 1;   /* pulsacion o repeticion */
        else if (!down[n])
            kbdFLAGS[n] = 0;   /* liberacion */

        if (keylog() && kbdFLAGS[n] != antes)
            fprintf(stderr, "[flg] scan=0x%02X -> %d\n", n, kbdFLAGS[n]);
    }
}

/* ------------------------------------------------------------------ */
/*  Relleno de la cola                                                 */
/* ------------------------------------------------------------------ */

/* PORT: equivalente de tecla_bios(), que vaciaba el buffer de la BIOS con
 * INT 16h AH=0 hasta agotarlo. */
void tecla_bios(void)
{
    io_key_event_t ev;

    io_input_pump();

    while (io_key_event_pop(&ev)) {
        byte as = ev.ascii;
        byte sc = ev.scan;

        /* Codigo del original: en el teclado numerico los digitos comparten
         * scancode con las flechas y el bloque de edicion. Si ha llegado un
         * digito se anula el scancode y se limpian las flechas, para que no
         * se interprete como navegacion. */
        if (as >= '0' && as <= '9') {
            kbdFLAGS[0x4B] = 0;
            kbdFLAGS[0x4D] = 0;
            kbdFLAGS[0x48] = 0;
            kbdFLAGS[0x50] = 0;
            sc = 0;
        }

        buf[fbuf] = as;
        buf[fbuf + 1] = sc;
        buf[fbuf + 2] = (byte)(ev.shift & 0xff);
        buf[fbuf + 3] = (byte)(ev.shift >> 8);
        if ((fbuf += 4) >= BUF_SIZE)
            fbuf = 0;

        if (keylog())
            fprintf(stderr, "[key] ascii=%3d ('%c') scan=0x%02X shift=0x%04X reloj=%d\n",
                    as, (as >= 32 && as < 127) ? as : '.', sc, ev.shift, reloj);
    }
}

/* ------------------------------------------------------------------ */
/*  API publica de divkeybo                                            */
/* ------------------------------------------------------------------ */

void kbdInit(void)
{
    int n;

    for (n = 0; n < 128; n++)
        kbdFLAGS[n] = 0;
    ibuf = fbuf = 0;
    io_key_events_clear();
}

void kbdReset(void)
{
    io_key_events_clear();
}

/* Extrae el siguiente evento a (ascii, scan_code, shift_status). Si no hay
 * ninguno deja ascii/scan_code a 0 y refresca solo los modificadores, igual
 * que hacia el original con INT 16h AH=12h. */
void tecla(void)
{
    /* PORT: el `reloj` de 100 Hz lo incrementaba la IRQ 0. tecla() se llama
     * en cada vuelta del bucle principal, que es el sitio natural para
     * refrescarlo. */
    port_ide_tick();

    refresca_kbdflags();

    tecla_bios();

    if (ibuf != fbuf) {
        ascii = buf[ibuf];
        scan_code = buf[ibuf + 1];
        shift_status = (word)(buf[ibuf + 2] | (buf[ibuf + 3] << 8));
        if ((ibuf += 4) >= BUF_SIZE)
            ibuf = 0;
    } else {
        ascii = 0;
        scan_code = 0;
        shift_status = io_shift_status();
    }

    /* PORT: filtrar el LCTRL falso que inyecta Windows al pulsar ALT GR.
     * Sin esto no se pueden teclear [ ] { } @ # en un teclado espanol: el
     * editor descarta el caracter al ver SS_CTRL (divedit.cpp:781). */
    if ((shift_status & (SS_RIGHT_ALT | SS_LEFT_CTRL)) == (SS_RIGHT_ALT | SS_LEFT_CTRL))
        shift_status &= ~(SS_CTRL | SS_LEFT_CTRL);
}

void vacia_buffer(void)
{
    io_input_pump();
    io_key_events_clear();

    ascii = 0;
    scan_code = 0;
    shift_status = io_shift_status();

    ibuf = fbuf = 0;
}
