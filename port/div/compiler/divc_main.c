/* ==========================================================================
 *  divc_main.c — Driver CLI del compilador DIV portado (hito C2)
 *
 *  Uso:  divc_port.exe <fichero.prg> [salida.div32]
 *
 *  Sustituye al flujo del IDE (div.cpp) para compilar sin editor:
 *    - carga el .prg en source_ptr/source_len (lo que el IDE tomaba del
 *      buffer del editor, div.cpp:1094),
 *    - carga los textos de idioma (system\LENGUAJE.DIV, para los mensajes
 *      de error del compilador),
 *    - llama a comp() (= compilar() protegido con setjmp; TANTO el exito
 *      como cualquier c_error salen por longjmp, divc.cpp:1141/1211),
 *    - distingue exito de error por numero_error (-1 = exito, patron del
 *      propio IDE en compilar2, divc.cpp:7319),
 *    - renombra la salida fija system\EXEC.EXE al nombre pedido.
 *
 *  Hay que ejecutarlo con cwd = raiz del repo (o cualquier dir con una
 *  carpeta system\ que contenga ltlex.def, ltobj.def y LENGUAJE.DIV),
 *  igual que el IDE original: todas las rutas del compilador son relativas
 *  (system\ltlex.def, system\EXEC.EXE, ...).
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"   /* solo declaraciones; las GLOBAL_DATA viven en port_compiler_stubs.c */

/* Tabla lower[256] byte-exacta (extraida de src/div/div.cpp:48 con un script;
 * clasifica que caracteres (incl. latin-1 >=0x80) son validos en
 * identificadores, y el compilador la remapea en _case_sensitive). */
byte lower[256]={
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x23,0x24,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x20,0x20,0x20,0x20,0x5f,
  0x20,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x20,0x20,0x20,0x20,0x20,
  0x87,0x75,0x65,0x61,0x61,0x61,0x61,0x87,0x65,0x65,0x65,0x69,0x69,0x69,0x61,0x61,
  0x65,0x91,0x91,0x6f,0x6f,0x6f,0x75,0x75,0x79,0x6f,0x75,0x9b,0x9c,0x9d,0x9e,0x9f,
  0x61,0x69,0x6f,0x75,0xa4,0xa4,0xa6,0xa7,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20
};

/* Definidas en divc.cpp */
extern int numero_error;
extern int linea_error;
extern int columna_error;
void comp(void);
void inicializa_compilador(void);
void free_resources(void);

/* Definida en divlengu.cpp */
void inicializa_textos(byte * fichero);

/* Definida en port_compiler_stubs.c */
extern char cerror[128];
void get_error(int n);

static int copia_fichero(const char * origen, const char * destino) {
  FILE * i = fopen(origen, "rb");
  FILE * o;
  byte tmp[8192];
  size_t n;
  if (i == NULL) return -1;
  o = fopen(destino, "wb");
  if (o == NULL) { fclose(i); return -1; }
  while ((n = fread(tmp, 1, sizeof(tmp), i)) > 0) fwrite(tmp, 1, n, o);
  fclose(i); fclose(o);
  return 0;
}

int main(int argc, char ** argv) {
  FILE * f;
  long len;
  byte * buf;
  char salida[PATH_MAX+1];
  char * base, * ext;

  if (argc < 2) {
    printf("divc_port — compilador DIV Games Studio 2 (port Windows)\n"
           "Uso: divc_port <fichero.prg> [salida.div32]\n"
           "Ejecutar con cwd = raiz del repo (necesita system\\ltlex.def, "
           "ltobj.def y LENGUAJE.DIV).\n");
    return 2;
  }

  /* 1. Cargar el fuente (binario, tal cual; el lexer entiende CR/LF DOS) */
  if ((f = fopen(argv[1], "rb")) == NULL) {
    printf("ERROR: no se puede abrir %s\n", argv[1]);
    return 1;
  }
  fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
  if ((buf = (byte *)malloc(len + 2)) == NULL) { fclose(f); return 1; }
  if (fread(buf, 1, len, f) != (size_t)len) { fclose(f); free(buf); return 1; }
  fclose(f);
  /* compilar() escribe dos CR tras el final (divc.cpp:1022): reservados arriba */
  source_ptr = buf;
  source_len = (int)len;

  /* 2. Textos de idioma (para mensajes de error del compilador) */
  inicializa_textos((byte *)"system\\LENGUAJE.DIV");
  if (texto[200] == NULL) {
    printf("ERROR: no se ha podido cargar system\\LENGUAJE.DIV "
           "(ejecuta divc_port desde la raiz del repo)\n");
    free(buf);
    return 1;
  }

  /* 3. Compilar (comp() siempre vuelve por longjmp; exito = numero_error<0) */
  inicializa_compilador();
  ejecutar_programa = 0; /* 0 = solo compilar (global.h:908) */
  numero_error = -1;     /* patron del IDE (compilar2, divc.cpp:7319) */
  big = 0; big2 = 1;     /* PORT: el IDE fija big2 al arrancar (global.h:434);
                          * sin esto mensaje_compilacion divide v.an/big2=0
                          * (STATUS_INTEGER_DIVIDE_BY_ZERO, 0xC0000094) */
  comp();

  if (numero_error >= 0) {
    get_error(500 + numero_error); /* rellena cerror con texto[500+n] */
    printf("Error %d", numero_error);
    if (numero_error >= 10) printf(" (linea %d, columna %d)", linea_error, columna_error);
    if (cerror[0]) printf(": %s", cerror);
    printf("\n");
    free_resources();
    free(buf);
    return 1;
  }

  /* 4. Exito: renombrar la salida fija system\EXEC.EXE */
  if (argc >= 3) {
    strncpy(salida, argv[2], PATH_MAX); salida[PATH_MAX] = 0;
  } else {
    /* <base del .prg>.div32 en el cwd */
    base = argv[1];
    { char * s = strrchr(base, '\\'); if (s != NULL) base = s + 1; }
    { char * s = strrchr(base, '/');  if (s != NULL) base = s + 1; }
    strncpy(salida, base, PATH_MAX - 8); salida[PATH_MAX - 8] = 0;
    ext = strrchr(salida, '.');
    if (ext == NULL) ext = salida + strlen(salida);
    strcpy(ext, ".div32");
  }
  if (copia_fichero("system\\EXEC.EXE", salida) != 0) {
    printf("ERROR: no se puede escribir %s\n", salida);
    free_resources();
    free(buf);
    return 1;
  }

  { FILE * r = fopen(salida, "rb");
    long t = 0;
    if (r != NULL) { fseek(r, 0, SEEK_END); t = ftell(r); fclose(r); }
    printf("OK: %s compilado -> %s (%ld bytes)\n", argv[1], salida, t); }

  free_resources();
  free(buf);
  return 0;
}
