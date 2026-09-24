// port_ide_stubs.c: stubs para funciones de modulos no incluidos en E1
#include "global.h"
#include <stdio.h>
#include "judas/judas.h"
#include "ifs.h"

FILE *stdprn = NULL;

// Stubs para simbolos de datos/funciones de editores de recursos no incluidos en E1
void *mixer0 = NULL;
void *pcminfo_aux = NULL;
/* PORT: el real es "unsigned char *aligned[2] = {NULL,NULL};" (divsb.cpp,
 * buffers DMA alineados de grabacion, no compilado) -- divhandl.cpp lee
 * "aligned[0]!=NULL && aligned[1]!=NULL" (menu Sonido -> Grabar) antes de
 * abrir el dialogo de grabacion. Con un void* de 8 bytes aqui, aligned[1]
 * leia 8 bytes mas alla del propio stub (mismo patron de tamano que
 * thumb_map/nueva_paleta, aunque aqui es solo una LECTURA fuera de
 * limites, no una escritura). Con el array real de 2 punteros ambos
 * quedan NULL de verdad, así que ese "if" nunca es cierto -- la grabacion
 * ya esta bloqueada aguas arriba (requiere DEV_SBPRO/DEV_SB16, y este port
 * fija judascfg_device a DEV_SB, ver mas abajo), esto es solo para que la
 * lectura en si sea segura si algun camino nuevo llega a mirarlo. */
unsigned char *aligned[2] = { NULL, NULL };
void *m3d = NULL;
/* PORT: nueva_paleta ya tiene definicion real (byte[768], no extern) en
 * divpalet.cpp -- el mismo patron de fusion "common symbol" que thumb_map
 * (ver comentario mas abajo): con un void* de 8 bytes aqui, ese habria
 * ganado el enlazado y los escritos de hasta 768 bytes en divpalet.cpp
 * habrian desbordado sobre memoria vecina. Sin stub aqui, gana la
 * definicion real. */
void *muestra = NULL;

/* PORT: M3D_crear_thumbs ya no esta aqui: real en divmap3d.cpp desde que
 * entro al build (Modo-8). Antes era no-op por el mismo motivo que
 * MapperCreator2/map_save/etc. mas abajo (el codigo real LLAMA a estas
 * funciones, no solo las guarda como puntero -- ver §43-44 de
 * 12-port-progreso.md para el patron completo). */

void CDiv0(void) {
}

void Clock0(void) {
}

/* PORT: GenExplodes ya no es no-op -- real en diveffec.cpp desde que entro
 * al build (E10). MapperVisor0 ya no esta aqui: real en divmap3d.cpp. */

void MapperWarning0(void) {
}

void PRJ0() {
}


void RenderToMed() {
}



int an_setup(void) {
    return 0;
}

void comp(void) {
}

void comp_exit(void) {
}

void compilar(void) {
}


void crear_instalacion(void) {
}


/* PORT (E6a): BMP/MAP/PCX ya son reales (divforma.cpp); JPG sigue stub. */
int descomprime_JPG(unsigned char*buffer, unsigned char*mapa, int vent, int img_filesize) {
    return 0;
}

















void error0(void) {
}

int es_JPG(unsigned char*buffer, int img_filesize) {
    return 0;
}


void finaliza_compilador(void) {
}





void free_resources(void) {
}

/* PORT: generador_sprites ya no es no-op -- real en divspr.cpp (GENSPR). */

/* PORT: help/help0/load_index/make_helpidx ya no son no-ops -- reales en
 * divhelp.cpp desde que entro al build (E8). */

/* PORT (E2.1): NO es un stub. Es el port literal de divc.cpp:939. El lexico
 * del coloreador (divcolor.cpp) recorre identificadores con
 * `while (*icvnom.b = lower[*buf++])`, asi que necesita que los huecos de
 * `lower[]` (inicializada con espacios en div.cpp:48) valgan 0. Sin esto el
 * bucle no termina nunca y se sale del buffer. Se elimina de aqui cuando
 * divc.cpp entre en el target (hito E3). */
void inicializa_compilador(void) {
  int n;
  for (n = 0; n < 256; n++) if (lower[n] == ' ') lower[n] = 0;
}


/* PORT: invierte_hor/invierte_ver ya no son no-ops -- reales en divsprit.cpp
 * (GENSPR). */



void mouse_off(void) {
}

void mouse_on(void) {
}

void mouse_window(void) {
}


void muestra_cd_player() {
}

void muestra_reloj() {
}

void nuevo_mapa3D(void) {
}







void set_mickeys(unsigned short _p0) {
}


/* PORT: sp_normal/sp_rotado/sp_size ya no son no-ops -- reales en
 * divsprit.cpp (GENSPR). */

/* PORT: tabula_help ya no es no-op -- real en divhelp.cpp desde que entro
 * al build (E8). */

// Auto-stubs from link errors
void *CDPlaying = NULL;
void CDiv1(void) {}
void Clock1(void) {}
/* PORT: MapperCreator2 ya no esta aqui: real en divmap3d.cpp desde que
 * entro al build (Modo-8). Mismo patron "dato en vez de funcion" que
 * M3D_crear_thumbs (ver arriba) -- el codigo real la llama de verdad. */

/* PORT (audio del IDE): OpenSound/OpenSong/OpenSoundFile/PasteNewSounds/
 * SaveSound/OpenDesktopSound/SaveDesktopSound/OpenDesktopSong ya no son
 * no-ops -- desde que divpcm.cpp entro al build tienen su implementacion
 * real ahi (llaman al shim de port_ide_judas.c). */

char SongName[14];
char SongPathName[256];
void *TipoTex = NULL;
void *a_back = NULL;
void *back = NULL;
/* PORT: backto[64] (divhelp.cpp, cola circular de topicos consultados) es
 * un array real de 64 int (256 bytes) -- mismo patron que thumb_map/
 * nueva_paleta/helpidx (ver mas abajo): estaba stubeado como un void* de
 * 8 bytes, que habria ganado el enlazado (fusion "common symbol") sobre el
 * array real de divhelp.cpp. Sin stub aqui, gana la definicion real. */
/* PORT: barra_vertical/vuelca_help ya no son no-ops -- reales en
 * divhelp.cpp desde que entro al build (E8). calc0/calc2/calculadora/
 * superget ya no estan aqui -- reales en divcalc.cpp desde que entro al
 * build (E9). */
/* PORT (E6a): cargadac_BMP/MAP/PCX, cargar_paleta y cargar_thumbs ya son
 * reales (divforma.cpp/divfpg.cpp). JPG sigue stub -- pero, mismo patron que
 * el resto de esta familia de bugs: el codigo real LO LLAMA de verdad
 * (divpalet.cpp:555,599, "try|=cargadac_JPG(PalName);", tambien
 * divhandl.cpp:2687), asi que no puede ser un void* de datos. La version
 * real (divforma.cpp, excluida bajo #ifndef PORT_IDE por el choque de
 * jpeglib con windows.h, ver 13-handoff.md) devuelve 0 si el fichero no es
 * un .JP* o si falla la decodificacion -- error controlado, no crash. Aqui
 * se replica solo esa parte (siempre "no es JPG / no soportado"), sin
 * decodificar nada. */
int cargadac_JPG(char *name) { (void)name; return 0; }
void *f_back = NULL;
void *h_buffer = NULL;
/* PORT: help2/help_buffer/help_title/helpidx/index ya no estan aqui --
 * reales en divhelp.cpp desde que entro al build (E8). help_buffer/
 * help_title/helpidx/index eran el mismo patron de array/buffer real
 * tapado por un void* de 8 bytes (helpidx[4096]=16 KB, help_title[128]=128
 * bytes -- ver backto arriba para el detalle del mecanismo). */
void *help_al = NULL;
void *help_an = NULL;
void *help_item = NULL;
void *help_l = NULL;
void *help_line = NULL;
void *help_lines = NULL;
void *i_back = NULL;
void *index_end = NULL;
/* PORT: judas_stopsample ya no es no-op -- real en port_ide_judas.c. */
/* PORT (audio del IDE, E7): antes fijo a DEV_NOSOUND (E3) -- eso bloqueaba
 * "Sonido -> Abrir"/"Abrir cancion"/preescuchar en el browser de verdad
 * (divhandl.cpp/divbrow.cpp/divpcm.cpp comprueban
 * "judascfg_device==DEV_NOSOUND" antes de llamar a OpenSound()/OpenSong(),
 * que ya son reales desde que divpcm.cpp entro al build). DEV_SB (no
 * DEV_SBPRO/DEV_SB16): suficiente para desbloquear reproduccion, pero el
 * menu "Sonido -> Grabar" exige explicitamente DEV_SBPRO/DEV_SB16
 * (divhandl.cpp:1190-1191), asi que con DEV_SB la grabacion muestra un
 * dialogo de "no soportado" en vez de intentar grabar con hardware que no
 * existe -- no hace falta emular nada mas para eso, ver `aligned` arriba. */
unsigned judascfg_device = DEV_SB;

/* PORT: mismo patron que thumb_map/thumb_tex (ver comentario mas abajo):
 * ltexturasbr/m3d_edit no son punteros sueltos, son la instancia real de
 * struct t_listboxbr / M3D_info que mapa2() y crear_mapbr_thumbs()
 * (divhandl.cpp/divpaint.cpp) rellenan campo a campo -- incluido un
 * strcpy(m3d_edit.fpg_path, ...) de hasta 256 bytes. Con un void* de 8
 * bytes aqui, eso desbordaba sobre lo que el enlazador hubiera colocado a
 * continuacion (en este caso, los buffers estaticos de teclado de
 * port_ide_keybo.c: crash 0xC0000005 en tecla() nada mas entrar en
 * "Mapas -> Editar mapa"). Los tipos ya estan en global.h, no hace falta
 * redeclararlos. */
struct t_listboxbr ltexturasbr;
M3D_info m3d_edit;

/* PORT: map_save/map_read/map_saveedit/map_readedit/nuevo_mapa3d ya no
 * estan aqui: reales en divmap3d.cpp desde que entro al build (Modo-8).
 * (mostrar_mod_meters/vuelca_help ya no estan aqui tampoco: reales en
 * divpcm.cpp/divhelp.cpp desde que entraron al build.) */

void *old_prg = NULL;
void *pcalc = NULL;
void *readcalc = NULL;
void *scroll_x = NULL;
void *scroll_y = NULL;
/* PORT: era codigo muerto mientras judascfg_device se fijaba a DEV_NOSOUND
 * (div.cpp:2971, "if(judascfg_device!=DEV_NOSOUND) set_init_mixer();") --
 * dejo de serlo al cambiar a DEV_SB para el audio del IDE (ver
 * judascfg_device mas arriba), y un void* de datos ahi crasheaba con
 * 0xC0000005 nada mas arrancar el IDE (mismo patron de siempre: la
 * implementacion real vive en divmixer.cpp, que sigue sin compilarse --
 * acceso a hardware real de tarjeta de sonido, ver
 * docs/architecture/13-handoff.md). No-op real. */
void set_init_mixer(void) { }
void *zoom_level = NULL;

/* PORT: thumb_map/thumb_tex tampoco pueden ser un simple void* como los de
 * arriba, aunque en E3 (antes de que divpaint.cpp entrara al build en E6a)
 * lo parecian: mapa2() (divhandl.cpp) y crear_mapbr_thumbs()/select_color()
 * (divpaint.cpp) los tratan como arrays de structs de verdad --
 * "for(n=0;n<max_texturas;n++) thumb_tex[n].ptr=NULL;" en mapa2(), por
 * ejemplo -- e iteran hasta max_windows/max_texturas escribiendo campo a
 * campo. divpaint.cpp SI define thumb_map[max_windows] como el array real,
 * pero MSVC/link.exe fusiona esa definicion tentativa (sin inicializador)
 * con esta, mas pequena y con inicializador explicito (regla de "common
 * symbol" de C): con thumb_map/thumb_tex reducidos aqui a un puntero de 8
 * bytes, esas escrituras (hasta ~3.8 KB para thumb_map, ~40 KB para
 * thumb_tex) desbordaban sobre lo que sea que el enlazador hubiera colocado
 * a continuacion en el segmento de datos -- incluida, en la practica, la
 * textura de framebuffer de port/io/io_video.c ("TEXTURE: Failed to update
 * for current texture format (0)" nada mas entrar en el editor de mapas) --
 * y terminaban en el crash STATUS_HEAP_CORRUPTION (0xC0000374) al salir con
 * ESC. thumb_tex no tiene ninguna definicion real en el build (vivia en
 * divmap3d.cpp, que sigue sin compilarse: Modo-8 es stub) asi que se define
 * aqui con el layout exacto de "struct _thumb_tex" (divpaint.cpp/
 * divhandl.cpp); thumb_map ya no se stubea, para que la definicion real de
 * divpaint.cpp sea la unica. */
struct port_thumb_tex_layout {
  int an, al;
  int RealAn, RealAl;
  char *ptr;
  int status;
  int FilePos;
  int Code;
  int Cuad;
};
struct port_thumb_tex_layout thumb_tex[1000]; /* max_texturas, divpaint.cpp */

/* ------------------------------------------------------------------------
 *  E2.1: simbolos que arrastran divpaint.cpp y divpalet.cpp y que viven en
 *  modulos aun no portados (divbrow, divfont, divfpg, divhelp, divmap3d).
 *  Solo se usan en rutas de editores de recursos (hito E4).
 * ---------------------------------------------------------------------- */

int FPG_thumbpos = 0;
char m3d_fpgcodesbr[1000 * 4];  /* max_texturas * an_textura (divpaint.cpp) */
struct t_listboxbr copia_br;

/* PORT: exp_Color0/exp_Color1/exp_Color2 ya no estan aqui -- reales en
 * diveffec.cpp desde que entro al build (E10); divpaint.cpp los referencia
 * como extern. */

/* PORT: t64 ya no es no-op -- real en divspr.cpp (GENSPR). */

/* PORT: determina_help/help_paint ya no son no-ops -- reales en
 * divhelp.cpp desde que entro al build (E8). MapperBrowseFPG0 ya no esta
 * aqui: real en divmap3d.cpp desde que entro al build (Modo-8). */

void GetCharSizeBuffer(int c, int *an, int *al, char *buf) {
  (void)c; (void)buf; if (an) *an = 0; if (al) *al = 0;
}
int ShowCharBuffer(int c, int cx, int cy, char *p, int an, char *buf) {
  (void)c; (void)cx; (void)cy; (void)p; (void)an; (void)buf; return 0;
}

/* PORT (E6a): cargadac_FNT/FPG/PAL ya son reales (divforma.cpp). */

/* ------------------------------------------------------------------------
 *  E3 (divbrow.cpp + divfont.cpp): stubs de funciones/datos que viven en
 *  modulos aun no portados (divpcm, divsetup, ifs.cpp) pero que los modulos
 *  nuevos referencian. El audio JUDAS se mantiene NOSOUND (judascfg_device
 *  arriba): los browsers desactivan la "prueba" y los thumbnails de PCM usan
 *  la ruta de error controlada.
 * ---------------------------------------------------------------------- */

/* PORT: SongType/SongCode/last_mod_clean/FreeMOD/IsWAV ya no estan aqui --
 * reales en divpcm.cpp desde que entro al build. judas_error/judas_channel/
 * judas_freesample/judas_loadrawsample/judas_loadwav/judas_playsample/
 * judas_loadxm/judas_playxm/judas_loadmod/judas_playmod/judas_loads3m/
 * judas_plays3m: reales en port_ide_judas.c (shim de audio del IDE sobre
 * port/io/io_audio.c + io_song.c, ver comentario de cabecera de ese
 * fichero). */

/* ifs.cpp: generacion de fuentes FNT para el editor de fuentes/texto */
char *bodyTexBuffer = NULL, *outTexBuffer = NULL, *shadowTexBuffer = NULL;
IFS ifs;
int Jorge_Crea_el_font(int GenCode) { (void)GenCode; return 0; }
int ShowChar(int WhatChar, int cx, int cy, char *ptr, int an) {
  (void)WhatChar; (void)cx; (void)cy; (void)ptr; (void)an; return 0;
}
void GetCharSize(int WhatChar, int *ancho, int *alto) {
  (void)WhatChar; if (ancho) *ancho = 8; if (alto) *alto = 8;
}
void ConvertFntToPal(char *Buffer) { (void)Buffer; }

/* ------------------------------------------------------------------------
 *  Grabacion de sonido (RecordSound()/PollRecord(), divpcm.cpp): via DSP y
 *  DMA reales de Sound Blaster (divsb.cpp) + registros del mezclador
 *  (divmixer.cpp), acceso directo a hardware ISA de 1999 sin ningun
 *  equivalente moderno -- igual que el resto de divsb.cpp/divmixer.cpp
 *  (ver docs/architecture/13-handoff.md, hito "audio del IDE"), no se
 *  portan. No-ops reales (no datos: el codigo real SI llama a estas, desde
 *  RecordSound()/PollRecord()): la grabacion queda deshabilitada (mismo
 *  criterio que JPG en el editor de imagenes), en vez de crashear con un
 *  0xC0000005 al pulsar "Grabar" en el editor de sonido.
 * ---------------------------------------------------------------------- */

unsigned judascfg_port = 0;
unsigned char DmaBuf = 0;

void timer_uninit(void) { }
void SetCDVolume(unsigned short vol) { (void)vol; }
void MIX_Reset(void) { }
void MIX_SetInput(unsigned char opt) { (void)opt; }
void MIX_SetVolume(unsigned char reg, unsigned char left, unsigned char right) { (void)reg; (void)left; (void)right; }
void set_mixer(void) { }
int sbinit(void) { return 0; }
void sbsettc(unsigned char tc) { (void)tc; }
void sbrec(unsigned char far *buf, unsigned len) { (void)buf; (void)len; }
void spkon(void) { }
void spkoff(void) { }
unsigned dmacount(unsigned char channel) { (void)channel; return 0; }
unsigned char dmastatus(void) { return 0; }

/* PORT: last_x/last_y son estado propio del editor de mapas 3D
 * (MapperCreator2, divmap3d.cpp), no del generador de sprites -- ambos
 * modulos declaran variables con el mismo nombre para lo mismo (tracking
 * de arrastre del raton) pero solo divspr.cpp (aparcado, no compilado) las
 * definia. divmap3d.cpp las declara "extern int last_x, last_y;". */
int last_x = 0, last_y = 0;
