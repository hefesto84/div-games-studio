
//�����������������������������������������������������������������������������
//      Teclado
//�����������������������������������������������������������������������������

/*
 * PORT: reescrito por completo. El original instalaba un manejador de la
 * IRQ9 (interrupcion de teclado) manipulando directamente los vectores de
 * interrupcion de DOS (int386x 0x21, AH=0x25/0x35 -- "set/get interrupt
 * vector"), y ademas leia el buffer de teclado real via BIOS (int 16h)
 * como respaldo. Nada de eso existe en Windows: el teclado se lee via
 * port/io (io_poll()/io_key_down()), que ya usa la misma convencion de
 * scancodes BIOS/set-1 que las constantes _XXX de divkeybo.h -- por eso
 * kbdFLAGS[] se puede rellenar con un simple bucle, sin tabla de
 * traduccion.
 *
 * ascii/scan_code (macros sobre mem[], ver inter.h) si reproducen la
 * semantica del manejador de IRQ 9 original: scan_code es el scancode de la
 * ultima tecla pulsada MIENTRAS siga pulsada (0 al soltarla) y ascii su
 * caracter en CP850 (0 si no genera texto). Se alimentan de la cola de
 * eventos de port/io/io_keyboard.c -- la misma que usa el IDE --, que
 * ademas conserva las pulsaciones mas cortas que un frame, imposibles de
 * ver con un polling por estado. Sin esto, un juego como STEROID se queda
 * colgado para siempre en su "PRESIONE UNA TECLA PARA JUGAR", que espera a
 * que scan_code deje de ser 0.
 */

#include "inter.h"
#include "div_io.h"

int ctrl_c=0,alt_x=0;

void kbdInit(void) {
  /* PORT: instalaba el manejador de IRQ9; no aplica sin interrupciones
   * de hardware reales. */
}

void kbdReset(void) {
  /* PORT: desinstalaba el manejador de IRQ9; no aplica. */
}

//�����������������������������������������������������������������������������
//      Actualiza kbdFLAGS[]/ascii/scan_code/shift_status para este frame
//�����������������������������������������������������������������������������

/* PORT: tecla "viva" de la que cuelgan scan_code/ascii (ver cabecera). */
static int ultimo_scan=0;
static int ultimo_ascii=0;

void tecla(void) {
  int n,evento=0;
  unsigned char down[128];
  io_key_event_t ev;

  io_poll();

  for (n=0;n<128;n++) kbdFLAGS[n] = io_key_down(n) ? 1 : 0;

  shift_status = 0;
  if (io_key_down(_L_SHIFT) || io_key_down(_R_SHIFT)) shift_status |= 3; /* bits 0/1: shift, sin distinguir izq/der */
  if (io_key_down(_L_CTRL)  || io_key_down(_R_CTRL))  shift_status |= 4; /* bit 2: ctrl */
  if (io_key_down(_L_ALT)   || io_key_down(_R_ALT))   shift_status |= 8; /* bit 3: alt */

  /* PORT: la cola de eventos aporta las PULSACIONES (incluidas las mas
   * breves que un frame); el estado mantenido dice cuando volver a 0. */
  while (io_key_event_pop(&ev)) {
    if (ev.scan!=0 && ev.scan<128) {
      ultimo_scan=ev.scan; ultimo_ascii=ev.ascii; evento=1;
    }
  }

  io_keys_state(down,NULL);

  if (!evento && (ultimo_scan==0 || !down[ultimo_scan])) {
    ultimo_scan=0; ultimo_ascii=0;
    /* Si queda otra tecla pulsada, es la que pasa a estar "viva". */
    for (n=1;n<128;n++) if (down[n]) { ultimo_scan=n; break; }
  }

  scan_code=ultimo_scan;
  ascii=ultimo_ascii;

  if ((shift_status&8) && io_key_down(_X)) alt_x=1; else alt_x=0;
}

//�����������������������������������������������������������������������������
//      Vacia el buffer de teclado (real e interno)
//�����������������������������������������������������������������������������

void vacia_buffer(void) {
  io_key_events_clear();
  ultimo_scan=0; ultimo_ascii=0;
  ascii=0; scan_code=0; shift_status=0;
}
