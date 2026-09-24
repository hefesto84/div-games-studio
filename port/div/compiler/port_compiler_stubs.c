/* ==========================================================================
 *  port_compiler_stubs.c — Stubs del IDE para enlazar el compilador (C2)
 *
 *  divc.cpp referencia funciones del IDE (ventanas, dialogos, textos) que
 *  no existen en un driver CLI. Aqui se les da cuerpo no-op (o minimo
 *  funcional cuando el compilador los llama de verdad). Es el equivalente
 *  a port_stubs.c del runtime. Cualquier stub que empiece a usarse de
 *  verdad debe documentarse aqui.
 * ========================================================================== */

/* DEFINIR_AQUI hace que esta unidad DEFINA todas las variables GLOBAL_DATA
 * de global.h (texto[], ventana[], Setupfile, ejecutar_programa, ...). */
#define DEFINIR_AQUI
#include "global.h"

#include "divdll.h"

#include <string.h>

/* Buffer del mensaje de error (compilar2 lo imprime via mensaje_compilacion) */
char cerror[128];

/* --- Mensajeria de compilacion: compilar() los llama DE VERDAD ------------
 * (mensaje_compilacion, divc.cpp:948). En el IDE pintan una caja de
 * progreso; en CLI, wwrite imprime el texto (es el canal natural de
 * progreso del compilador: texto[200..205] = fases de la compilacion). --- */
void wbox(byte*copia,int an_copia,int al_copia,byte c,int x,int y,int an,int al) {}
void wwrite(byte*copia,int an_copia,int al_copia,int x,int y,int centro,byte * ptr,byte c) {
  if (ptr != NULL && *ptr != 0) { printf("[%s]\n", (char *)ptr); fflush(stdout); }
}
void vuelca_ventana(int n) {}
void volcado_copia(void) {}

/* --- Dialogo de compilar (compilar0/1/2, compilar_programa): solo los usa
 * el wrapper de dialogo del IDE, que el driver CLI no invoca nunca --- */
void _show_items(void) {}
void _process_items(void) {}
void _button(int texto,int x,int y,int centro) {}
void dialogo(int paint_handler) {}
void tecla(void) {}
int text_len(byte*ptr) { return ptr ? (int)strlen((char*)ptr) : 0; }

/* get_error(n): en el IDE vive en la ayuda; rellena cerror con texto[n].
 * Version funcional minima (el driver la usa para reportar errores). */
void get_error(int n) {
  if (n >= 0 && n < max_textos && texto[n] != NULL) {
    strncpy(cerror, (char *)texto[n], 127);
    cerror[127] = 0;
  } else cerror[0] = 0;
}

/* --- IsWAV: NO es un stub, es la implementacion real (divpcm.cpp:1940).
 * lexico() la usa para marcar recursos empaquetables (divc.cpp:1797). --- */
int IsWAV(char *FileName) {
  FILE *f;
  int ok = 1;
  if ((f = fopen(FileName, "rb")) == NULL) return 0;
  if (fgetc(f) != 'R') ok = 0;
  if (fgetc(f) != 'I') ok = 0;
  if (fgetc(f) != 'F') ok = 0;
  if (fgetc(f) != 'F') ok = 0;
  fseek(f, 4, SEEK_CUR);
  if (fgetc(f) != 'W') ok = 0;
  if (fgetc(f) != 'A') ok = 0;
  if (fgetc(f) != 'V') ok = 0;
  if (fgetc(f) != 'E') ok = 0;
  fclose(f);
  return ok;
}

/* --- Maquinaria DLL (IMPORT): fuera de alcance, igual que en el runtime
 * (DIV_LoadDll siempre falla limpio). DIV_ImportDll=NULL hace que
 * ImportDll() devuelva 0 y el compilador emita su error 63 si un fuente
 * usa IMPORT — comportamiento correcto y ruidoso, no un fallo mudo. --- */
COM_export_t COM_export;
PE* DIV_LoadDll(const char* name) { return NULL; }
PE* DIV_ImportDll(const char* name) { return NULL; }
void DIV_UnLoadDll(PE* pefile) {}
void DIV_UnImportDll(PE* pefile) {}
void DIV_export(const char* name, void* obj) {}
void* DIV_import(const char* name) { return NULL; }
void DIV_RemoveExport(const char* name, void* obj) {}
