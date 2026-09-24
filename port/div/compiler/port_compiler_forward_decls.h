#ifndef DIV_PORT_COMPILER_FORWARD_DECLS_H
#define DIV_PORT_COMPILER_FORWARD_DECLS_H

/* ==========================================================================
 *  Declaraciones adelantadas para el port del compilador (hito C1).
 *  Patron K&R que Watcom aceptaba (llamar a una funcion antes de definirla
 *  la declaraba implicitamente como "int") y MSVC no: al encontrar luego la
 *  definicion real (void ...) da C2371 "nueva definicion; tipos basicos
 *  distintos". Mismo criterio que port_forward_decls.h del nucleo: todas
 *  las declaraciones adelantadas aqui, no dispersas por el fuente.
 *
 *  Lista generada mecanicamente extrayendo TODAS las definiciones de
 *  funcion de divc.cpp (66) + las 5 de divlengu.cpp. Los prototipos
 *  duplicados con las declaraciones que el propio fuente ya tiene son
 *  inocuos en C (identicos).
 * ========================================================================== */

/* --- divc.cpp (en orden de aparicion) --- */
void comp(void);
void comp_exit(void);
void inicializa_compilador(void);
void mensaje_compilacion(unsigned char * p);
void compilar(void);
void free_resources(void);
void save_error(unsigned short tipo);        /* word == unsigned short (aun no definido aqui) */
void c_error(unsigned short tipo, unsigned short e);
void inicio_sentencia(void);
void final_sentencia(void);
void grabar_sentencia(void);
void test_buffer(int * * buffer,int * maximo,int n);
void precarga_obj (void);
void lexico(void);
void pasa_ptocoma(void);
struct objeto * analiza_pointer(int tipo, int offset);
int analiza_pointer_struct(int tipo, int offset, struct objeto * estructura);
int analiza_struct(int offstruct);
int analiza_struct_local(int offstruct);
int analiza_struct_private(int offstruct);
void CNT_export(char *name,void *dir,int nparms);
void CMP_export(char *name,void *dir,int nparms);
int ImportDll(char *name);
void UnimportDll();
void sintactico (void);
void analiza_private(void);
void tglo_init(int tipo);
void tloc_init(int tipo);
void tglo_init2(int tipo);
void sentencia();
void condicion(void);
void con00(int tipo_exp);
void con0();
void con1();
void con2();
void expresion(void);
void expresion_cpa(void);
void generar_expresion(void);
int constante (void);
void exp00(int tipo_exp);
void exp0();
void exp1();
void exp2();
void exp3();
void exp4();
void exp5();
void unario();
void exp6();
void factor(void);
void factor_struct(void);
void l_objetos (void);
void save_dbg(void);
void save_exec_bin(void);
void l_ensamblador (void);
void compilar1(void);
void compilar2(void);
void compilar0(void);
void compilar_programa(void);
void g1(int op);
void g2(int op, int pa);
void gen(int param, int op, int pa);
void remove_code(int i);
void delete_code(void);
void add_code(int dir, int param, int op);
void plexico(void);
void psintactico(void);

/* --- divlengu.cpp (unsigned char* == byte*; aqui no se puede usar "byte"
 * porque este force-include se procesa antes de que global.h defina el
 * typedef) --- */
void analiza_textos(void);
void an_numero(void);
void an_comentario(void);
void an_texto(void);
void coder(unsigned char * ptr, int len, char * clave);

#endif /* DIV_PORT_COMPILER_FORWARD_DECLS_H */
