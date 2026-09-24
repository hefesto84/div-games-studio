/* PORT: shim para <mem.h> (Watcom/Borland DOS) -- solo declara memcpy/
 * memmove/memset/memcmp y similares, todas ya en <string.h> del C
 * estandar. divfli.cpp (port/div/core/) es el unico fichero portado que lo
 * incluye. */
#ifndef PORT_MEM_H
#define PORT_MEM_H
#include <string.h>
#endif
