/*
 * Prueba de humo: verifica en caliente (no solo "compila limpio") las
 * funciones YA PORTADAS de v.cpp/mouse.cpp/divkeybo.cpp/det_vesa.cpp
 * (svmode/volcado/set_dac, readmouse, tecla, detectar_vesa), sin
 * necesitar el interprete completo ni un fichero de bytecode -- util
 * como test de regresion rapido si se toca alguno de esos ficheros mas
 * adelante. NO es parte del runtime del port en si (no se enlaza en
 * div_core); es su propio ejecutable de diagnostico, ver target
 * `div_smoke_test` en CMakeLists.txt.
 *
 * Reutiliza el mismo patron que usa i.cpp: define DEFINIR_AQUI e incluye
 * inter.h para obtener almacenamiento real de las variables GLOBAL que
 * v.cpp/mouse.cpp/divkeybo.cpp esperan (vga_an, paleta, dac, mouse,
 * kbdFLAGS, mem, etc.), sin arrastrar el resto de dependencias de i.cpp
 * (interprete, DLLs, sonido...).
 *
 * Se auto-cierra tras 360 frames (~6s a 60 fps) para poder correrlo sin
 * interaccion manual; ESC tambien cierra antes.
 *
 * Bugs reales encontrados y corregidos escribiendo esta prueba (ver
 * docs/architecture/12-port-progreso.md, checkpoint 7): `video_modes`
 * y `mem` son punteros GLOBAL que en el runtime real (i.cpp) se apuntan
 * dentro de un bloque de memoria reservado en la inicializacion; aqui
 * hay que inicializarlos a mano (mem con calloc, video_modes a un array
 * local) o se obtiene una escritura a traves de NULL.
 */
#define DEFINIR_AQUI
#include "port_pre.h"
#include "inter.h"

#include <stdio.h>

void svmode(void);
void volcado(byte *p);
void set_dac(void);
void readmouse(void);
void tecla(void);
void detectar_vesa(void);

extern byte *vga;
struct _mouse mouse_instance;
struct _video_modes vmodes_storage[10];
int fli_palette_update=0;
int _mouse_x=0,_mouse_y=0;

int main(void) {
  byte framebuf[320*200];
  byte pal[768];
  int x,y,n,frame=0;
  double t=0.0;

  setvbuf(stdout, NULL, _IONBF, 0);

  mem=(int*)calloc(20000,sizeof(int)); /* varias macros (num_video_modes, shift_status, ascii, scan_code...) leen/escriben mem[end_struct+N] */

  vga_an=320; vga_al=200;
  mouse=&mouse_instance;
  video_modes=vmodes_storage; /* en el runtime real apunta dentro de mem[]; aqui basta un array local */

  printf("[smoke] llamando a detectar_vesa()...\n");
  detectar_vesa();
  printf("[smoke] num_video_modes=%d VersionVesa=%d\n", num_video_modes, VersionVesa);

  printf("[smoke] llamando a svmode() (deberia abrir la ventana raylib)...\n");
  svmode();
  if (!vga) { printf("[smoke] FALLO: svmode() no devolvio framebuffer\n"); return 1; }
  printf("[smoke] OK: vga=%p (framebuffer real de port/io)\n", (void*)vga);

  while (frame<360 && !key(_ESC)) {
    tecla();
    readmouse();

    /* paleta: barrido de tono simple para probar set_dac()/io_palette_set */
    for (n=0;n<256;n++) {
      pal[n*3+0]=(byte)((n+frame)&63);
      pal[n*3+1]=(byte)((n*2+frame)&63);
      pal[n*3+2]=(byte)((n*3+frame)&63);
    }
    memcpy(paleta,pal,768);
    set_dac();

    /* patron de prueba + cursor del raton (readmouse real) */
    for (y=0;y<vga_al;y++)
      for (x=0;x<vga_an;x++)
        framebuf[y*vga_an+x]=(byte)((x+y+frame)&255);

    if (mouse->x>=0 && mouse->x<vga_an && mouse->y>=0 && mouse->y<vga_al)
      framebuf[mouse->y*vga_an+mouse->x]=255;

    volcado(framebuf);

    if ((frame%60)==0)
      printf("[smoke] frame=%d mouse=(%d,%d) left=%d key_A=%d\n",
             frame, mouse->x, mouse->y, mouse->left, key(_A));

    frame++;
  }

  printf("[smoke] salida normal tras %d frames\n", frame);
  return 0;
}
