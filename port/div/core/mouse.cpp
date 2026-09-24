
//�����������������������������������������������������������������������������
//      Rat�n
//�����������������������������������������������������������������������������

/*
 * PORT: reescrito (mismo criterio que v.cpp -- este fichero tambien toca
 * "hardware" real, el driver de raton de DOS via INT 33h simulado con
 * int386). Sustituido por port/io (io_get_mouse/io_mouse_button), que ya
 * lee el raton real de Windows a traves de raylib.
 *
 * check_mouse() fijaba mouse->cursor=1 cuando NO habia driver de raton
 * de DOS instalado, para activar mas abajo en readmouse() un control del
 * cursor por teclado (flechas). En Windows siempre hay un raton real
 * disponible, asi que se deja mouse->cursor=0 fijo -- esa rama de
 * control por teclado ya no aplica y se elimino junto con las variables
 * que solo usaba (vx/vy/vmax), no referenciadas desde ningun otro
 * fichero (confirmado por grep).
 *
 * CAMBIO DE COMPORTAMIENTO DELIBERADO: el original acumulaba movimiento
 * RELATIVO del raton (contador de motion de INT 33h AX=0xb) con un
 * divisor de "velocidad" configurable. io_get_mouse() da la posicion
 * ABSOLUTA ya en el espacio logico del framebuffer (ver div_io.h), que
 * es el equivalente moderno natural y evita tener que reinventar la
 * escala de velocidad; el resultado percibido por el jugador (el cursor seguido)
 * es equivalente o mejor.
 */

#include "inter.h"
#include "div_io.h"

float m_x=0.0,m_y=0.0;

void check_mouse(void) {
  mouse->cursor=0;
}

void set_mouse(int x,int y) {
  m_x=(float)x;
  m_y=(float)y;
}

void readmouse(void) {
  int mx,my,n=0;

  tecla();

  mouse->left   = io_mouse_button(0) ? 1 : 0;
  mouse->right  = io_mouse_button(1) ? 1 : 0;
  mouse->middle = io_mouse_button(2) ? 1 : 0;

  io_get_mouse(&mx,&my);
  m_x=(float)mx;
  m_y=(float)my;

  _mouse_x=(int)m_x;
  _mouse_y=(int)m_y;

  if (_mouse_x<0) { _mouse_x=0; n++; }
  else if (_mouse_x>=vga_an) { _mouse_x=vga_an-1; n++; }
  if (_mouse_y<0) { _mouse_y=0; n++; }
  else if (_mouse_y>=vga_al) { _mouse_y=vga_al-1; n++; }

  if (n) set_mouse(_mouse_x,_mouse_y);

  mouse->x=_mouse_x;
  mouse->y=_mouse_y;
}
