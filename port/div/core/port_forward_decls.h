#ifndef PORT_DIV_FORWARD_DECLS_H
#define PORT_DIV_FORWARD_DECLS_H
/*
 *  Declaraciones adelantadas de funciones de i.cpp/f.cpp/kernel.cpp que el
 *  C original de Watcom podia usar antes de definir (declaracion implicita
 *  de K&R C), pero que MSVC en modo C++ estricto rechaza con "identifier
 *  not found". No son cambios de comportamiento: son la MISMA firma que ya
 *  tiene la funcion real mas adelante en el propio fichero, solo que
 *  visible antes. Cada entrada anota donde esta definida y donde se usa
 *  antes de esa definicion. Ver docs/architecture/12-port-progreso.md.
 *
 *  Incluido desde port_pre.h (fuerza-incluido antes de i.cpp/f.cpp), asi
 *  que estas declaraciones existen ya para cuando el propio fichero llega
 *  al punto de uso.
 */

/* OJO: se compila el nucleo como C (/TC, ver docs/architecture/12-port-
 * progreso.md §3), asi que estas declaraciones tienen enlace C llano;
 * no hace falta envolverlas en extern "C". */

#include <stdio.h> /* PORT (E5): FILE, para port_open_file_extra() */

/* Definida en i.cpp:1408; usada en i.cpp:314 (dentro de inicializacion(),
 * bajo #ifndef DEBUG) antes de su definicion. */
void busca_packfile(void);

/* El resto son del mismo patron, todas en f.cpp: la funcion se llama
 * (dentro de otra funcion definida antes en el fichero) antes de su
 * propia definicion mas abajo en el mismo f.cpp. Watcom en modo C
 * asumia "int" de retorno para la llamada implicita y no se quejaba al
 * ver luego la definicion real; MSVC si (error C2371, "tipos basicos
 * distintos" entre la declaracion implicita y la definicion real). */
/* NOTA: se usa "unsigned char" en vez de "byte" a proposito -- este
 * header se incluye (via port_pre.h, forzado con /FI) ANTES de que
 * inter.h defina "#define byte unsigned char", asi que "byte" todavia
 * no existe aqui. */
void  load_pal(void);                                            /* f.cpp:275, usada en f.cpp:270 */
void  adaptar(unsigned char *ptr, int len, unsigned char *pal, unsigned char *xlat); /* f.cpp:1041, usada en f.cpp:536 */
void  put_screen(void);                                           /* f.cpp:1425, usada en f.cpp:1415 */
void  texn2(unsigned char *copia, int vga_an, unsigned char *p, int x, int y, unsigned char an, int al); /* f.cpp:3593, usada en f.cpp:3586 */
void  expres0(void);                                              /* f.cpp:3692, usada en f.cpp:3636 */
void  expres1(void);                                              /* f.cpp:3701, usada en f.cpp:3694 */
void  expres2(void);                                              /* f.cpp:3710, usada en f.cpp:3703 */
void  expres3(void);                                              /* f.cpp:3719, usada en f.cpp:3712 */
void  expres4(void);                                              /* f.cpp:3728, usada en f.cpp:3721 */
void  expres5(void);                                              /* f.cpp:3740, usada en f.cpp:3737 */
void  get_token(void);                                            /* f.cpp:3761, usada en f.cpp:3635 */
void  _encriptar(int encode, char *fichero, char *clave);         /* f.cpp:3994, usada en f.cpp:3987 */
void  _comprimir(int encode, char *fichero);                      /* f.cpp:4097, usada en f.cpp:4090 */

/* divlengu.cpp: mismo patron. */
void  analiza_textos(void);                                       /* divlengu.cpp:68 */
void  an_numero(void);                                            /* divlengu.cpp:84 */
void  an_comentario(void);                                        /* divlengu.cpp:92 */
void  an_texto(void);                                             /* divlengu.cpp:97 */
void  coder(unsigned char *ptr, int len, char *clave);             /* divlengu.cpp:159 */

/* s.cpp: mismo patron. */
void  caja(int x, int y, int an, int al);                          /* s.cpp:1421 */
void  caja_rellena(int x, int y, int an, int al);                  /* s.cpp:1429 */
void  circulo(int relleno, int x0, int y0, int x1, int y1);        /* s.cpp:1437 */
void  line(int x0, int y0, int x1, int y1);                        /* s.cpp:1492 */
void  line_pixel(int x, int y);                                    /* s.cpp:1548 */

/* v.cpp: mismo patron. */
void  snapshot(unsigned char *p);                                  /* v.cpp */
void  crear_ghost_vc(int m);                                       /* v.cpp */
void  crear_ghost_slow(void);                                      /* v.cpp */

/* PORT (E5): busqueda de recursos en raices adicionales (DIV_RES_PATH).
 * La define port_res_path.c y la llama open_file() (f.cpp) cuando agota su
 * cadena de busqueda relativa al cwd. */
FILE *port_open_file_extra(unsigned char *file);

/* PORT: cierre de la ventana del juego (boton X / ALT+F4). La define
 * port_cierre.c y la llama volcado() (v.cpp) una vez por frame. */
void port_check_cierre(void);

#endif /* PORT_DIV_FORWARD_DECLS_H */
