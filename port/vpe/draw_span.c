/* PORT: reemplaza los 3 span-painters en ensamblador Watcom (draw_wa.asm,
 * draw_fa.asm, draw_oa.asm + sus *.inc). Son bucles simples de stepping en
 * punto fijo 16.16 con lookup de paleta (sombreado) o de tabla de
 * traslucencia 256x256 -- no hace falta preservar los trucos de registros
 * del original (eran optimizaciones para un 386), un bucle C directo con
 * las mismas formulas de indice es equivalente y mucho mas mantenible.
 *
 * Traslucencia: el indice de la tabla es ghost[(texel<<8)+dest], mismo
 * orden que usa el resto del runtime DIV ya portado (ver s.cpp: "ghost[(*p<<8)+*q]",
 * *p=origen/texel, *q=destino). Pal.Trans apunta a "ghost" (load.c,
 * LoadPalette). */
#include "internal.h"

/* -------------------------------------------------------------------- */
/*  Paredes / suelo-techo verticales (WLine): paso 1D, PixPtr avanza     */
/*  BufWidth por fila (columna de pantalla).                             */
/* -------------------------------------------------------------------- */

void DrawWSpan(struct WLine *w)
{
  BYTE *pix=w->PixPtr;
  FIXED coord=w->Coord;
  int i;

  for(i=0;i<w->Count;i++) {
    BYTE texel=w->RawPtr[(coord>>16)&w->Mask];
    *pix=w->PalPtr[texel];
    pix+=w->BufWidth;
    coord+=w->Delta;
  }
}

void DrawMaskWSpan(struct WLine *w)
{
  BYTE *pix=w->PixPtr;
  FIXED coord=w->Coord;
  int i;

  for(i=0;i<w->Count;i++) {
    BYTE texel=w->RawPtr[(coord>>16)&w->Mask];
    if (texel!=0)
      *pix=w->PalPtr[texel];
    pix+=w->BufWidth;
    coord+=w->Delta;
  }
}

void DrawTransWSpan(struct WLine *w)
{
  BYTE *pix=w->PixPtr;
  FIXED coord=w->Coord;
  int i;

  for(i=0;i<w->Count;i++) {
    BYTE texel=w->RawPtr[(coord>>16)&w->Mask];
    *pix=w->PalPtr[((int)texel<<8)+*pix];
    pix+=w->BufWidth;
    coord+=w->Delta;
  }
}

/* -------------------------------------------------------------------- */
/*  Objetos/sprites verticales (WLine reusada, ViewWidth==BufWidth):     */
/*  mismo paso 1D que las paredes, RawPtr ya apunta a la fila del sprite.*/
/* -------------------------------------------------------------------- */

void DrawOSpan(struct WLine *w)
{
  BYTE *pix=w->PixPtr;
  FIXED coord=w->Coord;
  int i;

  for(i=0;i<w->Count;i++) {
    BYTE texel=w->RawPtr[coord>>16];
    *pix=w->PalPtr[texel];
    pix+=w->BufWidth;
    coord+=w->Delta;
  }
}

void DrawMaskOSpan(struct WLine *w)
{
  BYTE *pix=w->PixPtr;
  FIXED coord=w->Coord;
  int i;

  for(i=0;i<w->Count;i++) {
    BYTE texel=w->RawPtr[coord>>16];
    if (texel!=0)
      *pix=w->PalPtr[texel];
    pix+=w->BufWidth;
    coord+=w->Delta;
  }
}

void DrawTransOSpan(struct WLine *w)
{
  BYTE *pix=w->PixPtr;
  FIXED coord=w->Coord;
  int i;

  for(i=0;i<w->Count;i++) {
    BYTE texel=w->RawPtr[coord>>16];
    *pix=w->PalPtr[((int)texel<<8)+*pix];
    pix+=w->BufWidth;
    coord+=w->Delta;
  }
}

/* -------------------------------------------------------------------- */
/*  Suelo/techo horizontal (FLine): paso 2D (U,V), PixPtr avanza 1       */
/*  byte por pixel (es una fila de pantalla). Textura cuadrada de lado   */
/*  1<<Width2 (misma asuncion que el original: un solo Width2 para       */
/*  ambos ejes).                                                         */
/* -------------------------------------------------------------------- */

void DrawFSpan(struct FLine *f)
{
  BYTE *pix=f->PixPtr;
  FIXED u=f->U, v=f->V;
  FIXED du=f->dU, dv=f->dV;
  int mask=(1<<f->Width2)-1;
  int width=1<<f->Width2;
  int i;

  for(i=0;i<f->Count;i++) {
    int tx=(u>>16)&mask;
    int ty=(v>>16)&mask;
    BYTE texel=f->RawPtr[ty*width+tx];
    *pix=f->PalPtr[texel];
    pix++;
    u+=du; v+=dv;
  }
}
