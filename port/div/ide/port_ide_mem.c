/*
 * port_ide_mem.c -- asignador del IDE portado.
 *
 * Motivo (hito E2.1): varias estructuras del IDE se reservan con un tamano
 * calculado a ojo asumiendo punteros de 32 bits, y luego se rellenan con
 * nodos que contienen punteros. Ejemplo real: el coloreador de sintaxis
 * (src/div/divcolor.cpp) reserva `max_obj*long_med_id+1024` bytes para el
 * vector de nombres y va escribiendo nodos {siguiente:ptr, token:ptr, asciiz}
 * con `*icvnom.p++`. En x64 cada cabecera de nodo pasa de 8 a 16 bytes, asi
 * que el mismo fichero de entrada necesita bastante mas memoria de la que se
 * reservo. El original no tenia forma de detectarlo: escribia fuera del
 * bloque y seguia.
 *
 * En lugar de ir tocando cada calculo de tamano en src/div (y arriesgarse a
 * olvidar alguno), se sobredimensionan TODAS las reservas del IDE por un
 * factor fijo. La memoria es barata; el IDE completo pide unos pocos MB.
 *
 * Importante: no cambia el comportamiento del programa, solo el tamano del
 * bloque devuelto. Los punteros siguen viniendo del heap de la CRT, que con
 * /LARGEADDRESSAWARE:NO esta por debajo de 2 GB (ver CMakeLists.txt).
 */

#include "global.h"

/* Este fichero implementa los envoltorios: aqui hay que usar la CRT real. */
#undef malloc
#undef calloc
#undef realloc

/* Con punteros de 64 bits, cualquier estructura del IDE hecha de punteros
 * ocupa el doble. El margen extra absorbe ademas las cabeceras de nodo
 * mixtas (int + puntero, que ademas se alinean a 8). */
#define PORT_IDE_MEM_FACTOR 4
#define PORT_IDE_MEM_EXTRA  65536

static size_t port_ide_grow(size_t n)
{
    return n * PORT_IDE_MEM_FACTOR + PORT_IDE_MEM_EXTRA;
}

void *port_ide_malloc(size_t n)
{
    return malloc(port_ide_grow(n));
}

void *port_ide_calloc(size_t count, size_t size)
{
    size_t n = count * size;
    return calloc(1, port_ide_grow(n));
}

void *port_ide_realloc(void *p, size_t n)
{
    return realloc(p, port_ide_grow(n));
}
