/* PORT: reemplaza src/vpe/gfx.cpp. InitGraph/ShutGraph/GetGraphWidth/
 * GetGraphHeight son triviales, sin cambios de fondo. SetPalette (outp al
 * DAC VGA) no se llama nunca en el flujo real (confirmado por grep en
 * src/vpe/) -- el port ya gestiona la paleta via port/io (io_palette_set,
 * alimentada de "paleta[]" de DIV) -- se deja como no-op documentado.
 * DrawBuffer/draw_buffer sustituyen el "#pragma aux" en ensamblador Watcom
 * por un memcpy rectangular en C plano; en la práctica View->ScrX/ScrY
 * siempre son 0 y View->Buffer ya apunta directo a la región de "copia"
 * que usa loop_mode8() (ver vpedll.c), así que esto es un auto-copy
 * inofensivo, no una ruta caliente. */
#include "vpe.h"
#include "gfx.h"
#include <string.h>

/* PORT: puntero real (no DWORD truncado a 32 bits como el original DOS
 * -- aqui si importa, div32run_port no tiene la mitigacion de direcciones
 * <2GB que usa el IDE, ver CMakeLists.txt). */
BYTE *ScrBase;
int ScrWidth, ScrHeight;
int GfxOn=FALSE;

void InitGraph(char *buffer,int ancho,int alto)
{
  GfxOn=TRUE;
  ScrBase=(BYTE *)buffer;
  ScrWidth=ancho;
  ScrHeight=alto;
}

void ShutGraph(void)
{
  GfxOn=FALSE;
}

int GetGraphWidth(void)
{
  return(ScrWidth);
}

int GetGraphHeight(void)
{
  return(ScrHeight);
}

void SetPalette(BYTE *pal_ptr)
{
  (void)pal_ptr;
}

void DrawBuffer(struct View *v)
{
  BYTE *src, *dest;
  int row;

  src=v->Buffer;
  dest=ScrBase+v->ScrX+v->ScrY*ScrWidth;
  for(row=0;row<v->Height;row++) {
    memcpy(dest,src,v->Width);
    src+=v->Width;
    dest+=ScrWidth;
  }
}
