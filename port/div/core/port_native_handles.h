#ifndef PORT_DIV_NATIVE_HANDLES_H
#define PORT_DIV_NATIVE_HANDLES_H
/*
 *  Tabla generica: puntero nativo (malloc) <-> handle pequeno que SI cabe
 *  en una celda de mem[] (el array de "palabras" de 32 bits de la VM de
 *  DIV -- ver 07-modelo-ejecucion.md). mem[] no se puede ensanchar a 64
 *  bits sin romper el formato de bytecode/proceso; por eso, a diferencia
 *  de tabfiles[] (tabla puramente nativa, ver inter.h), aqui no basta con
 *  ampliar un tipo -- hace falta esta indireccion.
 *
 *  Caso de uso real localizado (unico hasta ahora, ver
 *  docs/architecture/12-port-progreso.md): guarda_pila()/carga_pila()/
 *  actualiza_pila() en i.cpp guardaban un FILE*-like "int* p" (un
 *  fragmento de pila aparcado mientras un proceso esta a mitad de una
 *  llamada a funcion con FRAME de por medio) directamente en
 *  mem[id+_SP], truncado a 32 bits con "(int)p". En x64 eso es un bug de
 *  memoria real. Con esta tabla, mem[id+_SP] guarda un handle (indice
 *  pequeno), no el puntero.
 *
 *  Limite de PORT_MAX_NATIVE_HANDLES: acotado a la cantidad de procesos
 *  que pueden estar simultaneamente "a mitad de una llamada a funcion
 *  con FRAME dentro" -- 256 es un margen holgado para el uso tipico del
 *  lenguaje DIV; si se agota, falla de forma segura (mismo camino que
 *  "sin memoria" ya tenia el codigo original), no crashea.
 */

#define PORT_MAX_NATIVE_HANDLES 256

static void *port_native_handle_table[PORT_MAX_NATIVE_HANDLES];

/* Devuelve un handle > 0, o 0 si la tabla esta llena (igual que "malloc
 * devolvio NULL" en el codigo original: 0 es el valor que ya usaba
 * mem[id+_SP] para "no hay pila guardada"). */
static __inline int port_native_handle_alloc(void *ptr) {
    int i;
    for (i = 1; i < PORT_MAX_NATIVE_HANDLES; i++) {
        if (!port_native_handle_table[i]) {
            port_native_handle_table[i] = ptr;
            return i;
        }
    }
    return 0;
}

static __inline void *port_native_handle_get(int handle) {
    if (handle <= 0 || handle >= PORT_MAX_NATIVE_HANDLES)
        return NULL;
    return port_native_handle_table[handle];
}

static __inline void port_native_handle_free(int handle) {
    if (handle > 0 && handle < PORT_MAX_NATIVE_HANDLES)
        port_native_handle_table[handle] = NULL;
}

#endif /* PORT_DIV_NATIVE_HANDLES_H */
