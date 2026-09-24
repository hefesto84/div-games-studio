#ifndef __T_H_
#define __T_H_

//컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�
//  Procedimientos en ensamblador
//컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�

#include <stdint.h>
/* PORT: era "int" (asumia puntero de 32 bits, valido en DOS); en x64
 * truncaba el puntero real asignado por llrender.cpp y producia lecturas
 * fuera de rango. */
extern intptr_t direccion_textura;

//extern "C" {

void nucleo8_8(int v, int u, int du, int dv, int ancho, short *destino);
void nucleo8_16(int v, int u, int du, int dv, int ancho, short *destino);
void nucleo8_32(int v, int u, int du, int dv, int ancho, short *destino);
void nucleo8_64(int v, int u, int du, int dv, int ancho, short *destino);
void nucleo8_128(int v, int u, int du, int dv, int ancho, short *destino);
void nucleo8_256(int v, int u, int du, int dv, int ancho, short *destino);

void mask_nucleo8_8(int v, int u, int du, int dv, int ancho, short *destino);
void mask_nucleo8_16(int v, int u, int du, int dv, int ancho, short *destino);
void mask_nucleo8_32(int v, int u, int du, int dv, int ancho, short *destino);
void mask_nucleo8_64(int v, int u, int du, int dv, int ancho, short *destino);
void mask_nucleo8_128(int v, int u, int du, int dv, int ancho, short *destino);
void mask_nucleo8_256(int v, int u, int du, int dv, int ancho, short *destino);

//};

#endif
