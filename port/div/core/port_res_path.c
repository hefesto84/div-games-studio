/*
 * port_res_path.c -- Hito E5: raices adicionales de busqueda de recursos.
 *
 * open_file() (f.cpp) resuelve los recursos de un juego DIV (fpg\, pcm\,
 * fnt\, map\...) SIEMPRE relativos al directorio de trabajo: es el mecanismo
 * original de DIV, no un bug del port (ver docs/architecture/13-handoff.md
 * SS5.5). Un juego instalado cumple eso porque se ejecuta desde la raiz de
 * la instalacion de DIV.
 *
 * Al "Probar" desde el IDE portado hay dos raices plausibles y ninguna es
 * suficiente por si sola:
 *
 *   - la raiz de DIV (donde viven fpg\, fnt\, ... comunes del entorno), y
 *   - la carpeta del propio .div32 (proyectos del usuario con sus recursos
 *     al lado).
 *
 * El IDE deja la primera como cwd (como hacia DIV.BAT) y pasa la segunda en
 * la variable de entorno DIV_RES_PATH, una lista de directorios separados
 * por ';'. Cuando la cadena de busqueda normal agota sus opciones, f.cpp
 * llama aqui y se reintenta la misma cadena con el cwd puesto en cada raiz.
 *
 * Se reutiliza open_file() en vez de duplicar su logica para que no puedan
 * divergir; el reentry se corta con un flag, asi que la busqueda extra nunca
 * se anida.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#define PORT_RES_MAXPATH 1024

FILE *open_file(unsigned char *file);
FILE *port_open_file_extra(unsigned char *file);

static int port_res_busy = 0;

FILE *port_open_file_extra(unsigned char *file)
{
    const char *env;
    char lista[PORT_RES_MAXPATH * 4];
    char cwd[PORT_RES_MAXPATH];
    char *raiz;
    char *sig;
    FILE *f = NULL;

    if (port_res_busy) return NULL;

    env = getenv("DIV_RES_PATH");
    if (env == NULL || env[0] == 0) return NULL;
    if (strlen(env) >= sizeof lista) return NULL;

    if (_getcwd(cwd, (int)sizeof cwd) == NULL) return NULL;

    strcpy(lista, env);

    port_res_busy = 1;
    for (raiz = lista; raiz != NULL && *raiz; raiz = sig) {
        sig = strchr(raiz, ';');
        if (sig != NULL) *sig++ = 0;
        if (*raiz == 0) continue;
        if (_chdir(raiz) != 0) continue;
        f = open_file(file);
        if (f != NULL) break;
    }
    port_res_busy = 0;

    _chdir(cwd);
    return f;
}
