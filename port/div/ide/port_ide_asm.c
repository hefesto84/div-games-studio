/*
 * port_ide_asm.c -- equivalentes en C de las rutinas en ensamblador x86 de
 * src/a.asm que usa el nucleo UI del IDE (memcpyb_, call_, get_t_).
 *
 * src/a.asm es ASM 32-bit con convencion de registros de Watcom: no se puede
 * ensamblar ni enlazar en x64. Las tres rutinas son triviales de reescribir.
 *
 * Ver docs/architecture/12-port-progreso.md (hito E2.1).
 */

#include "global.h"

/*
 * call_: "JMP NEAR PTR EAX" -- salta a la direccion que viene en un int.
 *
 * Todo el sistema de ventanas y dialogos del IDE guarda sus manejadores como
 * `int` (`v.paint_handler=(int)mi_funcion;`) y los invoca con call(). En el
 * DOS 32-bit original int y puntero median lo mismo, asi que era gratis.
 *
 * En x64 un puntero son 64 bits: el cast a int trunca los 32 altos (306
 * avisos C4311 en el build). La solucion adoptada NO es tocar las ~300
 * llamadas de src/div, sino forzar que la imagen y el heap del proceso vivan
 * por debajo de 2 GB, de modo que la truncacion sea reversible:
 *
 *   /LARGEADDRESSAWARE:NO   -> el proceso no recibe direcciones sobre 2 GB
 *   /BASE:0x10000000        -> la imagen se carga en 0x10000000
 *   /DYNAMICBASE:NO /FIXED  -> sin ASLR, la base es la pedida
 *
 * (ver CMakeLists.txt, target div_ide_port). Con eso los 32 bits bajos son
 * la direccion completa y basta con extender a cero.
 */
void call(int n)
{
    void (*f)(void);

    if (!n)
        return;

    f = (void (*)(void))(uintptr_t)(unsigned int)n;
    f();
}

/*
 * memcpyb_: copia hacia adelante byte a byte (REP MOVSB). No es memcpy():
 * el IDE lo usa con regiones que se solapan y depende de la semantica exacta
 * de copia ascendente (por ejemplo al hacer scroll de una zona de pantalla).
 */
void memcpyb(byte *d, byte *s, int n)
{
    if (d == NULL || s == NULL || n <= 0)
        return;

    while (n--)
        *d++ = *s++;
}

/*
 * get_t_: leia el Time Stamp Counter con RDMSR/RDTSC via un salto far a codigo
 * de anillo 0. Sin equivalente ni utilidad aqui.
 */
int get_t(void)
{
    return 0;
}
