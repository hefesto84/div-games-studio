/*
 * port_stubs.c -- implementaciones minimas o vacias de todo lo que
 * i.cpp/f.cpp/kernel.cpp referencian pero que NO hace falta para el
 * primer .exe de prueba de este port (sin Modo-8, sin DLLs de plugin,
 * sin CD/red/FLI). Existen SOLO para que el enlazador este contento --
 * i.cpp/f.cpp son ficheros monoliticos que referencian estos simbolos
 * sin importar si el programa DIV concreto que se ejecute los usa de
 * verdad o no (ver docs/architecture/12-port-progreso.md §9.3/§11.3
 * para el inventario completo y de donde sale cada uno).
 *
 * REGLA DE ESTE FICHERO: si una funcion esta protegida en el codigo
 * real por una condicion que en nuestro caso de prueba nunca se cumple
 * (p.ej. "solo se llama si el proceso es Modo-8"), el cuerpo puede ser
 * un no-op sin riesgo. Si make falta ADEMAS mantener la contabilidad de
 * pila del lenguaje DIV (funciones "void X(void)" que la VM invoca via
 * el dispatcher de f.cpp, sacando sus propios argumentos de pila[]),
 * se documenta explicitamente que el stub NO es correcto si un
 * programa las llega a invocar de verdad -- no se ha intentado adivinar
 * cuantos argumentos esperan, porque el codigo original de esos
 * builtins no esta en este repositorio (ver 11-port-windows11-mikedx.md
 * sobre los huecos de la restauracion original).
 */

#include "inter.h"
#include "dll.h"
#include "divmixer.hpp"
#include "cdrom.h"
#include "netlib.h"

/* ==========================================================================
 *  Carga de DLLs / plugins (Fase 4 del plan, no implementada todavia).
 *  DIV_export/DIV_import SI son una implementacion real (el pool de
 *  simbolos nombre->puntero que ya describimos en
 *  11-port-windows11-mikedx.md §4.5) -- es logica portable sin ninguna
 *  dependencia de hardware ni de un loader PE real, asi que no hay
 *  motivo para dejarla en stub. DIV_LoadDll simplemente nunca encuentra
 *  nada que cargar (no hay loader PE en este port todavia), asi que el
 *  pool en la practica siempre esta vacio salvo por lo que el propio
 *  runtime exporte a mano (ninguno todavia).
 * ========================================================================== */

#define PORT_MAX_EXPORTS 256

static struct { const char *name; void *obj; } port_export_pool[PORT_MAX_EXPORTS];
static int port_export_count = 0;

void DIV_export(const char *name, void *obj) {
  int i;
  for (i=0;i<port_export_count;i++)
    if (!strcmp(port_export_pool[i].name,name)) { port_export_pool[i].obj=obj; return; }
  if (port_export_count<PORT_MAX_EXPORTS) {
    port_export_pool[port_export_count].name=name;
    port_export_pool[port_export_count].obj=obj;
    port_export_count++;
  }
}

void DIV_RemoveExport(const char *name, void *obj) {
  int i;
  (void)obj;
  for (i=0;i<port_export_count;i++)
    if (!strcmp(port_export_pool[i].name,name)) {
      port_export_pool[i]=port_export_pool[port_export_count-1];
      port_export_count--;
      return;
    }
}

void *DIV_import(const char *name) {
  int i;
  for (i=0;i<port_export_count;i++)
    if (!strcmp(port_export_pool[i].name,name)) return port_export_pool[i].obj;
  return NULL; /* PORT: exactamente lo que hacia el original cuando nadie habia registrado ese nombre -- todos los ~18 ganchos opcionales de video (ver 12-port-progreso.md §8.2) ya toleran esto con su propio fallback nativo */
}

PE *pe[128];
int nDLL=0;
void *ExternDirs[1024];
COM_export_t COM_export=NULL;

PE *DIV_LoadDll(const char *name) {
  (void)name;
  return NULL; /* PORT: sin loader PE real todavia (Fase 4); toda carga de DLL falla limpiamente */
}
PE *DIV_ImportDll(const char *name) {
  (void)name;
  return NULL;
}
void DIV_UnLoadDll(PE *pefile) {
  (void)pefile;
}
void DIV_UnImportDll(PE *pefile) {
  (void)pefile;
}
void LookForAutoLoadDlls() {
  /* PORT: el original escaneaba el directorio del juego por *.DLL y las
   * cargaba con DIV_LoadDll. Como DIV_LoadDll siempre falla en este
   * port (sin loader PE, ver arriba), no tiene sentido ni escanear. */
}

/* Opcode `lext` (kernel.cpp): salta a una funcion exportada por una DLL ya
 * importada. En este port nunca hay una DLL importada de verdad (ver
 * arriba), asi que este opcode nunca deberia alcanzarse en la practica;
 * no-op en vez de un salto real a una direccion arbitraria. */
void call(unsigned int addr) {
  (void)addr;
}

/* ==========================================================================
 *  Motor Modo-8 (3D) -- portado de verdad, ver docs/architecture (hito
 *  "runtime Modo-8"). Los opcodes (load_wld/start_mode8/stop_mode8/
 *  loop_mode8/set_fog/set_sector_texture/get_sector_texture/
 *  set_wall_texture/get_wall_texture/set_env_color/set_point_m8/
 *  get_point_m8/go_to_flag/set_sector_height/get_sector_height) y el
 *  sistema de "objetos" (create_object/_object_data_input/
 *  _object_data_output/_object_destroy/_object_avance, llamados desde
 *  i.cpp SOLO cuando un proceso tiene _Ctype==3) viven ahora en
 *  port/vpe/vpedll.c (copiado de src/vpe/vpedll.cpp, que ya era su
 *  implementacion real).
 *
 *  NOTA: path_find/path_line/path_free NO estan aqui -- el port real
 *  (busqueda de caminos sobre el mapa de busqueda del scroll, builtins que
 *  TOKENKAI necesita) vive en ia.c.
 * ========================================================================== */

/* mismo patron, otro builtin del dispatcher no relacionado con Modo-8.
 * collision() y out_region() ya estan portadas de verdad en s.cpp
 * (ver 12-port-progreso.md); graphic_info sigue stub: */
void graphic_info(void) {}

/* PORT: fli_palette_update/StartFLI/Nextframe/EndFli/ResetFli ya no estan
 * aqui -- reales en divfli.cpp (copiado sin tocar a port/div/core/) desde
 * que se implemento un decodificador FLI/FLC nuevo (port_topflc.c, API
 * compatible con TopFLC v1.0, la libreria de terceros que divfli.cpp
 * espera y de la que solo teniamos vendored el header). */

/* ==========================================================================
 *  CD-audio -- src/div32run/cdrom.cpp no portado.
 * ========================================================================== */

void Init_CD() {}
void Play_CD(int pista,int modo) { (void)pista; (void)modo; }
void Stop_CD() {}
int  IsPaying_CD() { return 0; }
unsigned int get_cd_error(void) { return 0; }

/* ==========================================================================
 *  Red -- fuera de alcance por decision explicita (ver
 *  11-port-windows11-mikedx.md §0.2). inicializacion_red es una variable de
 *  estado que el resto del codigo consulta; en su valor inerte de siempre,
 *  el codigo que depende de ella simplemente no hace nada (comportamiento
 *  seguro, no un intento de red real). (find_status ya NO vive aqui: es el
 *  flag global del modulo ia.c de busqueda de caminos, que si lo usa.)
 * ========================================================================== */

int inicializacion_red=0;

void net_end(void) {}
void _net_loop(void) {}
int  net_join_game(void) { return -1; }
int  net_get_games(void) { return 0; }

/* ==========================================================================
 *  Mezclador / volumen -- divmixer.hpp, sin hardware real que controlar
 *  en este port (el volumen real lo gestiona port/io via
 *  io_change_sound/io_change_channel, ya conectado en divsound.cpp).
 * ========================================================================== */

void SetVocVolume(UWORD volumen) { (void)volumen; }
void SetCDVolume(UWORD volumen) { (void)volumen; }
void set_mixer(void) {}
void set_init_mixer(void) {}

/* ==========================================================================
 *  Multiplicaciones de punto fijo (s.cpp: pinta_modo7). Declaradas en el
 *  original con "#pragma aux" (ensamblador embebido especifico de
 *  Watcom, ignorado en silencio por MSVC -- ver warning C4068 "pragma
 *  aux desconocida" ya visto en varios checkpoints). Son IMPLEMENTACION
 *  REAL, no un stub: equivalen exactamente a "imul edx / shrd eax,edx,N"
 *  -- multiplicar dos enteros de 32 bits en un producto de 64 y quedarse
 *  con el resultado desplazado N bits (formato de punto fijo 8.24 y
 *  16.16 respectivamente).
 * ========================================================================== */

int mul_24(int a, int b) {
  return (int)(((long long)a * (long long)b) >> 24);
}

int mul_16(int a, int b) {
  return (int)(((long long)a * (long long)b) >> 16);
}

/* ==========================================================================
 *  Memoria DOS/DPMI -- i.cpp: GetFree4kBlocks() (diagnostico de memoria
 *  libre, no se llama desde ningun sitio del propio i.cpp -- codigo
 *  muerto en la practica, confirmado por grep). DOSalloc4k/DPMIalloc4k
 *  tambien estaban declaradas con "#pragma aux" en el original. Devolver
 *  0 ("no hay mas bloques libres") es una respuesta segura dado que la
 *  funcion que las usa nunca se invoca.
 * ========================================================================== */

int DOSalloc4k(void) { return 0; }
int DPMIalloc4k(void) { return 0; }

/* ==========================================================================
 *  _heapshrink -- funcion de heap especifica del CRT de Watcom (no
 *  existe en el CRT de MSVC). Solo la usa el builtin SYSTEM() del
 *  lenguaje DIV (f.cpp, para lanzar COMMAND.COM u otro programa externo)
 *  antes/despues de invocar system(). No-op: en un SO moderno con
 *  memoria virtual no hace falta "encoger" el heap manualmente para que
 *  un proceso hijo tenga memoria disponible.
 * ========================================================================== */

void _heapshrink(void) {}
