/*
 * port_ide_setup.c -- validacion de system\setup.bin.
 *
 * Load_Cfgbin() lee la configuracion con un `fread` directo sobre la struct
 * SetupFile. El fichero que dejo el DIV 2 original tiene el layout del
 * compilador de DOS (entre otras cosas _MAX_PATH valia 144 en Watcom y vale
 * 260 en MSVC), asi que al leerlo en el port todos los campos quedan
 * desplazados: colors_rgb sale a cero (c0..c4 = negro, escritorio invisible),
 * la resolucion es arbitraria, etc.
 *
 * Solucion: si el tamano del fichero no coincide con sizeof(SetupFile) se
 * trata como inexistente. Se usan entonces los valores por defecto y
 * Save_Cfgbin() lo reescribe ya con el layout actual.
 *
 * Ver docs/architecture/12-port-progreso.md (hito E2.1).
 */

#include "global.h"
#include <stdio.h>

int port_setup_bin_valido(void)
{
    FILE *f = fopen("system\\setup.bin", "rb");
    long n;

    if (f == NULL)
        return 0;

    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);

    return n == (long)sizeof(Setupfile);
}
