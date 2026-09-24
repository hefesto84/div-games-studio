/*
 * port_cierre.c -- cierre de la ventana del juego (boton X / ALT+F4).
 *
 * En DOS un programa DIV terminaba solo de tres maneras: acabandose todos
 * sus procesos, llamando a exit() del lenguaje, o con un error. No habia
 * "ventana" que cerrar, asi que el runtime original no tiene ningun camino
 * para "el usuario quiere salir YA".
 *
 * En el port si la hay, y sin esto un programa con un bucle infinito (lo
 * normal en un juego: nadie programa una salida hasta que hace el menu)
 * deja una ventana inmortal que hay que matar desde el administrador de
 * tareas -- y ademas cuelga al IDE, que espera al proceso hijo.
 *
 * El apagado replica el de _exit_dos() (f.cpp), la implementacion del
 * exit() del lenguaje DIV: se restaura el modo de video, se suelta el
 * teclado y se vuelve al directorio de arranque antes de salir. Se usa el
 * mismo codigo de salida 26 que la terminacion normal (i.cpp).
 *
 * OJO: esto es SOLO del runtime. El IDE tiene su propia ventana y su propio
 * dialogo de confirmacion ("Salir de DIV?"), asi que el chequeo se engancha
 * en volcado() (v.cpp, bridge de video del runtime) y no en la capa
 * port/io, que es compartida.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <direct.h>

extern char divpath[];

void rvmode(void);
void kbdReset(void);
bool io_window_should_close(void);

void port_check_cierre(void)
{
    if (!io_window_should_close()) return;

    rvmode();
    kbdReset();
    if (divpath[0]) _chdir(divpath);

    exit(26);
}
